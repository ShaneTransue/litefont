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
	return this->space == SpriteSpace::Viewport ? OpenGLFunctions::NEAREST : OpenGLFunctions::LINEAR;
}

}
