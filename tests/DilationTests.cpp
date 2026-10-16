#include <litefont/Outline.h>
#include <litefont/Rasterizer.h>
#include <litefont/Dilation.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <cmath>

namespace litefont::tests {

class DilationTests {
	// A clockwise rectangle with y up, as TrueType draws filled contours.
	static Outline makeRectangle(float left, float bottom, float right, float top, bool clockwise = true) {
		Outline outline;
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
		return outline;
	}

	static bool isNear(const Rectf& actual, const Rectf& expected) {
		return std::fabs(actual.x - expected.x) < 1e-3f && std::fabs(actual.y - expected.y) < 1e-3f
			&& std::fabs(actual.width - expected.width) < 1e-3f && std::fabs(actual.height - expected.height) < 1e-3f;
	}

	static inline test::Test emboldens{"Outline: getEmboldened grows filled shapes, shrinks holes and keeps corners", [](test::Context& context) {
		const Outline wide = makeRectangle(0.0f, 0.0f, 100.0f, 50.0f).getEmboldened(20.0f, 0.0f);
		context.check(isNear(wide.getBounds(), Rectf{-10.0f, 0.0f, 120.0f, 50.0f}), "x only");
		const Outline both = makeRectangle(0.0f, 0.0f, 100.0f, 50.0f).getEmboldened(20.0f, 10.0f);
		context.check(isNear(both.getBounds(), Rectf{-10.0f, -5.0f, 120.0f, 60.0f}), "both axes");
		const Outline reversed = makeRectangle(0.0f, 0.0f, 100.0f, 50.0f, false).getEmboldened(20.0f, 0.0f);
		context.check(isNear(reversed.getBounds(), Rectf{-10.0f, 0.0f, 120.0f, 50.0f}), "either drawing direction");

		// A ring: the outer square grows, the counter-clockwise hole shrinks.
		Outline ring = makeRectangle(0.0f, 0.0f, 100.0f, 100.0f);
		ring.append(makeRectangle(30.0f, 30.0f, 70.0f, 70.0f, false));
		const Outline bolder = ring.getEmboldened(10.0f, 10.0f);
		const std::vector<Vector2f>& points = bolder.getPoints();
		context.check((points[0] - Vector2f{-5.0f, -5.0f}).getLength() < 1e-3f, "the outer corner moves out");
		context.check((points[4] - Vector2f{35.0f, 35.0f}).getLength() < 1e-3f, "the hole's corner moves in");
		context.check(ring.getEmboldened(0.0f, 0.0f).getPoints() == ring.getPoints(), "zero strength changes nothing");
	}};

	static inline test::Test grows{"Dilation: grows a block by its radius with an antialiased rim and round corners", [](test::Context& context) {
		Polylines block;
		block.contourStarts.push_back(0);
		block.points = {Vector2f{3.0f, 3.0f}, Vector2f{7.0f, 3.0f}, Vector2f{7.0f, 6.0f}, Vector2f{3.0f, 6.0f}};
		Image image(10, 9, PixelFormat::Gray8);
		Rasterizer rasterizer;
		rasterizer.fill(block, FillRule::NonZero, image);
		context.check(Dilation::apply(block, 1.0f, image), "gray coverage dilates");

		context.checkEqual(image.getRow(4)[4], std::uint8_t{255});
		context.checkEqual(image.getRow(4)[2], std::uint8_t{255});
		context.checkEqual(image.getRow(2)[5], std::uint8_t{255});
		context.checkEqual(image.getRow(4)[1], std::uint8_t{0});
		// The corner pixel's center is sqrt(0.5) from the corner: 1.5 - 0.707.
		context.checkEqual(image.getRow(2)[2], std::uint8_t{202});
		context.checkEqual(image.getRow(0)[0], std::uint8_t{0});

		Image half(10, 9, PixelFormat::Gray8);
		rasterizer.fill(block, FillRule::NonZero, half);
		Dilation::apply(block, 0.5f, half);
		context.checkEqual(half.getRow(4)[2], std::uint8_t{128});
		Image rgb(4, 4, PixelFormat::RGB8);
		context.check(!Dilation::apply(block, 1.0f, rgb), "only gray coverage");
	}};
};

}
