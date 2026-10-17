#pragma once

#include <litefont/OpenGLDevice.h>
#include <litefont/OpenGLFunctions.h>

namespace litefont {

/**
 * \class OpenGL11Device OpenGL11Device.h <litefont/OpenGL11Device.h>
 * @addtogroup Graphics
 *
 * OpenGL11Device - An OpenGLDevice on OpenGL 1.1 fixed function, for
 * compatibility contexts of any version: it draws next to glBegin and glEnd
 * code. Coverage atlases are alpha textures modulated by the vertex color
 * and blended with straight alpha, the same math as SoftwareDevice; vertices
 * come from client-side arrays.
 *
 * Drawing sets what text needs: the projection and model-view for the space
 * (an orthographic projection over the viewport by default), an identity
 * texture matrix, blending and 2D texturing on, depth test, lighting, face
 * culling, alpha test, fog, stencil test and texture coordinate generation
 * off, polygons filled and smoothly shaded (so gradients blend across each
 * quad), the atlas texture bound with the modulate
 * environment, the vertex, texture coordinate and color arrays, and the
 * scissor rectangle for clips. By default it saves the program's state first
 * and restores it afterwards (glPushAttrib, glPushClientAttrib and the three
 * matrix stacks); with setRestoreState(false) it skips that work and leaves
 * the state as drawing set it, apart from turning its own client arrays off
 * again. It expects texture unit 0 to be active, no shader program and no
 * array buffer bound, as in a fixed-function program; a program that draws
 * with shaders wants OpenGL33Device.
 *
 * useMatrixStack() draws through whatever the program last set on its
 * projection and model-view stacks (glOrtho, glTranslatef, glRotatef,
 * glScalef...), with the placement multiplied onto the model-view; that
 * multiplication is always undone, whatever setRestoreState says.
 */
class OpenGL11Device : public OpenGLDevice {
public:
	void updateTexture(TextureHandle texture, const Image& source, const Recti& region) override;
	void drawSprites(GeometryHandle geometry, std::span<const SpriteBatch> batches) override;
	void release() override;

	void useMatrixStack();

protected:
	const OpenGLFunctions* getFunctions() const override;
	bool getStackMatrix(Matrix4f& matrix) const override;

private:
	mutable OpenGL11Functions gl;
};

}
