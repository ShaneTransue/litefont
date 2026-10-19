#include <litefont/GraphicsDevice.h>

#include <algorithm>
#include <cmath>

namespace litefont {

void GraphicsDevice::release() {
}

void GraphicsDevice::useViewport() {
	this->space = SpriteSpace::Viewport;
}

void GraphicsDevice::setMatrix(const Matrix4f& matrix) {
	this->matrix = matrix;
	this->space = SpriteSpace::Matrix;
}

SpriteSpace GraphicsDevice::getSpace() const {
	return this->space;
}

const Matrix4f& GraphicsDevice::getMatrix() const {
	return this->matrix;
}

void GraphicsDevice::setYUp(bool yUp) {
	this->yUp = yUp;
}

bool GraphicsDevice::isYUp() const {
	return this->yUp;
}

void GraphicsDevice::setPlacement(const Affine2f& placement) {
	this->placement = placement;
}

const Affine2f& GraphicsDevice::getPlacement() const {
	return this->placement;
}

float GraphicsDevice::getPixelScale(Vector2f point) const {
	// The longer of the two axes a unit square at the point becomes in
	// viewport pixels, from the derivative of the projection there
	// (perspective too).
	if (this->space == SpriteSpace::Viewport) {
		return 1.0f;
	}
	Matrix4f transform = this->matrix;
	if (this->space == SpriteSpace::MatrixStack && !this->getStackMatrix(transform)) {
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

bool GraphicsDevice::getStackMatrix(Matrix4f&) const {
	return false;
}

Matrix4f GraphicsDevice::getSpriteTransform(const Recti& viewport) const {
	// Sprite coordinates to clip space: the placement, then the program's
	// matrix, or pixels from the viewport's top-left corner with y down.
	const Matrix4f space = this->space == SpriteSpace::Matrix ? this->matrix
		: Matrix4f::ortho(0.0f, static_cast<float>(viewport.width), static_cast<float>(viewport.height), 0.0f, -1.0f, 1.0f);
	return space * Matrix4f::fromAffine(this->placement);
}

}
