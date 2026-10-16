#include "TestFontBuilder.h"

#include <litefont/Font.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <random>

namespace litefont::tests {

/**
 * Damaged fonts must fail cleanly: no crash, no hang, no read outside the
 * data. Run these under AddressSanitizer to catch out-of-bounds reads that
 * would otherwise go unnoticed.
 */
class FontFuzzTests {
	static Bytes buildRichFont(bool longLocations, std::uint16_t characterMapFormat) {
		TestFontBuilder builder;
		builder.setLineMetrics(1000, 800, -200, 50).setOs2(780, -220, 0, true, 500, 700);
		builder.setLongLocations(longLocations).setCharacterMapFormat(characterMapFormat).setGlyphIdArray(true);

		TestGlyph square;
		square.contours = {{{0, 0, true}, {300, 0, true}, {300, 300, true}, {0, 300, true}}};
		TestGlyph curvy;
		curvy.contours = {
			{{0, 0, false}, {200, 0, true}, {400, 300, false}, {200, 600, false}, {0, 300, true}},
			{{100, 100, false}, {150, 100, false}, {150, 150, false}, {100, 150, false}}};
		const std::uint16_t squareId = builder.addGlyph(square);
		const std::uint16_t curvyId = builder.addGlyph(curvy);
		TestGlyph composite;
		composite.components = {{squareId, 10, 10, 0.5f, false}, {curvyId, -40, 300, 1.0f, false}};
		const std::uint16_t compositeId = builder.addGlyph(composite);
		TestGlyph nested;
		nested.components = {{compositeId, 1000, -2000, 1.0f, false}};
		builder.addGlyph(nested);
		builder.addGlyph(TestGlyph{});

		builder.map(U'A', squareId).map(U'B', curvyId).map(U'C', compositeId).map(U'D', 4).map(U' ', 5);
		builder.map(0x1F600, curvyId);
		builder.addKerning(1, 2, -30).addKerning(2, 3, 12).addKerning(3, 1, 5);
		return builder.build();
	}

	static void exercise(const Font& font) {
		Outline outline;
		for (std::uint32_t glyph = 0; glyph <= font.getGlyphCount(); glyph++) {
			font.getOutline(static_cast<GlyphId>(glyph), outline);
			font.getGlyphMetrics(static_cast<GlyphId>(glyph));
			font.getKerning(static_cast<GlyphId>(glyph), static_cast<GlyphId>(glyph + 1));
		}
		for (char32_t codePoint = 0; codePoint < 0x300; codePoint++) {
			font.getGlyph(codePoint);
		}
		font.getGlyph(0x1F600);
		font.getGlyph(0x10FFFF);
	}

	static inline test::Test truncations{"Font: every truncation of a valid font fails cleanly", [](test::Context& context) {
		for (const bool longLocations : {false, true}) {
			const Bytes valid = buildRichFont(longLocations, longLocations ? 12 : 4);
			Font reference;
			context.check(reference.load(valid), "the untouched font loads");

			std::int32_t loaded = 0;
			for (std::size_t length = 0; length < valid.data.size(); length++) {
				Bytes truncated{std::vector<std::uint8_t>(valid.data.begin(), valid.data.begin() + static_cast<std::ptrdiff_t>(length))};
				Font font;
				if (font.load(std::move(truncated))) {
					loaded++;
					exercise(font);
				}
			}
			context.check(loaded < 8, "almost every truncation is rejected (only trailing padding may go)");
		}
	}};

	static inline test::Test corruptions{"Font: random byte corruption never crashes or reads out of bounds", [](test::Context& context) {
		std::mt19937 random(20261015u);
		std::int32_t loaded = 0;
		for (const bool longLocations : {false, true}) {
			const Bytes valid = buildRichFont(longLocations, longLocations ? 12 : 4);
			std::uniform_int_distribution<std::size_t> position(0, valid.data.size() - 1);
			std::uniform_int_distribution<std::int32_t> value(0, 255);
			std::uniform_int_distribution<std::int32_t> changes(1, 8);

			for (std::int32_t trial = 0; trial < 1500; trial++) {
				Bytes damaged = valid;
				const std::int32_t count = changes(random);
				for (std::int32_t i = 0; i < count; i++) {
					damaged.data[position(random)] = static_cast<std::uint8_t>(value(random));
				}
				Font font;
				if (font.load(std::move(damaged))) {
					loaded++;
					exercise(font);
				}
			}
		}
		context.note(std::to_string(loaded) + " of 3000 damaged fonts still loaded and were fully exercised");
	}};
};

}
