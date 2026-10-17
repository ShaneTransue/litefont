#include <litefont/OpenGLDevice.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace litefont {

TextureHandle OpenGLDevice::createTexture(const TextureDesc& desc) {
	// The OpenGL texture is made at the first upload, when a context is current.
	this->textures.push_back(OpenGLTexture{desc, 0, 0});
	return TextureHandle{static_cast<std::uint32_t>(this->textures.size())};
}

GeometryHandle OpenGLDevice::createGeometry(const VertexLayout& layout) {
	OpenGLGeometry geometry;
	geometry.layout = layout;
	this->geometries.push_back(std::move(geometry));
	return GeometryHandle{static_cast<std::uint32_t>(this->geometries.size())};
}

void OpenGLDevice::updateGeometry(GeometryHandle geometry, const GeometryView& view) {
	if (geometry.value == 0 || geometry.value > this->geometries.size()) {
		return;
	}
	OpenGLGeometry& target = this->geometries[geometry.value - 1];
	target.vertexBytes.assign(view.vertexBytes.begin(), view.vertexBytes.end());
	target.layout = view.layout;
	target.indices.assign(view.indices.begin(), view.indices.end());
	target.uploaded = false;
}

void OpenGLDevice::beginFrame(Size2i target, Color4f clear) {
	// For programs that hand the whole frame to the device; immediate drawing
	// never calls it and draws into whatever the program has drawn.
	const OpenGLFunctions* gl = this->getFunctions();
	if (gl == nullptr) {
		return;
	}
	gl->glViewport(0, 0, target.width, target.height);
	gl->glClearColor(clear.r, clear.g, clear.b, clear.a);
	gl->glClear(OpenGLFunctions::COLOR_BUFFER_BIT);
}

void OpenGLDevice::endFrame() {
}

Image OpenGLDevice::readPixels() const {
	// The viewport's pixels, turned so the first row is the top one.
	const OpenGLFunctions* gl = this->getFunctions();
	if (gl == nullptr) {
		return Image();
	}
	const Recti viewport = this->getViewport();
	if (viewport.width <= 0 || viewport.height <= 0) {
		return Image();
	}
	Image upsideDown(viewport.width, viewport.height, PixelFormat::RGBA8);
	std::int32_t alignment = 4;
	gl->glGetIntegerv(OpenGLFunctions::PACK_ALIGNMENT, &alignment);
	gl->glPixelStorei(OpenGLFunctions::PACK_ALIGNMENT, 1);
	gl->glReadPixels(viewport.x, viewport.y, viewport.width, viewport.height, OpenGLFunctions::RGBA, OpenGLFunctions::UNSIGNED_BYTE, upsideDown.getPixels().data());
	gl->glPixelStorei(OpenGLFunctions::PACK_ALIGNMENT, alignment);
	Image frame(viewport.width, viewport.height, PixelFormat::RGBA8);
	for (std::int32_t y = 0; y < frame.getHeight(); y++) {
		std::memcpy(frame.getRow(y), upsideDown.getRow(frame.getHeight() - 1 - y), frame.getRowSize());
	}
	return frame;
}

void OpenGLDevice::useViewport() {
	this->space = OpenGLSpace::Viewport;
}

void OpenGLDevice::setMatrix(const Matrix4f& matrix) {
	this->matrix = matrix;
	this->space = OpenGLSpace::Matrix;
}

OpenGLSpace OpenGLDevice::getSpace() const {
	return this->space;
}

const Matrix4f& OpenGLDevice::getMatrix() const {
	return this->matrix;
}

void OpenGLDevice::setYUp(bool yUp) {
	this->yUp = yUp;
}

bool OpenGLDevice::isYUp() const {
	return this->yUp;
}

void OpenGLDevice::setPlacement(const Affine2f& placement) {
	this->placement = placement;
}

const Affine2f& OpenGLDevice::getPlacement() const {
	return this->placement;
}

float OpenGLDevice::getPixelScale(Vector2f point) const {
	// The longer of the two axes a unit square at the point becomes in window
	// pixels, from the derivative of the projection there (perspective too).
	if (this->space == OpenGLSpace::Viewport || this->getFunctions() == nullptr) {
		return 1.0f;
	}
	Matrix4f transform = this->matrix;
	if (this->space == OpenGLSpace::MatrixStack && !this->getStackMatrix(transform)) {
		return 1.0f;
	}
	const Recti viewport = this->getViewport();
	const std::array<float, 16>& m = transform.values;
	const double x = point.x;
	const double y = point.y;
	const double clipX = m[0] * x + m[4] * y + m[12];
	const double clipY = m[1] * x + m[5] * y + m[13];
	const double clipW = m[3] * x + m[7] * y + m[15];
	if (!(clipW > 1e-9)) {
		return 1.0f;
	}
	double longest = 0.0;
	for (std::size_t column = 0; column < 2; column++) {
		const double dx = m[column * 4 + 0];
		const double dy = m[column * 4 + 1];
		const double dw = m[column * 4 + 3];
		const double windowX = (dx * clipW - clipX * dw) / (clipW * clipW) * 0.5 * viewport.width;
		const double windowY = (dy * clipW - clipY * dw) / (clipW * clipW) * 0.5 * viewport.height;
		longest = std::max(longest, std::sqrt(windowX * windowX + windowY * windowY));
	}
	return std::isfinite(longest) && longest > 0.0 ? static_cast<float>(longest) : 1.0f;
}

void OpenGLDevice::setRestoreState(bool restore) {
	this->restoreState = restore;
}

bool OpenGLDevice::isRestoringState() const {
	return this->restoreState;
}

OpenGLError OpenGLDevice::getError() const {
	return this->error;
}

void OpenGLDevice::release() {
	const OpenGLFunctions* gl = this->getFunctions();
	for (OpenGLTexture& texture : this->textures) {
		if (texture.name != 0 && gl != nullptr) {
			gl->glDeleteTextures(1, &texture.name);
		}
		texture.name = 0;
	}
	this->textures.clear();
	this->geometries.clear();
}

bool OpenGLDevice::getStackMatrix(Matrix4f&) const {
	return false;
}

Recti OpenGLDevice::getViewport() const {
	std::int32_t viewport[4] = {0, 0, 0, 0};
	const OpenGLFunctions* gl = this->getFunctions();
	if (gl != nullptr) {
		gl->glGetIntegerv(OpenGLFunctions::VIEWPORT, viewport);
	}
	return Recti{viewport[0], viewport[1], viewport[2], viewport[3]};
}

std::uint32_t OpenGLDevice::getFilter() const {
	// Nearest on the pixel grid, where texels map one to one; linear under a
	// matrix, where they do not.
	return this->space == OpenGLSpace::Viewport ? OpenGLFunctions::NEAREST : OpenGLFunctions::LINEAR;
}

}
