#include <litefont/Outline.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <cmath>
#include <random>

namespace litefont::tests {

class OutlineTests {
	static float distanceToSegment(Vector2f point, Vector2f a, Vector2f b) {
		const Vector2f edge = b - a;
		const float lengthSquared = edge.getLengthSquared();
		float t = lengthSquared > 0.0f ? (point - a).dot(edge) / lengthSquared : 0.0f;
		t = std::fmax(0.0f, std::fmin(1.0f, t));
		return (point - (a + edge * t)).getLength();
	}

	static float distanceToPolyline(Vector2f point, const Polylines& polylines) {
		float best = 1e30f;
		for (std::size_t i = 1; i < polylines.points.size(); i++) {
			best = std::fmin(best, distanceToSegment(point, polylines.points[i - 1], polylines.points[i]));
		}
		return best;
	}

	static inline test::Test records{"Outline: records verbs and points and writes SVG path text", [](test::Context& context) {
		Outline outline;
		outline.moveTo(Vector2f{0.0f, 0.0f});
		outline.lineTo(Vector2f{10.0f, 0.0f});
		outline.quadTo(Vector2f{10.0f, 10.0f}, Vector2f{0.5f, 10.0f});
		outline.close();

		context.checkEqual(outline.getVerbs().size(), std::size_t{4});
		context.checkEqual(outline.getPoints().size(), std::size_t{4});
		context.checkEqual(outline.toSvgPath(), std::string("M 0 0 L 10 0 Q 10 10 0.5 10 Z"));
		context.check(!outline.isEmpty(), "not empty");
		outline.clear();
		context.check(outline.isEmpty(), "empty after clear");
	}};

	static inline test::Test exactBounds{"Outline: bounds include where curves bulge past their end points", [](test::Context& context) {
		Outline quad;
		quad.moveTo(Vector2f{0.0f, 0.0f});
		quad.quadTo(Vector2f{50.0f, 100.0f}, Vector2f{100.0f, 0.0f});
		context.checkEqual(quad.getBounds(), Rectf{0.0f, 0.0f, 100.0f, 50.0f});

		Outline cubic;
		cubic.moveTo(Vector2f{0.0f, 0.0f});
		cubic.cubicTo(Vector2f{0.0f, 100.0f}, Vector2f{100.0f, 100.0f}, Vector2f{100.0f, 0.0f});
		const Rectf bounds = cubic.getBounds();
		context.check(std::fabs(bounds.height - 75.0f) < 1e-4f, "cubic peak is three quarters of the control height");
		context.checkEqual(bounds.width, 100.0f);
	}};

	static inline test::Test transforms{"Outline: getTransformed moves every point", [](test::Context& context) {
		Outline outline;
		outline.moveTo(Vector2f{1.0f, 2.0f});
		outline.lineTo(Vector2f{3.0f, 4.0f});
		const Outline moved = outline.getTransformed(Affine2f::scale(2.0f, 2.0f).then(Affine2f::translate(1.0f, 0.0f)));
		context.checkEqual(moved.getPoints()[0], Vector2f{3.0f, 4.0f});
		context.checkEqual(moved.getPoints()[1], Vector2f{7.0f, 8.0f});
	}};

	static inline test::Test flattensWithinTolerance{"Outline: flattened curves stay within the tolerance (random curves)", [](test::Context& context) {
		std::mt19937 random(7u);
		std::uniform_real_distribution<float> coordinate(-500.0f, 500.0f);
		constexpr float tolerance = 0.25f;

		for (std::int32_t trial = 0; trial < 200; trial++) {
			const Vector2f p0{coordinate(random), coordinate(random)};
			const Vector2f p1{coordinate(random), coordinate(random)};
			const Vector2f p2{coordinate(random), coordinate(random)};
			const Vector2f p3{coordinate(random), coordinate(random)};
			const bool cubic = trial % 2 == 1;

			Outline outline;
			outline.moveTo(p0);
			if (cubic) {
				outline.cubicTo(p1, p2, p3);
			}
			else {
				outline.quadTo(p1, p2);
			}
			const Polylines flat = outline.flatten(tolerance);

			float worst = 0.0f;
			for (std::int32_t sample = 0; sample <= 400; sample++) {
				const float t = static_cast<float>(sample) / 400.0f;
				const float u = 1.0f - t;
				const Vector2f point = cubic
					? p0 * (u * u * u) + p1 * (3.0f * u * u * t) + p2 * (3.0f * u * t * t) + p3 * (t * t * t)
					: p0 * (u * u) + p1 * (2.0f * u * t) + p2 * (t * t);
				worst = std::fmax(worst, distanceToPolyline(point, flat));
			}
			context.check(worst <= tolerance * 1.01f, "curve stays within the tolerance of its polyline");
		}
	}};

	static inline test::Test contourLayout{"Outline: flatten starts a contour at each move and drops empty ones", [](test::Context& context) {
		Outline outline;
		outline.moveTo(Vector2f{0.0f, 0.0f});
		outline.lineTo(Vector2f{1.0f, 0.0f});
		outline.lineTo(Vector2f{1.0f, 1.0f});
		outline.close();
		outline.moveTo(Vector2f{5.0f, 5.0f});      // a lone move encloses nothing
		outline.moveTo(Vector2f{2.0f, 2.0f});
		outline.lineTo(Vector2f{3.0f, 2.0f});
		outline.close();

		const Polylines flat = outline.flatten(0.1f);
		context.checkEqual(flat.contourStarts.size(), std::size_t{2});
		context.checkEqual(flat.points.size(), std::size_t{5});
		context.checkEqual(flat.contourStarts[1], std::uint32_t{3});
		context.checkEqual(flat.points[3], Vector2f{2.0f, 2.0f});
	}};
};

}
