#include "TestFontBuilder.h"

#include <litefont/Font.h>
#include <litefont/Hash.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <filesystem>
#include <system_error>

namespace litefont::tests {

class FontTests {
	static TestGlyph makeSquare(std::int16_t size, std::uint16_t advance = 600) {
		TestGlyph glyph;
		glyph.contours = {{{0, 0, true}, {size, 0, true}, {size, size, true}, {0, size, true}}};
		glyph.advance = advance;
		return glyph;
	}

	static TestGlyph makeContour(std::vector<TestPoint> points) {
		TestGlyph glyph;
		glyph.contours = {std::move(points)};
		return glyph;
	}

	static std::string outlineOf(const Font& font, GlyphId glyph) {
		Outline outline;
		return font.getOutline(glyph, outline) ? outline.toSvgPath() : std::string("<failed>");
	}

	static inline test::Test readsMetrics{"Font: reads units per em, line metrics and glyph count", [](test::Context& context) {
		TestFontBuilder builder;
		builder.setLineMetrics(2048, 1900, -500, 67);
		builder.addGlyph(makeSquare(100));
		builder.map(U'A', 1);

		Font font;
		context.check(font.load(builder.build()), "the font loads");
		context.check(font.isLoaded(), "and says so");
		context.checkEqual(font.getError(), FontError::None);
		context.checkEqual(font.getMetrics().unitsPerEm, std::uint16_t{2048});
		context.checkEqual(font.getMetrics().ascender, std::int16_t{1900});
		context.checkEqual(font.getMetrics().descender, std::int16_t{-500});
		context.checkEqual(font.getMetrics().lineGap, std::int16_t{67});
		context.checkEqual(font.getGlyphCount(), std::uint16_t{2});
	}};

	static inline test::Test usesTypoMetricsWhenAsked{"Font: OS/2 typographic metrics replace hhea only when USE_TYPO_METRICS is set", [](test::Context& context) {
		TestFontBuilder plain;
		plain.setLineMetrics(1000, 900, -300, 0).setOs2(750, -250, 100, false, 480, 690);
		Font withoutFlag;
		withoutFlag.load(plain.build());
		context.checkEqual(withoutFlag.getMetrics().ascender, std::int16_t{900});
		context.checkEqual(withoutFlag.getMetrics().xHeight, std::int16_t{480});
		context.checkEqual(withoutFlag.getMetrics().capHeight, std::int16_t{690});

		TestFontBuilder typo;
		typo.setLineMetrics(1000, 900, -300, 0).setOs2(750, -250, 100, true, 480, 690);
		Font withFlag;
		withFlag.load(typo.build());
		context.checkEqual(withFlag.getMetrics().ascender, std::int16_t{750});
		context.checkEqual(withFlag.getMetrics().descender, std::int16_t{-250});
		context.checkEqual(withFlag.getMetrics().lineGap, std::int16_t{100});
	}};

	static inline test::Test estimatesMissingHeights{"Font: estimates x-height and cap-height from x and H without OS/2", [](test::Context& context) {
		TestFontBuilder builder;
		const std::uint16_t x = builder.addGlyph(makeSquare(500));
		const std::uint16_t h = builder.addGlyph(makeSquare(700));
		builder.map(U'x', x).map(U'H', h);
		Font font;
		font.load(builder.build());
		context.checkEqual(font.getMetrics().xHeight, std::int16_t{500});
		context.checkEqual(font.getMetrics().capHeight, std::int16_t{700});
	}};

	static inline test::Test mapsFormat4Deltas{"Font: maps code points through cmap format 4 deltas", [](test::Context& context) {
		TestFontBuilder builder;
		for (std::int32_t i = 0; i < 5; i++) {
			builder.addGlyph(makeSquare(100));
		}
		builder.map(U'A', 1).map(U'B', 2).map(U'C', 5).map(0x00E9, 3);
		Font font;
		font.load(builder.build());
		context.checkEqual(font.getGlyph(U'A'), GlyphId{1});
		context.checkEqual(font.getGlyph(U'B'), GlyphId{2});
		context.checkEqual(font.getGlyph(U'C'), GlyphId{5});
		context.checkEqual(font.getGlyph(0x00E9), GlyphId{3});
		context.checkEqual(font.getGlyph(U'Z'), GlyphId{0});
		context.checkEqual(font.getGlyph(0x1F600), GlyphId{0});
	}};

	static inline test::Test mapsFormat4Arrays{"Font: maps code points through format 4 glyph id arrays", [](test::Context& context) {
		TestFontBuilder builder;
		for (std::int32_t i = 0; i < 9; i++) {
			builder.addGlyph(makeSquare(100));
		}
		builder.setGlyphIdArray(true);
		builder.map(U'a', 5).map(U'b', 9).map(U'c', 2).map(U'x', 7);
		Font font;
		font.load(builder.build());
		context.checkEqual(font.getGlyph(U'a'), GlyphId{5});
		context.checkEqual(font.getGlyph(U'b'), GlyphId{9});
		context.checkEqual(font.getGlyph(U'c'), GlyphId{2});
		context.checkEqual(font.getGlyph(U'x'), GlyphId{7});
		context.checkEqual(font.getGlyph(U'd'), GlyphId{0});
	}};

	static inline test::Test mapsFormat12{"Font: maps supplementary code points through cmap format 12", [](test::Context& context) {
		TestFontBuilder builder;
		for (std::int32_t i = 0; i < 4; i++) {
			builder.addGlyph(makeSquare(100));
		}
		builder.setCharacterMapFormat(12);
		builder.map(U'A', 1).map(U'B', 2).map(0x1F600, 3).map(0x1F601, 4);
		Font font;
		font.load(builder.build());
		context.checkEqual(font.getGlyph(U'B'), GlyphId{2});
		context.checkEqual(font.getGlyph(0x1F600), GlyphId{3});
		context.checkEqual(font.getGlyph(0x1F601), GlyphId{4});
		context.checkEqual(font.getGlyph(0x1F602), GlyphId{0});
	}};

	static inline test::Test repeatsLastAdvance{"Font: glyphs past the horizontal metric count repeat the last advance", [](test::Context& context) {
		TestFontBuilder builder;
		TestGlyph first = makeSquare(100, 700);
		first.leftSideBearing = 11;
		TestGlyph second = makeSquare(100, 800);
		second.leftSideBearing = 22;
		TestGlyph third = makeSquare(100, 900);
		third.leftSideBearing = 33;
		builder.addGlyph(first);
		builder.addGlyph(second);
		builder.addGlyph(third);
		builder.setHorizontalMetricCount(2);

		Font font;
		font.load(builder.build());
		context.checkEqual(font.getGlyphMetrics(1).advance, std::uint16_t{700});
		context.checkEqual(font.getGlyphMetrics(1).leftSideBearing, std::int16_t{11});
		context.checkEqual(font.getGlyphMetrics(2).advance, std::uint16_t{700});
		context.checkEqual(font.getGlyphMetrics(2).leftSideBearing, std::int16_t{22});
		context.checkEqual(font.getGlyphMetrics(3).advance, std::uint16_t{700});
		context.checkEqual(font.getGlyphMetrics(3).leftSideBearing, std::int16_t{33});
		context.checkEqual(font.getGlyphMetrics(99).advance, std::uint16_t{0});
	}};

	static inline test::Test straightContours{"Font: straight contours close without a final line", [](test::Context& context) {
		TestFontBuilder builder;
		const std::uint16_t square = builder.addGlyph(makeSquare(100));
		Font font;
		font.load(builder.build());
		context.checkEqual(outlineOf(font, square), std::string("M 0 0 L 100 0 L 100 100 L 0 100 Z"));
	}};

	static inline test::Test impliedMidpoints{"Font: runs of off-curve points split at their implied midpoints", [](test::Context& context) {
		TestFontBuilder builder;
		const std::uint16_t glyph = builder.addGlyph(makeContour({{0, 0, true}, {100, 0, false}, {100, 100, false}, {0, 100, true}}));
		Font font;
		font.load(builder.build());
		context.checkEqual(outlineOf(font, glyph), std::string("M 0 0 Q 100 0 100 50 Q 100 100 0 100 Z"));
	}};

	static inline test::Test startsAtFirstOnCurve{"Font: contours that begin off-curve start at their first on-curve point", [](test::Context& context) {
		TestFontBuilder builder;
		const std::uint16_t glyph = builder.addGlyph(makeContour({{0, 0, false}, {100, 0, true}, {100, 100, true}}));
		Font font;
		font.load(builder.build());
		context.checkEqual(outlineOf(font, glyph), std::string("M 100 0 L 100 100 Q 0 0 100 0 Z"));
	}};

	static inline test::Test allOffCurve{"Font: contours without on-curve points start at an implied midpoint", [](test::Context& context) {
		TestFontBuilder builder;
		const std::uint16_t glyph = builder.addGlyph(makeContour({{0, 0, false}, {100, 0, false}, {100, 100, false}, {0, 100, false}}));
		Font font;
		font.load(builder.build());
		context.checkEqual(outlineOf(font, glyph), std::string("M 0 50 Q 0 0 50 0 Q 100 0 100 50 Q 100 100 50 100 Q 0 100 0 50 Z"));
	}};

	static inline test::Test bigCoordinates{"Font: decodes 16-bit, short, repeated and negative coordinates", [](test::Context& context) {
		TestFontBuilder builder;
		const std::uint16_t glyph = builder.addGlyph(makeContour({{-1000, 5, true}, {2000, 5, true}, {2000, -300, true}, {1999, -300, true}}));
		Font font;
		font.load(builder.build());
		context.checkEqual(outlineOf(font, glyph), std::string("M -1000 5 L 2000 5 L 2000 -300 L 1999 -300 Z"));
	}};

	static inline test::Test composites{"Font: composite glyphs place scaled and offset components", [](test::Context& context) {
		TestFontBuilder builder;
		const std::uint16_t square = builder.addGlyph(makeSquare(100));
		TestGlyph composite;
		composite.components = {{square, 10, 20, 0.5f, false}, {square, 200, 0, 1.0f, false}};
		const std::uint16_t glyph = builder.addGlyph(composite);
		Font font;
		font.load(builder.build());
		context.checkEqual(outlineOf(font, glyph),
			std::string("M 10 20 L 60 20 L 60 70 L 10 70 Z M 200 0 L 300 0 L 300 100 L 200 100 Z"));
	}};

	static inline test::Test rejectsBadComposites{"Font: self-referencing and point-matched composites fail without failing the font", [](test::Context& context) {
		TestFontBuilder builder;
		const std::uint16_t square = builder.addGlyph(makeSquare(100));
		TestGlyph loop;
		loop.components = {{2, 0, 0, 1.0f, false}};        // glyph 2 is itself
		const std::uint16_t looping = builder.addGlyph(loop);
		TestGlyph matched;
		matched.components = {{square, 0, 1, 1.0f, true}};
		const std::uint16_t pointMatched = builder.addGlyph(matched);

		Font font;
		context.check(font.load(builder.build()), "the font still loads");
		Outline outline;
		context.check(!font.getOutline(looping, outline), "a self-referencing composite fails");
		context.check(outline.isEmpty(), "and leaves the outline empty");
		context.check(!font.getOutline(pointMatched, outline), "a point-matched composite fails");
		context.check(font.getOutline(square, outline), "other glyphs still work");
	}};

	static inline test::Test emptyGlyphs{"Font: empty glyphs succeed with an empty outline", [](test::Context& context) {
		TestFontBuilder builder;
		TestGlyph space;
		space.advance = 250;
		const std::uint16_t glyph = builder.addGlyph(space);
		Font font;
		font.load(builder.build());
		Outline outline;
		context.check(font.getOutline(glyph, outline), "empty glyph succeeds");
		context.check(outline.isEmpty(), "with nothing in it");
		context.check(!font.getOutline(99, outline), "a glyph id past the end fails");
		context.checkEqual(font.getGlyphMetrics(glyph).advance, std::uint16_t{250});
	}};

	static inline test::Test kerning{"Font: kerning pairs are found, and missing pairs are 0", [](test::Context& context) {
		TestFontBuilder builder;
		for (std::int32_t i = 0; i < 4; i++) {
			builder.addGlyph(makeSquare(100));
		}
		builder.addKerning(2, 1, 15).addKerning(1, 3, -20).addKerning(1, 2, -50);
		Font font;
		font.load(builder.build());
		context.checkEqual(font.getKerning(1, 2), std::int16_t{-50});
		context.checkEqual(font.getKerning(1, 3), std::int16_t{-20});
		context.checkEqual(font.getKerning(2, 1), std::int16_t{15});
		context.checkEqual(font.getKerning(3, 1), std::int16_t{0});
	}};

	static inline test::Test locationFormats{"Font: short and long location tables give the same outlines", [](test::Context& context) {
		auto buildWith = [](bool longLocations) {
			TestFontBuilder builder;
			builder.addGlyph(makeSquare(100));
			builder.addGlyph(makeContour({{0, 0, true}, {50, 80, false}, {100, 0, true}}));
			builder.setLongLocations(longLocations);
			return builder.build();
		};
		Font shortFont;
		Font longFont;
		shortFont.load(buildWith(false));
		longFont.load(buildWith(true));
		context.checkEqual(outlineOf(shortFont, 2), outlineOf(longFont, 2));
		context.checkEqual(outlineOf(shortFont, 1), outlineOf(longFont, 1));
	}};

	static inline test::Test loadsFromFiles{"Font: loads from a path and reports missing files", [](test::Context& context) {
		TestFontBuilder builder;
		builder.addGlyph(makeSquare(100));
		std::error_code error;
		const std::filesystem::path path = std::filesystem::temp_directory_path(error) / "px_font_test.ttf";
		File::write(path, builder.build());

		Font font;
		context.check(font.load(path), "loads from the file");
		context.check(!font.load(path.parent_path() / "px_font_missing.ttf"), "a missing file fails");
		context.checkEqual(font.getError(), FontError::File);
		context.check(!font.isLoaded(), "and the font is unloaded");
		std::filesystem::remove(path, error);
	}};

	static inline test::Test rejectsWrongFiles{"Font: rejects unknown formats, missing faces, truncated headers and missing tables", [](test::Context& context) {
		Font font;
		ByteWriter unknown;
		unknown.write(std::uint32_t{0x774F4646u});    // 'wOFF'
		unknown.write(std::uint16_t{0});
		unknown.writeBuffer(std::vector<std::uint8_t>(6, 0));
		context.check(!font.load(unknown.takeBytes()), "WOFF is not supported");
		context.checkEqual(font.getError(), FontError::UnsupportedFormat);

		ByteWriter cff;
		cff.write(std::uint32_t{0x4F54544Fu});    // 'OTTO'
		cff.write(std::uint16_t{0});
		cff.writeBuffer(std::vector<std::uint8_t>(6, 0));
		context.check(!font.load(cff.takeBytes()), "a CFF font without tables fails");
		context.checkEqual(font.getError(), FontError::MissingTable);

		TestFontBuilder single;
		single.addGlyph(makeSquare(100));
		context.check(!font.load(single.build(), 1), "a single font has no face 1");
		context.checkEqual(font.getError(), FontError::MissingFace);

		ByteWriter collection;
		collection.write(std::uint32_t{0x74746366u});    // 'ttcf'
		collection.write(std::uint32_t{0x00010000u});
		collection.write(std::uint32_t{1});
		collection.write(std::uint32_t{16});
		context.check(!font.load(collection.takeBytes(), 1), "a collection of one has no face 1");
		context.checkEqual(font.getError(), FontError::MissingFace);

		context.check(!font.load(Bytes{{0, 1, 0, 0}}), "four bytes are not a font");
		context.checkEqual(font.getError(), FontError::Truncated);

		TestFontBuilder noMap;
		noMap.addGlyph(makeSquare(100));
		noMap.omitTable("cmap");
		context.check(!font.load(noMap.build()), "a font without cmap fails");
		context.checkEqual(font.getError(), FontError::MissingTable);
		context.checkEqual(font.getGlyph(U'A'), GlyphId{0});
	}};

	static inline test::Test idFollowsContent{"Font: the id is the content hash of the file", [](test::Context& context) {
		TestFontBuilder first;
		first.addGlyph(makeSquare(100));
		TestFontBuilder second;
		second.addGlyph(makeSquare(200));
		const Bytes firstBytes = first.build();

		Font a;
		Font b;
		Font c;
		a.load(firstBytes);
		b.load(firstBytes);
		c.load(second.build());
		context.checkEqual(a.getId(), b.getId());
		context.check(a.getId() != c.getId(), "different files have different ids");
		context.checkEqual(a.getId(), Hash::fnv1a(firstBytes.data));
	}};
};

}
