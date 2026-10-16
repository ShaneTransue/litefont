#include <litefont/FontFamily.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include "TestFontBuilder.h"

#include <cmath>

namespace litefont::tests {

class FontFamilyTests {
	static Bytes makeFace(std::uint16_t weight, bool bold, bool italic, std::int16_t angle) {
		TestFontBuilder builder;
		builder.setStyle(weight, bold, italic, angle);
		TestGlyph glyph;
		glyph.contours = {{{0, 0, true}, {0, 500, true}, {400, 500, true}, {400, 0, true}}};
		builder.map(U'a', builder.addGlyph(glyph));
		return builder.build();
	}

	static inline test::Test traits{"Font: reads its weight, bold and italic flags and italic angle", [](test::Context& context) {
		Font bold;
		bold.load(makeFace(700, true, false, 0));
		context.check(bold.getTraits().bold && !bold.getTraits().italic && bold.getTraits().weight == 700, "bold from OS/2");
		Font italic;
		italic.load(makeFace(400, false, true, -12));
		context.check(italic.getTraits().italic && !italic.getTraits().bold, "italic from OS/2");
		context.checkEqual(italic.getTraits().italicAngle, -12.0f);

		TestFontBuilder noOs2;
		noOs2.setStyle(700, true, true, -9).omitTable("OS/2");
		Font fromHead;
		fromHead.load(noOs2.build());
		context.check(fromHead.getTraits().bold && fromHead.getTraits().italic, "macStyle when OS/2 is missing");
		context.checkEqual(fromHead.getTraits().weight, std::uint16_t{700});

		Font plain;
		plain.load(TestFontBuilder().build());
		context.check(!plain.getTraits().bold && !plain.getTraits().italic && plain.getTraits().weight == 400, "regular by default");
		context.checkEqual(plain.getTraits().italicAngle, 0.0f);
	}};

	static inline test::Test realFaces{"FontFamily: each style uses its own face when the family has it", [](test::Context& context) {
		Font regular;
		Font bold;
		Font italic;
		Font boldItalic;
		regular.load(makeFace(400, false, false, 0));
		bold.load(makeFace(700, true, false, 0));
		italic.load(makeFace(400, false, true, -12));
		boldItalic.load(makeFace(700, true, true, -12));
		FontFamily family;
		for (const Font* face : {&boldItalic, &italic, &regular, &bold}) {
			context.check(family.addFace(*face), "a loaded face is added");
		}
		context.check(family.getRegular() == &regular, "the regular face is primary");
		context.check(family.resolve(FontStyle::Regular) == FontFace{&regular, false, false}, "regular");
		context.check(family.resolve(FontStyle::Bold) == FontFace{&bold, false, false}, "bold");
		context.check(family.resolve(FontStyle::Italic) == FontFace{&italic, false, false}, "italic");
		context.check(family.resolve(FontStyle::BoldItalic) == FontFace{&boldItalic, false, false}, "bold italic");
		context.check(!family.addFace(Font()), "an unloaded font is refused");
	}};

	static inline test::Test fallbacks{"FontFamily: missing faces fall back and simulate only what the face lacks", [](test::Context& context) {
		Font regular;
		Font bold;
		regular.load(makeFace(400, false, false, 0));
		bold.load(makeFace(700, true, false, 0));
		FontFamily pair;
		pair.addFace(regular);
		pair.addFace(bold);
		context.check(pair.resolve(FontStyle::Italic) == FontFace{&regular, false, true}, "italic slants the regular face");
		context.check(pair.resolve(FontStyle::BoldItalic) == FontFace{&bold, false, true}, "bold italic slants the bold face");

		const FontFamily single(regular);
		context.check(single.resolve(FontStyle::Bold) == FontFace{&regular, true, false}, "bold thickens a lone font");
		context.check(single.resolve(FontStyle::BoldItalic) == FontFace{&regular, true, true}, "and bold italic does both");

		const FontFamily alreadyBold(bold);
		context.check(alreadyBold.resolve(FontStyle::Bold) == FontFace{&bold, false, false}, "a bold font is not thickened again");
		context.check(alreadyBold.resolve(FontStyle::BoldItalic) == FontFace{&bold, false, true}, "only slanted");
		context.check(FontFamily().resolve(FontStyle::Bold).font == nullptr, "an empty family has no face");
		context.check(std::fabs(FontFamily::getItalicSlant() - std::tan(12.0f * 3.14159265f / 180.0f)) < 1e-6f, "12 degrees");
		context.checkEqual(FontFamily::getBoldStrength(regular), 1000.0f / 24.0f);
	}};

	static Bytes makeCoverage(std::vector<char32_t> codePoints, bool bold = false) {
		TestFontBuilder builder;
		builder.setStyle(bold ? 700 : 400, bold, false, 0);
		for (const char32_t codePoint : codePoints) {
			builder.map(codePoint, builder.addGlyph(TestGlyph{}));
		}
		return builder.build();
	}

	static inline test::Test fallbackFamilies{"FontFamily: code points the family lacks come from fallback families, in order", [](test::Context& context) {
		Font latin;
		Font symbols;
		Font chinese;
		Font chineseBold;
		Font beyond;
		latin.load(makeCoverage({U'a', U'b'}));
		symbols.load(makeCoverage({U'a', 0x2192, U'?', 0x0001}));
		chinese.load(makeCoverage({0x4E2D, 0x2192, 0xFFFD}));
		chineseBold.load(makeCoverage({0x4E2D}, true));
		beyond.load(makeCoverage({0x263A}));

		FontFamily symbolFamily(symbols);
		FontFamily chineseFamily(chinese);
		chineseFamily.addFace(chineseBold);
		const FontFamily beyondFamily(beyond);
		symbolFamily.addFallback(beyondFamily);
		FontFamily family(latin);
		context.check(family.addFallback(symbolFamily), "a fallback is added");
		context.check(family.addFallback(chineseFamily), "and a second one");
		context.check(!family.addFallback(symbolFamily), "but not twice");
		context.check(!family.addFallback(family), "and never itself");
		context.checkEqual(family.getFallbacks().size(), std::size_t{2});

		auto check = [&context](const FontGlyph& found, const Font* font, GlyphId glyph, const char* what) {
			context.check(found.face.font == font && found.glyph == glyph, what);
		};
		check(family.resolve(FontStyle::Regular, U'a'), &latin, 1, "the family's own glyph comes first");
		check(family.resolve(FontStyle::Regular, 0x2192), &symbols, 2, "then the first fallback that has it");
		check(family.resolve(FontStyle::Regular, 0x4E2D), &chinese, 1, "then the next");
		check(family.resolve(FontStyle::Bold, 0x4E2D), &chineseBold, 1, "a fallback uses its own face for the style");
		context.check(family.resolve(FontStyle::Bold, U'a').face == FontFace{&latin, true, false}, "the family's simulation stays");
		check(family.resolve(FontStyle::Regular, 0x263A), &chinese, 3, "fallbacks of fallbacks are not followed: U+FFFD instead");
		check(family.resolve(FontStyle::Regular, 0x1F600), &chinese, 3, "nobody has it: the replacement character");
		check(family.resolve(FontStyle::Regular, 0x0001), &latin, 0, "control characters never fall back");

		family.setReplacementCharacter(U'?');
		context.checkEqual(family.getReplacementCharacter(), char32_t{U'?'});
		check(family.resolve(FontStyle::Regular, 0x1F600), &symbols, 3, "another replacement character");
		family.setReplacementCharacter(0x25A1);
		check(family.resolve(FontStyle::Regular, 0x1F600), &symbols, 3, "a missing replacement shows a question mark");

		family.clearFallbacks();
		check(family.resolve(FontStyle::Regular, 0x4E2D), &latin, 0, "without fallbacks, the missing glyph");
		check(symbolFamily.resolve(FontStyle::Italic, 0x263A), &beyond, 1, "the symbol family uses its own fallback");
	}};

	static inline test::Test installed{"FontFamily: Segoe UI's four local faces resolve to themselves", [](test::Context& context) {
		Font faces[4];
		const char* names[] = {"segoeui", "segoeuib", "segoeuii", "segoeuiz"};
		FontFamily family;
		for (std::size_t i = 0; i < 4; i++) {
			if (!faces[i].load(test::Context::getFontFolder() / (std::string(names[i]) + ".ttf"))) {
				context.note(std::format("{} is not in the fonts folder; skipped", names[i]));
				return;
			}
			family.addFace(faces[i]);
		}
		const FontStyle styles[] = {FontStyle::Regular, FontStyle::Bold, FontStyle::Italic, FontStyle::BoldItalic};
		for (std::size_t i = 0; i < 4; i++) {
			context.check(family.resolve(styles[i]) == FontFace{&faces[i], false, false}, std::format("{} resolves to itself", names[i]));
		}
		context.checkEqual(faces[1].getTraits().weight, std::uint16_t{700});
		context.checkEqual(faces[3].getTraits().italicAngle, -12.0f);
	}};
};

}
