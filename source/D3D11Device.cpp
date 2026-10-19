#include <litefont/D3D11Device.h>

#include "D3D11Shaders.h"

// Direct3D 11 itself, seen by this file alone.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>

#include <algorithm>
#include <cstring>

namespace litefont {

/**
 * D3D11Com - Releases a COM reference and forgets it.
 */
struct D3D11Com {
	template <typename T>
	static void release(T*& object) {
		if (object != nullptr) {
			object->Release();
			object = nullptr;
		}
	}
};

/**
 * D3D11Constants - The shader constants, laid out as sprite.hlsl's cbuffer:
 * the transform (column-major) and the options (x is 1 for coverage).
 */
struct D3D11Constants {
	float transform[16];
	float options[4];
};

/**
 * D3D11ShaderSlot - A shader stage's bound shader and its class instances,
 * as a Get call returns them, so a Set call can put them back.
 */
template <typename Shader>
struct D3D11ShaderSlot {
	Shader* shader = nullptr;
	ID3D11ClassInstance* instances[256] = {};
	UINT instanceCount = 256;

	void release();
};

template <typename Shader>
void D3D11ShaderSlot<Shader>::release() {
	D3D11Com::release(this->shader);
	for (UINT i = 0; i < this->instanceCount && i < 256; i++) {
		D3D11Com::release(this->instances[i]);
	}
}

/**
 * D3D11DrawState - The pipeline state D3D11Device changes to draw, read
 * before drawing and set back afterwards. Get calls add references, which
 * restore() releases.
 */
struct D3D11DrawState {
	UINT scissorCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	D3D11_RECT scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
	ID3D11RasterizerState* rasterizer = nullptr;
	ID3D11BlendState* blend = nullptr;
	FLOAT blendFactor[4] = {};
	UINT sampleMask = 0;
	ID3D11DepthStencilState* depthStencil = nullptr;
	UINT stencilReference = 0;
	ID3D11ShaderResourceView* resource = nullptr;
	ID3D11SamplerState* sampler = nullptr;
	D3D11ShaderSlot<ID3D11VertexShader> vertex;
	D3D11ShaderSlot<ID3D11PixelShader> pixel;
	D3D11ShaderSlot<ID3D11GeometryShader> geometry;
	D3D11ShaderSlot<ID3D11HullShader> hull;
	D3D11ShaderSlot<ID3D11DomainShader> domain;
	ID3D11Buffer* vertexConstants = nullptr;
	ID3D11Buffer* pixelConstants = nullptr;
	D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	ID3D11Buffer* indexBuffer = nullptr;
	DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
	UINT indexOffset = 0;
	ID3D11Buffer* vertexBuffer = nullptr;
	UINT vertexStride = 0;
	UINT vertexOffset = 0;
	ID3D11InputLayout* inputLayout = nullptr;

	void save(ID3D11DeviceContext* context);
	void restore(ID3D11DeviceContext* context);
};

void D3D11DrawState::save(ID3D11DeviceContext* context) {
	context->RSGetScissorRects(&this->scissorCount, this->scissors);
	context->RSGetState(&this->rasterizer);
	context->OMGetBlendState(&this->blend, this->blendFactor, &this->sampleMask);
	context->OMGetDepthStencilState(&this->depthStencil, &this->stencilReference);
	context->PSGetShaderResources(0, 1, &this->resource);
	context->PSGetSamplers(0, 1, &this->sampler);
	context->VSGetShader(&this->vertex.shader, this->vertex.instances, &this->vertex.instanceCount);
	context->PSGetShader(&this->pixel.shader, this->pixel.instances, &this->pixel.instanceCount);
	context->GSGetShader(&this->geometry.shader, this->geometry.instances, &this->geometry.instanceCount);
	context->HSGetShader(&this->hull.shader, this->hull.instances, &this->hull.instanceCount);
	context->DSGetShader(&this->domain.shader, this->domain.instances, &this->domain.instanceCount);
	context->VSGetConstantBuffers(0, 1, &this->vertexConstants);
	context->PSGetConstantBuffers(0, 1, &this->pixelConstants);
	context->IAGetPrimitiveTopology(&this->topology);
	context->IAGetIndexBuffer(&this->indexBuffer, &this->indexFormat, &this->indexOffset);
	context->IAGetVertexBuffers(0, 1, &this->vertexBuffer, &this->vertexStride, &this->vertexOffset);
	context->IAGetInputLayout(&this->inputLayout);
}

void D3D11DrawState::restore(ID3D11DeviceContext* context) {
	context->RSSetScissorRects(this->scissorCount, this->scissors);
	context->RSSetState(this->rasterizer);
	context->OMSetBlendState(this->blend, this->blendFactor, this->sampleMask);
	context->OMSetDepthStencilState(this->depthStencil, this->stencilReference);
	context->PSSetShaderResources(0, 1, &this->resource);
	context->PSSetSamplers(0, 1, &this->sampler);
	context->VSSetShader(this->vertex.shader, this->vertex.instances, this->vertex.instanceCount);
	context->PSSetShader(this->pixel.shader, this->pixel.instances, this->pixel.instanceCount);
	context->GSSetShader(this->geometry.shader, this->geometry.instances, this->geometry.instanceCount);
	context->HSSetShader(this->hull.shader, this->hull.instances, this->hull.instanceCount);
	context->DSSetShader(this->domain.shader, this->domain.instances, this->domain.instanceCount);
	context->VSSetConstantBuffers(0, 1, &this->vertexConstants);
	context->PSSetConstantBuffers(0, 1, &this->pixelConstants);
	context->IASetPrimitiveTopology(this->topology);
	context->IASetIndexBuffer(this->indexBuffer, this->indexFormat, this->indexOffset);
	context->IASetVertexBuffers(0, 1, &this->vertexBuffer, &this->vertexStride, &this->vertexOffset);
	context->IASetInputLayout(this->inputLayout);

	D3D11Com::release(this->rasterizer);
	D3D11Com::release(this->blend);
	D3D11Com::release(this->depthStencil);
	D3D11Com::release(this->resource);
	D3D11Com::release(this->sampler);
	this->vertex.release();
	this->pixel.release();
	this->geometry.release();
	this->hull.release();
	this->domain.release();
	D3D11Com::release(this->vertexConstants);
	D3D11Com::release(this->pixelConstants);
	D3D11Com::release(this->indexBuffer);
	D3D11Com::release(this->vertexBuffer);
	D3D11Com::release(this->inputLayout);
}

D3D11Device::D3D11Device(ID3D11Device* device, ID3D11DeviceContext* context) {
	this->attach(device, context);
}

D3D11Device::~D3D11Device() {
	this->releaseObjects();
	D3D11Com::release(this->context);
	D3D11Com::release(this->device);
}

void D3D11Device::attach(ID3D11Device* device, ID3D11DeviceContext* context) {
	// Objects of another device cannot be used with this one, so they go;
	// textures and geometry keep their handles. Texture contents are lost with
	// them, so a program that moves to another device after drawing calls
	// TextPainter::release() first.
	if (device == this->device && context == this->context) {
		return;
	}
	this->releaseObjects();
	if (device != nullptr) {
		device->AddRef();
	}
	if (context != nullptr) {
		context->AddRef();
	}
	D3D11Com::release(this->context);
	D3D11Com::release(this->device);
	this->device = device;
	this->context = context;
	this->error = D3D11Error::None;
}

TextureHandle D3D11Device::createTexture(const TextureDesc& desc) {
	// The Direct3D texture is made at the first upload.
	this->textures.push_back(D3D11Texture{desc, nullptr, nullptr});
	return TextureHandle{static_cast<std::uint32_t>(this->textures.size())};
}

void D3D11Device::updateTexture(TextureHandle texture, const Image& source, const Recti& region) {
	if (texture.value == 0 || texture.value > this->textures.size()) {
		return;
	}
	if (this->device == nullptr || this->context == nullptr) {
		this->error = D3D11Error::NotAttached;
		return;
	}
	D3D11Texture& target = this->textures[texture.value - 1];
	const Recti clipped = region.intersected(source.getBounds()).intersected(Recti{0, 0, target.desc.width, target.desc.height});
	if (clipped.isEmpty() || source.getFormat() != target.desc.format) {
		return;
	}
	// Coverage is one channel, read as alpha by the pixel shader; Direct3D
	// has no three-byte format, so RGB is widened to RGBA.
	const bool coverage = target.desc.format == PixelFormat::Gray8;
	if (target.texture == nullptr) {
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = target.desc.width;
		desc.Height = target.desc.height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = coverage ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		if (FAILED(this->device->CreateTexture2D(&desc, nullptr, &target.texture))
			|| FAILED(this->device->CreateShaderResourceView(target.texture, nullptr, &target.view))) {
			D3D11Com::release(target.view);
			D3D11Com::release(target.texture);
			this->error = D3D11Error::CreateFailed;
			return;
		}
	}
	D3D11_BOX box = {};
	box.left = static_cast<UINT>(clipped.x);
	box.top = static_cast<UINT>(clipped.y);
	box.front = 0;
	box.right = static_cast<UINT>(clipped.getRight());
	box.bottom = static_cast<UINT>(clipped.getBottom());
	box.back = 1;
	if (source.getFormat() == PixelFormat::RGB8) {
		std::vector<std::uint8_t> widened(static_cast<std::size_t>(clipped.width) * static_cast<std::size_t>(clipped.height) * 4);
		for (std::int32_t y = 0; y < clipped.height; y++) {
			const std::uint8_t* row = source.getRow(clipped.y + y) + static_cast<std::size_t>(clipped.x) * 3;
			std::uint8_t* out = widened.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(clipped.width) * 4;
			for (std::int32_t x = 0; x < clipped.width; x++) {
				out[x * 4 + 0] = row[x * 3 + 0];
				out[x * 4 + 1] = row[x * 3 + 1];
				out[x * 4 + 2] = row[x * 3 + 2];
				out[x * 4 + 3] = 255;
			}
		}
		this->context->UpdateSubresource(target.texture, 0, &box, widened.data(), static_cast<UINT>(clipped.width) * 4, 0);
		return;
	}
	const std::size_t bytesPerPixel = static_cast<std::size_t>(Image::getChannelCount(source.getFormat()));
	const std::uint8_t* first = source.getRow(clipped.y) + static_cast<std::size_t>(clipped.x) * bytesPerPixel;
	this->context->UpdateSubresource(target.texture, 0, &box, first, static_cast<UINT>(source.getRowSize()), 0);
}

GeometryHandle D3D11Device::createGeometry(const VertexLayout& layout) {
	D3D11Geometry geometry;
	geometry.layout = layout;
	this->geometries.push_back(std::move(geometry));
	return GeometryHandle{static_cast<std::uint32_t>(this->geometries.size())};
}

void D3D11Device::updateGeometry(GeometryHandle geometry, const GeometryView& view) {
	if (geometry.value == 0 || geometry.value > this->geometries.size()) {
		return;
	}
	D3D11Geometry& target = this->geometries[geometry.value - 1];
	target.vertexBytes.assign(view.vertexBytes.begin(), view.vertexBytes.end());
	target.layout = view.layout;
	target.indices.assign(view.indices.begin(), view.indices.end());
	target.uploaded = false;
}

void D3D11Device::beginFrame(Size2i target, Color4f clear) {
	// For programs that hand the whole frame to the device: clears the bound
	// render target and covers it with the viewport.
	if (this->context == nullptr) {
		this->error = D3D11Error::NotAttached;
		return;
	}
	ID3D11RenderTargetView* view = nullptr;
	this->context->OMGetRenderTargets(1, &view, nullptr);
	if (view != nullptr) {
		const FLOAT color[4] = {clear.r, clear.g, clear.b, clear.a};
		this->context->ClearRenderTargetView(view, color);
		D3D11Com::release(view);
	}
	D3D11_VIEWPORT viewport = {};
	viewport.Width = static_cast<FLOAT>(target.width);
	viewport.Height = static_cast<FLOAT>(target.height);
	viewport.MaxDepth = 1.0f;
	this->context->RSSetViewports(1, &viewport);
}

void D3D11Device::drawSprites(GeometryHandle geometry, std::span<const SpriteBatch> batches) {
	if (geometry.value == 0 || geometry.value > this->geometries.size() || batches.empty() || !this->prepare()) {
		return;
	}
	D3D11Geometry& source = this->geometries[geometry.value - 1];
	const Recti viewport = this->getViewport();
	if (source.vertexBytes.empty() || source.indices.empty() || viewport.width <= 0 || viewport.height <= 0 || !this->upload(source)) {
		return;
	}
	D3D11DrawState saved;
	if (this->restoreState) {
		saved.save(this->context);
	}

	ID3D11DeviceContext* context = this->context;
	const UINT stride = source.layout.getStride();
	const UINT offset = 0;
	context->IASetInputLayout(this->inputLayout);
	context->IASetVertexBuffers(0, 1, &source.vertexBuffer, &stride, &offset);
	context->IASetIndexBuffer(source.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetShader(this->vertexShader, nullptr, 0);
	context->VSSetConstantBuffers(0, 1, &this->constants);
	context->PSSetShader(this->pixelShader, nullptr, 0);
	context->PSSetConstantBuffers(0, 1, &this->constants);
	// Point sampling on the pixel grid, where texels map one to one; linear
	// under a matrix, where they do not.
	context->PSSetSamplers(0, 1, this->space == SpriteSpace::Viewport ? &this->pointSampler : &this->linearSampler);
	context->GSSetShader(nullptr, nullptr, 0);
	context->HSSetShader(nullptr, nullptr, 0);
	context->DSSetShader(nullptr, nullptr, 0);
	const FLOAT blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	context->OMSetBlendState(this->blendState, blendFactor, 0xFFFFFFFFu);
	context->OMSetDepthStencilState(this->depthStencilState, 0);
	context->RSSetState(this->rasterizerState);

	const Matrix4f transform = this->getSpriteTransform(viewport);
	int coverageSet = -1;
	for (const SpriteBatch& batch : batches) {
		const std::uint64_t end = std::uint64_t{batch.firstIndex} + batch.indexCount;
		if (batch.encoding != SpriteEncoding::Coverage || batch.texture.value == 0 || batch.texture.value > this->textures.size()
			|| end > source.indices.size()) {
			continue;
		}
		const D3D11Texture& texture = this->textures[batch.texture.value - 1];
		if (texture.view == nullptr) {
			continue;
		}
		const int coverage = texture.desc.format == PixelFormat::Gray8 ? 1 : 0;
		if (coverage != coverageSet) {
			this->setConstants(transform, coverage != 0);
			coverageSet = coverage;
		}
		context->PSSetShaderResources(0, 1, &texture.view);
		// The scissor counts in render target pixels from the top-left corner.
		D3D11_RECT rect = {};
		const Recti area = batch.clip.isEmpty() ? Recti{0, 0, viewport.width, viewport.height} : batch.clip;
		rect.left = viewport.x + area.x;
		rect.top = viewport.y + area.y;
		rect.right = viewport.x + area.getRight();
		rect.bottom = viewport.y + area.getBottom();
		context->RSSetScissorRects(1, &rect);
		context->DrawIndexed(batch.indexCount, batch.firstIndex, 0);
	}

	if (this->restoreState) {
		saved.restore(context);
	}
}

void D3D11Device::endFrame() {
}

Image D3D11Device::readPixels() const {
	// The bound render target's viewport, through a staging copy (resolved
	// first when it is multisampled).
	if (this->device == nullptr || this->context == nullptr) {
		return Image();
	}
	ID3D11RenderTargetView* view = nullptr;
	this->context->OMGetRenderTargets(1, &view, nullptr);
	if (view == nullptr) {
		return Image();
	}
	ID3D11Resource* resource = nullptr;
	view->GetResource(&resource);
	D3D11Com::release(view);
	ID3D11Texture2D* target = nullptr;
	if (resource == nullptr || FAILED(resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&target)))) {
		D3D11Com::release(resource);
		return Image();
	}
	D3D11Com::release(resource);
	D3D11_TEXTURE2D_DESC desc = {};
	target->GetDesc(&desc);
	const bool bgra = desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
		|| desc.Format == DXGI_FORMAT_B8G8R8X8_UNORM || desc.Format == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
	const bool rgba = desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	if (!bgra && !rgba) {
		D3D11Com::release(target);
		return Image();
	}

	ID3D11Texture2D* single = target;
	if (desc.SampleDesc.Count > 1) {
		D3D11_TEXTURE2D_DESC resolvedDesc = desc;
		resolvedDesc.SampleDesc.Count = 1;
		resolvedDesc.SampleDesc.Quality = 0;
		resolvedDesc.MipLevels = 1;
		resolvedDesc.ArraySize = 1;
		resolvedDesc.BindFlags = 0;
		resolvedDesc.MiscFlags = 0;
		single = nullptr;
		if (SUCCEEDED(this->device->CreateTexture2D(&resolvedDesc, nullptr, &single))) {
			this->context->ResolveSubresource(single, 0, target, 0, desc.Format);
		}
	}
	D3D11_TEXTURE2D_DESC stagingDesc = desc;
	stagingDesc.SampleDesc.Count = 1;
	stagingDesc.SampleDesc.Quality = 0;
	stagingDesc.MipLevels = 1;
	stagingDesc.ArraySize = 1;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = 0;
	ID3D11Texture2D* staging = nullptr;
	Image frame;
	const Recti viewport = this->getViewport().intersected(Recti{0, 0, static_cast<std::int32_t>(desc.Width), static_cast<std::int32_t>(desc.Height)});
	if (single != nullptr && !viewport.isEmpty() && SUCCEEDED(this->device->CreateTexture2D(&stagingDesc, nullptr, &staging))) {
		this->context->CopySubresourceRegion(staging, 0, 0, 0, 0, single, 0, nullptr);
		D3D11_MAPPED_SUBRESOURCE mapped = {};
		if (SUCCEEDED(this->context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
			frame = Image(viewport.width, viewport.height, PixelFormat::RGBA8);
			for (std::int32_t y = 0; y < viewport.height; y++) {
				const std::uint8_t* row = static_cast<const std::uint8_t*>(mapped.pData)
					+ static_cast<std::size_t>(viewport.y + y) * mapped.RowPitch + static_cast<std::size_t>(viewport.x) * 4;
				std::uint8_t* out = frame.getRow(y);
				std::memcpy(out, row, static_cast<std::size_t>(viewport.width) * 4);
				if (bgra) {
					for (std::int32_t x = 0; x < viewport.width; x++) {
						std::swap(out[x * 4], out[x * 4 + 2]);
					}
				}
			}
			this->context->Unmap(staging, 0);
		}
	}
	D3D11Com::release(staging);
	if (single != target) {
		D3D11Com::release(single);
	}
	D3D11Com::release(target);
	return frame;
}

Recti D3D11Device::getViewport() const {
	if (this->context == nullptr) {
		return Recti{};
	}
	UINT count = 1;
	D3D11_VIEWPORT viewport = {};
	this->context->RSGetViewports(&count, &viewport);
	if (count == 0) {
		return Recti{};
	}
	return Recti{static_cast<std::int32_t>(viewport.TopLeftX), static_cast<std::int32_t>(viewport.TopLeftY),
		static_cast<std::int32_t>(viewport.Width), static_cast<std::int32_t>(viewport.Height)};
}

void D3D11Device::release() {
	// The device and context stay attached; everything made from them goes,
	// and so do the textures and geometry.
	this->releaseObjects();
	this->textures.clear();
	this->geometries.clear();
}

void D3D11Device::setRestoreState(bool restore) {
	this->restoreState = restore;
}

bool D3D11Device::isRestoringState() const {
	return this->restoreState;
}

D3D11Error D3D11Device::getError() const {
	return this->error;
}

bool D3D11Device::prepare() {
	// Shaders, constants and fixed states, made once per attached device.
	if (this->device == nullptr || this->context == nullptr) {
		this->error = D3D11Error::NotAttached;
		return false;
	}
	if (this->vertexShader != nullptr) {
		return true;
	}
	D3D11_BUFFER_DESC constantsDesc = {};
	constantsDesc.ByteWidth = sizeof(D3D11Constants);
	constantsDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantsDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantsDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	D3D11_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.DepthClipEnable = FALSE;
	rasterizerDesc.ScissorEnable = TRUE;

	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = FALSE;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	depthDesc.StencilEnable = FALSE;

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	D3D11_SAMPLER_DESC pointDesc = samplerDesc;
	pointDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	D3D11_SAMPLER_DESC linearDesc = samplerDesc;
	linearDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

	const bool made = SUCCEEDED(this->device->CreateVertexShader(D3D11Shaders::vertex, sizeof(D3D11Shaders::vertex), nullptr, &this->vertexShader))
		&& SUCCEEDED(this->device->CreatePixelShader(D3D11Shaders::pixel, sizeof(D3D11Shaders::pixel), nullptr, &this->pixelShader))
		&& SUCCEEDED(this->device->CreateBuffer(&constantsDesc, nullptr, &this->constants))
		&& SUCCEEDED(this->device->CreateBlendState(&blendDesc, &this->blendState))
		&& SUCCEEDED(this->device->CreateRasterizerState(&rasterizerDesc, &this->rasterizerState))
		&& SUCCEEDED(this->device->CreateDepthStencilState(&depthDesc, &this->depthStencilState))
		&& SUCCEEDED(this->device->CreateSamplerState(&pointDesc, &this->pointSampler))
		&& SUCCEEDED(this->device->CreateSamplerState(&linearDesc, &this->linearSampler));
	if (!made) {
		this->releasePipeline();
		this->error = D3D11Error::CreateFailed;
	}
	return made;
}

bool D3D11Device::upload(D3D11Geometry& geometry) {
	// The input layout follows the geometry's vertex layout; the buffers grow
	// with headroom and are refilled only after an update.
	const VertexAttribute* position = geometry.layout.find(VertexSemantic::Position);
	const VertexAttribute* texCoord = geometry.layout.find(VertexSemantic::TexCoord);
	const VertexAttribute* color = geometry.layout.find(VertexSemantic::Color);
	if (position == nullptr || texCoord == nullptr || color == nullptr) {
		return false;
	}
	if (this->inputLayout == nullptr || !(this->inputVertexLayout == geometry.layout)) {
		D3D11Com::release(this->inputLayout);
		const D3D11_INPUT_ELEMENT_DESC elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, position->offset, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, texCoord->offset, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 0, color->format == AttributeFormat::UNorm8x4 ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
				color->offset, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};
		if (FAILED(this->device->CreateInputLayout(elements, 3, D3D11Shaders::vertex, sizeof(D3D11Shaders::vertex), &this->inputLayout))) {
			this->error = D3D11Error::CreateFailed;
			return false;
		}
		this->inputVertexLayout = geometry.layout;
	}
	if (geometry.uploaded) {
		return true;
	}
	auto fill = [this](ID3D11Buffer*& buffer, std::uint32_t& capacity, UINT bind, const void* data, std::size_t size) {
		if (buffer == nullptr || capacity < size) {
			D3D11Com::release(buffer);
			D3D11_BUFFER_DESC desc = {};
			desc.ByteWidth = static_cast<UINT>(std::max<std::size_t>(size + size / 2, 4096));
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = bind;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			if (FAILED(this->device->CreateBuffer(&desc, nullptr, &buffer))) {
				capacity = 0;
				return false;
			}
			capacity = desc.ByteWidth;
		}
		D3D11_MAPPED_SUBRESOURCE mapped = {};
		if (FAILED(this->context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
			return false;
		}
		std::memcpy(mapped.pData, data, size);
		this->context->Unmap(buffer, 0);
		return true;
	};
	const bool filled = fill(geometry.vertexBuffer, geometry.vertexCapacity, D3D11_BIND_VERTEX_BUFFER, geometry.vertexBytes.data(), geometry.vertexBytes.size())
		&& fill(geometry.indexBuffer, geometry.indexCapacity, D3D11_BIND_INDEX_BUFFER, geometry.indices.data(), geometry.indices.size() * sizeof(std::uint32_t));
	if (!filled) {
		this->error = D3D11Error::CreateFailed;
		return false;
	}
	geometry.uploaded = true;
	return true;
}

void D3D11Device::setConstants(const Matrix4f& transform, bool coverage) {
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(this->context->Map(this->constants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
		return;
	}
	D3D11Constants values = {};
	std::memcpy(values.transform, transform.getData(), sizeof(values.transform));
	values.options[0] = coverage ? 1.0f : 0.0f;
	std::memcpy(mapped.pData, &values, sizeof(values));
	this->context->Unmap(this->constants, 0);
}

void D3D11Device::releaseObjects() {
	// Every Direct3D object made from the device; the textures and geometry
	// keep their handles and contents in memory, to be made again.
	for (D3D11Texture& texture : this->textures) {
		D3D11Com::release(texture.view);
		D3D11Com::release(texture.texture);
	}
	for (D3D11Geometry& geometry : this->geometries) {
		D3D11Com::release(geometry.vertexBuffer);
		D3D11Com::release(geometry.indexBuffer);
		geometry.vertexCapacity = 0;
		geometry.indexCapacity = 0;
		geometry.uploaded = false;
	}
	this->releasePipeline();
}

void D3D11Device::releasePipeline() {
	D3D11Com::release(this->vertexShader);
	D3D11Com::release(this->pixelShader);
	D3D11Com::release(this->inputLayout);
	this->inputVertexLayout = VertexLayout{};
	D3D11Com::release(this->constants);
	D3D11Com::release(this->blendState);
	D3D11Com::release(this->rasterizerState);
	D3D11Com::release(this->depthStencilState);
	D3D11Com::release(this->pointSampler);
	D3D11Com::release(this->linearSampler);
}

}
