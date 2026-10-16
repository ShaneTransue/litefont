#pragma once

#include <litefont/Vector2.h>
#include <litefont/Size2.h>
#include <litefont/Rect.h>
#include <litefont/Affine2.h>
#include <string>
#include <string_view>

#include <vector>

namespace litefont {

/**
 * @addtogroup Shape
 *
 * OutlineVerb - One drawing step of an Outline. Move and Line take one point,
 * Quad two (control, end), Cubic three (control, control, end), Close none.
 */
enum class OutlineVerb : std::uint8_t {
	Move,
	Line,
	Quad,
	Cubic,
	Close
};

/**
 * \struct Polylines Outline.h <litefont/Outline.h>
 * @addtogroup Shape
 *
 * Polylines - Flattened contours stored flat for cache-friendly rasterization:
 * one point array, and the index where each contour starts. Every contour is
 * closed implicitly from its last point back to its first.
 */
struct Polylines {
	std::vector<Vector2f> points;
	std::vector<std::uint32_t> contourStarts;
};

/**
 * \class Outline Outline.h <litefont/Outline.h>
 * @addtogroup Shape
 *
 * Outline - A vector shape made of contours of lines, quadratic and cubic
 * Bezier curves. Each moveTo starts a contour. Fonts produce outlines now and
 * SVG later; the rasterizer and the MSDF generator consume their flattened form.
 *
 * getEmboldened() thickens the filled shape the way FreeType's
 * FT_Outline_EmboldenXY does: every point, control points included, moves
 * outward along the bisector of its two edges' normals, so straight edges
 * move by half the strength (per axis) and corners keep their angle (miters
 * are limited to twice the distance). Holes shrink. Contours may be drawn
 * either way round; the shape's overall direction says which side is out.
 */
class Outline {
public:
	void moveTo(Vector2f point);
	void lineTo(Vector2f point);
	void quadTo(Vector2f control, Vector2f point);
	void cubicTo(Vector2f control1, Vector2f control2, Vector2f point);
	void close();
	void append(const Outline& other);
	void clear();

	Outline getTransformed(const Affine2f& transform) const;
	Outline getEmboldened(float xStrength, float yStrength) const;
	Rectf getBounds() const;
	Polylines flatten(float tolerance) const;
	bool isEmpty() const;
	std::string toSvgPath() const;

	const std::vector<OutlineVerb>& getVerbs() const;
	const std::vector<Vector2f>& getPoints() const;

private:
	std::vector<OutlineVerb> verbs;
	std::vector<Vector2f> points;
};

}
