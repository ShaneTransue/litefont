#include <litefont/Hinter.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace litefont {

Hinter::Hinter(const VerticalZones& zones, std::uint16_t unitsPerEm) {
	this->zones = zones;
	this->unitsPerEm = std::max<std::uint16_t>(unitsPerEm, 1);
}

Outline Hinter::apply(const Outline& fontUnits, float pixelsPerEm) const {
	const PixelZones pixelZones = this->getPixelZones(pixelsPerEm);
	auto place = [&pixelZones](Vector2f point) {
		return Vector2f{point.x * pixelZones.scale, Hinter::mapHeight(point.y, pixelZones)};
	};

	// Off-curve points move with the same mapping, so curves keep their shape
	// between zones and flatten onto a zone where their points share it.
	Outline result;
	const std::vector<Vector2f>& points = fontUnits.getPoints();
	std::size_t index = 0;
	for (const OutlineVerb verb : fontUnits.getVerbs()) {
		switch (verb) {
		case OutlineVerb::Move:
			result.moveTo(place(points[index]));
			index += 1;
			break;
		case OutlineVerb::Line:
			result.lineTo(place(points[index]));
			index += 1;
			break;
		case OutlineVerb::Quad:
			result.quadTo(place(points[index]), place(points[index + 1]));
			index += 2;
			break;
		case OutlineVerb::Cubic:
			result.cubicTo(place(points[index]), place(points[index + 1]), place(points[index + 2]));
			index += 3;
			break;
		case OutlineVerb::Close:
			result.close();
			break;
		}
	}
	return result;
}

PixelZones Hinter::getPixelZones(float pixelsPerEm) const {
	PixelZones result;
	result.scale = pixelsPerEm / static_cast<float>(this->unitsPerEm);

	// The same test FreeType and PostScript use to suppress overshoots: below
	// half a pixel an overshoot only blurs, above it the letter looks round.
	const float overshoot = std::max(this->zones.overshoot, 0.0f);
	const float band = overshoot * result.scale < 0.5f ? overshoot : 0.0f;

	// Glyphs that share a zone can miss it by a unit or two (an E a little
	// shorter than the H), so every zone also catches heights that close.
	const float fuzz = static_cast<float>(this->unitsPerEm) / 1000.0f;

	const VerticalZones& zones = this->zones;
	const float heights[] = {zones.descender, zones.baseline, zones.xHeight, zones.capHeight, zones.ascender};
	const bool used[] = {zones.descender < zones.baseline, true, zones.xHeight > zones.baseline,
		zones.capHeight > zones.baseline, zones.ascender > zones.baseline};
	const bool bottom[] = {true, true, false, false, false};

	for (std::size_t i = 0; i < 5; i++) {
		if (!used[i]) {
			continue;
		}
		PixelZone zone;
		zone.low = (bottom[i] ? heights[i] - band : heights[i]) - fuzz;
		zone.high = (bottom[i] ? heights[i] : heights[i] + band) + fuzz;
		zone.pixel = std::round(heights[i] * result.scale);
		if (result.count > 0) {
			// A zone inside the one below it (an ascender as tall as the caps)
			// adds nothing, and a zone never lands below the one under it.
			const PixelZone& below = result.zones[static_cast<std::size_t>(result.count - 1)];
			if (zone.low <= below.high) {
				continue;
			}
			zone.pixel = std::max(zone.pixel, below.pixel);
		}
		result.zones[static_cast<std::size_t>(result.count)] = zone;
		result.count++;
	}
	return result;
}

const VerticalZones& Hinter::getZones() const {
	return this->zones;
}

float Hinter::mapHeight(float height, const PixelZones& pixelZones) {
	if (pixelZones.count == 0) {
		return height * pixelZones.scale;
	}

	const PixelZone& first = pixelZones.zones[0];
	if (height < first.low) {
		return first.pixel + (height - first.low) * pixelZones.scale;
	}
	for (std::size_t i = 0; i < static_cast<std::size_t>(pixelZones.count); i++) {
		const PixelZone& zone = pixelZones.zones[i];
		if (height < zone.low) {
			const PixelZone& below = pixelZones.zones[i - 1];
			const float t = (height - below.high) / (zone.low - below.high);
			return below.pixel + (zone.pixel - below.pixel) * t;
		}
		if (height <= zone.high) {
			return zone.pixel;
		}
	}
	const PixelZone& last = pixelZones.zones[static_cast<std::size_t>(pixelZones.count - 1)];
	return last.pixel + (height - last.high) * pixelZones.scale;
}

float Hinter::getStemShift(const Outline& pixels) {
	constexpr float shortestEdge = 1.5f;     // shorter vertical edges are serifs and notches
	constexpr float steepest = 0.04f;        // largest |dx| / |dy| still counted as vertical
	constexpr float thinnest = 0.25f;        // stem widths considered, in pixels
	constexpr float thickest = 6.0f;
	constexpr float shortestOverlap = 1.0f;  // how much the two sides of a stem must face each other

	// Vertical edges as zero-width rectangles, split by direction, and twice
	// the signed area of the control polygons to tell which side is filled.
	std::vector<Rectf> rising;
	std::vector<Rectf> falling;
	float area = 0.0f;
	auto addSegment = [&](Vector2f from, Vector2f to, bool straight) {
		area += from.cross(to);
		const float height = to.y - from.y;
		if (!straight || std::fabs(height) < shortestEdge || std::fabs(to.x - from.x) > steepest * std::fabs(height)) {
			return;
		}
		const Rectf edge{(from.x + to.x) * 0.5f, std::min(from.y, to.y), 0.0f, std::fabs(height)};
		(height > 0.0f ? rising : falling).push_back(edge);
	};

	const std::vector<Vector2f>& points = pixels.getPoints();
	Vector2f start;
	Vector2f current;
	bool open = false;
	std::size_t index = 0;
	for (const OutlineVerb verb : pixels.getVerbs()) {
		switch (verb) {
		case OutlineVerb::Move:
			if (open) {
				addSegment(current, start, true);
			}
			start = points[index];
			current = start;
			open = true;
			index += 1;
			break;
		case OutlineVerb::Line:
			addSegment(current, points[index], true);
			current = points[index];
			index += 1;
			break;
		case OutlineVerb::Quad:
			addSegment(current, points[index], false);
			addSegment(points[index], points[index + 1], false);
			current = points[index + 1];
			index += 2;
			break;
		case OutlineVerb::Cubic:
			addSegment(current, points[index], false);
			addSegment(points[index], points[index + 1], false);
			addSegment(points[index + 1], points[index + 2], false);
			current = points[index + 2];
			index += 3;
			break;
		case OutlineVerb::Close:
			// The closing edge is drawn by close(), and is often a stem side.
			addSegment(current, start, true);
			current = start;
			open = false;
			break;
		}
	}
	if (open) {
		addSegment(current, start, true);
	}

	// With y up, a clockwise outline (TrueType's convention, negative area) is
	// filled to the right of its rising edges; a counter-clockwise one is the
	// mirror image.
	const std::vector<Rectf>& leftEdges = area < 0.0f ? rising : falling;
	const std::vector<Rectf>& rightEdges = area < 0.0f ? falling : rising;

	// A stem is a left edge and the nearest right edge facing it. The tallest
	// stem wins, the leftmost one on a tie (the H's left side).
	bool found = false;
	float stemX = 0.0f;
	float stemHeight = 0.0f;
	for (const Rectf& left : leftEdges) {
		float width = thickest + 1.0f;
		float height = 0.0f;
		for (const Rectf& right : rightEdges) {
			// The rectangles have y up here, so getBottom() is the top end.
			const float distance = right.x - left.x;
			const float overlap = std::min(left.getBottom(), right.getBottom()) - std::max(left.y, right.y);
			if (distance < thinnest || distance > thickest || overlap < shortestOverlap || distance >= width) {
				continue;
			}
			width = distance;
			height = overlap;
		}
		if (width > thickest) {
			continue;
		}
		const bool taller = height > stemHeight + 0.01f;
		const bool tiedLeftOf = std::fabs(height - stemHeight) <= 0.01f && left.x < stemX;
		if (!found || taller || tiedLeftOf) {
			found = true;
			stemX = left.x;
			stemHeight = height;
		}
	}
	return found ? std::round(stemX) - stemX : 0.0f;
}

VerticalZones Hinter::findZones(const Font& font) {
	auto measure = [&font](char32_t codePoint, Rectf& bounds) {
		const GlyphId glyph = font.getGlyph(codePoint);
		Outline outline;
		if (glyph == 0 || !font.getOutline(glyph, outline) || outline.isEmpty()) {
			return false;
		}
		bounds = outline.getBounds();
		return true;
	};

	// The zones must match the flat tops the glyphs really have, which can be a
	// unit away from the OS/2 heights (Arial's H is 1466, its sCapHeight 1467),
	// so measure x and H and fall back to the metrics.
	const FontMetrics& metrics = font.getMetrics();
	VerticalZones zones;
	Rectf bounds;
	zones.xHeight = measure(U'x', bounds) ? bounds.getBottom() : static_cast<float>(metrics.xHeight);
	zones.capHeight = measure(U'H', bounds) ? bounds.getBottom() : static_cast<float>(metrics.capHeight);
	if (measure(U'p', bounds) || measure(U'q', bounds)) {
		zones.descender = bounds.y;
	}
	if (measure(U'l', bounds) || measure(U'h', bounds)) {
		zones.ascender = bounds.getBottom();
	}

	// Overshoot: how far o and O reach past the baseline, x-height and caps.
	float overshoot = 0.0f;
	if (measure(U'o', bounds)) {
		overshoot = std::max(overshoot, -bounds.y);
		if (zones.xHeight > 0.0f) {
			overshoot = std::max(overshoot, bounds.getBottom() - zones.xHeight);
		}
	}
	if (measure(U'O', bounds) && zones.capHeight > 0.0f) {
		overshoot = std::max(overshoot, bounds.getBottom() - zones.capHeight);
	}
	const float largestOvershoot = 0.04f * static_cast<float>(metrics.unitsPerEm);
	zones.overshoot = std::clamp(overshoot, 0.0f, largestOvershoot);
	return zones;
}

}
