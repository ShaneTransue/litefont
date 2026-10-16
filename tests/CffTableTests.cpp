#include "TestFontBuilder.h"

#include <litefont/Font.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <filesystem>
#include <random>

namespace litefont::tests {

/**
 * The Type 2 charstring interpreter, through fonts built with charstrings
 * written byte by byte. Installed .otf fonts are compared with fontTools by
 * the oracle test; these pin down each rule on its own.
 */
class CffTableTests {
	static constexpr std::uint8_t hstemhm = 18;
	static constexpr std::uint8_t hintmask = 19;
	static constexpr std::uint8_t cntrmask = 20;
	static constexpr std::uint8_t rmoveto = 21;
	static constexpr std::uint8_t hmoveto = 22;
	static constexpr std::uint8_t rlineto = 5;
	static constexpr std::uint8_t hlineto = 6;
	static constexpr std::uint8_t vlineto = 7;
	static constexpr std::uint8_t rrcurveto = 8;
	static constexpr std::uint8_t callsubr = 10;
	static constexpr std::uint8_t returnOperator = 11;
	static constexpr std::uint8_t endchar = 14;
	static constexpr std::uint8_t callgsubr = 29;

	static Font makeFont(const TestCff& cff) {
		TestFontBuilder builder;
		builder.setCff(cff);
		builder.map(U'A', 1);
		Font font;
		font.load(builder.build());
		return font;
	}

	static std::string outlineOf(const Font& font, GlyphId glyph) {
		Outline outline;
		return font.getOutline(glyph, outline) ? outline.toSvgPath() : std::string("<failed>");
	}

	static std::vector<std::uint8_t> emptyGlyph() {
		return TestCharString().op(endchar).take();
	}

	static inline test::Test drawsPaths{"CffTable: draws lines and curves, reading the advance width off the first operator", [](test::Context& context) {
		TestCff cff;
		cff.charStrings.push_back(emptyGlyph());
		// A width before rmoveto's two arguments, then lines in every form.
		cff.charStrings.push_back(TestCharString().number(500).number(100).number(0).op(rmoveto)
			.number(0).number(200).number(300).number(0).op(rlineto).number(-200).op(vlineto).op(endchar).take());
		// No width: rmoveto has exactly two arguments.
		cff.charStrings.push_back(TestCharString().number(100).number(0).op(rmoveto)
			.number(50).number(0).number(50).number(100).number(0).number(100).op(rrcurveto).op(endchar).take());
		// hmoveto takes one argument, so two mean a width; hlineto alternates.
		cff.charStrings.push_back(TestCharString().number(600).number(10).op(hmoveto)
			.number(20).number(30).op(hlineto).op(endchar).take());
		// Just a width.
		cff.charStrings.push_back(TestCharString().number(500).op(endchar).take());
		// A line before any moveto starts at the origin.
		cff.charStrings.push_back(TestCharString().number(40).number(0).op(rlineto).op(endchar).take());

		const Font font = makeFont(cff);
		context.check(font.isLoaded(), "the CFF font loads");
		context.check(font.hasPostScriptOutlines(), "with PostScript outlines");
		context.checkEqual(font.getGlyphCount(), std::uint16_t{6});
		context.checkEqual(outlineOf(font, 1), std::string("M 100 0 L 100 200 L 400 200 L 400 0 Z"));
		context.checkEqual(outlineOf(font, 2), std::string("M 100 0 C 150 0 200 100 200 200 Z"));
		context.checkEqual(outlineOf(font, 3), std::string("M 10 0 L 30 0 L 30 30 Z"));
		context.checkEqual(outlineOf(font, 4), std::string(""));
		context.checkEqual(outlineOf(font, 5), std::string("M 0 0 L 40 0 Z"));
		context.checkEqual(outlineOf(font, 6), std::string("<failed>"));
	}};

	static inline test::Test hintMasks{"CffTable: hints are counted up to the first hint mask, which fixes its size", [](test::Context& context) {
		// Five horizontal hints with a width, five implicit vertical hints
		// before the mask: ten hints need two mask bytes. The mask bytes look
		// like rmoveto and endchar, so a wrong size draws something else.
		TestCharString program;
		program.number(500);
		for (std::int32_t i = 0; i < 10; i++) {
			program.number(10 * i);
		}
		program.op(hstemhm);
		for (std::int32_t i = 0; i < 10; i++) {
			program.number(10 * i);
		}
		program.op(hintmask).raw(rmoveto).raw(endchar);
		program.number(100).number(100).op(rmoveto).number(50).number(0).op(rlineto);
		program.op(cntrmask).raw(endchar).raw(endchar);
		program.number(0).number(50).op(rlineto).op(endchar);

		TestCff cff;
		cff.charStrings.push_back(emptyGlyph());
		cff.charStrings.push_back(program.take());
		const Font font = makeFont(cff);
		context.checkEqual(outlineOf(font, 1), std::string("M 100 100 L 150 100 L 150 150 Z"));
	}};

	static inline test::Test flexes{"CffTable: flex, hflex, hflex1 and flex1 each draw two curves", [](test::Context& context) {
		auto flexGlyph = [](std::vector<std::int32_t> arguments, std::uint8_t code) {
			TestCharString program;
			program.number(0).number(0).op(rmoveto);
			for (const std::int32_t argument : arguments) {
				program.number(argument);
			}
			return program.escape(code).op(endchar).take();
		};
		TestCff cff;
		cff.charStrings.push_back(emptyGlyph());
		cff.charStrings.push_back(flexGlyph({10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 50}, 35));
		cff.charStrings.push_back(flexGlyph({10, 20, 30, 40, 50, 60, 70}, 34));
		cff.charStrings.push_back(flexGlyph({10, 5, 20, 15, 30, 40, 50, -10, 60}, 36));
		cff.charStrings.push_back(flexGlyph({10, 0, 20, 10, 30, 10, 30, -10, 20, -10, 10}, 37));
		cff.charStrings.push_back(flexGlyph({10, 20, 30}, 35));

		const Font font = makeFont(cff);
		context.checkEqual(outlineOf(font, 1), std::string("M 0 0 C 10 20 40 60 90 120 C 160 200 250 300 360 420 Z"));
		context.checkEqual(outlineOf(font, 2), std::string("M 0 0 C 10 0 30 30 70 30 C 120 30 180 0 250 0 Z"));
		context.checkEqual(outlineOf(font, 3), std::string("M 0 0 C 10 5 30 20 60 20 C 100 20 150 10 210 0 Z"));
		context.checkEqual(outlineOf(font, 4), std::string("M 0 0 C 10 0 30 10 60 20 C 90 10 110 0 120 0 Z"));
		context.checkEqual(outlineOf(font, 5), std::string("<failed>"));
	}};

	static inline test::Test subroutines{"CffTable: subroutine numbers are biased by the size of their index", [](test::Context& context) {
		TestCff cff;
		// Three local subroutines: bias 107.
		cff.localSubrs.push_back(TestCharString().number(0).number(100).op(rlineto).op(returnOperator).take());
		cff.localSubrs.push_back(TestCharString().op(endchar).take());
		cff.localSubrs.push_back(TestCharString().number(-107).op(callsubr).op(returnOperator).take());
		// 1,240 global subroutines: bias 1131.
		cff.globalSubrs.assign(1240, TestCharString().op(returnOperator).take());
		cff.globalSubrs.front() = TestCharString().number(100).number(0).op(rlineto).op(returnOperator).take();
		cff.globalSubrs.back() = TestCharString().number(0).number(50).op(rlineto).op(returnOperator).take();

		cff.charStrings.push_back(emptyGlyph());
		cff.charStrings.push_back(TestCharString().number(0).number(0).op(rmoveto).number(-107).op(callsubr)
			.number(100).number(0).op(rlineto).op(endchar).take());
		cff.charStrings.push_back(TestCharString().number(0).number(0).op(rmoveto).number(-1131).op(callgsubr)
			.number(108).op(callgsubr).op(endchar).take());
		// endchar inside a subroutine ends the glyph: the line after the call is never drawn.
		cff.charStrings.push_back(TestCharString().number(0).number(0).op(rmoveto).number(10).number(0).op(rlineto)
			.number(-107).op(callsubr).number(-106).op(callsubr).number(999).number(0).op(rlineto).take());
		// A subroutine calling another.
		cff.charStrings.push_back(TestCharString().number(5).number(5).op(rmoveto).number(-105).op(callsubr).op(endchar).take());

		const Font font = makeFont(cff);
		context.checkEqual(outlineOf(font, 1), std::string("M 0 0 L 0 100 L 100 100 Z"));
		context.checkEqual(outlineOf(font, 2), std::string("M 0 0 L 100 0 L 100 50 Z"));
		context.checkEqual(outlineOf(font, 3), std::string("M 0 0 L 10 0 L 10 100 Z"));
		context.checkEqual(outlineOf(font, 4), std::string("M 5 5 L 5 105 Z"));
	}};

	static inline test::Test rejects{"CffTable: seac, arithmetic, runaway recursion, overflow and truncation fail only their glyph", [](test::Context& context) {
		TestCff cff;
		cff.localSubrs.push_back(TestCharString().number(-107).op(callsubr).op(returnOperator).take());
		cff.charStrings.push_back(TestCharString().number(0).number(0).op(rmoveto).number(10).number(0).op(rlineto).op(endchar).take());
		cff.charStrings.push_back(TestCharString().number(500).number(0).number(0).number(65).number(97).op(endchar).take());
		cff.charStrings.push_back(TestCharString().number(0).number(0).number(65).number(97).op(endchar).take());
		cff.charStrings.push_back(TestCharString().number(1).number(2).escape(10).op(endchar).take());
		cff.charStrings.push_back(TestCharString().number(-107).op(callsubr).op(endchar).take());
		TestCharString overflow;
		for (std::int32_t i = 0; i < 49; i++) {
			overflow.number(i);
		}
		cff.charStrings.push_back(overflow.op(rlineto).op(endchar).take());
		cff.charStrings.push_back(TestCharString().number(7).op(rmoveto).op(endchar).take());
		cff.charStrings.push_back(TestCharString().raw(28).raw(1).take());
		cff.charStrings.push_back(TestCharString().number(0).number(0).op(rmoveto).number(5).op(callgsubr).op(endchar).take());

		const Font font = makeFont(cff);
		context.check(font.isLoaded(), "a font with bad glyphs still loads");
		context.checkEqual(outlineOf(font, 0), std::string("M 0 0 L 10 0 Z"));
		context.checkEqual(outlineOf(font, 1), std::string("<failed>"));    // seac after a width
		context.checkEqual(outlineOf(font, 2), std::string("<failed>"));    // seac
		context.checkEqual(outlineOf(font, 3), std::string("<failed>"));    // add
		context.checkEqual(outlineOf(font, 4), std::string("<failed>"));    // a subroutine calling itself
		context.checkEqual(outlineOf(font, 5), std::string("<failed>"));    // 49 arguments
		context.checkEqual(outlineOf(font, 6), std::string("<failed>"));    // rmoveto with one argument
		context.checkEqual(outlineOf(font, 7), std::string("<failed>"));    // a cut-off number
		context.checkEqual(outlineOf(font, 8), std::string("<failed>"));    // no global subroutines
		context.checkEqual(outlineOf(font, 0), std::string("M 0 0 L 10 0 Z"));
	}};

	static inline test::Test measuresHeights{"CffTable: a CFF font without OS/2 heights measures them from its outlines", [](test::Context& context) {
		TestCff cff;
		cff.charStrings.push_back(emptyGlyph());
		cff.charStrings.push_back(TestCharString().number(0).number(0).op(rmoveto).number(400).number(0).number(0).number(480)
			.number(-400).number(0).op(rlineto).op(endchar).take());
		cff.charStrings.push_back(TestCharString().number(0).number(0).op(rmoveto).number(400).number(0).number(0).number(690)
			.number(-400).number(0).op(rlineto).op(endchar).take());
		TestFontBuilder builder;
		builder.setCff(cff).map(U'x', 1).map(U'H', 2);
		Font font;
		context.check(font.load(builder.build()), "the font loads");
		context.checkEqual(font.getMetrics().xHeight, std::int16_t{480});
		context.checkEqual(font.getMetrics().capHeight, std::int16_t{690});
		context.checkEqual(font.getGlyph(U'H'), GlyphId{2});
	}};

	static Bytes buildRichCffFont() {
		TestCff cff;
		cff.localSubrs.push_back(TestCharString().number(0).number(100).op(rlineto).op(returnOperator).take());
		cff.localSubrs.push_back(TestCharString().number(-107).op(callsubr).number(50).number(0).op(rlineto).op(returnOperator).take());
		cff.globalSubrs.push_back(TestCharString().number(10).number(20).number(30).number(40).number(50).number(60).op(rrcurveto).op(returnOperator).take());
		cff.charStrings.push_back(emptyGlyph());
		TestCharString hinted;
		hinted.number(500).number(0).number(20).number(100).number(20).op(hstemhm).number(0).number(30).op(hintmask).raw(0xC0);
		hinted.number(10).number(10).op(rmoveto).number(-106).op(callsubr).number(-107).op(callgsubr).op(endchar);
		cff.charStrings.push_back(hinted.take());
		TestCharString flex;
		flex.number(0).number(0).op(rmoveto);
		for (std::int32_t i = 1; i <= 13; i++) {
			flex.number(i * 7);
		}
		flex.escape(35).number(30).number(20).number(10).number(5).escape(34).op(endchar);
		cff.charStrings.push_back(flex.take());

		TestFontBuilder builder;
		builder.setCff(cff).map(U'A', 1).map(U'B', 2);
		TestGposLookup lookup;
		TestGposSubtable pairs;
		pairs.pairs = {{1, 2, -40}, {2, 1, -20}};
		TestGposSubtable classes;
		classes.format = 2;
		classes.coverage = {1, 2};
		classes.firstClasses = {{2, 1}};
		classes.secondClasses = {{1, 1}};
		classes.classValues = {{0, -5}, {-6, -7}};
		lookup.subtables = {pairs, classes};
		lookup.extension = true;
		builder.addGposLookup(lookup);
		builder.setNames("Rich", "Regular", "Rich Regular");
		return builder.build();
	}

	static void exercise(const Font& font) {
		Outline outline;
		for (std::uint32_t glyph = 0; glyph <= font.getGlyphCount(); glyph++) {
			font.getOutline(static_cast<GlyphId>(glyph), outline);
			font.getKerning(static_cast<GlyphId>(glyph), static_cast<GlyphId>(glyph + 1));
			font.getKerning(static_cast<GlyphId>(glyph + 1), static_cast<GlyphId>(glyph));
		}
	}

	static inline test::Test fuzz{"CffTable: damaged CFF and GPOS data never crashes or reads out of bounds", [](test::Context& context) {
		const Bytes valid = buildRichCffFont();
		Font reference;
		context.check(reference.load(valid), "the untouched font loads");
		context.checkEqual(outlineOf(reference, 1), std::string("M 10 10 L 10 110 L 60 110 C 70 130 100 170 150 230 Z"));
		context.checkEqual(reference.getKerning(1, 2), std::int16_t{-40});
		context.checkEqual(reference.getKerning(2, 1), std::int16_t{-20});
		context.checkEqual(reference.getKerning(2, 2), std::int16_t{-6});

		std::int32_t loaded = 0;
		for (std::size_t length = 0; length < valid.data.size(); length++) {
			Font font;
			if (font.load(Bytes{std::vector<std::uint8_t>(valid.data.begin(), valid.data.begin() + static_cast<std::ptrdiff_t>(length))})) {
				loaded++;
				exercise(font);
			}
		}
		context.check(loaded < 8, "almost every truncation is rejected");

		std::mt19937 random(20261016u);
		std::uniform_int_distribution<std::size_t> position(0, valid.data.size() - 1);
		std::uniform_int_distribution<std::int32_t> value(0, 255);
		std::uniform_int_distribution<std::int32_t> changes(1, 8);
		std::int32_t survivors = 0;
		for (std::int32_t trial = 0; trial < 3000; trial++) {
			Bytes damaged = valid;
			const std::int32_t count = changes(random);
			for (std::int32_t i = 0; i < count; i++) {
				damaged.data[position(random)] = static_cast<std::uint8_t>(value(random));
			}
			Font font;
			if (font.load(std::move(damaged))) {
				survivors++;
				exercise(font);
			}
		}
		context.note(std::to_string(survivors) + " of 3000 damaged CFF fonts still loaded and were exercised");
	}};

	static inline test::Test installedFuzz{"CffTable: damaging a real .otf inside its CFF and GPOS tables fails cleanly", [](test::Context& context) {
		const std::filesystem::path path = test::Context::getFontFolder() / "MyriadPro-Regular.otf";
		Bytes valid;
		if (!File::read(path, valid)) {
			context.note("MyriadPro-Regular.otf is not in the fonts folder; skipped");
			return;
		}
		FontTables tables;
		context.checkEqual(Font::readTableDirectory(ByteReader(valid.data), 0, tables), FontError::None);
		context.check(tables.cff.present && tables.gpos.present, "it has CFF and GPOS tables");

		std::mt19937 random(7u);
		std::uniform_int_distribution<std::int32_t> value(0, 255);
		std::int32_t survivors = 0;
		for (std::int32_t trial = 0; trial < 60; trial++) {
			const FontTable& table = trial % 2 == 0 ? tables.cff : tables.gpos;
			std::uniform_int_distribution<std::uint32_t> position(table.offset, table.offset + table.length - 1);
			Bytes damaged = valid;
			for (std::int32_t i = 0; i < 16; i++) {
				damaged.data[position(random)] = static_cast<std::uint8_t>(value(random));
			}
			Font font;
			if (font.load(std::move(damaged))) {
				survivors++;
				Outline outline;
				for (std::uint32_t glyph = 0; glyph < font.getGlyphCount(); glyph += 3) {
					font.getOutline(static_cast<GlyphId>(glyph), outline);
					font.getKerning(static_cast<GlyphId>(glyph), static_cast<GlyphId>(glyph / 2));
				}
			}
		}
		context.note(std::to_string(survivors) + " of 60 damaged copies of Myriad Pro loaded and were drawn");
	}};
};

}
