#pragma once

#include <litefont/Vector2.h>

#include <litefont/Image.h>

namespace litefont {

/**
 * \struct Sprite Sprite.h <litefont/Sprite.h>
 * @addtogroup Image
 *
 * Sprite - An image that hangs from a point: origin is the offset from the pen
 * or anchor to the image's top-left pixel. A rendered glyph is a Sprite.
 */
struct Sprite {
	Image image;
	Vector2f origin;
};

}
