#pragma once

#include <litefont/Image.h>

namespace litefont {

/**
 * \class Blur Blur.h <litefont/Blur.h>
 * @addtogroup Image
 *
 * Blur - A Gaussian blur of an image in place, for soft shadows. It runs in
 * two passes, across then down, with the kernel cut at three standard
 * deviations (getRadius) and normalized, so a shape with that much room
 * around it keeps its total coverage. Pixels beyond the edges count as 0,
 * every channel is blurred alike, and results round to the nearest level.
 * A standard deviation of 0 or less leaves the image as it is.
 */
class Blur {
public:
	static void gaussian(Image& image, float sigma);
	static std::int32_t getRadius(float sigma);
};

}
