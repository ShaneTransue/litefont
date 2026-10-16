#pragma once

#include <litefont/Color4.h>
#include <litefont/Image.h>
#include <litefont/Sprite.h>
#include <litefont/Blur.h>
#include <litefont/Size2.h>

#include <litefont/Geometry.h>

namespace litefont {

/**
 * \struct TextureDesc GraphicsDevice.h <litefont/GraphicsDevice.h>
 * @addtogroup Graphics
 *
 * TextureDesc - The size and pixel format of a texture to create.
 */
struct TextureDesc {
	std::uint16_t width = 0;
	std::uint16_t height = 0;
	PixelFormat format = PixelFormat::Gray8;
};

/**
 * \class GraphicsDevice GraphicsDevice.h <litefont/GraphicsDevice.h>
 * @addtogroup Graphics
 *
 * GraphicsDevice - The concept of something that draws. It owns textures and
 * geometry buffers, draws sprite batches into a frame, and reads the frame
 * back as RGBA8. GPUs and the software renderer implement it; code that draws
 * only ever sees this interface, and names a concrete device once, where it
 * is created.
 *
 * Sprites blend over the frame with straight alpha in the frame's encoded
 * (sRGB) values, a GPU's ordinary alpha blend state: the texel's coverage or
 * alpha times the vertex alpha mixes the vertex color over what is there.
 * It draws sprites because that is all text needs; the full library adds
 * draw calls as it needs them.
 */
class GraphicsDevice {
public:
	virtual ~GraphicsDevice() = default;

	virtual TextureHandle createTexture(const TextureDesc& desc) = 0;
	virtual void updateTexture(TextureHandle texture, const Image& source, const Recti& region) = 0;
	virtual GeometryHandle createGeometry(const VertexLayout& layout) = 0;
	virtual void updateGeometry(GeometryHandle geometry, const GeometryView& view) = 0;

	virtual void beginFrame(Size2i target, Color4f clear) = 0;
	virtual void drawSprites(GeometryHandle geometry, std::span<const SpriteBatch> batches) = 0;
	virtual void endFrame() = 0;
	virtual Image readPixels() const = 0;
};

}
