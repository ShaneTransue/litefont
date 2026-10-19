#pragma once

#include <litefont/GraphicsDevice.h>

#include <cstdint>
#include <vector>

// Direct3D 11 interfaces, declared only: this header never includes a Windows
// header, so any program can include it.
struct ID3D11BlendState;
struct ID3D11Buffer;
struct ID3D11DepthStencilState;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11InputLayout;
struct ID3D11PixelShader;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;
struct ID3D11VertexShader;

namespace litefont {

/**
 * @addtogroup Graphics
 *
 * D3D11Error - Why a D3D11Device last failed to draw: no device attached, or
 * a Direct3D object it could not create.
 */
enum class D3D11Error : std::uint8_t {
	None,
	NotAttached,
	CreateFailed
};

/**
 * \struct D3D11Texture D3D11Device.h <litefont/D3D11Device.h>
 * @addtogroup Graphics
 *
 * D3D11Texture - A texture a D3D11Device owns: its size and format, and once
 * made, the Direct3D texture and the view shaders read it through.
 */
struct D3D11Texture {
	TextureDesc desc;
	ID3D11Texture2D* texture = nullptr;
	ID3D11ShaderResourceView* view = nullptr;
};

/**
 * \struct D3D11Geometry D3D11Device.h <litefont/D3D11Device.h>
 * @addtogroup Graphics
 *
 * D3D11Geometry - Geometry a D3D11Device draws: the vertex bytes, their
 * layout and the indices as last updated, and the dynamic vertex and index
 * buffers that hold them, with their capacities in bytes and whether they
 * hold the latest update.
 */
struct D3D11Geometry {
	std::vector<std::uint8_t> vertexBytes;
	VertexLayout layout;
	std::vector<std::uint32_t> indices;
	ID3D11Buffer* vertexBuffer = nullptr;
	ID3D11Buffer* indexBuffer = nullptr;
	std::uint32_t vertexCapacity = 0;
	std::uint32_t indexCapacity = 0;
	bool uploaded = false;
};

/**
 * \class D3D11Device D3D11Device.h <litefont/D3D11Device.h>
 * @addtogroup Graphics
 *
 * D3D11Device - A GraphicsDevice on Direct3D 11 (feature level 10.0 or
 * newer), drawing into whatever render target the program has bound, in its
 * first viewport. The program makes the Direct3D device and context and
 * hands them over, at construction or later with attach(); the device keeps
 * a reference to each until release() or its destruction. litefont calls
 * Direct3D only through those objects, so it links no Direct3D library.
 *
 * It draws with one small shader pair, compiled ahead of time and built into
 * the library, from a dynamic vertex and index buffer per geometry. Coverage
 * atlases are one-channel (R8) textures read as alpha, so it blends exactly
 * as the OpenGL devices and SoftwareDevice do: point sampling on the pixel
 * grid, linear under a matrix. Depth clipping is off, so a matrix may give
 * any depth.
 *
 * Drawing sets what text needs: its input layout, buffers and topology, its
 * vertex and pixel shaders with their constants, sampler and texture, no
 * geometry, hull or domain shader, its blend state (straight alpha), depth
 * and stencil off, its rasterizer state (no culling, scissor on) and the
 * scissor rectangle for clips. By default it saves each of those first and
 * restores it afterwards; with setRestoreState(false) it leaves them as
 * drawing set them. Direct3D objects are made at the first draw.
 * readPixels() copies the bound render target's viewport through a staging
 * texture, for tests. getError() says why the last draw drew nothing.
 */
class D3D11Device : public GraphicsDevice {
public:
	D3D11Device() = default;
	D3D11Device(ID3D11Device* device, ID3D11DeviceContext* context);
	~D3D11Device() override;
	D3D11Device(const D3D11Device&) = delete;
	D3D11Device& operator=(const D3D11Device&) = delete;

	void attach(ID3D11Device* device, ID3D11DeviceContext* context);

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

	void setRestoreState(bool restore);
	bool isRestoringState() const;
	D3D11Error getError() const;

private:
	bool prepare();
	bool upload(D3D11Geometry& geometry);
	void setConstants(const Matrix4f& transform, bool coverage);
	void releaseObjects();
	void releasePipeline();

	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* context = nullptr;
	ID3D11VertexShader* vertexShader = nullptr;
	ID3D11PixelShader* pixelShader = nullptr;
	ID3D11InputLayout* inputLayout = nullptr;
	VertexLayout inputVertexLayout;
	ID3D11Buffer* constants = nullptr;
	ID3D11BlendState* blendState = nullptr;
	ID3D11RasterizerState* rasterizerState = nullptr;
	ID3D11DepthStencilState* depthStencilState = nullptr;
	ID3D11SamplerState* pointSampler = nullptr;
	ID3D11SamplerState* linearSampler = nullptr;
	std::vector<D3D11Texture> textures;
	std::vector<D3D11Geometry> geometries;
	bool restoreState = true;
	D3D11Error error = D3D11Error::None;
};

}
