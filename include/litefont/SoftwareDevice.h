#pragma once

#include <litefont/GraphicsDevice.h>
#include <litefont/Vector2.h>

#include <array>
#include <vector>

namespace litefont {

/**
 * \struct SoftwareGeometry SoftwareDevice.h <litefont/SoftwareDevice.h>
 * @addtogroup Graphics
 *
 * SoftwareGeometry - A geometry buffer of the SoftwareDevice: a copy of the
 * uploaded vertex bytes and indices, and the layout that reads them.
 */
struct SoftwareGeometry {
	VertexLayout layout;
	std::vector<std::uint8_t> vertexBytes;
	std::vector<std::uint32_t> indices;
};

/**
 * \struct SoftwareVertex SoftwareDevice.h <litefont/SoftwareDevice.h>
 * @addtogroup Graphics
 *
 * SoftwareVertex - One vertex as the SoftwareDevice reads it through a
 * layout: position, texture coordinate and color.
 */
struct SoftwareVertex {
	Vector2f position;
	Vector2f texCoord;
	Color4f color;
};

/**
 * \class SoftwareDevice SoftwareDevice.h <litefont/SoftwareDevice.h>
 * @addtogroup Graphics
 *
 * SoftwareDevice - A GraphicsDevice in standard C++ that draws into an RGBA8
 * image it owns: the CPU reference, and the device for tests and
 * command-line rendering. It rasterizes triangles the way GPUs do (pixel
 * centers, the top-left fill rule, so the two triangles of a quad cover each
 * pixel once), interpolates attributes linearly, samples textures
 * bilinearly with texel centers at half coordinates and edges clamped, and
 * blends as GraphicsDevice describes. A sprite drawn at whole pixels with
 * matching texture coordinates copies its texels exactly. Floating-point
 * contraction is off in the build, so frames are byte-identical across
 * compilers. Distance-field batches arrive with MSDF (F7) and are skipped
 * until then.
 *
 * The viewport is the whole frame. Under a matrix (setMatrix) vertices go
 * through it and the perspective divide to frame pixels, and attributes are
 * interpolated across the screen; for 2D matrices that is exactly what a
 * GPU does, and under perspective a GPU's interpolation differs slightly.
 * release() frees the textures and geometry.
 */
class SoftwareDevice : public GraphicsDevice {
public:
	TextureHandle createTexture(const TextureDesc& desc) override;
	void updateTexture(TextureHandle texture, const Image& source, const Recti& region) override;
	GeometryHandle createGeometry(const VertexLayout& layout) override;
	void updateGeometry(GeometryHandle geometry, const GeometryView& view) override;

	void beginFrame(Size2i target, Color4f clear) override;
	void drawSprites(GeometryHandle geometry, std::span<const SpriteBatch> batches) override;
	void endFrame() override;
	Image readPixels() const override;
	Recti getViewport() const override;
	void release() override;

private:
	void drawTriangle(const std::array<SoftwareVertex, 3>& corners, const Image& texture, const Recti& clip);

	static bool readVertex(const SoftwareGeometry& geometry, std::uint32_t index, SoftwareVertex& vertex);
	static Color4f sample(const Image& texture, Vector2f texCoord);
	static void blend(std::uint8_t* pixel, const Color4f& source);

	std::vector<Image> textures;
	std::vector<SoftwareGeometry> geometries;
	Image frame;
};

}
