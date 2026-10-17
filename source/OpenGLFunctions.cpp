#include <litefont/OpenGLFunctions.h>

// The only file in litefont that knows the operating system: how to find an
// OpenGL function, and the calling convention OpenGL functions use.
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define LITEFONT_APIENTRY __stdcall
#else
#include <dlfcn.h>
#define LITEFONT_APIENTRY
#endif

namespace litefont {

/**
 * OpenGLCall - Calls an OpenGL function found by address through the type its
 * arguments give it, with OpenGL's calling convention (which matters only on
 * 32-bit Windows).
 */
class OpenGLCall {
public:
	template <typename Result, typename... Arguments>
	static Result invoke(OpenGLProc proc, Arguments... arguments) {
		using Function = Result (LITEFONT_APIENTRY*)(Arguments...);
		return reinterpret_cast<Function>(proc)(arguments...);
	}
};

OpenGLProc OpenGLFunctions::getProcAddress(const char* name) {
#if defined(_WIN32)
	// wglGetProcAddress finds what the driver adds past OpenGL 1.1, and some
	// drivers answer a miss with 1, 2, 3 or -1 instead of null; opengl32.dll
	// exports the 1.1 functions itself.
	PROC proc = wglGetProcAddress(name);
	const std::intptr_t value = reinterpret_cast<std::intptr_t>(proc);
	if (value >= -1 && value <= 3) {
		const HMODULE library = GetModuleHandleA("opengl32.dll");
		proc = library != nullptr ? GetProcAddress(library, name) : nullptr;
	}
	return reinterpret_cast<OpenGLProc>(proc);
#elif defined(__APPLE__)
	// The OpenGL framework exports every function of every version it has.
	return reinterpret_cast<OpenGLProc>(dlsym(RTLD_DEFAULT, name));
#else
	// EGL or GLX, whichever made the current context, and failing both, what
	// the OpenGL library exports itself.
	using GetCurrentContext = void* (*)();
	using GetEglAddress = OpenGLProc (*)(const char*);
	using GetGlxAddress = OpenGLProc (*)(const unsigned char*);
	OpenGLProc proc = nullptr;
	const auto eglGetCurrentContext = reinterpret_cast<GetCurrentContext>(dlsym(RTLD_DEFAULT, "eglGetCurrentContext"));
	const auto eglGetProcAddress = reinterpret_cast<GetEglAddress>(dlsym(RTLD_DEFAULT, "eglGetProcAddress"));
	const auto glXGetProcAddressARB = reinterpret_cast<GetGlxAddress>(dlsym(RTLD_DEFAULT, "glXGetProcAddressARB"));
	if (eglGetCurrentContext != nullptr && eglGetProcAddress != nullptr && eglGetCurrentContext() != nullptr) {
		proc = eglGetProcAddress(name);
	}
	else if (glXGetProcAddressARB != nullptr) {
		proc = glXGetProcAddressARB(reinterpret_cast<const unsigned char*>(name));
	}
	if (proc == nullptr) {
		proc = reinterpret_cast<OpenGLProc>(dlsym(RTLD_DEFAULT, name));
	}
	return proc;
#endif
}

bool OpenGLFunctions::resolve(const char* name, OpenGLProc& proc) {
	proc = OpenGLFunctions::getProcAddress(name);
	if (proc == nullptr && this->missing == nullptr) {
		this->missing = name;
	}
	return proc != nullptr;
}

//------------------------------------------------------------------------------
// OpenGLFunctions
//------------------------------------------------------------------------------
bool OpenGLFunctions::load() {
	this->missing = nullptr;
	bool complete = true;
	complete = this->resolve("glBindTexture", this->bindTexture) && complete;
	complete = this->resolve("glBlendFunc", this->blendFunc) && complete;
	complete = this->resolve("glClear", this->clear) && complete;
	complete = this->resolve("glClearColor", this->clearColor) && complete;
	complete = this->resolve("glDeleteTextures", this->deleteTextures) && complete;
	complete = this->resolve("glDisable", this->disable) && complete;
	complete = this->resolve("glDrawElements", this->drawElements) && complete;
	complete = this->resolve("glEnable", this->enable) && complete;
	complete = this->resolve("glGenTextures", this->genTextures) && complete;
	complete = this->resolve("glGetFloatv", this->getFloatv) && complete;
	complete = this->resolve("glGetIntegerv", this->getIntegerv) && complete;
	complete = this->resolve("glIsEnabled", this->isEnabled) && complete;
	complete = this->resolve("glPixelStorei", this->pixelStorei) && complete;
	complete = this->resolve("glPolygonMode", this->polygonMode) && complete;
	complete = this->resolve("glReadPixels", this->readPixels) && complete;
	complete = this->resolve("glScissor", this->scissor) && complete;
	complete = this->resolve("glTexImage2D", this->texImage2D) && complete;
	complete = this->resolve("glTexParameteri", this->texParameteri) && complete;
	complete = this->resolve("glTexSubImage2D", this->texSubImage2D) && complete;
	complete = this->resolve("glViewport", this->viewport) && complete;
	this->loaded = complete;
	return complete;
}

bool OpenGLFunctions::isLoaded() const {
	return this->loaded;
}

const char* OpenGLFunctions::getMissing() const {
	return this->missing;
}

void OpenGLFunctions::glBindTexture(std::uint32_t target, std::uint32_t texture) const {
	OpenGLCall::invoke<void>(this->bindTexture, target, texture);
}

void OpenGLFunctions::glBlendFunc(std::uint32_t source, std::uint32_t destination) const {
	OpenGLCall::invoke<void>(this->blendFunc, source, destination);
}

void OpenGLFunctions::glClear(std::uint32_t mask) const {
	OpenGLCall::invoke<void>(this->clear, mask);
}

void OpenGLFunctions::glClearColor(float red, float green, float blue, float alpha) const {
	OpenGLCall::invoke<void>(this->clearColor, red, green, blue, alpha);
}

void OpenGLFunctions::glDeleteTextures(std::int32_t count, const std::uint32_t* textures) const {
	OpenGLCall::invoke<void>(this->deleteTextures, count, textures);
}

void OpenGLFunctions::glDisable(std::uint32_t capability) const {
	OpenGLCall::invoke<void>(this->disable, capability);
}

void OpenGLFunctions::glDrawElements(std::uint32_t mode, std::int32_t count, std::uint32_t type, const void* indices) const {
	OpenGLCall::invoke<void>(this->drawElements, mode, count, type, indices);
}

void OpenGLFunctions::glEnable(std::uint32_t capability) const {
	OpenGLCall::invoke<void>(this->enable, capability);
}

void OpenGLFunctions::glGenTextures(std::int32_t count, std::uint32_t* textures) const {
	OpenGLCall::invoke<void>(this->genTextures, count, textures);
}

void OpenGLFunctions::glGetFloatv(std::uint32_t name, float* values) const {
	OpenGLCall::invoke<void>(this->getFloatv, name, values);
}

void OpenGLFunctions::glGetIntegerv(std::uint32_t name, std::int32_t* values) const {
	OpenGLCall::invoke<void>(this->getIntegerv, name, values);
}

std::uint8_t OpenGLFunctions::glIsEnabled(std::uint32_t capability) const {
	return OpenGLCall::invoke<std::uint8_t>(this->isEnabled, capability);
}

void OpenGLFunctions::glPixelStorei(std::uint32_t name, std::int32_t value) const {
	OpenGLCall::invoke<void>(this->pixelStorei, name, value);
}

void OpenGLFunctions::glPolygonMode(std::uint32_t face, std::uint32_t mode) const {
	OpenGLCall::invoke<void>(this->polygonMode, face, mode);
}

void OpenGLFunctions::glReadPixels(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height, std::uint32_t format, std::uint32_t type, void* pixels) const {
	OpenGLCall::invoke<void>(this->readPixels, x, y, width, height, format, type, pixels);
}

void OpenGLFunctions::glScissor(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) const {
	OpenGLCall::invoke<void>(this->scissor, x, y, width, height);
}

void OpenGLFunctions::glTexImage2D(std::uint32_t target, std::int32_t level, std::int32_t internalFormat, std::int32_t width, std::int32_t height,
	std::int32_t border, std::uint32_t format, std::uint32_t type, const void* pixels) const {
	OpenGLCall::invoke<void>(this->texImage2D, target, level, internalFormat, width, height, border, format, type, pixels);
}

void OpenGLFunctions::glTexParameteri(std::uint32_t target, std::uint32_t name, std::int32_t value) const {
	OpenGLCall::invoke<void>(this->texParameteri, target, name, value);
}

void OpenGLFunctions::glTexSubImage2D(std::uint32_t target, std::int32_t level, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height,
	std::uint32_t format, std::uint32_t type, const void* pixels) const {
	OpenGLCall::invoke<void>(this->texSubImage2D, target, level, x, y, width, height, format, type, pixels);
}

void OpenGLFunctions::glViewport(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) const {
	OpenGLCall::invoke<void>(this->viewport, x, y, width, height);
}

//------------------------------------------------------------------------------
// OpenGL11Functions
//------------------------------------------------------------------------------
bool OpenGL11Functions::load() {
	bool complete = OpenGLFunctions::load();
	complete = this->resolve("glColorPointer", this->colorPointer) && complete;
	complete = this->resolve("glDisableClientState", this->disableClientState) && complete;
	complete = this->resolve("glEnableClientState", this->enableClientState) && complete;
	complete = this->resolve("glLoadIdentity", this->loadIdentity) && complete;
	complete = this->resolve("glLoadMatrixf", this->loadMatrixf) && complete;
	complete = this->resolve("glMatrixMode", this->matrixMode) && complete;
	complete = this->resolve("glMultMatrixf", this->multMatrixf) && complete;
	complete = this->resolve("glOrtho", this->ortho) && complete;
	complete = this->resolve("glPopAttrib", this->popAttrib) && complete;
	complete = this->resolve("glPopClientAttrib", this->popClientAttrib) && complete;
	complete = this->resolve("glPopMatrix", this->popMatrix) && complete;
	complete = this->resolve("glPushAttrib", this->pushAttrib) && complete;
	complete = this->resolve("glPushClientAttrib", this->pushClientAttrib) && complete;
	complete = this->resolve("glPushMatrix", this->pushMatrix) && complete;
	complete = this->resolve("glShadeModel", this->shadeModel) && complete;
	complete = this->resolve("glTexCoordPointer", this->texCoordPointer) && complete;
	complete = this->resolve("glTexEnvi", this->texEnvi) && complete;
	complete = this->resolve("glVertexPointer", this->vertexPointer) && complete;
	this->loaded = complete;
	return complete;
}

void OpenGL11Functions::glColorPointer(std::int32_t size, std::uint32_t type, std::int32_t stride, const void* pointer) const {
	OpenGLCall::invoke<void>(this->colorPointer, size, type, stride, pointer);
}

void OpenGL11Functions::glDisableClientState(std::uint32_t array) const {
	OpenGLCall::invoke<void>(this->disableClientState, array);
}

void OpenGL11Functions::glEnableClientState(std::uint32_t array) const {
	OpenGLCall::invoke<void>(this->enableClientState, array);
}

void OpenGL11Functions::glLoadIdentity() const {
	OpenGLCall::invoke<void>(this->loadIdentity);
}

void OpenGL11Functions::glLoadMatrixf(const float* matrix) const {
	OpenGLCall::invoke<void>(this->loadMatrixf, matrix);
}

void OpenGL11Functions::glMatrixMode(std::uint32_t mode) const {
	OpenGLCall::invoke<void>(this->matrixMode, mode);
}

void OpenGL11Functions::glMultMatrixf(const float* matrix) const {
	OpenGLCall::invoke<void>(this->multMatrixf, matrix);
}

void OpenGL11Functions::glOrtho(double left, double right, double bottom, double top, double zNear, double zFar) const {
	OpenGLCall::invoke<void>(this->ortho, left, right, bottom, top, zNear, zFar);
}

void OpenGL11Functions::glPopAttrib() const {
	OpenGLCall::invoke<void>(this->popAttrib);
}

void OpenGL11Functions::glPopClientAttrib() const {
	OpenGLCall::invoke<void>(this->popClientAttrib);
}

void OpenGL11Functions::glPopMatrix() const {
	OpenGLCall::invoke<void>(this->popMatrix);
}

void OpenGL11Functions::glPushAttrib(std::uint32_t mask) const {
	OpenGLCall::invoke<void>(this->pushAttrib, mask);
}

void OpenGL11Functions::glPushClientAttrib(std::uint32_t mask) const {
	OpenGLCall::invoke<void>(this->pushClientAttrib, mask);
}

void OpenGL11Functions::glPushMatrix() const {
	OpenGLCall::invoke<void>(this->pushMatrix);
}

void OpenGL11Functions::glShadeModel(std::uint32_t mode) const {
	OpenGLCall::invoke<void>(this->shadeModel, mode);
}

void OpenGL11Functions::glTexCoordPointer(std::int32_t size, std::uint32_t type, std::int32_t stride, const void* pointer) const {
	OpenGLCall::invoke<void>(this->texCoordPointer, size, type, stride, pointer);
}

void OpenGL11Functions::glTexEnvi(std::uint32_t target, std::uint32_t name, std::int32_t value) const {
	OpenGLCall::invoke<void>(this->texEnvi, target, name, value);
}

void OpenGL11Functions::glVertexPointer(std::int32_t size, std::uint32_t type, std::int32_t stride, const void* pointer) const {
	OpenGLCall::invoke<void>(this->vertexPointer, size, type, stride, pointer);
}

//------------------------------------------------------------------------------
// OpenGL33Functions
//------------------------------------------------------------------------------
bool OpenGL33Functions::load() {
	bool complete = OpenGLFunctions::load();
	complete = this->resolve("glActiveTexture", this->activeTexture) && complete;
	complete = this->resolve("glAttachShader", this->attachShader) && complete;
	complete = this->resolve("glBindBuffer", this->bindBuffer) && complete;
	complete = this->resolve("glBindSampler", this->bindSampler) && complete;
	complete = this->resolve("glBindVertexArray", this->bindVertexArray) && complete;
	complete = this->resolve("glBlendEquationSeparate", this->blendEquationSeparate) && complete;
	complete = this->resolve("glBlendFuncSeparate", this->blendFuncSeparate) && complete;
	complete = this->resolve("glBufferData", this->bufferData) && complete;
	complete = this->resolve("glCompileShader", this->compileShader) && complete;
	complete = this->resolve("glCreateProgram", this->createProgram) && complete;
	complete = this->resolve("glCreateShader", this->createShader) && complete;
	complete = this->resolve("glDeleteBuffers", this->deleteBuffers) && complete;
	complete = this->resolve("glDeleteProgram", this->deleteProgram) && complete;
	complete = this->resolve("glDeleteShader", this->deleteShader) && complete;
	complete = this->resolve("glDeleteVertexArrays", this->deleteVertexArrays) && complete;
	complete = this->resolve("glEnableVertexAttribArray", this->enableVertexAttribArray) && complete;
	complete = this->resolve("glGenBuffers", this->genBuffers) && complete;
	complete = this->resolve("glGenVertexArrays", this->genVertexArrays) && complete;
	complete = this->resolve("glGetProgramiv", this->getProgramiv) && complete;
	complete = this->resolve("glGetShaderiv", this->getShaderiv) && complete;
	complete = this->resolve("glGetUniformLocation", this->getUniformLocation) && complete;
	complete = this->resolve("glLinkProgram", this->linkProgram) && complete;
	complete = this->resolve("glShaderSource", this->shaderSource) && complete;
	complete = this->resolve("glUniformMatrix4fv", this->uniformMatrix4fv) && complete;
	complete = this->resolve("glUseProgram", this->useProgram) && complete;
	complete = this->resolve("glVertexAttribPointer", this->vertexAttribPointer) && complete;
	this->loaded = complete;
	return complete;
}

void OpenGL33Functions::glActiveTexture(std::uint32_t unit) const {
	OpenGLCall::invoke<void>(this->activeTexture, unit);
}

void OpenGL33Functions::glAttachShader(std::uint32_t program, std::uint32_t shader) const {
	OpenGLCall::invoke<void>(this->attachShader, program, shader);
}

void OpenGL33Functions::glBindBuffer(std::uint32_t target, std::uint32_t buffer) const {
	OpenGLCall::invoke<void>(this->bindBuffer, target, buffer);
}

void OpenGL33Functions::glBindSampler(std::uint32_t unit, std::uint32_t sampler) const {
	OpenGLCall::invoke<void>(this->bindSampler, unit, sampler);
}

void OpenGL33Functions::glBindVertexArray(std::uint32_t array) const {
	OpenGLCall::invoke<void>(this->bindVertexArray, array);
}

void OpenGL33Functions::glBlendEquationSeparate(std::uint32_t rgb, std::uint32_t alpha) const {
	OpenGLCall::invoke<void>(this->blendEquationSeparate, rgb, alpha);
}

void OpenGL33Functions::glBlendFuncSeparate(std::uint32_t sourceRgb, std::uint32_t destinationRgb, std::uint32_t sourceAlpha, std::uint32_t destinationAlpha) const {
	OpenGLCall::invoke<void>(this->blendFuncSeparate, sourceRgb, destinationRgb, sourceAlpha, destinationAlpha);
}

void OpenGL33Functions::glBufferData(std::uint32_t target, std::ptrdiff_t size, const void* data, std::uint32_t usage) const {
	OpenGLCall::invoke<void>(this->bufferData, target, size, data, usage);
}

void OpenGL33Functions::glCompileShader(std::uint32_t shader) const {
	OpenGLCall::invoke<void>(this->compileShader, shader);
}

std::uint32_t OpenGL33Functions::glCreateProgram() const {
	return OpenGLCall::invoke<std::uint32_t>(this->createProgram);
}

std::uint32_t OpenGL33Functions::glCreateShader(std::uint32_t type) const {
	return OpenGLCall::invoke<std::uint32_t>(this->createShader, type);
}

void OpenGL33Functions::glDeleteBuffers(std::int32_t count, const std::uint32_t* buffers) const {
	OpenGLCall::invoke<void>(this->deleteBuffers, count, buffers);
}

void OpenGL33Functions::glDeleteProgram(std::uint32_t program) const {
	OpenGLCall::invoke<void>(this->deleteProgram, program);
}

void OpenGL33Functions::glDeleteShader(std::uint32_t shader) const {
	OpenGLCall::invoke<void>(this->deleteShader, shader);
}

void OpenGL33Functions::glDeleteVertexArrays(std::int32_t count, const std::uint32_t* arrays) const {
	OpenGLCall::invoke<void>(this->deleteVertexArrays, count, arrays);
}

void OpenGL33Functions::glEnableVertexAttribArray(std::uint32_t index) const {
	OpenGLCall::invoke<void>(this->enableVertexAttribArray, index);
}

void OpenGL33Functions::glGenBuffers(std::int32_t count, std::uint32_t* buffers) const {
	OpenGLCall::invoke<void>(this->genBuffers, count, buffers);
}

void OpenGL33Functions::glGenVertexArrays(std::int32_t count, std::uint32_t* arrays) const {
	OpenGLCall::invoke<void>(this->genVertexArrays, count, arrays);
}

void OpenGL33Functions::glGetProgramiv(std::uint32_t program, std::uint32_t name, std::int32_t* value) const {
	OpenGLCall::invoke<void>(this->getProgramiv, program, name, value);
}

void OpenGL33Functions::glGetShaderiv(std::uint32_t shader, std::uint32_t name, std::int32_t* value) const {
	OpenGLCall::invoke<void>(this->getShaderiv, shader, name, value);
}

std::int32_t OpenGL33Functions::glGetUniformLocation(std::uint32_t program, const char* name) const {
	return OpenGLCall::invoke<std::int32_t>(this->getUniformLocation, program, name);
}

void OpenGL33Functions::glLinkProgram(std::uint32_t program) const {
	OpenGLCall::invoke<void>(this->linkProgram, program);
}

void OpenGL33Functions::glShaderSource(std::uint32_t shader, std::int32_t count, const char* const* strings, const std::int32_t* lengths) const {
	OpenGLCall::invoke<void>(this->shaderSource, shader, count, strings, lengths);
}

void OpenGL33Functions::glUniformMatrix4fv(std::int32_t location, std::int32_t count, std::uint8_t transpose, const float* values) const {
	OpenGLCall::invoke<void>(this->uniformMatrix4fv, location, count, transpose, values);
}

void OpenGL33Functions::glUseProgram(std::uint32_t program) const {
	OpenGLCall::invoke<void>(this->useProgram, program);
}

void OpenGL33Functions::glVertexAttribPointer(std::uint32_t index, std::int32_t size, std::uint32_t type, std::uint8_t normalized, std::int32_t stride, const void* pointer) const {
	OpenGLCall::invoke<void>(this->vertexAttribPointer, index, size, type, normalized, stride, pointer);
}

}
