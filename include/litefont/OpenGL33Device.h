#pragma once

#include <litefont/OpenGLDevice.h>
#include <litefont/OpenGLFunctions.h>

namespace litefont {

/**
 * \class OpenGL33Device OpenGL33Device.h <litefont/OpenGL33Device.h>
 * @addtogroup Graphics
 *
 * OpenGL33Device - An OpenGLDevice on OpenGL 3.3, for core profiles and for
 * compatibility contexts of version 3.3 or newer. It draws with one small
 * shader program (GLSL 3.30) from one vertex array and a vertex and index
 * buffer per geometry; coverage atlases are one-channel (R8) textures read as
 * alpha, so it blends exactly as OpenGL11Device and SoftwareDevice do. Core
 * profiles have no matrix stack: draw through a matrix with setMatrix().
 *
 * Drawing sets what text needs: its program, vertex array and buffers,
 * texture unit 0 active with the atlas bound and no sampler object, blending
 * on with straight alpha, depth test, face culling and stencil test off,
 * polygons filled, and the scissor rectangle for clips. By default it saves
 * each of those first and restores it afterwards, as glGet calls (core
 * profiles have no attribute stack); uploads also save and restore the
 * texture binding, the unpack settings and any pixel unpack buffer. With
 * setRestoreState(false) it leaves the state as drawing set it, apart from
 * unbinding its vertex array. getError() reports a shader that did not
 * compile or link.
 */
class OpenGL33Device : public OpenGLDevice {
public:
	void updateTexture(TextureHandle texture, const Image& source, const Recti& region) override;
	void drawSprites(GeometryHandle geometry, std::span<const SpriteBatch> batches) override;
	Image readPixels() const override;
	void release() override;

protected:
	const OpenGLFunctions* getFunctions() const override;

private:
	bool prepareProgram();
	std::uint32_t compileShader(std::uint32_t type, const char* source);

	mutable OpenGL33Functions gl;
	std::uint32_t program = 0;
	std::uint32_t vertexArray = 0;
	std::int32_t transformLocation = -1;
	bool programFailed = false;
};

}
