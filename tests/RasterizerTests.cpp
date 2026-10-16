#include <litefont/Font.h>
#include <litefont/Rasterizer.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <cmath>
#include <limits>
#include <random>

namespace litefont::tests {

/**
 * Checks the Rasterizer against an analytic oracle: the exact area of every
 * contour clipped to each pixel (Sutherland-Hodgman, in double precision),
 * summed with its sign and folded by the fill rule. That is the coverage the
 * rasterizer is defined to produce, so any shape can be checked, glyphs from
 * installed fonts included.
 */
class RasterizerTests {
	static void clipAxis(const std::vector<Vector2d>& input, std::vector<Vector2d>& output, bool vertical, double bound, bool keepGreater) {
		output.clear();
		if (input.empty()) {
			return;
		}
		auto coordinate = [vertical](const Vector2d& point) {
			return vertical ? point.y : point.x;
		};
		auto inside = [&](const Vector2d& point) {
			return keepGreater ? coordinate(point) >= bound : coordinate(point) <= bound;
		};
		Vector2d previous = input.back();
		for (const Vector2d& current : input) {
			const bool currentInside = inside(current);
			if (currentInside != inside(previous)) {
				const double t = (bound - coordinate(previous)) / (coordinate(current) - coordinate(previous));
				output.push_back(previous + (current - previous) * t);
			}
			if (currentInside) {
				output.push_back(current);
			}
			previous = current;
		}
	}

	static double getClippedArea(const std::vector<Vector2d>& polygon, double left, double top) {
		std::vector<Vector2d> first;
		std::vector<Vector2d> second;
		clipAxis(polygon, first, false, left, true);
		clipAxis(first, second, false, left + 1.0, false);
		clipAxis(second, first, true, top, true);
		clipAxis(first, second, true, top + 1.0, false);
		double area = 0.0;
		for (std::size_t i = 0; i < second.size(); i++) {
			area += second[i].cross(second[(i + 1) % second.size()]);
		}
		return area * 0.5;
	}

	static Image renderOracle(const Polylines& polylines, FillRule rule, std::int32_t width, std::int32_t height) {
		std::vector<std::vector<Vector2d>> polygons;
		std::vector<Rect<double>> bounds;
		for (std::size_t contour = 0; contour < polylines.contourStarts.size(); contour++) {
			const std::size_t start = polylines.contourStarts[contour];
			const std::size_t end = contour + 1 < polylines.contourStarts.size() ? polylines.contourStarts[contour + 1] : polylines.points.size();
			std::vector<Vector2d> polygon;
			Vector2d minimum{1e300, 1e300};
			Vector2d maximum{-1e300, -1e300};
			for (std::size_t i = start; i < end; i++) {
				const Vector2d point = polylines.points[i].convert<double>();
				polygon.push_back(point);
				minimum = Vector2d{std::fmin(minimum.x, point.x), std::fmin(minimum.y, point.y)};
				maximum = Vector2d{std::fmax(maximum.x, point.x), std::fmax(maximum.y, point.y)};
			}
			polygons.push_back(std::move(polygon));
			bounds.push_back(Rect<double>::fromCorners(minimum, maximum));
		}

		Image image(width, height, PixelFormat::Gray8);
		for (std::int32_t y = 0; y < height; y++) {
			for (std::int32_t x = 0; x < width; x++) {
				double winding = 0.0;
				for (std::size_t i = 0; i < polygons.size(); i++) {
					const Rect<double>& box = bounds[i];
					if (box.x > x + 1.0 || box.getRight() < x || box.y > y + 1.0 || box.getBottom() < y) {
						continue;
					}
					winding += getClippedArea(polygons[i], x, y);
				}
				double coverage = std::fabs(winding);
				if (rule == FillRule::EvenOdd) {
					coverage = std::fmod(coverage, 2.0);
					coverage = coverage > 1.0 ? 2.0 - coverage : coverage;
				}
				else {
					coverage = std::fmin(coverage, 1.0);
				}
				image.getRow(y)[x] = static_cast<std::uint8_t>(std::floor(coverage * 255.0 + 0.5));
			}
		}
		return image;
	}

	// The rasterizer rounds points to 1/256 pixel; give the oracle the same points.
	static Polylines quantize(const Polylines& polylines) {
		Polylines result = polylines;
		for (Vector2f& point : result.points) {
			point = Vector2f{std::round(point.x * 256.0f) / 256.0f, std::round(point.y * 256.0f) / 256.0f};
		}
		return result;
	}

	static std::int32_t getLargestDifference(const Image& actual, const Image& expected, std::size_t& differing) {
		std::int32_t largest = 0;
		differing = 0;
		for (std::size_t i = 0; i < actual.getPixels().size(); i++) {
			const std::int32_t difference = std::abs(std::int32_t{actual.getPixels()[i]} - std::int32_t{expected.getPixels()[i]});
			largest = std::max(largest, difference);
			differing += difference != 0 ? 1 : 0;
		}
		return largest;
	}

	static void addPolygon(Polylines& polylines, std::initializer_list<Vector2f> points) {
		polylines.contourStarts.push_back(static_cast<std::uint32_t>(polylines.points.size()));
		polylines.points.insert(polylines.points.end(), points.begin(), points.end());
	}

	static inline test::Test exactRectangles{"Rasterizer: rectangles at fractional offsets get their exact area", [](test::Context& context) {
		std::mt19937 random(3u);
		std::uniform_int_distribution<std::int32_t> corner(-3 * 256, 11 * 256);
		Rasterizer rasterizer;
		std::int32_t wrong = 0;
		for (std::int32_t trial = 0; trial < 400; trial++) {
			const float x0 = static_cast<float>(corner(random)) / 256.0f;
			const float x1 = static_cast<float>(corner(random)) / 256.0f;
			const float y0 = static_cast<float>(corner(random)) / 256.0f;
			const float y1 = static_cast<float>(corner(random)) / 256.0f;
			Polylines polylines;
			addPolygon(polylines, {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}});

			Image image(8, 8, PixelFormat::Gray8);
			rasterizer.fill(polylines, FillRule::NonZero, image);
			for (std::int32_t y = 0; y < 8; y++) {
				for (std::int32_t x = 0; x < 8; x++) {
					auto overlap = [](double a, double b, double cell) {
						return std::fmax(0.0, std::fmin(std::fmax(a, b), cell + 1.0) - std::fmax(std::fmin(a, b), cell));
					};
					const double area = overlap(x0, x1, x) * overlap(y0, y1, y);
					const std::uint8_t expected = static_cast<std::uint8_t>(std::floor(area * 255.0 + 0.5));
					wrong += image.getRow(y)[x] != expected ? 1 : 0;
				}
			}
		}
		context.checkEqual(wrong, 0);
	}};

	static inline test::Test analyticTriangles{"Rasterizer: random triangles match the analytic coverage within one level", [](test::Context& context) {
		std::mt19937 random(5u);
		std::uniform_real_distribution<float> coordinate(-4.0f, 20.0f);
		Rasterizer rasterizer;
		std::int32_t largest = 0;
		std::size_t differing = 0;
		for (std::int32_t trial = 0; trial < 300; trial++) {
			Polylines polylines;
			addPolygon(polylines, {{coordinate(random), coordinate(random)}, {coordinate(random), coordinate(random)}, {coordinate(random), coordinate(random)}});
			polylines = quantize(polylines);
			Image image(16, 16, PixelFormat::Gray8);
			rasterizer.fill(polylines, FillRule::NonZero, image);
			std::size_t count = 0;
			largest = std::max(largest, getLargestDifference(image, renderOracle(polylines, FillRule::NonZero, 16, 16), count));
			differing += count;
		}
		context.check(largest <= 1, "within one level");
		context.note(std::format("{} of {} pixels differ by one level", differing, 300 * 256));
	}};

	static inline test::Test fillRules{"Rasterizer: the fill rules differ only where contours overlap", [](test::Context& context) {
		Polylines sameDirection;
		addPolygon(sameDirection, {{1.0f, 1.0f}, {9.0f, 1.0f}, {9.0f, 9.0f}, {1.0f, 9.0f}});
		addPolygon(sameDirection, {{3.0f, 3.0f}, {7.0f, 3.0f}, {7.0f, 7.0f}, {3.0f, 7.0f}});
		Polylines hole;
		addPolygon(hole, {{1.0f, 1.0f}, {9.0f, 1.0f}, {9.0f, 9.0f}, {1.0f, 9.0f}});
		addPolygon(hole, {{3.0f, 3.0f}, {3.0f, 7.0f}, {7.0f, 7.0f}, {7.0f, 3.0f}});

		Rasterizer rasterizer;
		Image image(10, 10, PixelFormat::Gray8);
		rasterizer.fill(sameDirection, FillRule::NonZero, image);
		context.checkEqual(image.getRow(5)[5], std::uint8_t{255});
		context.checkEqual(image.getRow(2)[2], std::uint8_t{255});
		rasterizer.fill(sameDirection, FillRule::EvenOdd, image);
		context.checkEqual(image.getRow(5)[5], std::uint8_t{0});
		context.checkEqual(image.getRow(2)[2], std::uint8_t{255});
		rasterizer.fill(hole, FillRule::NonZero, image);
		context.checkEqual(image.getRow(5)[5], std::uint8_t{0});
		context.checkEqual(image.getRow(0)[0], std::uint8_t{0});

		Polylines star;
		const float pi = 3.14159265f;
		star.contourStarts.push_back(0);
		for (std::int32_t i = 0; i < 5; i++) {
			const float angle = static_cast<float>(i * 2) * 2.0f * pi / 5.0f;
			star.points.push_back(Vector2f{12.0f + 11.0f * std::sin(angle), 12.0f - 11.0f * std::cos(angle)});
		}
		star = quantize(star);
		for (const FillRule rule : {FillRule::NonZero, FillRule::EvenOdd}) {
			Image starImage(24, 24, PixelFormat::Gray8);
			rasterizer.fill(star, rule, starImage);
			std::size_t count = 0;
			context.check(getLargestDifference(starImage, renderOracle(star, rule, 24, 24), count) <= 1, "self-intersecting star matches the oracle");
		}
	}};

	static inline test::Test clips{"Rasterizer: shapes crossing or far outside the edges are clipped", [](test::Context& context) {
		std::mt19937 random(9u);
		std::uniform_real_distribution<float> coordinate(-40.0f, 50.0f);
		Rasterizer rasterizer;
		std::int32_t largest = 0;
		for (std::int32_t trial = 0; trial < 200; trial++) {
			Polylines polylines;
			polylines.contourStarts.push_back(0);
			for (std::int32_t i = 0; i < 6; i++) {
				polylines.points.push_back(Vector2f{coordinate(random), coordinate(random)});
			}
			polylines = quantize(polylines);
			const FillRule rule = trial % 2 == 0 ? FillRule::NonZero : FillRule::EvenOdd;
			Image image(10, 7, PixelFormat::Gray8);
			rasterizer.fill(polylines, rule, image);
			std::size_t count = 0;
			largest = std::max(largest, getLargestDifference(image, renderOracle(polylines, rule, 10, 7), count));
		}
		context.check(largest <= 1, "clipped random polygons match the oracle");

		const float huge = 1e30f;
		Polylines covering;
		addPolygon(covering, {{-huge, -huge}, {huge, -huge}, {0.0f, huge}});
		Image image(5, 5, PixelFormat::Gray8);
		rasterizer.fill(covering, FillRule::NonZero, image);
		bool full = true;
		for (const std::uint8_t value : image.getPixels()) {
			full = full && value == 255;
		}
		context.check(full, "a clamped huge triangle still covers the image");

		const float nan = std::numeric_limits<float>::quiet_NaN();
		const float infinity = std::numeric_limits<float>::infinity();
		Polylines broken;
		addPolygon(broken, {{nan, 1.0f}, {infinity, -infinity}, {2.0f, nan}, {3.0f, 3.0f}});
		broken.contourStarts.push_back(99);
		context.check(rasterizer.fill(broken, FillRule::NonZero, image), "NaN, infinity and a bad contour start are survived");
	}};

	static inline test::Test reusesBuffer{"Rasterizer: results do not depend on earlier calls", [](test::Context& context) {
		Polylines shape;
		addPolygon(shape, {{0.5f, 0.25f}, {6.75f, 2.0f}, {2.0f, 5.5f}});
		Rasterizer used;
		Image large(40, 30, PixelFormat::Gray8);
		used.fill(shape, FillRule::NonZero, large);
		Image second(7, 6, PixelFormat::Gray8);
		used.fill(shape, FillRule::NonZero, second);
		Rasterizer fresh;
		Image expected(7, 6, PixelFormat::Gray8);
		fresh.fill(shape, FillRule::NonZero, expected);
		context.check(second == expected, "same bytes");

		Image rgb(4, 4, PixelFormat::RGB8);
		context.check(!used.fill(shape, FillRule::NonZero, rgb), "only gray targets");
		Image empty;
		context.check(used.fill(shape, FillRule::NonZero, empty), "an empty target is fine");
	}};

	static inline test::Test golden{"Rasterizer: a star and a ring match their golden image byte for byte", [](test::Context& context) {
		// Exact integer accumulation makes this image the same on every compiler.
		Outline shapes;
		const float pi = 3.14159265f;
		for (std::int32_t i = 0; i < 5; i++) {
			const float angle = static_cast<float>(i * 2) * 2.0f * pi / 5.0f;
			const Vector2f point{16.3f + 14.0f * std::sin(angle), 16.6f - 14.0f * std::cos(angle)};
			if (i == 0) {
				shapes.moveTo(point);
			}
			else {
				shapes.lineTo(point);
			}
		}
		shapes.close();
		const float k = 0.5523f;
		for (const float radius : {12.0f, -7.5f}) {
			// Four cubic quarter circles; the negative radius winds the other way, cutting a hole.
			const float cx = 46.4f;
			const float cy = 16.2f;
			const float r = std::fabs(radius);
			const float s = radius > 0.0f ? 1.0f : -1.0f;
			shapes.moveTo(Vector2f{cx + r, cy});
			shapes.cubicTo(Vector2f{cx + r, cy + s * k * r}, Vector2f{cx + k * r, cy + s * r}, Vector2f{cx, cy + s * r});
			shapes.cubicTo(Vector2f{cx - k * r, cy + s * r}, Vector2f{cx - r, cy + s * k * r}, Vector2f{cx - r, cy});
			shapes.cubicTo(Vector2f{cx - r, cy - s * k * r}, Vector2f{cx - k * r, cy - s * r}, Vector2f{cx, cy - s * r});
			shapes.cubicTo(Vector2f{cx + k * r, cy - s * r}, Vector2f{cx + r, cy - s * k * r}, Vector2f{cx + r, cy});
			shapes.close();
		}

		Rasterizer rasterizer;
		Image image(64, 32, PixelFormat::Gray8);
		rasterizer.fill(shapes.flatten(0.02f), FillRule::NonZero, image);
		context.checkImage(image, "tests/golden/rasterizer-star-ring.pgm");
	}};

	static inline test::Test glyphs{"Rasterizer: glyphs from installed fonts match the analytic coverage within one level", [](test::Context& context) {
		const char* fontNames[] = {"segoeui", "bahnschrift", "consola"};
		const std::string_view sample = "agQ@&%$Rgsw8B";
		const float sizes[] = {9.0f, 13.0f, 27.0f};
		std::mt19937 random(13u);
		std::uniform_real_distribution<float> shift(0.0f, 1.0f);

		Rasterizer rasterizer;
		std::int32_t largest = 0;
		std::size_t differing = 0;
		std::size_t pixels = 0;
		std::int32_t glyphCount = 0;
		for (const char* name : fontNames) {
			const std::filesystem::path path = test::Context::getFontFolder() / (std::string(name) + ".ttf");
			Font font;
			if (!font.load(path)) {
				context.note(std::format("{} is not in the fonts folder; skipped", name));
				continue;
			}
			for (const char character : sample) {
				Outline outline;
				if (!font.getOutline(font.getGlyph(static_cast<char32_t>(character)), outline)) {
					context.check(false, "glyph outline");
					continue;
				}
				for (const float size : sizes) {
					const float scale = size / static_cast<float>(font.getMetrics().unitsPerEm);
					const Rectf bounds = outline.getBounds();
					const float left = 1.0f - bounds.x * scale + shift(random);
					const float baseline = 1.0f + bounds.getBottom() * scale + shift(random);
					const Outline placed = outline.getTransformed(Affine2f::scale(scale, -scale).then(Affine2f::translate(left, baseline)));
					const Polylines polylines = quantize(placed.flatten(0.05f));

					const std::int32_t width = static_cast<std::int32_t>(std::ceil(bounds.width * scale)) + 3;
					const std::int32_t height = static_cast<std::int32_t>(std::ceil(bounds.height * scale)) + 3;
					Image image(width, height, PixelFormat::Gray8);
					rasterizer.fill(polylines, FillRule::NonZero, image);
					std::size_t count = 0;
					largest = std::max(largest, getLargestDifference(image, renderOracle(polylines, FillRule::NonZero, width, height), count));
					differing += count;
					pixels += image.getPixels().size();
					glyphCount++;
				}
			}
		}
		context.check(largest <= 1, "within one level");
		context.note(std::format("{} glyph images, {} of {} pixels differ by one level", glyphCount, differing, pixels));
	}};
};

}
