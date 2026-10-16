#pragma once

#include <litefont/Image.h>
#include <litefont/Sprite.h>
#include <litefont/Blur.h>
#include <litefont/Outline.h>

#include <vector>

namespace litefont {

/**
 * @addtogroup Shape
 *
 * FillRule - Which points a set of contours encloses. NonZero counts a point
 * the contours wind around at all; EvenOdd counts it only when they wind
 * around it an odd number of times, so overlaps cancel.
 */
enum class FillRule : std::uint8_t {
	NonZero,
	EvenOdd
};

/**
 * \class Rasterizer Rasterizer.h <litefont/Rasterizer.h>
 * @addtogroup Shape
 *
 * Rasterizer - Fills polylines into a gray coverage image: every pixel gets
 * the exact area of the shape inside it, from 0 to 255. Coordinates are
 * pixels with y pointing down, and pixel (x, y) covers [x, x + 1) by
 * [y, y + 1).
 *
 * This is the signed-area method of FreeType's gray rasterizer and font-rs.
 * Each edge adds its height and the area to its right into a cell per pixel,
 * and a running sum along each row turns the cells into coverage. Points are
 * rounded to 1/256 pixel and accumulated in integers, so the same polylines
 * give the same bytes on every compiler. Shapes may cross the image edges or
 * lie far outside it; coordinates beyond getCoordinateLimit() are clamped.
 * The cell buffer is kept between calls.
 */
class Rasterizer {
public:
	bool fill(const Polylines& contours, FillRule rule, Image& target);

	static float getCoordinateLimit();

private:
	void addLine(std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1);
	void addRowLine(std::int32_t row, std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1, std::int64_t direction);
	void addCell(std::int32_t row, std::int32_t column, std::int32_t left, std::int32_t right, std::int64_t cover);

	static std::int32_t toFixed(float value);
	static std::int64_t divideRounded(std::int64_t numerator, std::int64_t denominator);

	static constexpr std::int32_t unitsPerPixel = 256;
	static constexpr std::int64_t fullCoverage = std::int64_t{2} * unitsPerPixel * unitsPerPixel;

	std::vector<std::int64_t> cells;
	std::int32_t width = 0;
	std::int32_t height = 0;
};

}
