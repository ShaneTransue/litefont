#pragma once

#include <litefont/Affine2.h>
#include <litefont/Color4.h>
#include <litefont/Image.h>
#include <litefont/Matrix4.h>
#include <litefont/Rect.h>
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
 * @addtogroup Graphics
 *
 * SpriteSpace - Where a GraphicsDevice draws sprites: in pixels from the
 * top-left corner of the viewport (crisp, on whole pixels), through a matrix
 * the program gives (setMatrix), or through the graphics API's own transform
 * state, for the devices that have one (OpenGL11Device::useMatrixStack).
 */
enum class SpriteSpace : std::uint8_t {
	Viewport,
	Matrix,
	MatrixStack
};

/**
 * \class GraphicsDevice GraphicsDevice.h <litefont/GraphicsDevice.h>
 * @addtogroup Graphics
 *
 * GraphicsDevice - The concept of something that draws: a graphics API, or
 * the CPU. It owns textures and geometry buffers, draws sprite batches into
 * a frame, and reads the frame back as RGBA8. Each API implements it once
 * (OpenGL11Device, OpenGL33Device, D3D11Device, SoftwareDevice); code that
 * draws only ever sees this interface, and names a concrete device once,
 * where it is created.
 *
 * Sprites blend over the frame with straight alpha in the frame's encoded
 * (sRGB) values, a GPU's ordinary alpha blend state: the texel's coverage or
 * alpha times the vertex alpha mixes the vertex color over what is there.
 * Vertex colors blend across each triangle, which is how gradients draw.
 *
 * Sprites are drawn in one of the spaces of SpriteSpace. The default is
 * pixels from the top-left corner of the viewport (getViewport), y down.
 * After setMatrix(), sprites go through that matrix instead (column-major,
 * projection times model-view, the way a shader takes it), into the
 * viewport's clip space. In either space the placement (setPlacement) first
 * maps sprite coordinates into the space's units, which is how a renderer
 * sizes and places text in them; useViewport() goes back to pixels. With y
 * up (the default, as glOrtho(0, w, 0, h) and world coordinates have it), a
 * renderer turns text over so it reads top to bottom; setYUp(false) is for
 * matrices with y down. getPixelScale() says how many viewport pixels one
 * unit of the space covers at a point, so text can be rendered at the size
 * it shows.
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
	virtual Recti getViewport() const = 0;
	virtual void release();

	void useViewport();
	void setMatrix(const Matrix4f& matrix);
	SpriteSpace getSpace() const;
	const Matrix4f& getMatrix() const;
	void setYUp(bool yUp);
	bool isYUp() const;
	void setPlacement(const Affine2f& placement);
	const Affine2f& getPlacement() const;
	float getPixelScale(Vector2f point) const;

protected:
	virtual bool getStackMatrix(Matrix4f& matrix) const;

	Matrix4f getSpriteTransform(const Recti& viewport) const;

	Matrix4f matrix;
	Affine2f placement;
	SpriteSpace space = SpriteSpace::Viewport;
	bool yUp = true;
};

}
