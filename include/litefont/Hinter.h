#pragma once

#include <litefont/Font.h>
#include <litefont/Outline.h>

#include <array>

namespace litefont {

/**
 * \struct VerticalZones Hinter.h <litefont/Hinter.h>
 * @addtogroup Text
 *
 * VerticalZones - The heights the Hinter puts on whole pixels, in font units:
 * the bottom of descenders, the baseline, the x-height, the cap height and
 * the top of ascenders. Round letters reach past these by up to overshoot
 * units. A zone that is not above the baseline (or, for the descender, not
 * below it) is not used.
 */
struct VerticalZones {
	float descender = 0.0f;
	float baseline = 0.0f;
	float xHeight = 0.0f;
	float capHeight = 0.0f;
	float ascender = 0.0f;
	float overshoot = 0.0f;
};

/**
 * \struct PixelZone Hinter.h <litefont/Hinter.h>
 * @addtogroup Text
 *
 * PixelZone - One zone at one size: every font-unit height from low to high
 * lands on the whole-pixel height pixel.
 */
struct PixelZone {
	float low = 0.0f;
	float high = 0.0f;
	float pixel = 0.0f;
};

/**
 * \struct PixelZones Hinter.h <litefont/Hinter.h>
 * @addtogroup Text
 *
 * PixelZones - The zones in use at one size, lowest first, and the scale from
 * font units to pixels.
 */
struct PixelZones {
	std::array<PixelZone, 5> zones;
	std::int32_t count = 0;
	float scale = 0.0f;
};

/**
 * \class Hinter Hinter.h <litefont/Hinter.h>
 * @addtogroup Text
 *
 * Hinter - Light vertical hinting. apply() scales an outline from font units
 * to pixels (y up, baseline at 0) and moves its points vertically so every
 * zone lands on a whole pixel. Heights between two zones stretch linearly and
 * heights beyond the outermost zones shift with them, so no point changes
 * order with another. While an overshoot is under half a pixel, points within
 * it snap onto their zone, so round letters do not smear a faint row past the
 * baseline or the x-height. Each zone also catches heights within a
 * thousandth of an em. x is only scaled, so spacing stays true.
 *
 * getStemShift() is the optional horizontal step: the shift, at most half a
 * pixel, that puts the left edge of a hinted glyph's tallest vertical stem on
 * a pixel boundary. Drawn at a whole-pixel pen position with that shift, a
 * 1.1 px stem fills one column instead of smearing over two, at the cost of
 * moving the glyph up to half a pixel within its advance.
 *
 * findZones() measures the zones from a font's x, H, p, l and o glyphs.
 */
class Hinter {
public:
	Hinter(const VerticalZones& zones, std::uint16_t unitsPerEm);

	Outline apply(const Outline& fontUnits, float pixelsPerEm) const;
	PixelZones getPixelZones(float pixelsPerEm) const;
	const VerticalZones& getZones() const;

	static float mapHeight(float height, const PixelZones& pixelZones);
	static float getStemShift(const Outline& pixels);
	static VerticalZones findZones(const Font& font);

private:
	VerticalZones zones;
	std::uint16_t unitsPerEm = 0;
};

}
