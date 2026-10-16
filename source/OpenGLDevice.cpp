#include <litefont/OpenGLDevice.h>

// The only file that sees OpenGL: the system's own header, version 1.1.
#if defined(_WIN32)
#include <windows.h>
#endif
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <algorithm>
#include <cstring>

namespace litefont {

TextureHandle OpenGLDevice::createTexture(const TextureDesc& desc) {
	// The OpenGL texture is made at the first upload, when a context is current.
	this->textures.push_back(OpenGLTexture{desc, 0});
	return TextureHandle{static_cast<std::uint32_t>(this->textures.size())};
}

void OpenGLDevice::updateTexture(TextureHandle texture, const Image& source, const Recti& region) {
	if (texture.value == 0 || texture.value > this->textures.size()) {
		return;
	}
	OpenGLTexture& target = this->textures[texture.value - 1];
	const Recti clipped = region.intersected(source.getBounds()).intersected(Recti{0, 0, target.desc.width, target.desc.height});
	if (clipped.isEmpty() || source.getFormat() != target.desc.format) {
		return;
	}
	GLenum internalFormat = GL_ALPHA8;
	GLenum format = GL_ALPHA;
	if (target.desc.format == PixelFormat::RGB8) {
		internalFormat = GL_RGB8;
		format = GL_RGB;
	}
	else if (target.desc.format == PixelFormat::RGBA8) {
		internalFormat = GL_RGBA8;
		format = GL_RGBA;
	}

	if (this->restoreState) {
		glPushAttrib(GL_TEXTURE_BIT);
		glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
	}
	if (target.name == 0) {
		// Nearest sampling: quads land on whole pixels, so texels map one to one.
		GLuint name = 0;
		glGenTextures(1, &name);
		target.name = name;
		glBindTexture(GL_TEXTURE_2D, name);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat), target.desc.width, target.desc.height, 0, format, GL_UNSIGNED_BYTE, nullptr);
	}
	else {
		glBindTexture(GL_TEXTURE_2D, target.name);
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, source.getWidth());
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, clipped.x);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, clipped.y);
	glTexSubImage2D(GL_TEXTURE_2D, 0, clipped.x, clipped.y, clipped.width, clipped.height, format, GL_UNSIGNED_BYTE, source.getRow(0));
	if (this->restoreState) {
		glPopClientAttrib();
		glPopAttrib();
	}
	else {
		// Unpacking settings left behind would break the program's own uploads.
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	}
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
}

void OpenGLDevice::beginFrame(Size2i target, Color4f clear) {
	// For programs that hand the whole frame to the device; immediate drawing
	// never calls it and draws into whatever the program has drawn.
	glViewport(0, 0, target.width, target.height);
	glClearColor(clear.r, clear.g, clear.b, clear.a);
	glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLDevice::drawSprites(GeometryHandle geometry, std::span<const SpriteBatch> batches) {
	if (geometry.value == 0 || geometry.value > this->geometries.size() || batches.empty()) {
		return;
	}
	const OpenGLGeometry& source = this->geometries[geometry.value - 1];
	const VertexAttribute* position = source.layout.find(VertexSemantic::Position);
	const VertexAttribute* texCoord = source.layout.find(VertexSemantic::TexCoord);
	const VertexAttribute* color = source.layout.find(VertexSemantic::Color);
	const GLsizei stride = source.layout.getStride();
	if (position == nullptr || texCoord == nullptr || color == nullptr || source.vertexBytes.empty() || source.indices.empty()) {
		return;
	}
	GLint viewport[4] = {0, 0, 0, 0};
	glGetIntegerv(GL_VIEWPORT, viewport);

	if (this->restoreState) {
		glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_LIGHTING_BIT
			| GL_POLYGON_BIT | GL_SCISSOR_BIT | GL_TEXTURE_BIT | GL_TRANSFORM_BIT);
		glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
	}
	// Pixels from the viewport's top-left corner, y down, on identity matrices.
	glMatrixMode(GL_TEXTURE);
	if (this->restoreState) {
		glPushMatrix();
	}
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	if (this->restoreState) {
		glPushMatrix();
	}
	glLoadIdentity();
	glOrtho(0.0, static_cast<GLdouble>(viewport[2]), static_cast<GLdouble>(viewport[3]), 0.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	if (this->restoreState) {
		glPushMatrix();
	}
	glLoadIdentity();

	// Coverage times the vertex color, blended with straight alpha.
	for (const GLenum capability : {GL_DEPTH_TEST, GL_LIGHTING, GL_CULL_FACE, GL_ALPHA_TEST, GL_FOG, GL_STENCIL_TEST, GL_TEXTURE_GEN_S, GL_TEXTURE_GEN_T}) {
		glDisable(capability);
	}
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	const std::uint8_t* base = source.vertexBytes.data();
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_INDEX_ARRAY);
	glDisableClientState(GL_EDGE_FLAG_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(2, GL_FLOAT, stride, base + position->offset);
	glTexCoordPointer(2, GL_FLOAT, stride, base + texCoord->offset);
	glColorPointer(4, GL_UNSIGNED_BYTE, stride, base + color->offset);

	for (const SpriteBatch& batch : batches) {
		const std::uint64_t end = std::uint64_t{batch.firstIndex} + batch.indexCount;
		if (batch.encoding != SpriteEncoding::Coverage || batch.texture.value == 0 || batch.texture.value > this->textures.size()
			|| end > source.indices.size()) {
			continue;
		}
		const OpenGLTexture& texture = this->textures[batch.texture.value - 1];
		if (texture.name == 0) {
			continue;
		}
		glBindTexture(GL_TEXTURE_2D, texture.name);
		if (batch.clip.isEmpty()) {
			glDisable(GL_SCISSOR_TEST);
		}
		else {
			// The scissor counts from the window's bottom-left corner.
			glEnable(GL_SCISSOR_TEST);
			glScissor(viewport[0] + batch.clip.x, viewport[1] + viewport[3] - batch.clip.getBottom(), batch.clip.width, batch.clip.height);
		}
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(batch.indexCount), GL_UNSIGNED_INT, source.indices.data() + batch.firstIndex);
	}

	if (this->restoreState) {
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_TEXTURE);
		glPopMatrix();
		glPopClientAttrib();
		glPopAttrib();
	}
	else {
		// The arrays point into memory the device may reuse: never leave them on.
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}
}

void OpenGLDevice::endFrame() {
}

Image OpenGLDevice::readPixels() const {
	// The viewport's pixels, turned so the first row is the top one.
	GLint viewport[4] = {0, 0, 0, 0};
	glGetIntegerv(GL_VIEWPORT, viewport);
	if (viewport[2] <= 0 || viewport[3] <= 0) {
		return Image();
	}
	Image upsideDown(viewport[2], viewport[3], PixelFormat::RGBA8);
	glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(viewport[0], viewport[1], viewport[2], viewport[3], GL_RGBA, GL_UNSIGNED_BYTE, upsideDown.getPixels().data());
	glPopClientAttrib();
	Image frame(viewport[2], viewport[3], PixelFormat::RGBA8);
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

void OpenGLDevice::release() {
	for (OpenGLTexture& texture : this->textures) {
		if (texture.name != 0) {
			const GLuint name = texture.name;
			glDeleteTextures(1, &name);
			texture.name = 0;
		}
	}
	this->textures.clear();
	this->geometries.clear();
}

}
