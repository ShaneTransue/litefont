#include <litefont/Blur.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace litefont {

void Blur::gaussian(Image& image, float sigma) {
	const std::int32_t radius = Blur::getRadius(sigma);
	if (radius == 0 || image.isEmpty()) {
		return;
	}

	// Weights for offsets 0 to radius, normalized over the whole kernel.
	std::vector<double> weights(static_cast<std::size_t>(radius) + 1);
	double total = 0.0;
	for (std::int32_t k = 0; k <= radius; k++) {
		weights[static_cast<std::size_t>(k)] = std::exp(-static_cast<double>(k) * k / (2.0 * static_cast<double>(sigma) * sigma));
		total += k == 0 ? weights[0] : 2.0 * weights[static_cast<std::size_t>(k)];
	}
	for (double& weight : weights) {
		weight /= total;
	}

	const std::int32_t width = image.getWidth();
	const std::int32_t height = image.getHeight();
	const std::int32_t channels = image.getChannelCount();
	const std::size_t rowValues = static_cast<std::size_t>(width) * channels;
	std::vector<double> across(rowValues * static_cast<std::size_t>(height));
	for (std::int32_t y = 0; y < height; y++) {
		const std::uint8_t* row = image.getRow(y);
		double* target = across.data() + rowValues * static_cast<std::size_t>(y);
		for (std::int32_t x = 0; x < width; x++) {
			for (std::int32_t c = 0; c < channels; c++) {
				double sum = 0.0;
				for (std::int32_t k = -radius; k <= radius; k++) {
					const std::int32_t source = x + k;
					if (source >= 0 && source < width) {
						sum += weights[static_cast<std::size_t>(std::abs(k))] * row[static_cast<std::size_t>(source) * channels + c];
					}
				}
				target[static_cast<std::size_t>(x) * channels + c] = sum;
			}
		}
	}
	for (std::int32_t y = 0; y < height; y++) {
		std::uint8_t* row = image.getRow(y);
		for (std::size_t value = 0; value < rowValues; value++) {
			double sum = 0.0;
			for (std::int32_t k = -radius; k <= radius; k++) {
				const std::int32_t source = y + k;
				if (source >= 0 && source < height) {
					sum += weights[static_cast<std::size_t>(std::abs(k))] * across[rowValues * static_cast<std::size_t>(source) + value];
				}
			}
			row[value] = static_cast<std::uint8_t>(std::clamp(std::lround(sum), 0L, 255L));
		}
	}
}

std::int32_t Blur::getRadius(float sigma) {
	if (!(sigma > 0.0f)) {
		return 0;
	}
	return static_cast<std::int32_t>(std::ceil(3.0f * std::min(sigma, 256.0f)));
}

}
