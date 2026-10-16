#include <litefont/Blur.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <cmath>

namespace litefont::tests {

class BlurTests {
	static std::int64_t getTotal(const Image& image) {
		std::int64_t total = 0;
		for (const std::uint8_t value : image.getPixels()) {
			total += value;
		}
		return total;
	}

	static inline test::Test spreads{"Blur: a dot spreads into a symmetric Gaussian and keeps its total", [](test::Context& context) {
		Image image(41, 41, PixelFormat::Gray8);
		image.fill(Recti{18, 18, 5, 5}, Color4f{1.0f, 1.0f, 1.0f, 1.0f});
		const std::int64_t before = getTotal(image);
		Blur::gaussian(image, 2.0f);
		const std::int64_t after = getTotal(image);
		context.check(std::llabs(after - before) <= 41 * 41 / 2, "the total stays within rounding");

		bool symmetric = true;
		for (std::int32_t y = 0; y < 41; y++) {
			for (std::int32_t x = 0; x < 41; x++) {
				const std::uint8_t value = image.getPixel(x, y).r;
				symmetric = symmetric && value == image.getPixel(40 - x, y).r && value == image.getPixel(x, 40 - y).r && value == image.getPixel(y, x).r;
			}
		}
		context.check(symmetric, "mirror and diagonal symmetric");
		context.check(image.getPixel(20, 20).r < 255 && image.getPixel(20, 20).r > image.getPixel(20, 24).r, "the center softens and falls off");
		context.checkEqual(image.getPixel(20, 20 + 2 + Blur::getRadius(2.0f) + 1).r, std::uint8_t{0});
	}};

	static inline test::Test matchesGaussian{"Blur: a single pixel becomes the sampled Gaussian within one level", [](test::Context& context) {
		Image image(31, 31, PixelFormat::Gray8);
		image.setPixel(15, 15, Color4u8{255, 255, 255, 255});
		const float sigma = 1.5f;
		Blur::gaussian(image, sigma);
		double norm = 0.0;
		for (std::int32_t k = -Blur::getRadius(sigma); k <= Blur::getRadius(sigma); k++) {
			norm += std::exp(-static_cast<double>(k) * k / (2.0 * sigma * sigma));
		}
		std::int32_t largest = 0;
		for (std::int32_t y = 0; y < 31; y++) {
			for (std::int32_t x = 0; x < 31; x++) {
				const double dx = x - 15;
				const double dy = y - 15;
				const bool inside = std::fabs(dx) <= Blur::getRadius(sigma) && std::fabs(dy) <= Blur::getRadius(sigma);
				const double expected = inside ? 255.0 * std::exp(-(dx * dx + dy * dy) / (2.0 * sigma * sigma)) / (norm * norm) : 0.0;
				largest = std::max(largest, static_cast<std::int32_t>(std::abs(image.getPixel(x, y).r - std::lround(expected))));
			}
		}
		context.check(largest <= 1, "within one level everywhere");
	}};

	static inline test::Test edges{"Blur: no blur leaves the image alone, and every channel blurs alike", [](test::Context& context) {
		Image image(9, 9, PixelFormat::Gray8);
		image.setPixel(4, 4, Color4u8{200, 200, 200, 255});
		const Image original = image;
		Blur::gaussian(image, 0.0f);
		context.check(image == original, "sigma 0 changes nothing");
		context.checkEqual(Blur::getRadius(0.0f), 0);
		context.checkEqual(Blur::getRadius(2.0f), 6);
		context.checkEqual(Blur::getRadius(0.1f), 1);

		Image color(9, 9, PixelFormat::RGBA8);
		color.setPixel(4, 4, Color4u8{200, 100, 50, 255});
		Blur::gaussian(color, 1.0f);
		Image gray(9, 9, PixelFormat::Gray8);
		gray.setPixel(4, 4, Color4u8{100, 100, 100, 255});
		Blur::gaussian(gray, 1.0f);
		context.check(color.getPixel(5, 4).g == gray.getPixel(5, 4).r && color.getPixel(4, 6).g == gray.getPixel(4, 6).r, "channels blur independently");
	}};
};

}
