#include <litefont/Font.h>
#include <litefont/Matrix4.h>
#include <litefont/SoftwareDevice.h>
#include <litefont/Text.h>
#include <litefont/TextPainter.h>
#include "TestKit.h"

#include "BlockFont.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>

namespace litefont::tests {

class TextPainterTests {
	// The largest difference of any channel of any pixel, or 255 when the
	// sizes differ.
	static std::int32_t compare(const Image& a, const Image& b) {
		if (a.getSize() != b.getSize()) {
			return 255;
		}
		std::int32_t largest = 0;
		for (std::int32_t y = 0; y < a.getHeight(); y++) {
			for (std::int32_t x = 0; x < a.getWidth(); x++) {
				const Color4u8 p = a.getPixel(x, y);
				const Color4u8 q = b.getPixel(x, y);
				largest = std::max({largest, std::abs(p.r - q.r), std::abs(p.g - q.g), std::abs(p.b - q.b), std::abs(p.a - q.a)});
			}
		}
		return largest;
	}

	static std::int32_t countLit(const Image& image) {
		std::int32_t lit = 0;
		for (std::int32_t y = 0; y < image.getHeight(); y++) {
			for (std::int32_t x = 0; x < image.getWidth(); x++) {
				lit += image.getPixel(x, y).r > 100 ? 1 : 0;
			}
		}
		return lit;
	}

	static Image paint(SoftwareDevice& device, const std::function<void()>& draw) {
		device.beginFrame(Size2i{64, 40}, Color4f{0.0f, 0.0f, 0.0f, 1.0f});
		draw();
		device.endFrame();
		return device.readPixels();
	}

	static inline test::Test spaces{"TextPainter: pixels, a y-down matrix and a y-up matrix give the same picture on the CPU", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		SoftwareDevice device;
		TextPainter painter(device);
		Text text("ab pW\nxa", font, 13.0f);
		text.setPosition(Vector2f{7.0f, 5.0f});
		text.setGradient(TextGradient::vertical(Color4f{1.0f, 0.5f, 0.2f, 1.0f}, Color4f{0.3f, 0.6f, 1.0f, 1.0f}));
		Text raised = text;
		raised.setPosition(Vector2f{7.0f, 35.0f});

		const Image pixels = TextPainterTests::paint(device, [&]() {
			painter.draw(text);
		});
		const Image down = TextPainterTests::paint(device, [&]() {
			device.setMatrix(Matrix4f::ortho(0.0f, 64.0f, 40.0f, 0.0f, -1.0f, 1.0f));
			device.setYUp(false);
			painter.draw(text);
		});
		const Image up = TextPainterTests::paint(device, [&]() {
			device.setMatrix(Matrix4f::ortho(0.0f, 64.0f, 0.0f, 40.0f, -1.0f, 1.0f));
			device.setYUp(true);
			painter.draw(raised);
		});
		device.useViewport();
		// The matrices move vertices by float rounding, which can shift a
		// gradient's blend by one level.
		context.check(TextPainterTests::countLit(pixels) > 40, "the text draws");
		context.check(TextPainterTests::compare(pixels, down) <= 1, "y down matches pixels to one level");
		context.check(TextPainterTests::compare(pixels, up) <= 1, "y up matches pixels to one level");
	}};

	static inline test::Test zoom{"TextPainter: under a 2x matrix, text renders at twice its size, as sharp as drawing it twice as large", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		SoftwareDevice device;
		TextPainter painter(device);
		const Image large = TextPainterTests::paint(device, [&]() {
			painter.drawText(font, 26.0f, "abW", 14.0f, 10.0f);
		});
		const Image zoomed = TextPainterTests::paint(device, [&]() {
			device.setMatrix(Matrix4f::ortho(0.0f, 32.0f, 20.0f, 0.0f, -1.0f, 1.0f));
			device.setYUp(false);
			painter.drawText(font, 13.0f, "abW", 7.0f, 5.0f);
		});
		device.useViewport();
		context.check(TextPainterTests::countLit(large) > 100, "the text draws");
		context.checkEqual(TextPainterTests::compare(large, zoomed), 0);
	}};

	static inline test::Test pixelScale{"GraphicsDevice: getPixelScale measures the matrix at a point, and is 1 on pixels", [](test::Context& context) {
		SoftwareDevice device;
		device.beginFrame(Size2i{64, 40}, Color4f{});
		context.checkEqual(device.getPixelScale(Vector2f{3.0f, 4.0f}), 1.0f);
		device.setMatrix(Matrix4f::ortho(0.0f, 32.0f, 20.0f, 0.0f, -1.0f, 1.0f));
		context.check(std::fabs(device.getPixelScale(Vector2f{3.0f, 4.0f}) - 2.0f) < 1e-5f, "a 2x zoom");
		device.setMatrix(Matrix4f::ortho(0.0f, 32.0f, 20.0f, 0.0f, -1.0f, 1.0f) * Matrix4f::rotateZ(0.5f) * Matrix4f::scale(1.5f, 1.5f, 1.0f));
		context.check(std::fabs(device.getPixelScale(Vector2f{3.0f, 4.0f}) - 3.0f) < 1e-4f, "turning keeps the scale");
		device.useViewport();
		context.checkEqual(device.getPixelScale(Vector2f{3.0f, 4.0f}), 1.0f);
	}};

	static inline test::Test rasterScale{"TextPainter: raster scales step by sixths of an octave and stop at 256 pixel glyphs", [](test::Context& context) {
		context.checkEqual(TextPainter::getRasterScale(1.0f, 16.0f), 1.0f);
		context.checkEqual(TextPainter::getRasterScale(2.0f, 16.0f), 2.0f);
		context.checkEqual(TextPainter::getRasterScale(1.05f, 16.0f), 1.0f);
		context.check(std::fabs(TextPainter::getRasterScale(3.0f, 16.0f) - std::exp2(10.0f / 6.0f)) < 1e-5f, "3x rounds to 2^(10/6)");
		context.checkEqual(TextPainter::getRasterScale(100.0f, 16.0f), 16.0f);
		context.checkEqual(TextPainter::getRasterScale(0.0f, 16.0f), 1.0f);
	}};
};

}
