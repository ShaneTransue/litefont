#include "TestFontBuilder.h"

#include <litefont/Font.h>
#include "TestKit.h"
#include "RecordingDevice.h"

namespace litefont::tests {

/**
 * GPOS pair kerning through fonts with hand-built GPOS tables: which lookups
 * count, how subtables of one lookup hand a pair on, and how lookups add.
 */
class GposTableTests {
	static TestFontBuilder makeBuilder() {
		TestFontBuilder builder;
		TestGlyph glyph;
		glyph.contours = {{{0, 0, true}, {100, 0, true}, {100, 100, true}}};
		for (std::int32_t i = 0; i < 6; i++) {
			builder.addGlyph(glyph);
		}
		return builder;
	}

	static TestGposSubtable makePairs(std::vector<TestKerningPair> pairs) {
		TestGposSubtable subtable;
		subtable.pairs = std::move(pairs);
		return subtable;
	}

	static TestGposLookup makeLookup(std::vector<TestGposSubtable> subtables, const std::string& feature = "kern",
		const std::string& script = "DFLT", bool extension = false) {
		TestGposLookup lookup;
		lookup.subtables = std::move(subtables);
		lookup.feature = feature;
		lookup.script = script;
		lookup.extension = extension;
		return lookup;
	}

	static Font load(const TestFontBuilder& builder) {
		Font font;
		font.load(builder.build());
		return font;
	}

	static inline test::Test pairLists{"GposTable: a pair list without the pair hands it to the next subtable", [](test::Context& context) {
		TestGposSubtable classes;
		classes.format = 2;
		classes.coverage = {1, 2};
		classes.firstClasses = {{1, 1}, {2, 1}};
		classes.secondClasses = {{2, 1}, {6, 1}};
		classes.classValues = {{0, 0}, {-7, -11}};
		TestFontBuilder builder = makeBuilder();
		builder.addGposLookup(makeLookup({makePairs({{1, 2, -50}, {1, 3, -20}, {4, 5, 30}}), classes}));

		const Font font = load(builder);
		context.check(font.isLoaded(), "the font loads");
		context.checkEqual(font.getKerning(1, 2), std::int16_t{-50});
		context.checkEqual(font.getKerning(1, 3), std::int16_t{-20});
		context.checkEqual(font.getKerning(1, 6), std::int16_t{-11});      // not in the list: the class table
		context.checkEqual(font.getKerning(1, 5), std::int16_t{-7});       // second class 0
		context.checkEqual(font.getKerning(2, 6), std::int16_t{-11});
		context.checkEqual(font.getKerning(4, 5), std::int16_t{30});
		context.checkEqual(font.getKerning(4, 6), std::int16_t{0});        // the class table does not cover 4
		context.checkEqual(font.getKerning(3, 1), std::int16_t{0});
		context.checkEqual(font.getKerning(5, 4), std::int16_t{0});
	}};

	static inline test::Test classesStopAndLookupsAdd{"GposTable: a covered class pair applies even at zero, and lookups add up", [](test::Context& context) {
		TestGposSubtable classes;
		classes.format = 2;
		classes.coverage = {1};
		classes.secondClasses = {{2, 1}};
		classes.classValues = {{0, -40}};
		TestFontBuilder builder = makeBuilder();
		builder.addGposLookup(makeLookup({classes, makePairs({{1, 3, -99}})}));
		builder.addGposLookup(makeLookup({makePairs({{1, 2, -5}, {1, 3, -6}})}, "kern", "DFLT", true));

		const Font font = load(builder);
		context.checkEqual(font.getKerning(1, 2), std::int16_t{-45});
		context.checkEqual(font.getKerning(1, 3), std::int16_t{-6});       // the class table's 0 hides the -99
		context.checkEqual(font.getKerning(2, 3), std::int16_t{0});
	}};

	static inline test::Test featureChoice{"GposTable: only kern features count, from the default script, else Latin, else all", [](test::Context& context) {
		TestFontBuilder markThenLatin = makeBuilder();
		markThenLatin.addGposLookup(makeLookup({makePairs({{1, 2, -100}})}, "mark", "DFLT"));
		markThenLatin.addGposLookup(makeLookup({makePairs({{1, 2, -10}})}, "kern", "latn"));
		context.checkEqual(load(markThenLatin).getKerning(1, 2), std::int16_t{-10});

		TestFontBuilder defaultFirst = makeBuilder();
		defaultFirst.addGposLookup(makeLookup({makePairs({{1, 2, -10}})}, "kern", "DFLT"));
		defaultFirst.addGposLookup(makeLookup({makePairs({{1, 2, -20}})}, "kern", "latn"));
		context.checkEqual(load(defaultFirst).getKerning(1, 2), std::int16_t{-10});

		TestFontBuilder otherScripts = makeBuilder();
		otherScripts.addGposLookup(makeLookup({makePairs({{1, 2, -30}})}, "kern", "cyrl"));
		otherScripts.addGposLookup(makeLookup({makePairs({{1, 2, -40}})}, "kern", "grek"));
		context.checkEqual(load(otherScripts).getKerning(1, 2), std::int16_t{-70});

		TestFontBuilder noKernFeature = makeBuilder();
		noKernFeature.addGposLookup(makeLookup({makePairs({{1, 2, -100}})}, "mark", "DFLT"));
		noKernFeature.addKerning(1, 2, -15);
		context.checkEqual(load(noKernFeature).getKerning(1, 2), std::int16_t{-15});

		TestFontBuilder both = makeBuilder();
		both.addGposLookup(makeLookup({makePairs({{1, 2, -10}})}));
		both.addKerning(1, 2, -15).addKerning(2, 1, -25);
		const Font font = load(both);
		context.checkEqual(font.getKerning(1, 2), std::int16_t{-10});
		context.checkEqual(font.getKerning(2, 1), std::int16_t{0});        // the kern table is ignored
	}};

	static inline test::Test valueSizes{"GposTable: value records of every size are stepped over correctly", [](test::Context& context) {
		TestGposSubtable wide = makePairs({{1, 2, -12}, {1, 3, -13}, {1, 4, -14}, {1, 5, -15}, {2, 6, -16}});
		wide.valueFormat1 = 0x00FF;
		wide.valueFormat2 = 0x0005;
		TestGposSubtable classes;
		classes.format = 2;
		classes.valueFormat1 = 0x0005;
		classes.valueFormat2 = 0x0008;
		classes.coverage = {3, 4};
		classes.firstClasses = {{4, 1}};
		classes.secondClasses = {{5, 1}, {6, 2}};
		classes.classValues = {{-1, -2, -3}, {-4, -5, -6}};
		TestGposSubtable placementOnly = makePairs({{5, 1, -50}});
		placementOnly.valueFormat1 = 0x0001;

		TestFontBuilder builder = makeBuilder();
		builder.addGposLookup(makeLookup({wide, classes, placementOnly}));
		const Font font = load(builder);
		context.checkEqual(font.getKerning(1, 2), std::int16_t{-12});
		context.checkEqual(font.getKerning(1, 4), std::int16_t{-14});
		context.checkEqual(font.getKerning(1, 5), std::int16_t{-15});
		context.checkEqual(font.getKerning(2, 6), std::int16_t{-16});
		context.checkEqual(font.getKerning(3, 1), std::int16_t{-1});
		context.checkEqual(font.getKerning(3, 6), std::int16_t{-3});
		context.checkEqual(font.getKerning(4, 5), std::int16_t{-5});
		context.checkEqual(font.getKerning(4, 6), std::int16_t{-6});
		context.checkEqual(font.getKerning(5, 1), std::int16_t{0});        // no x advance in the record
	}};
};

}
