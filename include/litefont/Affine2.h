#pragma once

#include <litefont/Vector2.h>

namespace litefont {

/**
 * \struct Affine2 Affine2.h <litefont/Affine2.h>
 * @addtogroup Math
 *
 * Affine2 - A 2D affine transform in the same order as SVG's matrix(a b c d tx ty):
 *     x' = a * x + c * y + tx
 *     y' = b * x + d * y + ty
 * then(next) returns the transform that applies this one first and next second.
 */
template <typename T>
struct Affine2 {
	T a = T(1);
	T b = T(0);
	T c = T(0);
	T d = T(1);
	T tx = T(0);
	T ty = T(0);

	Vector2<T> apply(const Vector2<T>& point) const;
	Affine2 then(const Affine2& next) const;
	bool isIdentity() const;
	bool operator==(const Affine2&) const = default;

	static Affine2 identity();
	static Affine2 scale(T sx, T sy);
	static Affine2 translate(T x, T y);
};

using Affine2f = Affine2<float>;
using Affine2d = Affine2<double>;

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------
template <typename T>
Vector2<T> Affine2<T>::apply(const Vector2<T>& point) const {
	return Vector2<T>{
		this->a * point.x + this->c * point.y + this->tx,
		this->b * point.x + this->d * point.y + this->ty};
}

template <typename T>
Affine2<T> Affine2<T>::then(const Affine2& next) const {
	Affine2 result;
	result.a = next.a * this->a + next.c * this->b;
	result.b = next.b * this->a + next.d * this->b;
	result.c = next.a * this->c + next.c * this->d;
	result.d = next.b * this->c + next.d * this->d;
	result.tx = next.a * this->tx + next.c * this->ty + next.tx;
	result.ty = next.b * this->tx + next.d * this->ty + next.ty;
	return result;
}

template <typename T>
bool Affine2<T>::isIdentity() const {
	return *this == Affine2::identity();
}

template <typename T>
Affine2<T> Affine2<T>::identity() {
	return Affine2{};
}

template <typename T>
Affine2<T> Affine2<T>::scale(T sx, T sy) {
	Affine2 result;
	result.a = sx;
	result.d = sy;
	return result;
}

template <typename T>
Affine2<T> Affine2<T>::translate(T x, T y) {
	Affine2 result;
	result.tx = x;
	result.ty = y;
	return result;
}

}
