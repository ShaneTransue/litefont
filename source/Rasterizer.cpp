#include <litefont/Rasterizer.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace litefont {

bool Rasterizer::fill(const Polylines& contours, FillRule rule, Image& target) {
	if (target.getFormat() != PixelFormat::Gray8) {
		return false;
	}
	this->width = target.getWidth();
	this->height = target.getHeight();
	if (target.isEmpty()) {
		return true;
	}

	// One extra cell per row receives the share of edges on the last column.
	const std::size_t stride = static_cast<std::size_t>(this->width) + 1;
	this->cells.assign(stride * static_cast<std::size_t>(this->height), 0);

	const std::vector<Vector2f>& points = contours.points;
	for (std::size_t contour = 0; contour < contours.contourStarts.size(); contour++) {
		const std::size_t start = contours.contourStarts[contour];
		const std::size_t end = contour + 1 < contours.contourStarts.size() ? contours.contourStarts[contour + 1] : points.size();
		if (start >= end || end > points.size()) {
			continue;
		}

		// Every contour is closed: start with the edge from its last point.
		std::int32_t previousX = Rasterizer::toFixed(points[end - 1].x);
		std::int32_t previousY = Rasterizer::toFixed(points[end - 1].y);
		for (std::size_t i = start; i < end; i++) {
			const std::int32_t x = Rasterizer::toFixed(points[i].x);
			const std::int32_t y = Rasterizer::toFixed(points[i].y);
			this->addLine(previousX, previousY, x, y);
			previousX = x;
			previousY = y;
		}
	}

	for (std::int32_t row = 0; row < this->height; row++) {
		const std::int64_t* rowCells = this->cells.data() + static_cast<std::size_t>(row) * stride;
		std::uint8_t* pixels = target.getRow(row);
		std::int64_t sum = 0;
		for (std::int32_t column = 0; column < this->width; column++) {
			sum += rowCells[column];
			std::int64_t coverage = sum < 0 ? -sum : sum;
			if (rule == FillRule::EvenOdd) {
				// Fold the winding area: 1 is inside, 2 is outside again.
				coverage %= 2 * Rasterizer::fullCoverage;
				if (coverage > Rasterizer::fullCoverage) {
					coverage = 2 * Rasterizer::fullCoverage - coverage;
				}
			}
			else {
				coverage = std::min(coverage, Rasterizer::fullCoverage);
			}
			pixels[column] = static_cast<std::uint8_t>((coverage * 255 + Rasterizer::fullCoverage / 2) / Rasterizer::fullCoverage);
		}
	}
	return true;
}

float Rasterizer::getCoordinateLimit() {
	return 4194304.0f;
}

void Rasterizer::addLine(std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1) {
	// Horizontal edges change no winding.
	if (y0 == y1) {
		return;
	}
	std::int64_t direction = 1;
	if (y0 > y1) {
		std::swap(x0, x1);
		std::swap(y0, y1);
		direction = -1;
	}

	// Rows above and below the image never show, so they are simply dropped.
	const std::int32_t bottom = this->height * Rasterizer::unitsPerPixel;
	if (y1 <= 0 || y0 >= bottom) {
		return;
	}
	auto xAt = [x0, y0, x1, y1](std::int32_t y) {
		return static_cast<std::int32_t>(x0 + Rasterizer::divideRounded((std::int64_t{x1} - x0) * (std::int64_t{y} - y0), std::int64_t{y1} - y0));
	};

	const std::int32_t top = std::max(y0, 0);
	const std::int32_t end = std::min(y1, bottom);
	std::int32_t rowTop = top;
	std::int32_t xTop = top == y0 ? x0 : xAt(top);
	for (std::int32_t row = top / Rasterizer::unitsPerPixel; rowTop < end; row++) {
		const std::int32_t rowBottom = std::min(end, (row + 1) * Rasterizer::unitsPerPixel);
		const std::int32_t xBottom = rowBottom == y1 ? x1 : xAt(rowBottom);
		const std::int32_t rowStart = row * Rasterizer::unitsPerPixel;
		this->addRowLine(row, xTop, rowTop - rowStart, xBottom, rowBottom - rowStart, direction);
		rowTop = rowBottom;
		xTop = xBottom;
	}
}

void Rasterizer::addRowLine(std::int32_t row, std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1, std::int64_t direction) {
	// Walk from left to right. Every piece covers its height, with the sign of
	// the edge's direction, whichever way the edge leans.
	if (x0 > x1) {
		std::swap(x0, x1);
		std::swap(y0, y1);
	}
	auto yAt = [x0, y0, x1, y1](std::int32_t x) {
		return static_cast<std::int32_t>(y0 + Rasterizer::divideRounded((std::int64_t{y1} - y0) * (std::int64_t{x} - x0), std::int64_t{x1} - x0));
	};

	std::int32_t startX = x0;
	std::int32_t startY = y0;
	std::int32_t endX = x1;
	std::int32_t endY = y1;

	// Left of the image the edge still changes the winding of every pixel to
	// its right, so that part counts as a vertical edge on the left border.
	if (startX < 0) {
		if (endX <= 0) {
			this->addCell(row, 0, 0, 0, std::abs(endY - startY) * direction);
			return;
		}
		const std::int32_t y = yAt(0);
		this->addCell(row, 0, 0, 0, std::abs(y - startY) * direction);
		startX = 0;
		startY = y;
	}

	// Right of the image nothing shows.
	const std::int32_t right = this->width * Rasterizer::unitsPerPixel;
	if (endX > right) {
		if (startX >= right) {
			return;
		}
		endY = yAt(right);
		endX = right;
	}

	std::int32_t x = startX;
	std::int32_t y = startY;
	while (true) {
		const std::int32_t column = x / Rasterizer::unitsPerPixel;
		const std::int32_t columnLeft = column * Rasterizer::unitsPerPixel;
		const std::int32_t columnRight = columnLeft + Rasterizer::unitsPerPixel;
		if (endX <= columnRight) {
			this->addCell(row, column, x - columnLeft, endX - columnLeft, std::abs(endY - y) * direction);
			return;
		}
		const std::int32_t nextY = yAt(columnRight);
		this->addCell(row, column, x - columnLeft, Rasterizer::unitsPerPixel, std::abs(nextY - y) * direction);
		x = columnRight;
		y = nextY;
	}
}

void Rasterizer::addCell(std::int32_t row, std::int32_t column, std::int32_t left, std::int32_t right, std::int64_t cover) {
	if (cover == 0 || column >= this->width) {
		return;
	}
	// The piece crosses the cell between left and right (0 to 256). The area to
	// its right stays in this cell; the rest of its height carries over to the
	// next one, and the running sum spreads it along the row.
	std::int64_t* rowCells = this->cells.data() + static_cast<std::size_t>(row) * (static_cast<std::size_t>(this->width) + 1);
	const std::int64_t sum = std::int64_t{left} + right;
	rowCells[column] += cover * (2 * Rasterizer::unitsPerPixel - sum);
	rowCells[column + 1] += cover * sum;
}

std::int32_t Rasterizer::toFixed(float value) {
	if (std::isnan(value)) {
		return 0;
	}
	const float limit = Rasterizer::getCoordinateLimit();
	return static_cast<std::int32_t>(std::lround(std::clamp(value, -limit, limit) * static_cast<float>(Rasterizer::unitsPerPixel)));
}

std::int64_t Rasterizer::divideRounded(std::int64_t numerator, std::int64_t denominator) {
	// Rounds halves up for either sign; plain division truncates toward zero.
	const std::int64_t twice = 2 * numerator + denominator;
	const std::int64_t divisor = 2 * denominator;
	std::int64_t quotient = twice / divisor;
	if (twice % divisor != 0 && twice < 0) {
		quotient -= 1;
	}
	return quotient;
}

}
