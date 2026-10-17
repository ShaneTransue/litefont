#pragma once

#include <litefont/Affine2.h>
#include <litefont/GraphicsDevice.h>
#include <litefont/Matrix4.h>
#include <litefont/OpenGLFunctions.h>

#include <cstdint>
#include <vector>

namespace litefont {

/**
 * @addtogroup Graphics
 *
 * OpenGLSpace - Where an OpenGLDevice draws sprites: in pixels from the
 * top-left corner of the viewport (crisp, on whole pixels), through a matrix
 * the program gives (setMatrix), or through the program's own OpenGL matrix
 * stack (OpenGL11Device::useMatrixStack, compatibility contexts only).
 */
enum class OpenGLSpace : std::uint8_t {
	Viewport,
	Matrix,
	MatrixStack
};

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
 * that matches its context and passes it on as an OpenGLDevice:
 * OpenGL11Device for compatibility contexts of any version (next to glBegin
 * and glEnd code), OpenGL33Device for core or compatibility contexts of
 * version 3.3 or newer. Either finds its OpenGL functions itself at the
 * first draw (see OpenGLFunctions), so no loader is needed.
 *
 * Sprites are drawn in one of three spaces (OpenGLSpace). The default is
 * pixels from the viewport's top-left corner, y down, on whole pixels. After
 * setMatrix(), sprites go through that matrix instead (column-major, the
 * product of projection and model-view, the way a shader would take it), and
 * OpenGL11Device can use the program's own matrix stack. In those two spaces
 * the device draws in the units of the matrix, with linear filtering; the
 * placement (setPlacement) maps sprite coordinates into those units, which
 * is how OpenGLRenderer sizes and places text in them. With y up (the
 * default, as glOrtho(0, w, 0, h) and world coordinates have it), text grows
 * downward from its position toward smaller y; setYUp(false) is for spaces
 * with y down. getPixelScale() says how many window pixels one unit covers
 * at a point, so text can be rendered at the size it shows.
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

	void useViewport();
	void setMatrix(const Matrix4f& matrix);
	OpenGLSpace getSpace() const;
	const Matrix4f& getMatrix() const;
	void setYUp(bool yUp);
	bool isYUp() const;
	void setPlacement(const Affine2f& placement);
	const Affine2f& getPlacement() const;
	float getPixelScale(Vector2f point) const;

	void setRestoreState(bool restore);
	bool isRestoringState() const;
	OpenGLError getError() const;
	virtual void release();

protected:
	virtual const OpenGLFunctions* getFunctions() const = 0;
	virtual bool getStackMatrix(Matrix4f& matrix) const;

	Recti getViewport() const;
	std::uint32_t getFilter() const;

	std::vector<OpenGLTexture> textures;
	std::vector<OpenGLGeometry> geometries;
	Matrix4f matrix;
	Affine2f placement;
	OpenGLSpace space = OpenGLSpace::Viewport;
	bool yUp = true;
	bool restoreState = true;
	mutable OpenGLError error = OpenGLError::None;
};

}
