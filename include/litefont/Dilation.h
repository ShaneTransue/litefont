#pragma once

#include <litefont/Image.h>
#include <litefont/Sprite.h>
#include <litefont/Blur.h>
#include <litefont/Outline.h>

namespace litefont {

/**
 * \class Dilation Dilation.h <litefont/Dilation.h>
 * @addtogroup Shape
 *
 * Dilation - Grows a filled shape by a radius, for outlines (borders)
 * around text and icons. Given the shape's exact coverage (from the
 * Rasterizer) and the same polylines, each pixel's coverage becomes at
 * least radius + 0.5 minus its center's distance to the nearest edge,
 * clamped to 0 and 1: a band as wide as the radius with a one-pixel
 * antialiased rim, rounded at corners. Pixels inside the shape keep their
 * coverage. Distances are measured exactly to every segment, so it costs
 * pixels times segments, which is small for glyphs and done once per atlas
 * entry.
 */
class Dilation {
public:
	static bool apply(const Polylines& contours, float radius, Image& coverage);
};

}
