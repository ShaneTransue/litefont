#include <litefont/Hinter.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace litefont::tests {

class HinterTests {
	// Shaped like Bahnschrift: 2048 units per em, x-height about half an em.
	static VerticalZones makeZones() {
		VerticalZones zones;
		zones.descender = -430.0f;
		zones.xHeight = 1062.0f;
		zones.capHeight = 1466.0f;
		zones.ascender = 1530.0f;
		zones.overshoot = 24.0f;
		return zones;
	}

	static bool isWhole(float value) {
		return value == std::round(value);
	}

	static Rectf getHintedBounds(const Font& font, const Hinter& hinter, char32_t codePoint, float pixelsPerEm) {
		Outline outline;
		font.getOutline(font.getGlyph(codePoint), outline);
		return hinter.apply(outline, pixelsPerEm).getBounds();
	}

	// A filled rectangle; TrueType draws filled contours clockwise (y up).
	static void addRectangle(Outline& outline, float left, float bottom, float right, float top, bool clockwise = true) {
		outline.moveTo(Vector2f{left, bottom});
		if (clockwise) {
			outline.lineTo(Vector2f{left, top});
			outline.lineTo(Vector2f{right, top});
			outline.lineTo(Vector2f{right, bottom});
		}
		else {
			outline.lineTo(Vector2f{right, bottom});
			outline.lineTo(Vector2f{right, top});
			outline.lineTo(Vector2f{left, top});
		}
		outline.close();
	}

	static bool isNear(float actual, float expected) {
		return std::fabs(actual - expected) < 1e-4f;
	}

	static inline test::Test wholePixels{"Hinter: zone heights land on whole pixels from 8 to 32 px", [](test::Context& context) {
		const VerticalZones zones = makeZones();
		const Hinter hinter(zones, 2048);
		std::int32_t wrong = 0;
		std::int32_t far = 0;
		for (float size = 8.0f; size <= 32.0f; size += 0.25f) {
			const PixelZones pixelZones = hinter.getPixelZones(size);
			const float heights[] = {zones.descender, 0.0f, zones.xHeight, zones.capHeight, zones.ascender};
			for (const float height : heights) {
				const float pixel = Hinter::mapHeight(height, pixelZones);
				wrong += isWhole(pixel) ? 0 : 1;
				far += std::fabs(pixel - height * pixelZones.scale) <= 1.0f ? 0 : 1;
			}
			// Overshoots are under half a pixel at every size here.
			wrong += isWhole(Hinter::mapHeight(-20.0f, pixelZones)) ? 0 : 1;
			wrong += isWhole(Hinter::mapHeight(zones.xHeight + 20.0f, pixelZones)) ? 0 : 1;
		}
		context.checkEqual(wrong, 0);
		context.checkEqual(far, 0);
		context.checkEqual(Hinter::mapHeight(0.0f, hinter.getPixelZones(11.0f)), 0.0f);
	}};

	static inline test::Test scalesX{"Hinter: x is only scaled and the verbs are kept", [](test::Context& context) {
		Outline outline;
		outline.moveTo(Vector2f{13.0f, -400.0f});
		outline.lineTo(Vector2f{-77.0f, 1062.0f});
		outline.quadTo(Vector2f{501.0f, 1100.0f}, Vector2f{999.0f, 700.0f});
		outline.cubicTo(Vector2f{1200.0f, 0.0f}, Vector2f{1333.0f, 1466.0f}, Vector2f{2047.0f, 1600.0f});
		outline.close();

		const Hinter hinter(makeZones(), 2048);
		const Outline hinted = hinter.apply(outline, 11.0f);
		const Outline scaled = outline.getTransformed(Affine2f::scale(11.0f / 2048.0f, 11.0f / 2048.0f));
		context.check(hinted.getVerbs() == outline.getVerbs(), "same verbs");
		bool sameX = hinted.getPoints().size() == scaled.getPoints().size();
		for (std::size_t i = 0; sameX && i < scaled.getPoints().size(); i++) {
			sameX = hinted.getPoints()[i].x == scaled.getPoints()[i].x;
		}
		context.check(sameX, "x equals plain scaling");
	}};

	static inline test::Test keepsOrder{"Hinter: heights keep their order and move less than a pixel", [](test::Context& context) {
		const Hinter hinter(makeZones(), 2048);
		std::mt19937 random(17u);
		std::uniform_real_distribution<float> height(-900.0f, 2400.0f);
		std::vector<float> heights(4000);
		for (float& value : heights) {
			value = height(random);
		}
		std::sort(heights.begin(), heights.end());

		std::int32_t folds = 0;
		std::int32_t far = 0;
		for (const float size : {6.0f, 8.0f, 9.5f, 11.0f, 13.0f, 16.0f, 24.0f, 41.0f, 64.0f, 200.0f}) {
			const PixelZones pixelZones = hinter.getPixelZones(size);
			float previous = -1e30f;
			for (const float value : heights) {
				const float pixel = Hinter::mapHeight(value, pixelZones);
				folds += pixel < previous ? 1 : 0;
				far += std::fabs(pixel - value * pixelZones.scale) < 1.0f ? 0 : 1;
				previous = pixel;
			}
		}
		context.checkEqual(folds, 0);
		context.checkEqual(far, 0);
	}};

	static inline test::Test overshoots{"Hinter: overshoots snap onto their zone only while under half a pixel", [](test::Context& context) {
		const VerticalZones zones = makeZones();
		const Hinter hinter(zones, 2048);
		const PixelZones small = hinter.getPixelZones(11.0f);
		context.checkEqual(Hinter::mapHeight(-20.0f, small), 0.0f);
		context.checkEqual(Hinter::mapHeight(zones.xHeight + 20.0f, small), Hinter::mapHeight(zones.xHeight, small));

		// 24 units at 60 px is 0.7 px: big enough to keep.
		const PixelZones large = hinter.getPixelZones(60.0f);
		context.check(Hinter::mapHeight(-20.0f, large) < 0.0f, "the overshoot stays below the baseline");
		context.check(Hinter::mapHeight(zones.xHeight + 20.0f, large) > Hinter::mapHeight(zones.xHeight, large), "and above the x-height");
	}};

	static inline test::Test noZones{"Hinter: without zones it only scales", [](test::Context& context) {
		const Hinter hinter(VerticalZones{}, 1024);
		const PixelZones pixelZones = hinter.getPixelZones(16.0f);
		context.checkEqual(pixelZones.count, 1);
		// Only the baseline's thousandth-of-an-em catch shifts the rest, by 0.016 px.
		context.check(std::fabs(Hinter::mapHeight(-256.0f, pixelZones) + 4.0f) <= 0.02f, "below the baseline");
		context.check(std::fabs(Hinter::mapHeight(736.0f, pixelZones) - 11.5f) <= 0.02f, "above the baseline");
	}};

	static inline test::Test mergesZones{"Hinter: an ascender within the cap overshoot merges with the caps", [](test::Context& context) {
		VerticalZones zones = makeZones();
		zones.ascender = zones.capHeight + 10.0f;
		const Hinter hinter(zones, 2048);
		context.checkEqual(hinter.getPixelZones(11.0f).count, 4);
		context.checkEqual(hinter.getPixelZones(80.0f).count, 5);
	}};

	static inline test::Test stemShift{"Hinter: the stem shift puts the tallest stem's left edge on a pixel", [](test::Context& context) {
		Outline letterI;
		addRectangle(letterI, 3.3f, 0.0f, 4.4f, 7.0f);
		context.check(isNear(Hinter::getStemShift(letterI), -0.3f), "a single stem moves left to x = 3");
		Outline reversed;
		addRectangle(reversed, 3.3f, 0.0f, 4.4f, 7.0f, false);
		context.check(isNear(Hinter::getStemShift(reversed), -0.3f), "the drawing direction does not matter");

		Outline letterN;
		addRectangle(letterN, 1.3f, 0.0f, 2.4f, 5.0f);
		addRectangle(letterN, 4.6f, 0.0f, 5.7f, 3.5f);
		context.check(isNear(Hinter::getStemShift(letterN), -0.3f), "the taller stem wins");
		Outline taller;
		addRectangle(taller, 1.3f, 0.0f, 2.4f, 5.0f);
		addRectangle(taller, 4.6f, 0.0f, 5.7f, 6.0f);
		context.check(isNear(Hinter::getStemShift(taller), 0.4f), "even when it is on the right");

		Outline letterH;
		addRectangle(letterH, 1.2f, 0.0f, 2.3f, 7.0f);
		addRectangle(letterH, 5.7f, 0.0f, 6.8f, 7.0f);
		addRectangle(letterH, 2.3f, 3.0f, 5.7f, 4.0f);
		context.check(isNear(Hinter::getStemShift(letterH), -0.2f), "a tie goes to the leftmost stem");

		Outline counter;
		addRectangle(counter, 0.6f, 0.0f, 5.6f, 7.0f);
		addRectangle(counter, 1.9f, 1.2f, 4.3f, 5.8f, false);
		context.check(isNear(Hinter::getStemShift(counter), 0.4f), "the side of a counter is a stem");
	}};

	static inline test::Test noStem{"Hinter: shapes without a vertical stem are not shifted", [](test::Context& context) {
		Outline diamond;
		diamond.moveTo(Vector2f{0.0f, 3.0f});
		diamond.lineTo(Vector2f{3.0f, 6.0f});
		diamond.lineTo(Vector2f{6.0f, 3.0f});
		diamond.lineTo(Vector2f{3.0f, 0.0f});
		diamond.close();
		context.checkEqual(Hinter::getStemShift(diamond), 0.0f);

		Outline serif;
		addRectangle(serif, 0.3f, 0.0f, 3.3f, 1.0f);
		context.checkEqual(Hinter::getStemShift(serif), 0.0f);

		Outline round;
		round.moveTo(Vector2f{3.3f, 0.0f});
		round.quadTo(Vector2f{0.3f, 0.0f}, Vector2f{0.3f, 3.0f});
		round.quadTo(Vector2f{0.3f, 6.0f}, Vector2f{3.3f, 6.0f});
		round.quadTo(Vector2f{6.3f, 6.0f}, Vector2f{6.3f, 3.0f});
		round.quadTo(Vector2f{6.3f, 0.0f}, Vector2f{3.3f, 0.0f});
		round.close();
		context.checkEqual(Hinter::getStemShift(round), 0.0f);
		context.checkEqual(Hinter::getStemShift(Outline()), 0.0f);
	}};

	static inline test::Test stemShiftOnFonts{"Hinter: stem shifts on the local fonts stay within half a pixel and land stems on pixels", [](test::Context& context) {
		const std::string_view letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		for (const char* name : {"bahnschrift", "segoeui", "segoeuib"}) {
			Font font;
			if (!font.load(test::Context::getFontFolder() / (std::string(name) + ".ttf"))) {
				context.note(std::format("{} is not in the fonts folder; skipped", name));
				continue;
			}
			const Hinter hinter(Hinter::findZones(font), font.getMetrics().unitsPerEm);
			std::int32_t shifted = 0;
			std::int32_t total = 0;
			for (float size = 9.0f; size <= 13.0f; size += 1.0f) {
				for (const char letter : letters) {
					Outline outline;
					font.getOutline(font.getGlyph(static_cast<char32_t>(letter)), outline);
					const Outline hinted = hinter.apply(outline, size);
					const float shift = Hinter::getStemShift(hinted);
					const Outline moved = hinted.getTransformed(Affine2f::translate(shift, 0.0f));
					context.check(std::fabs(shift) <= 0.5f, "at most half a pixel");
					context.check(std::fabs(Hinter::getStemShift(moved)) < 1e-3f, "a shifted glyph needs no further shift");
					if (letter == 'I') {
						const float left = moved.getBounds().x;
						context.check(isNear(left, std::round(left)), "I's stem starts on a pixel");
					}
					shifted += shift != 0.0f ? 1 : 0;
					total++;
				}
			}
			context.note(std::format("{}: {} of {} glyph images at 9 to 13 px shifted", name, shifted, total));
		}
	}};

	static inline test::Test installedFonts{"Hinter: zones found in the local fonts put x, H, o and p on whole pixels", [](test::Context& context) {
		for (const char* name : {"bahnschrift", "segoeui", "segoeuib"}) {
			Font font;
			if (!font.load(test::Context::getFontFolder() / (std::string(name) + ".ttf"))) {
				context.note(std::format("{} is not in the fonts folder; skipped", name));
				continue;
			}
			const VerticalZones zones = Hinter::findZones(font);
			const float unitsPerEm = static_cast<float>(font.getMetrics().unitsPerEm);
			context.check(zones.descender < 0.0f && zones.xHeight > 0.0f && zones.capHeight > zones.xHeight, "plausible zones");
			context.check(zones.overshoot > 0.0f && zones.overshoot < 0.04f * unitsPerEm, "a small overshoot");
			context.note(std::format("{}: descender {}, x-height {}, cap height {}, ascender {}, overshoot {} of {} units",
				name, zones.descender, zones.xHeight, zones.capHeight, zones.ascender, zones.overshoot, unitsPerEm));

			const Hinter hinter(zones, font.getMetrics().unitsPerEm);
			for (float size = 9.0f; size <= 13.0f; size += 1.0f) {
				const float xTop = Hinter::mapHeight(zones.xHeight, hinter.getPixelZones(size));
				const Rectf x = getHintedBounds(font, hinter, U'x', size);
				const Rectf o = getHintedBounds(font, hinter, U'o', size);
				const Rectf h = getHintedBounds(font, hinter, U'H', size);
				const Rectf p = getHintedBounds(font, hinter, U'p', size);
				context.check(x.y == 0.0f && x.getBottom() == xTop, "x sits on the baseline and reaches the x-height");
				context.check(o.y == 0.0f && o.getBottom() == xTop, "o is snapped to the same rows as x");
				context.check(h.y == 0.0f && isWhole(h.getBottom()), "H spans whole pixels");
				context.check(isWhole(p.y), "p's descender ends on a whole pixel");
			}
		}
	}};
};

}
