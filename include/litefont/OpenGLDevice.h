#pragma once

#include <litefont/GraphicsDevice.h>

#include <cstdint>
#include <vector>

namespace litefont {

/**
 * \struct OpenGLTexture OpenGLDevice.h <litefont/OpenGLDevice.h>
 * @addtogroup Graphics
 *
 * OpenGLTexture - A texture an OpenGLDevice owns: its size and format, and
 * its OpenGL texture name once made (0 before the first upload).
 */
struct OpenGLTexture {
	TextureDesc desc;
	std::uint32_t name = 0;
};

/**
 * \struct OpenGLGeometry OpenGLDevice.h <litefont/OpenGLDevice.h>
 * @addtogroup Graphics
 *
 * OpenGLGeometry - Geometry an OpenGLDevice draws: the vertex bytes, their
 * layout and the indices as last updated, kept in memory, since OpenGL 1.1
 * draws from client-side arrays.
 */
struct OpenGLGeometry {
	std::vector<std::uint8_t> vertexBytes;
	VertexLayout layout;
	std::vector<std::uint32_t> indices;
};

/**
 * \class OpenGLDevice OpenGLDevice.h <litefont/OpenGLDevice.h>
 * @addtogroup Graphics
 *
 * OpenGLDevice - A GraphicsDevice on OpenGL 1.1: the fixed-function subset
 * every desktop OpenGL library exports directly (opengl32 on Windows, libGL
 * on Linux, the OpenGL framework on macOS), so it needs no extension loader
 * and draws next to glBegin and glEnd code in any compatibility context.
 * Coverage atlases are alpha textures modulated by the vertex color and
 * blended with straight alpha, the same math as SoftwareDevice; vertices
 * come from client-side arrays.
 *
 * Sprites are drawn in pixels from the top-left corner of the current
 * viewport. Drawing sets what text needs: an orthographic projection over
 * the viewport, identity modelview and texture matrices, blending and 2D
 * texturing on, depth test, lighting, face culling, alpha test, fog,
 * stencil test and texture coordinate generation off, polygons filled, the
 * atlas texture bound with the modulate environment, the vertex, texture
 * coordinate and color arrays, and the scissor rectangle for clips. By
 * default it saves the program's state first and restores it afterwards
 * (glPushAttrib, glPushClientAttrib and the three matrix stacks); with
 * setRestoreState(false) it skips that work and leaves the state as drawing
 * set it, apart from turning its own client arrays off again. It expects
 * texture unit 0 to be the active one, as it is unless a program changes it.
 *
 * OpenGL objects are made on first use, so the device can be constructed
 * before a context exists, but every call that draws or uploads needs the
 * context current. release() deletes the textures (with the context
 * current); textures not released go away with their context.
 */
class OpenGLDevice : public GraphicsDevice {
public:
	OpenGLDevice() = default;
	OpenGLDevice(const OpenGLDevice&) = delete;
	OpenGLDevice& operator=(const OpenGLDevice&) = delete;

	TextureHandle createTexture(const TextureDesc& desc) override;
	void updateTexture(TextureHandle texture, const Image& source, const Recti& region) override;
	GeometryHandle createGeometry(const VertexLayout& layout) override;
	void updateGeometry(GeometryHandle geometry, const GeometryView& view) override;

	void beginFrame(Size2i target, Color4f clear) override;
	void drawSprites(GeometryHandle geometry, std::span<const SpriteBatch> batches) override;
	void endFrame() override;
	Image readPixels() const override;

	void setRestoreState(bool restore);
	bool isRestoringState() const;
	void release();

private:
	std::vector<OpenGLTexture> textures;
	std::vector<OpenGLGeometry> geometries;
	bool restoreState = true;
};

}
