#pragma once

#include <litefont/GraphicsDevice.h>
#include <litefont/OpenGLFunctions.h>

#include <cstdint>
#include <vector>

namespace litefont {

/**
 * @addtogroup Graphics
 *
 * OpenGLError - Why an OpenGLDevice last failed to draw: a function the
 * context lacks (or no context was current), or a shader that did not
 * compile or link.
 */
enum class OpenGLError : std::uint8_t {
	None,
	MissingFunction,
	ShaderCompile,
	ProgramLink
};

/**
 * \struct OpenGLTexture OpenGLDevice.h <litefont/OpenGLDevice.h>
 * @addtogroup Graphics
 *
 * OpenGLTexture - A texture an OpenGLDevice owns: its size and format, its
 * OpenGL texture name once made (0 before the first upload), and the filter
 * it was last set to (0 for none yet).
 */
struct OpenGLTexture {
	TextureDesc desc;
	std::uint32_t name = 0;
	std::uint32_t filter = 0;
};

/**
 * \struct OpenGLGeometry OpenGLDevice.h <litefont/OpenGLDevice.h>
 * @addtogroup Graphics
 *
 * OpenGLGeometry - Geometry an OpenGLDevice draws: the vertex bytes, their
 * layout and the indices as last updated, kept in memory (OpenGL 1.1 draws
 * from client-side arrays), and for devices that draw from buffers, the
 * buffer names and whether the buffers hold the latest update.
 */
struct OpenGLGeometry {
	std::vector<std::uint8_t> vertexBytes;
	VertexLayout layout;
	std::vector<std::uint32_t> indices;
	std::uint32_t vertexBuffer = 0;
	std::uint32_t indexBuffer = 0;
	bool uploaded = false;
};

/**
 * \class OpenGLDevice OpenGLDevice.h <litefont/OpenGLDevice.h>
 * @addtogroup Graphics
 *
 * OpenGLDevice - What the OpenGL devices share. A program creates the one
 * that matches its context: OpenGL11Device for compatibility contexts of any
 * version (next to glBegin and glEnd code), OpenGL33Device for core or
 * compatibility contexts of version 3.3 or newer. Either finds its OpenGL
 * functions itself at the first draw (see OpenGLFunctions), so no loader is
 * needed. Under a matrix (GraphicsDevice::setMatrix, or the 1.1 device's
 * matrix stack) textures are filtered linearly; on the pixel grid, nearest.
 *
 * Drawing sets what text needs and, by default, saves the program's state
 * first and restores it afterwards; setRestoreState(false) skips that. Each
 * device says what it touches. OpenGL objects are made on first use, so a
 * device can be constructed before a context exists, but every call that
 * draws or uploads needs the context current. release() deletes them (with
 * the context current); objects not released go away with their context.
 * getError() says why the last draw drew nothing.
 */
class OpenGLDevice : public GraphicsDevice {
public:
	OpenGLDevice() = default;
	OpenGLDevice(const OpenGLDevice&) = delete;
	OpenGLDevice& operator=(const OpenGLDevice&) = delete;

	TextureHandle createTexture(const TextureDesc& desc) override;
	GeometryHandle createGeometry(const VertexLayout& layout) override;
	void updateGeometry(GeometryHandle geometry, const GeometryView& view) override;
	void beginFrame(Size2i target, Color4f clear) override;
	void endFrame() override;
	Image readPixels() const override;
	Recti getViewport() const override;
	void release() override;

	void setRestoreState(bool restore);
	bool isRestoringState() const;
	OpenGLError getError() const;

protected:
	virtual const OpenGLFunctions* getFunctions() const = 0;

	std::uint32_t getFilter() const;

	std::vector<OpenGLTexture> textures;
	std::vector<OpenGLGeometry> geometries;
	bool restoreState = true;
	mutable OpenGLError error = OpenGLError::None;
};

}
