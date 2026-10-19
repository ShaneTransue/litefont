#include <litefont/OpenGL11Device.h>

namespace litefont {

void OpenGL11Device::updateTexture(TextureHandle texture, const Image& source, const Recti& region) {
	using GL = OpenGL11Functions;
	if (texture.value == 0 || texture.value > this->textures.size() || this->getFunctions() == nullptr) {
		return;
	}
	OpenGLTexture& target = this->textures[texture.value - 1];
	const Recti clipped = region.intersected(source.getBounds()).intersected(Recti{0, 0, target.desc.width, target.desc.height});
	if (clipped.isEmpty() || source.getFormat() != target.desc.format) {
		return;
	}
	std::uint32_t internalFormat = GL::ALPHA8;
	std::uint32_t format = GL::ALPHA;
	if (target.desc.format == PixelFormat::RGB8) {
		internalFormat = GL::RGB8;
		format = GL::RGB;
	}
	else if (target.desc.format == PixelFormat::RGBA8) {
		internalFormat = GL::RGBA8;
		format = GL::RGBA;
	}

	const OpenGL11Functions& gl = this->gl;
	if (this->restoreState) {
		gl.glPushAttrib(GL::TEXTURE_BIT);
		gl.glPushClientAttrib(GL::CLIENT_PIXEL_STORE_BIT);
	}
	if (target.name == 0) {
		gl.glGenTextures(1, &target.name);
		gl.glBindTexture(GL::TEXTURE_2D, target.name);
		target.filter = this->getFilter();
		gl.glTexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MIN_FILTER, static_cast<std::int32_t>(target.filter));
		gl.glTexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MAG_FILTER, static_cast<std::int32_t>(target.filter));
		gl.glTexParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_S, static_cast<std::int32_t>(GL::CLAMP));
		gl.glTexParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_T, static_cast<std::int32_t>(GL::CLAMP));
		gl.glTexImage2D(GL::TEXTURE_2D, 0, static_cast<std::int32_t>(internalFormat), target.desc.width, target.desc.height, 0, format, GL::UNSIGNED_BYTE, nullptr);
	}
	else {
		gl.glBindTexture(GL::TEXTURE_2D, target.name);
	}
	gl.glPixelStorei(GL::UNPACK_ALIGNMENT, 1);
	gl.glPixelStorei(GL::UNPACK_ROW_LENGTH, source.getWidth());
	gl.glPixelStorei(GL::UNPACK_SKIP_PIXELS, clipped.x);
	gl.glPixelStorei(GL::UNPACK_SKIP_ROWS, clipped.y);
	gl.glTexSubImage2D(GL::TEXTURE_2D, 0, clipped.x, clipped.y, clipped.width, clipped.height, format, GL::UNSIGNED_BYTE, source.getRow(0));
	if (this->restoreState) {
		gl.glPopClientAttrib();
		gl.glPopAttrib();
	}
	else {
		// Unpacking settings left behind would break the program's own uploads.
		gl.glPixelStorei(GL::UNPACK_ALIGNMENT, 4);
		gl.glPixelStorei(GL::UNPACK_ROW_LENGTH, 0);
		gl.glPixelStorei(GL::UNPACK_SKIP_PIXELS, 0);
		gl.glPixelStorei(GL::UNPACK_SKIP_ROWS, 0);
	}
}

void OpenGL11Device::drawSprites(GeometryHandle geometry, std::span<const SpriteBatch> batches) {
	using GL = OpenGL11Functions;
	if (geometry.value == 0 || geometry.value > this->geometries.size() || batches.empty() || this->getFunctions() == nullptr) {
		return;
	}
	const OpenGLGeometry& source = this->geometries[geometry.value - 1];
	const VertexAttribute* position = source.layout.find(VertexSemantic::Position);
	const VertexAttribute* texCoord = source.layout.find(VertexSemantic::TexCoord);
	const VertexAttribute* color = source.layout.find(VertexSemantic::Color);
	const std::int32_t stride = source.layout.getStride();
	if (position == nullptr || texCoord == nullptr || color == nullptr || source.vertexBytes.empty() || source.indices.empty()) {
		return;
	}
	const OpenGL11Functions& gl = this->gl;
	const Recti viewport = this->getViewport();
	const bool stack = this->space == SpriteSpace::MatrixStack;
	const Matrix4f placement = Matrix4f::fromAffine(this->placement);

	if (this->restoreState) {
		gl.glPushAttrib(GL::ENABLE_BIT | GL::COLOR_BUFFER_BIT | GL::CURRENT_BIT | GL::DEPTH_BUFFER_BIT | GL::LIGHTING_BIT
			| GL::POLYGON_BIT | GL::SCISSOR_BIT | GL::TEXTURE_BIT | GL::TRANSFORM_BIT);
		gl.glPushClientAttrib(GL::CLIENT_VERTEX_ARRAY_BIT);
	}
	gl.glMatrixMode(GL::TEXTURE);
	if (this->restoreState) {
		gl.glPushMatrix();
	}
	gl.glLoadIdentity();
	// The projection for the space: pixels from the viewport's top-left corner
	// with y down, the program's matrix, or left as the program set it.
	gl.glMatrixMode(GL::PROJECTION);
	if (!stack) {
		if (this->restoreState) {
			gl.glPushMatrix();
		}
		if (this->space == SpriteSpace::Matrix) {
			gl.glLoadMatrixf(this->matrix.getData());
		}
		else {
			gl.glLoadIdentity();
			gl.glOrtho(0.0, static_cast<double>(viewport.width), static_cast<double>(viewport.height), 0.0, -1.0, 1.0);
		}
	}
	// The placement on top of the program's model-view, or on its own.
	gl.glMatrixMode(GL::MODELVIEW);
	if (this->restoreState || stack) {
		gl.glPushMatrix();
	}
	if (stack) {
		gl.glMultMatrixf(placement.getData());
	}
	else {
		gl.glLoadMatrixf(placement.getData());
	}

	// Coverage times the vertex color, blended with straight alpha.
	for (const std::uint32_t capability : {GL::DEPTH_TEST, GL::LIGHTING, GL::CULL_FACE, GL::ALPHA_TEST, GL::FOG, GL::STENCIL_TEST, GL::TEXTURE_GEN_S, GL::TEXTURE_GEN_T}) {
		gl.glDisable(capability);
	}
	gl.glPolygonMode(GL::FRONT_AND_BACK, GL::FILL);
	gl.glShadeModel(GL::SMOOTH);
	gl.glEnable(GL::BLEND);
	gl.glBlendFunc(GL::SRC_ALPHA, GL::ONE_MINUS_SRC_ALPHA);
	gl.glEnable(GL::TEXTURE_2D);
	gl.glTexEnvi(GL::TEXTURE_ENV, GL::TEXTURE_ENV_MODE, static_cast<std::int32_t>(GL::MODULATE));

	const std::uint8_t* base = source.vertexBytes.data();
	gl.glDisableClientState(GL::NORMAL_ARRAY);
	gl.glDisableClientState(GL::INDEX_ARRAY);
	gl.glDisableClientState(GL::EDGE_FLAG_ARRAY);
	gl.glEnableClientState(GL::VERTEX_ARRAY);
	gl.glEnableClientState(GL::TEXTURE_COORD_ARRAY);
	gl.glEnableClientState(GL::COLOR_ARRAY);
	gl.glVertexPointer(2, GL::FLOAT, stride, base + position->offset);
	gl.glTexCoordPointer(2, GL::FLOAT, stride, base + texCoord->offset);
	gl.glColorPointer(4, GL::UNSIGNED_BYTE, stride, base + color->offset);

	const std::uint32_t filter = this->getFilter();
	for (const SpriteBatch& batch : batches) {
		const std::uint64_t end = std::uint64_t{batch.firstIndex} + batch.indexCount;
		if (batch.encoding != SpriteEncoding::Coverage || batch.texture.value == 0 || batch.texture.value > this->textures.size()
			|| end > source.indices.size()) {
			continue;
		}
		OpenGLTexture& texture = this->textures[batch.texture.value - 1];
		if (texture.name == 0) {
			continue;
		}
		gl.glBindTexture(GL::TEXTURE_2D, texture.name);
		if (texture.filter != filter) {
			gl.glTexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MIN_FILTER, static_cast<std::int32_t>(filter));
			gl.glTexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MAG_FILTER, static_cast<std::int32_t>(filter));
			texture.filter = filter;
		}
		if (batch.clip.isEmpty()) {
			gl.glDisable(GL::SCISSOR_TEST);
		}
		else {
			// The scissor counts from the window's bottom-left corner.
			gl.glEnable(GL::SCISSOR_TEST);
			gl.glScissor(viewport.x + batch.clip.x, viewport.y + viewport.height - batch.clip.getBottom(), batch.clip.width, batch.clip.height);
		}
		gl.glDrawElements(GL::TRIANGLES, static_cast<std::int32_t>(batch.indexCount), GL::UNSIGNED_INT, source.indices.data() + batch.firstIndex);
	}

	gl.glMatrixMode(GL::MODELVIEW);
	if (this->restoreState || stack) {
		gl.glPopMatrix();
	}
	if (this->restoreState) {
		if (!stack) {
			gl.glMatrixMode(GL::PROJECTION);
			gl.glPopMatrix();
		}
		gl.glMatrixMode(GL::TEXTURE);
		gl.glPopMatrix();
		gl.glPopClientAttrib();
		gl.glPopAttrib();
	}
	else {
		// The arrays point into memory the device may reuse: never leave them on.
		gl.glDisableClientState(GL::VERTEX_ARRAY);
		gl.glDisableClientState(GL::TEXTURE_COORD_ARRAY);
		gl.glDisableClientState(GL::COLOR_ARRAY);
	}
}

void OpenGL11Device::release() {
	OpenGLDevice::release();
	// Functions are found again for whatever context the next draw meets.
	this->gl = OpenGL11Functions();
}

void OpenGL11Device::useMatrixStack() {
	this->space = SpriteSpace::MatrixStack;
}

const OpenGLFunctions* OpenGL11Device::getFunctions() const {
	if (!this->gl.isLoaded() && !this->gl.load()) {
		this->error = OpenGLError::MissingFunction;
		return nullptr;
	}
	return &this->gl;
}

bool OpenGL11Device::getStackMatrix(Matrix4f& matrix) const {
	// What the program's stacks do to a vertex: projection times model-view.
	if (this->getFunctions() == nullptr) {
		return false;
	}
	Matrix4f projection;
	Matrix4f modelView;
	this->gl.glGetFloatv(OpenGL11Functions::PROJECTION_MATRIX, projection.values.data());
	this->gl.glGetFloatv(OpenGL11Functions::MODELVIEW_MATRIX, modelView.values.data());
	matrix = projection * modelView;
	return true;
}

}
