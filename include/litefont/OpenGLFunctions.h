#pragma once

#include <cstddef>
#include <cstdint>

namespace litefont {

/** OpenGLProc - The address of an OpenGL function, before it is called through its own type. */
using OpenGLProc = void (*)();

/**
 * \class OpenGLFunctions OpenGLFunctions.h <litefont/OpenGLFunctions.h>
 * @addtogroup Graphics
 *
 * OpenGLFunctions - The OpenGL calls litefont makes in every version: the 1.1
 * functions core profiles kept. Each table of functions is found at run time
 * for the context current when load() is called, in the spirit of Qt's
 * QOpenGLFunctions, so litefont needs no extension loader and no system
 * OpenGL header; OpenGL11Functions and OpenGL33Functions add what each device
 * needs on top. The methods take the OpenGL names and plain std types, and
 * the constants are OpenGL's without the GL_ prefix (so they never meet the
 * system header's macros).
 *
 * OpenGLFunctions.cpp is the only file in litefont that knows the operating
 * system: it finds functions with wglGetProcAddress on Windows (and in
 * opengl32.dll for the 1.1 functions, which wglGetProcAddress does not
 * return), in the OpenGL framework on macOS, and through EGL or GLX on
 * Linux, whichever made the current context. load() returns false and
 * getMissing() names the first function not found when the context lacks
 * one (or no context is current); calling a function that was not found is
 * an error.
 */
class OpenGLFunctions {
public:
	static constexpr std::uint32_t TRIANGLES = 0x0004;
	static constexpr std::uint32_t SRC_ALPHA = 0x0302;
	static constexpr std::uint32_t ONE_MINUS_SRC_ALPHA = 0x0303;
	static constexpr std::uint32_t FRONT_AND_BACK = 0x0408;
	static constexpr std::uint32_t POLYGON_MODE = 0x0B40;
	static constexpr std::uint32_t CULL_FACE = 0x0B44;
	static constexpr std::uint32_t DEPTH_TEST = 0x0B71;
	static constexpr std::uint32_t STENCIL_TEST = 0x0B90;
	static constexpr std::uint32_t VIEWPORT = 0x0BA2;
	static constexpr std::uint32_t BLEND = 0x0BE2;
	static constexpr std::uint32_t SCISSOR_BOX = 0x0C10;
	static constexpr std::uint32_t SCISSOR_TEST = 0x0C11;
	static constexpr std::uint32_t UNPACK_ROW_LENGTH = 0x0CF2;
	static constexpr std::uint32_t UNPACK_SKIP_ROWS = 0x0CF3;
	static constexpr std::uint32_t UNPACK_SKIP_PIXELS = 0x0CF4;
	static constexpr std::uint32_t UNPACK_ALIGNMENT = 0x0CF5;
	static constexpr std::uint32_t PACK_ALIGNMENT = 0x0D05;
	static constexpr std::uint32_t TEXTURE_2D = 0x0DE1;
	static constexpr std::uint32_t UNSIGNED_BYTE = 0x1401;
	static constexpr std::uint32_t UNSIGNED_INT = 0x1405;
	static constexpr std::uint32_t FLOAT = 0x1406;
	static constexpr std::uint32_t RGB = 0x1907;
	static constexpr std::uint32_t RGBA = 0x1908;
	static constexpr std::uint32_t FILL = 0x1B02;
	static constexpr std::uint32_t NEAREST = 0x2600;
	static constexpr std::uint32_t LINEAR = 0x2601;
	static constexpr std::uint32_t TEXTURE_MAG_FILTER = 0x2800;
	static constexpr std::uint32_t TEXTURE_MIN_FILTER = 0x2801;
	static constexpr std::uint32_t TEXTURE_WRAP_S = 0x2802;
	static constexpr std::uint32_t TEXTURE_WRAP_T = 0x2803;
	static constexpr std::uint32_t COLOR_BUFFER_BIT = 0x00004000;
	static constexpr std::uint32_t RGB8 = 0x8051;
	static constexpr std::uint32_t RGBA8 = 0x8058;
	static constexpr std::uint32_t TEXTURE_BINDING_2D = 0x8069;

	bool load();
	bool isLoaded() const;
	const char* getMissing() const;

	void glBindTexture(std::uint32_t target, std::uint32_t texture) const;
	void glBlendFunc(std::uint32_t source, std::uint32_t destination) const;
	void glClear(std::uint32_t mask) const;
	void glClearColor(float red, float green, float blue, float alpha) const;
	void glDeleteTextures(std::int32_t count, const std::uint32_t* textures) const;
	void glDisable(std::uint32_t capability) const;
	void glDrawElements(std::uint32_t mode, std::int32_t count, std::uint32_t type, const void* indices) const;
	void glEnable(std::uint32_t capability) const;
	void glGenTextures(std::int32_t count, std::uint32_t* textures) const;
	void glGetFloatv(std::uint32_t name, float* values) const;
	void glGetIntegerv(std::uint32_t name, std::int32_t* values) const;
	std::uint8_t glIsEnabled(std::uint32_t capability) const;
	void glPixelStorei(std::uint32_t name, std::int32_t value) const;
	void glPolygonMode(std::uint32_t face, std::uint32_t mode) const;
	void glReadPixels(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height, std::uint32_t format, std::uint32_t type, void* pixels) const;
	void glScissor(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) const;
	void glTexImage2D(std::uint32_t target, std::int32_t level, std::int32_t internalFormat, std::int32_t width, std::int32_t height,
		std::int32_t border, std::uint32_t format, std::uint32_t type, const void* pixels) const;
	void glTexParameteri(std::uint32_t target, std::uint32_t name, std::int32_t value) const;
	void glTexSubImage2D(std::uint32_t target, std::int32_t level, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height,
		std::uint32_t format, std::uint32_t type, const void* pixels) const;
	void glViewport(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) const;

protected:
	bool resolve(const char* name, OpenGLProc& proc);

	static OpenGLProc getProcAddress(const char* name);

	const char* missing = nullptr;
	bool loaded = false;

private:
	OpenGLProc bindTexture = nullptr;
	OpenGLProc blendFunc = nullptr;
	OpenGLProc clear = nullptr;
	OpenGLProc clearColor = nullptr;
	OpenGLProc deleteTextures = nullptr;
	OpenGLProc disable = nullptr;
	OpenGLProc drawElements = nullptr;
	OpenGLProc enable = nullptr;
	OpenGLProc genTextures = nullptr;
	OpenGLProc getFloatv = nullptr;
	OpenGLProc getIntegerv = nullptr;
	OpenGLProc isEnabled = nullptr;
	OpenGLProc pixelStorei = nullptr;
	OpenGLProc polygonMode = nullptr;
	OpenGLProc readPixels = nullptr;
	OpenGLProc scissor = nullptr;
	OpenGLProc texImage2D = nullptr;
	OpenGLProc texParameteri = nullptr;
	OpenGLProc texSubImage2D = nullptr;
	OpenGLProc viewport = nullptr;
};

/**
 * \class OpenGL11Functions OpenGLFunctions.h <litefont/OpenGLFunctions.h>
 * @addtogroup Graphics
 *
 * OpenGL11Functions - The fixed-function OpenGL 1.1 calls OpenGL11Device makes
 * besides the common ones: matrix stacks, attribute stacks, client-side
 * vertex arrays and the texture environment. Every compatibility context has
 * them; core profiles do not.
 */
class OpenGL11Functions : public OpenGLFunctions {
public:
	static constexpr std::uint32_t CURRENT_BIT = 0x00000001;
	static constexpr std::uint32_t POLYGON_BIT = 0x00000008;
	static constexpr std::uint32_t LIGHTING_BIT = 0x00000040;
	static constexpr std::uint32_t DEPTH_BUFFER_BIT = 0x00000100;
	static constexpr std::uint32_t TRANSFORM_BIT = 0x00001000;
	static constexpr std::uint32_t ENABLE_BIT = 0x00002000;
	static constexpr std::uint32_t TEXTURE_BIT = 0x00040000;
	static constexpr std::uint32_t SCISSOR_BIT = 0x00080000;
	static constexpr std::uint32_t CLIENT_PIXEL_STORE_BIT = 0x00000001;
	static constexpr std::uint32_t CLIENT_VERTEX_ARRAY_BIT = 0x00000002;
	static constexpr std::uint32_t LIGHTING = 0x0B50;
	static constexpr std::uint32_t FOG = 0x0B60;
	static constexpr std::uint32_t MODELVIEW_MATRIX = 0x0BA6;
	static constexpr std::uint32_t PROJECTION_MATRIX = 0x0BA7;
	static constexpr std::uint32_t ALPHA_TEST = 0x0BC0;
	static constexpr std::uint32_t TEXTURE_GEN_S = 0x0C60;
	static constexpr std::uint32_t TEXTURE_GEN_T = 0x0C61;
	static constexpr std::uint32_t MODELVIEW = 0x1700;
	static constexpr std::uint32_t PROJECTION = 0x1701;
	static constexpr std::uint32_t TEXTURE = 0x1702;
	static constexpr std::uint32_t ALPHA = 0x1906;
	static constexpr std::uint32_t SMOOTH = 0x1D01;
	static constexpr std::uint32_t MODULATE = 0x2100;
	static constexpr std::uint32_t TEXTURE_ENV_MODE = 0x2200;
	static constexpr std::uint32_t TEXTURE_ENV = 0x2300;
	static constexpr std::uint32_t CLAMP = 0x2900;
	static constexpr std::uint32_t ALPHA8 = 0x803C;
	static constexpr std::uint32_t VERTEX_ARRAY = 0x8074;
	static constexpr std::uint32_t NORMAL_ARRAY = 0x8075;
	static constexpr std::uint32_t COLOR_ARRAY = 0x8076;
	static constexpr std::uint32_t INDEX_ARRAY = 0x8077;
	static constexpr std::uint32_t TEXTURE_COORD_ARRAY = 0x8078;
	static constexpr std::uint32_t EDGE_FLAG_ARRAY = 0x8079;

	bool load();

	void glColorPointer(std::int32_t size, std::uint32_t type, std::int32_t stride, const void* pointer) const;
	void glDisableClientState(std::uint32_t array) const;
	void glEnableClientState(std::uint32_t array) const;
	void glLoadIdentity() const;
	void glLoadMatrixf(const float* matrix) const;
	void glMatrixMode(std::uint32_t mode) const;
	void glMultMatrixf(const float* matrix) const;
	void glOrtho(double left, double right, double bottom, double top, double zNear, double zFar) const;
	void glPopAttrib() const;
	void glPopClientAttrib() const;
	void glPopMatrix() const;
	void glPushAttrib(std::uint32_t mask) const;
	void glPushClientAttrib(std::uint32_t mask) const;
	void glPushMatrix() const;
	void glShadeModel(std::uint32_t mode) const;
	void glTexCoordPointer(std::int32_t size, std::uint32_t type, std::int32_t stride, const void* pointer) const;
	void glTexEnvi(std::uint32_t target, std::uint32_t name, std::int32_t value) const;
	void glVertexPointer(std::int32_t size, std::uint32_t type, std::int32_t stride, const void* pointer) const;

private:
	OpenGLProc colorPointer = nullptr;
	OpenGLProc disableClientState = nullptr;
	OpenGLProc enableClientState = nullptr;
	OpenGLProc loadIdentity = nullptr;
	OpenGLProc loadMatrixf = nullptr;
	OpenGLProc matrixMode = nullptr;
	OpenGLProc multMatrixf = nullptr;
	OpenGLProc ortho = nullptr;
	OpenGLProc popAttrib = nullptr;
	OpenGLProc popClientAttrib = nullptr;
	OpenGLProc popMatrix = nullptr;
	OpenGLProc pushAttrib = nullptr;
	OpenGLProc pushClientAttrib = nullptr;
	OpenGLProc pushMatrix = nullptr;
	OpenGLProc shadeModel = nullptr;
	OpenGLProc texCoordPointer = nullptr;
	OpenGLProc texEnvi = nullptr;
	OpenGLProc vertexPointer = nullptr;
};

/**
 * \class OpenGL33Functions OpenGLFunctions.h <litefont/OpenGLFunctions.h>
 * @addtogroup Graphics
 *
 * OpenGL33Functions - The OpenGL 3.3 calls OpenGL33Device makes besides the
 * common ones: shaders, buffers, vertex arrays, samplers and separate blend
 * state. Core and compatibility contexts of version 3.3 or newer have them.
 */
class OpenGL33Functions : public OpenGLFunctions {
public:
	static constexpr std::uint32_t RED = 0x1903;
	static constexpr std::uint32_t FUNC_ADD = 0x8006;
	static constexpr std::uint32_t BLEND_EQUATION_RGB = 0x8009;
	static constexpr std::uint32_t BLEND_DST_RGB = 0x80C8;
	static constexpr std::uint32_t BLEND_SRC_RGB = 0x80C9;
	static constexpr std::uint32_t BLEND_DST_ALPHA = 0x80CA;
	static constexpr std::uint32_t BLEND_SRC_ALPHA = 0x80CB;
	static constexpr std::uint32_t CLAMP_TO_EDGE = 0x812F;
	static constexpr std::uint32_t R8 = 0x8229;
	static constexpr std::uint32_t TEXTURE0 = 0x84C0;
	static constexpr std::uint32_t ACTIVE_TEXTURE = 0x84E0;
	static constexpr std::uint32_t VERTEX_ARRAY_BINDING = 0x85B5;
	static constexpr std::uint32_t BLEND_EQUATION_ALPHA = 0x883D;
	static constexpr std::uint32_t ARRAY_BUFFER = 0x8892;
	static constexpr std::uint32_t ELEMENT_ARRAY_BUFFER = 0x8893;
	static constexpr std::uint32_t ARRAY_BUFFER_BINDING = 0x8894;
	static constexpr std::uint32_t STREAM_DRAW = 0x88E0;
	static constexpr std::uint32_t PIXEL_PACK_BUFFER = 0x88EB;
	static constexpr std::uint32_t PIXEL_UNPACK_BUFFER = 0x88EC;
	static constexpr std::uint32_t PIXEL_PACK_BUFFER_BINDING = 0x88ED;
	static constexpr std::uint32_t PIXEL_UNPACK_BUFFER_BINDING = 0x88EF;
	static constexpr std::uint32_t SAMPLER_BINDING = 0x8919;
	static constexpr std::uint32_t FRAGMENT_SHADER = 0x8B30;
	static constexpr std::uint32_t VERTEX_SHADER = 0x8B31;
	static constexpr std::uint32_t COMPILE_STATUS = 0x8B81;
	static constexpr std::uint32_t LINK_STATUS = 0x8B82;
	static constexpr std::uint32_t CURRENT_PROGRAM = 0x8B8D;
	static constexpr std::uint32_t TEXTURE_SWIZZLE_R = 0x8E42;
	static constexpr std::uint32_t TEXTURE_SWIZZLE_G = 0x8E43;
	static constexpr std::uint32_t TEXTURE_SWIZZLE_B = 0x8E44;
	static constexpr std::uint32_t TEXTURE_SWIZZLE_A = 0x8E45;
	static constexpr std::uint32_t ONE = 1;

	bool load();

	void glActiveTexture(std::uint32_t unit) const;
	void glAttachShader(std::uint32_t program, std::uint32_t shader) const;
	void glBindBuffer(std::uint32_t target, std::uint32_t buffer) const;
	void glBindSampler(std::uint32_t unit, std::uint32_t sampler) const;
	void glBindVertexArray(std::uint32_t array) const;
	void glBlendEquationSeparate(std::uint32_t rgb, std::uint32_t alpha) const;
	void glBlendFuncSeparate(std::uint32_t sourceRgb, std::uint32_t destinationRgb, std::uint32_t sourceAlpha, std::uint32_t destinationAlpha) const;
	void glBufferData(std::uint32_t target, std::ptrdiff_t size, const void* data, std::uint32_t usage) const;
	void glCompileShader(std::uint32_t shader) const;
	std::uint32_t glCreateProgram() const;
	std::uint32_t glCreateShader(std::uint32_t type) const;
	void glDeleteBuffers(std::int32_t count, const std::uint32_t* buffers) const;
	void glDeleteProgram(std::uint32_t program) const;
	void glDeleteShader(std::uint32_t shader) const;
	void glDeleteVertexArrays(std::int32_t count, const std::uint32_t* arrays) const;
	void glEnableVertexAttribArray(std::uint32_t index) const;
	void glGenBuffers(std::int32_t count, std::uint32_t* buffers) const;
	void glGenVertexArrays(std::int32_t count, std::uint32_t* arrays) const;
	void glGetProgramiv(std::uint32_t program, std::uint32_t name, std::int32_t* value) const;
	void glGetShaderiv(std::uint32_t shader, std::uint32_t name, std::int32_t* value) const;
	std::int32_t glGetUniformLocation(std::uint32_t program, const char* name) const;
	void glLinkProgram(std::uint32_t program) const;
	void glShaderSource(std::uint32_t shader, std::int32_t count, const char* const* strings, const std::int32_t* lengths) const;
	void glUniformMatrix4fv(std::int32_t location, std::int32_t count, std::uint8_t transpose, const float* values) const;
	void glUseProgram(std::uint32_t program) const;
	void glVertexAttribPointer(std::uint32_t index, std::int32_t size, std::uint32_t type, std::uint8_t normalized, std::int32_t stride, const void* pointer) const;

private:
	OpenGLProc activeTexture = nullptr;
	OpenGLProc attachShader = nullptr;
	OpenGLProc bindBuffer = nullptr;
	OpenGLProc bindSampler = nullptr;
	OpenGLProc bindVertexArray = nullptr;
	OpenGLProc blendEquationSeparate = nullptr;
	OpenGLProc blendFuncSeparate = nullptr;
	OpenGLProc bufferData = nullptr;
	OpenGLProc compileShader = nullptr;
	OpenGLProc createProgram = nullptr;
	OpenGLProc createShader = nullptr;
	OpenGLProc deleteBuffers = nullptr;
	OpenGLProc deleteProgram = nullptr;
	OpenGLProc deleteShader = nullptr;
	OpenGLProc deleteVertexArrays = nullptr;
	OpenGLProc enableVertexAttribArray = nullptr;
	OpenGLProc genBuffers = nullptr;
	OpenGLProc genVertexArrays = nullptr;
	OpenGLProc getProgramiv = nullptr;
	OpenGLProc getShaderiv = nullptr;
	OpenGLProc getUniformLocation = nullptr;
	OpenGLProc linkProgram = nullptr;
	OpenGLProc shaderSource = nullptr;
	OpenGLProc uniformMatrix4fv = nullptr;
	OpenGLProc useProgram = nullptr;
	OpenGLProc vertexAttribPointer = nullptr;
};

}
