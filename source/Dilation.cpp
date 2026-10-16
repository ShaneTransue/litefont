#include <litefont/Dilation.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace litefont {

bool Dilation::apply(const Polylines& contours, float radius, Image& coverage) {
	if (coverage.getFormat() != PixelFormat::Gray8) {
		return false;
	}
	if (!(radius > 0.0f) || coverage.isEmpty()) {
		return true;
	}

	// Every closed segment, with its bounds grown by the reach of the band.
	const float reach = radius + 0.5f;
	std::vector<Vector2f> starts;
	std::vector<Vector2f> ends;
	const std::vector<Vector2f>& points = contours.points;
	for (std::size_t contour = 0; contour < contours.contourStarts.size(); contour++) {
		const std::size_t first = contours.contourStarts[contour];
		const std::size_t last = contour + 1 < contours.contourStarts.size() ? contours.contourStarts[contour + 1] : points.size();
		if (first >= last || last > points.size()) {
			continue;
		}
		for (std::size_t i = first; i < last; i++) {
			starts.push_back(points[i]);
			ends.push_back(i + 1 < last ? points[i + 1] : points[first]);
		}
	}

	for (std::int32_t y = 0; y < coverage.getHeight(); y++) {
		std::uint8_t* row = coverage.getRow(y);
		const float centerY = static_cast<float>(y) + 0.5f;
		for (std::int32_t x = 0; x < coverage.getWidth(); x++) {
			if (row[x] == 255) {
				continue;
			}
			const Vector2f center{static_cast<float>(x) + 0.5f, centerY};
			float nearest = reach * reach;
			for (std::size_t i = 0; i < starts.size(); i++) {
				const Vector2f a = starts[i];
				const Vector2f b = ends[i];
				if (std::min(a.x, b.x) - reach > center.x || std::max(a.x, b.x) + reach < center.x
					|| std::min(a.y, b.y) - reach > center.y || std::max(a.y, b.y) + reach < center.y) {
					continue;
				}
				const Vector2f edge = b - a;
				const float lengthSquared = edge.getLengthSquared();
				const float t = lengthSquared > 0.0f ? std::clamp((center - a).dot(edge) / lengthSquared, 0.0f, 1.0f) : 0.0f;
				nearest = std::min(nearest, (center - (a + edge * t)).getLengthSquared());
			}
			const float band = std::clamp(reach - std::sqrt(nearest), 0.0f, 1.0f);
			row[x] = std::max(row[x], ChannelConversion::convert<std::uint8_t>(band));
		}
	}
	return true;
}

}
