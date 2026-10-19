#include <litefont/OpenGL33Device.h>

#include <cstdint>

namespace litefont {

/**
 * OpenGL33DrawState - The state OpenGL33Device changes to draw, read with
 * glGet calls before drawing and set back afterwards, since core profiles
 * have no attribute stack. The texture and sampler are unit 0's.
 */
struct OpenGL33DrawState {
	std::int32_t program = 0;
	std::int32_t vertexArray = 0;
	std::int32_t arrayBuffer = 0;
	std::int32_t activeTexture = 0;
	std::int32_t texture = 0;
	std::int32_t sampler = 0;
	std::int32_t blendSourceRgb = 0;
	std::int32_t blendDestinationRgb = 0;
	std::int32_t blendSourceAlpha = 0;
	std::int32_t blendDestinationAlpha = 0;
	std::int32_t blendEquationRgb = 0;
	std::int32_t blendEquationAlpha = 0;
	std::int32_t scissorBox[4] = {0, 0, 0, 0};
	std::int32_t polygonMode[2] = {0, 0};
	std::uint8_t blend = 0;
	std::uint8_t cullFace = 0;
	std::uint8_t depthTest = 0;
	std::uint8_t stencilTest = 0;
	std::uint8_t scissorTest = 0;

	void save(const OpenGL33Functions& gl);
	void restore(const OpenGL33Functions& gl) const;
};

void OpenGL33DrawState::save(const OpenGL33Functions& gl) {
	using GL = OpenGL33Functions;
	gl.glGetIntegerv(GL::CURRENT_PROGRAM, &this->program);
	gl.glGetIntegerv(GL::VERTEX_ARRAY_BINDING, &this->vertexArray);
	gl.glGetIntegerv(GL::ARRAY_BUFFER_BINDING, &this->arrayBuffer);
	gl.glGetIntegerv(GL::ACTIVE_TEXTURE, &this->activeTexture);
	gl.glActiveTexture(GL::TEXTURE0);
	gl.glGetIntegerv(GL::TEXTURE_BINDING_2D, &this->texture);
	gl.glGetIntegerv(GL::SAMPLER_BINDING, &this->sampler);
	gl.glGetIntegerv(GL::BLEND_SRC_RGB, &this->blendSourceRgb);
	gl.glGetIntegerv(GL::BLEND_DST_RGB, &this->blendDestinationRgb);
	gl.glGetIntegerv(GL::BLEND_SRC_ALPHA, &this->blendSourceAlpha);
	gl.glGetIntegerv(GL::BLEND_DST_ALPHA, &this->blendDestinationAlpha);
	gl.glGetIntegerv(GL::BLEND_EQUATION_RGB, &this->blendEquationRgb);
	gl.glGetIntegerv(GL::BLEND_EQUATION_ALPHA, &this->blendEquationAlpha);
	gl.glGetIntegerv(GL::SCISSOR_BOX, this->scissorBox);
	gl.glGetIntegerv(GL::POLYGON_MODE, this->polygonMode);
	this->blend = gl.glIsEnabled(GL::BLEND);
	this->cullFace = gl.glIsEnabled(GL::CULL_FACE);
	this->depthTest = gl.glIsEnabled(GL::DEPTH_TEST);
	this->stencilTest = gl.glIsEnabled(GL::STENCIL_TEST);
	this->scissorTest = gl.glIsEnabled(GL::SCISSOR_TEST);
}

void OpenGL33DrawState::restore(const OpenGL33Functions& gl) const {
	using GL = OpenGL33Functions;
	auto set = [&gl](std::uint32_t capability, std::uint8_t enabled) {
		if (enabled != 0) {
			gl.glEnable(capability);
		}
		else {
			gl.glDisable(capability);
		}
	};
	gl.glUseProgram(static_cast<std::uint32_t>(this->program));
	gl.glBindVertexArray(static_cast<std::uint32_t>(this->vertexArray));
	gl.glBindBuffer(GL::ARRAY_BUFFER, static_cast<std::uint32_t>(this->arrayBuffer));
	gl.glBindTexture(GL::TEXTURE_2D, static_cast<std::uint32_t>(this->texture));
	gl.glBindSampler(0, static_cast<std::uint32_t>(this->sampler));
	gl.glActiveTexture(static_cast<std::uint32_t>(this->activeTexture));
	gl.glBlendEquationSeparate(static_cast<std::uint32_t>(this->blendEquationRgb), static_cast<std::uint32_t>(this->blendEquationAlpha));
	gl.glBlendFuncSeparate(static_cast<std::uint32_t>(this->blendSourceRgb), static_cast<std::uint32_t>(this->blendDestinationRgb),
		static_cast<std::uint32_t>(this->blendSourceAlpha), static_cast<std::uint32_t>(this->blendDestinationAlpha));
	gl.glScissor(this->scissorBox[0], this->scissorBox[1], this->scissorBox[2], this->scissorBox[3]);
	gl.glPolygonMode(GL::FRONT_AND_BACK, static_cast<std::uint32_t>(this->polygonMode[0]));
	set(GL::BLEND, this->blend);
	set(GL::CULL_FACE, this->cullFace);
	set(GL::DEPTH_TEST, this->depthTest);
	set(GL::STENCIL_TEST, this->stencilTest);
	set(GL::SCISSOR_TEST, this->scissorTest);
}

void OpenGL33Device::updateTexture(TextureHandle texture, const Image& source, const Recti& region) {
	using GL = OpenGL33Functions;
	if (texture.value == 0 || texture.value > this->textures.size() || this->getFunctions() == nullptr) {
		return;
	}
	OpenGLTexture& target = this->textures[texture.value - 1];
	const Recti clipped = region.intersected(source.getBounds()).intersected(Recti{0, 0, target.desc.width, target.desc.height});
	if (clipped.isEmpty() || source.getFormat() != target.desc.format) {
		return;
	}
	// Coverage is one channel, read as alpha through the swizzle below.
	std::uint32_t internalFormat = GL::R8;
	std::uint32_t format = GL::RED;
	if (target.desc.format == PixelFormat::RGB8) {
		internalFormat = GL::RGB8;
		format = GL::RGB;
	}
	else if (target.desc.format == PixelFormat::RGBA8) {
		internalFormat = GL::RGBA8;
		format = GL::RGBA;
	}

	const OpenGL33Functions& gl = this->gl;
	std::int32_t boundTexture = 0;
	std::int32_t unpackBuffer = 0;
	std::int32_t alignment = 4;
	std::int32_t rowLength = 0;
	std::int32_t skipPixels = 0;
	std::int32_t skipRows = 0;
	if (this->restoreState) {
		gl.glGetIntegerv(GL::TEXTURE_BINDING_2D, &boundTexture);
		gl.glGetIntegerv(GL::PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
		gl.glGetIntegerv(GL::UNPACK_ALIGNMENT, &alignment);
		gl.glGetIntegerv(GL::UNPACK_ROW_LENGTH, &rowLength);
		gl.glGetIntegerv(GL::UNPACK_SKIP_PIXELS, &skipPixels);
		gl.glGetIntegerv(GL::UNPACK_SKIP_ROWS, &skipRows);
	}
	// Pixels come from memory, never from a buffer the program left bound.
	gl.glBindBuffer(GL::PIXEL_UNPACK_BUFFER, 0);
	if (target.name == 0) {
		gl.glGenTextures(1, &target.name);
		gl.glBindTexture(GL::TEXTURE_2D, target.name);
		target.filter = this->getFilter();
		gl.glTexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MIN_FILTER, static_cast<std::int32_t>(target.filter));
		gl.glTexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MAG_FILTER, static_cast<std::int32_t>(target.filter));
		gl.glTexParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_S, static_cast<std::int32_t>(GL::CLAMP_TO_EDGE));
		gl.glTexParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_T, static_cast<std::int32_t>(GL::CLAMP_TO_EDGE));
		if (format == GL::RED) {
			// White with the coverage as alpha, as an alpha texture is in OpenGL 1.1.
			gl.glTexParameteri(GL::TEXTURE_2D, GL::TEXTURE_SWIZZLE_R, static_cast<std::int32_t>(GL::ONE));
			gl.glTexParameteri(GL::TEXTURE_2D, GL::TEXTURE_SWIZZLE_G, static_cast<std::int32_t>(GL::ONE));
			gl.glTexParameteri(GL::TEXTURE_2D, GL::TEXTURE_SWIZZLE_B, static_cast<std::int32_t>(GL::ONE));
			gl.glTexParameteri(GL::TEXTURE_2D, GL::TEXTURE_SWIZZLE_A, static_cast<std::int32_t>(GL::RED));
		}
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
		gl.glPixelStorei(GL::UNPACK_ALIGNMENT, alignment);
		gl.glPixelStorei(GL::UNPACK_ROW_LENGTH, rowLength);
		gl.glPixelStorei(GL::UNPACK_SKIP_PIXELS, skipPixels);
		gl.glPixelStorei(GL::UNPACK_SKIP_ROWS, skipRows);
		gl.glBindBuffer(GL::PIXEL_UNPACK_BUFFER, static_cast<std::uint32_t>(unpackBuffer));
		gl.glBindTexture(GL::TEXTURE_2D, static_cast<std::uint32_t>(boundTexture));
	}
	else {
		// Unpacking settings left behind would break the program's own uploads.
		gl.glPixelStorei(GL::UNPACK_ALIGNMENT, 4);
		gl.glPixelStorei(GL::UNPACK_ROW_LENGTH, 0);
		gl.glPixelStorei(GL::UNPACK_SKIP_PIXELS, 0);
		gl.glPixelStorei(GL::UNPACK_SKIP_ROWS, 0);
	}
}

void OpenGL33Device::drawSprites(GeometryHandle geometry, std::span<const SpriteBatch> batches) {
	using GL = OpenGL33Functions;
	if (geometry.value == 0 || geometry.value > this->geometries.size() || batches.empty() || this->getFunctions() == nullptr || !this->prepareProgram()) {
		return;
	}
	OpenGLGeometry& source = this->geometries[geometry.value - 1];
	const VertexAttribute* position = source.layout.find(VertexSemantic::Position);
	const VertexAttribute* texCoord = source.layout.find(VertexSemantic::TexCoord);
	const VertexAttribute* color = source.layout.find(VertexSemantic::Color);
	const std::int32_t stride = source.layout.getStride();
	if (position == nullptr || texCoord == nullptr || color == nullptr || source.vertexBytes.empty() || source.indices.empty()) {
		return;
	}
	const OpenGL33Functions& gl = this->gl;
	const Recti viewport = this->getViewport();
	OpenGL33DrawState saved;
	if (this->restoreState) {
		saved.save(gl);
	}

	// The geometry's buffers, filled again only after an update; the index
	// buffer binding belongs to the vertex array.
	gl.glUseProgram(this->program);
	gl.glBindVertexArray(this->vertexArray);
	if (source.vertexBuffer == 0) {
		gl.glGenBuffers(1, &source.vertexBuffer);
	}
	if (source.indexBuffer == 0) {
		gl.glGenBuffers(1, &source.indexBuffer);
	}
	gl.glBindBuffer(GL::ARRAY_BUFFER, source.vertexBuffer);
	gl.glBindBuffer(GL::ELEMENT_ARRAY_BUFFER, source.indexBuffer);
	if (!source.uploaded) {
		gl.glBufferData(GL::ARRAY_BUFFER, static_cast<std::ptrdiff_t>(source.vertexBytes.size()), source.vertexBytes.data(), GL::STREAM_DRAW);
		gl.glBufferData(GL::ELEMENT_ARRAY_BUFFER, static_cast<std::ptrdiff_t>(source.indices.size() * sizeof(std::uint32_t)), source.indices.data(), GL::STREAM_DRAW);
		source.uploaded = true;
	}
	auto offset = [](std::uint32_t bytes) {
		return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(bytes));
	};
	gl.glVertexAttribPointer(0, 2, GL::FLOAT, 0, stride, offset(position->offset));
	gl.glVertexAttribPointer(1, 2, GL::FLOAT, 0, stride, offset(texCoord->offset));
	gl.glVertexAttribPointer(2, 4, GL::UNSIGNED_BYTE, 1, stride, offset(color->offset));
	for (std::uint32_t attribute = 0; attribute < 3; attribute++) {
		gl.glEnableVertexAttribArray(attribute);
	}

	// Pixels from the viewport's top-left corner with y down, or the program's
	// matrix, after the placement.
	const Matrix4f transform = this->getSpriteTransform(viewport);
	gl.glUniformMatrix4fv(this->transformLocation, 1, 0, transform.getData());

	// Coverage times the vertex color, blended with straight alpha.
	gl.glDisable(GL::DEPTH_TEST);
	gl.glDisable(GL::CULL_FACE);
	gl.glDisable(GL::STENCIL_TEST);
	gl.glPolygonMode(GL::FRONT_AND_BACK, GL::FILL);
	gl.glEnable(GL::BLEND);
	gl.glBlendEquationSeparate(GL::FUNC_ADD, GL::FUNC_ADD);
	gl.glBlendFunc(GL::SRC_ALPHA, GL::ONE_MINUS_SRC_ALPHA);
	gl.glActiveTexture(GL::TEXTURE0);
	gl.glBindSampler(0, 0);

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
		gl.glDrawElements(GL::TRIANGLES, static_cast<std::int32_t>(batch.indexCount), GL::UNSIGNED_INT,
			offset(batch.firstIndex * static_cast<std::uint32_t>(sizeof(std::uint32_t))));
	}

	if (this->restoreState) {
		saved.restore(gl);
	}
	else {
		// Later changes to buffer bindings must never reach this vertex array.
		gl.glBindVertexArray(0);
	}
}

Image OpenGL33Device::readPixels() const {
	// Pixels go to memory, never into a buffer the program left bound.
	if (this->getFunctions() == nullptr) {
		return Image();
	}
	std::int32_t packBuffer = 0;
	this->gl.glGetIntegerv(OpenGL33Functions::PIXEL_PACK_BUFFER_BINDING, &packBuffer);
	this->gl.glBindBuffer(OpenGL33Functions::PIXEL_PACK_BUFFER, 0);
	Image frame = OpenGLDevice::readPixels();
	this->gl.glBindBuffer(OpenGL33Functions::PIXEL_PACK_BUFFER, static_cast<std::uint32_t>(packBuffer));
	return frame;
}

void OpenGL33Device::release() {
	if (this->getFunctions() != nullptr) {
		for (OpenGLGeometry& geometry : this->geometries) {
			const std::uint32_t buffers[2] = {geometry.vertexBuffer, geometry.indexBuffer};
			this->gl.glDeleteBuffers(2, buffers);
			geometry.vertexBuffer = 0;
			geometry.indexBuffer = 0;
		}
		if (this->vertexArray != 0) {
			this->gl.glDeleteVertexArrays(1, &this->vertexArray);
		}
		if (this->program != 0) {
			this->gl.glDeleteProgram(this->program);
		}
	}
	this->vertexArray = 0;
	this->program = 0;
	this->transformLocation = -1;
	this->programFailed = false;
	OpenGLDevice::release();
	// Functions are found again for whatever context the next draw meets.
	this->gl = OpenGL33Functions();
}

const OpenGLFunctions* OpenGL33Device::getFunctions() const {
	if (!this->gl.isLoaded() && !this->gl.load()) {
		this->error = OpenGLError::MissingFunction;
		return nullptr;
	}
	return &this->gl;
}

bool OpenGL33Device::prepareProgram() {
	// One program for every sprite: the vertex color times the texel, where a
	// coverage texel is white with the coverage as alpha.
	if (this->program != 0) {
		return true;
	}
	if (this->programFailed) {
		return false;
	}
	const char* vertexSource = R"(#version 330 core
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec4 color;
uniform mat4 transform;
out vec2 fragmentTexCoord;
out vec4 fragmentColor;
void main() {
	fragmentTexCoord = texCoord;
	fragmentColor = color;
	gl_Position = transform * vec4(position, 0.0, 1.0);
}
)";
	const char* fragmentSource = R"(#version 330 core
in vec2 fragmentTexCoord;
in vec4 fragmentColor;
uniform sampler2D atlas;
layout(location = 0) out vec4 outputColor;
void main() {
	outputColor = fragmentColor * texture(atlas, fragmentTexCoord);
}
)";
	using GL = OpenGL33Functions;
	const OpenGL33Functions& gl = this->gl;
	const std::uint32_t vertexShader = this->compileShader(GL::VERTEX_SHADER, vertexSource);
	const std::uint32_t fragmentShader = this->compileShader(GL::FRAGMENT_SHADER, fragmentSource);
	if (vertexShader == 0 || fragmentShader == 0) {
		if (vertexShader != 0) {
			gl.glDeleteShader(vertexShader);
		}
		if (fragmentShader != 0) {
			gl.glDeleteShader(fragmentShader);
		}
		this->error = OpenGLError::ShaderCompile;
		this->programFailed = true;
		return false;
	}
	const std::uint32_t linked = gl.glCreateProgram();
	gl.glAttachShader(linked, vertexShader);
	gl.glAttachShader(linked, fragmentShader);
	gl.glLinkProgram(linked);
	// The shaders go once the program does.
	gl.glDeleteShader(vertexShader);
	gl.glDeleteShader(fragmentShader);
	std::int32_t status = 0;
	gl.glGetProgramiv(linked, GL::LINK_STATUS, &status);
	if (status == 0) {
		gl.glDeleteProgram(linked);
		this->error = OpenGLError::ProgramLink;
		this->programFailed = true;
		return false;
	}
	this->program = linked;
	this->transformLocation = gl.glGetUniformLocation(linked, "transform");
	gl.glGenVertexArrays(1, &this->vertexArray);
	return true;
}

std::uint32_t OpenGL33Device::compileShader(std::uint32_t type, const char* source) {
	const OpenGL33Functions& gl = this->gl;
	const std::uint32_t shader = gl.glCreateShader(type);
	gl.glShaderSource(shader, 1, &source, nullptr);
	gl.glCompileShader(shader);
	std::int32_t status = 0;
	gl.glGetShaderiv(shader, OpenGL33Functions::COMPILE_STATUS, &status);
	if (status == 0) {
		gl.glDeleteShader(shader);
		return 0;
	}
	return shader;
}

}
