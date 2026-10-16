#include "TestFontBuilder.h"

#include <litefont/Font.h>
#include <litefont/Hash.h>
#include "TestKit.h"
#include "RecordingDevice.h"

namespace litefont::tests {

/**
 * Collections, names and the static readers a catalog uses to describe a
 * font without loading it.
 */
class FontFaceTests {
	static TestGlyph makeSquare(std::int16_t size) {
		TestGlyph glyph;
		glyph.contours = {{{0, 0, true}, {size, 0, true}, {size, size, true}, {0, size, true}}};
		return glyph;
	}

	static std::string outlineOf(const Font& font, GlyphId glyph) {
		Outline outline;
		return font.getOutline(glyph, outline) ? outline.toSvgPath() : std::string("<failed>");
	}

	static Bytes buildPair(Bytes& first, Bytes& second) {
		TestFontBuilder alpha;
		alpha.map(U'A', alpha.addGlyph(makeSquare(100)));
		alpha.setNames("Alpha", "Regular", "Alpha Regular");
		first = alpha.build();

		TestCff cff;
		cff.charStrings.push_back({14});
		cff.charStrings.push_back({139, 139, 21, 139 + 100, 139, 5, 14});    // 0 0 rmoveto 100 0 rlineto endchar
		TestFontBuilder beta;
		beta.setCff(cff).map(U'A', 1).setStyle(700, true, false, 0);
		beta.setNames("Beta", "Bold", "Beta Bold");
		second = beta.build();
		return TestFontBuilder::buildCollection({first, second});
	}

	static inline test::Test collections{"Font: loads each face of a collection, each with its own id", [](test::Context& context) {
		Bytes first;
		Bytes second;
		const Bytes collection = buildPair(first, second);

		context.checkEqual(Font::getFaceCount(ByteReader(collection.data)), std::uint32_t{2});
		context.checkEqual(Font::getFaceCount(ByteReader(first.data)), std::uint32_t{1});
		context.checkEqual(Font::getFaceCount(ByteReader(std::vector<std::uint8_t>{1, 2, 3, 4, 5})), std::uint32_t{0});

		Font alpha;
		context.check(alpha.load(collection, 0), "face 0 loads");
		context.checkEqual(alpha.getFace(), std::uint32_t{0});
		context.checkEqual(alpha.getNames().family, std::string("Alpha"));
		context.check(!alpha.hasPostScriptOutlines(), "face 0 has TrueType outlines");
		context.checkEqual(outlineOf(alpha, alpha.getGlyph(U'A')), std::string("M 0 0 L 100 0 L 100 100 L 0 100 Z"));
		context.checkEqual(alpha.getId(), Hash::fnv1a(collection.data));

		Font beta;
		context.check(beta.load(collection, 1), "face 1 loads");
		context.checkEqual(beta.getFace(), std::uint32_t{1});
		context.checkEqual(beta.getNames().family, std::string("Beta"));
		context.checkEqual(beta.getNames().subfamily, std::string("Bold"));
		context.check(beta.getTraits().bold, "face 1 is bold");
		context.check(beta.hasPostScriptOutlines(), "face 1 has CFF outlines");
		context.checkEqual(outlineOf(beta, beta.getGlyph(U'A')), std::string("M 0 0 L 100 0 Z"));
		context.check(alpha.getId() != beta.getId(), "the faces have different ids");

		Font single;
		single.load(first);
		context.check(single.getId() != alpha.getId(), "a font alone and inside a collection have different ids");

		Font missing;
		context.check(!missing.load(collection, 2), "there is no face 2");
		context.checkEqual(missing.getError(), FontError::MissingFace);
		context.check(!missing.load(Bytes{std::vector<std::uint8_t>(collection.data.begin(), collection.data.begin() + 20)}, 1), "a cut-off collection fails");
		context.checkEqual(missing.getError(), FontError::Truncated);
	}};

	static inline test::Test namePreference{"Font: names prefer Windows English, then other Windows names, then Unicode, then Macintosh", [](test::Context& context) {
		auto familyOf = [](TestFontBuilder builder) {
			builder.addGlyph(makeSquare(100));
			Font font;
			font.load(builder.build());
			return font.getNames().family;
		};
		TestFontBuilder all;
		all.addName(1, 0, 0, 1, "Mac Family").addName(0, 3, 0, 1, "Unicode Family");
		all.addName(3, 1, 0x040C, 1, "Famille").addName(3, 1, 0x0409, 1, "Family");
		context.checkEqual(familyOf(all), std::string("Family"));

		TestFontBuilder french;
		french.addName(1, 0, 0, 1, "Mac Family").addName(0, 3, 0, 1, "Unicode Family").addName(3, 1, 0x040C, 1, "Famille");
		context.checkEqual(familyOf(french), std::string("Famille"));

		TestFontBuilder unicode;
		unicode.addName(1, 0, 0, 1, "Mac Family").addName(0, 3, 0, 1, "Unicode Family");
		context.checkEqual(familyOf(unicode), std::string("Unicode Family"));

		TestFontBuilder mac;
		mac.addName(1, 0, 0, 1, "Mac Family").addName(1, 0, 11, 1, "Japanese Mac Family");
		context.checkEqual(familyOf(mac), std::string("Mac Family"));

		TestFontBuilder none;
		context.checkEqual(familyOf(none), std::string(""));
	}};

	static inline test::Test nameEncodings{"Font: names decode UTF-16 surrogate pairs and replace what cannot be decoded", [](test::Context& context) {
		TestFontBuilder builder;
		builder.addGlyph(makeSquare(100));
		builder.addName(3, 1, 0x0409, 1, "Caf\xC3\xA9 \xF0\x9F\x98\x80");
		builder.addRawName(3, 1, 0x0409, 2, {0xD8, 0x00, 0x00, 0x41, 0xDC, 0x00});
		builder.addRawName(1, 0, 0, 4, {0x41, 0xA5, 0x42});
		Font font;
		context.check(font.load(builder.build()), "the font loads");
		context.checkEqual(font.getNames().family, std::string("Caf\xC3\xA9 \xF0\x9F\x98\x80"));
		context.checkEqual(font.getNames().subfamily, std::string("\xEF\xBF\xBD" "A\xEF\xBF\xBD"));
		context.checkEqual(font.getNames().fullName, std::string("A\xEF\xBF\xBD" "B"));
	}};

	static inline test::Test staticReaders{"Font: the static readers describe a font from its directory and a few tables", [](test::Context& context) {
		Bytes first;
		Bytes second;
		const Bytes collection = buildPair(first, second);

		// The directory alone: the bytes up to face 1's table records are enough.
		ByteReader header(collection.data);
		std::uint32_t directory = 0;
		std::uint16_t tableCount = 0;
		header.seek(16);
		header.read(directory);
		header.seek(directory + 4);
		header.read(tableCount);
		const std::size_t prefixSize = directory + 12 + 16 * static_cast<std::size_t>(tableCount);
		FontTables tables;
		const Bytes prefix{std::vector<std::uint8_t>(collection.data.begin(), collection.data.begin() + static_cast<std::ptrdiff_t>(prefixSize))};
		context.checkEqual(Font::readTableDirectory(ByteReader(prefix.data), 1, tables), FontError::None);
		context.check(tables.cff.present && !tables.glyf.present && tables.name.present, "face 1 has CFF and names, no glyf");
		context.checkEqual(Font::readTableDirectory(ByteReader(prefix.data), 3, tables), FontError::MissingFace);

		Font::readTableDirectory(ByteReader(collection.data), 1, tables);
		auto tableOf = [&collection](const FontTable& table) {
			ByteReader reader;
			ByteReader(collection.data).slice(table.offset, table.length, reader);
			return reader;
		};
		FontTraits traits;
		Font::readTraits(tableOf(tables.head), tableOf(tables.os2), tableOf(tables.post), traits);
		context.check(traits.bold && !traits.italic, "bold, not italic");
		context.checkEqual(traits.weight, std::uint16_t{700});
		FontNames names;
		context.check(Font::readNames(tableOf(tables.name), names), "the names read");
		context.checkEqual(names.fullName, std::string("Beta Bold"));
		context.check(!Font::readNames(ByteReader(), names), "no name table, no names");
		context.checkEqual(names.family, std::string(""));

		FontTraits defaults;
		Font::readTraits(ByteReader(), ByteReader(), ByteReader(), defaults);
		context.checkEqual(defaults.weight, std::uint16_t{400});
		context.check(!defaults.bold && !defaults.italic, "missing tables give a regular face");
	}};
};

}
