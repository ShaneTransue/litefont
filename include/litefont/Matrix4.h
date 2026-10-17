#pragma once

#include <litefont/Affine2.h>

#include <array>
#include <cmath>
#include <cstddef>

namespace litefont {

/**
 * \struct Matrix4 Matrix4.h <litefont/Matrix4.h>
 * @addtogroup Math
 *
 * Matrix4 - A 4 x 4 matrix in OpenGL's order: column-major, so values[column
 * * 4 + row], the layout glLoadMatrixf, glUniformMatrix4fv and glm's
 * value_ptr use. Points are columns multiplied on the right, so a * b applies
 * b first and a second. ortho() is glOrtho's matrix.
 */
template <typename T>
struct Matrix4 {
	std::array<T, 16> values{
		T(1), T(0), T(0), T(0),
		T(0), T(1), T(0), T(0),
		T(0), T(0), T(1), T(0),
		T(0), T(0), T(0), T(1)};

	Matrix4 operator*(const Matrix4& other) const;
	bool operator==(const Matrix4&) const = default;

	T get(std::size_t row, std::size_t column) const;
	const T* getData() const;

	static Matrix4 identity();
	static Matrix4 fromColumnMajor(const T* values);
	static Matrix4 fromAffine(const Affine2<T>& affine);
	static Matrix4 ortho(T left, T right, T bottom, T top, T zNear, T zFar);
	static Matrix4 translate(T x, T y, T z);
	static Matrix4 scale(T x, T y, T z);
	static Matrix4 rotateZ(T radians);
};

using Matrix4f = Matrix4<float>;
using Matrix4d = Matrix4<double>;

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------
template <typename T>
Matrix4<T> Matrix4<T>::operator*(const Matrix4& other) const {
	Matrix4 result;
	for (std::size_t column = 0; column < 4; column++) {
		for (std::size_t row = 0; row < 4; row++) {
			T sum = T(0);
			for (std::size_t k = 0; k < 4; k++) {
				sum += this->values[k * 4 + row] * other.values[column * 4 + k];
			}
			result.values[column * 4 + row] = sum;
		}
	}
	return result;
}

template <typename T>
T Matrix4<T>::get(std::size_t row, std::size_t column) const {
	return this->values[column * 4 + row];
}

template <typename T>
const T* Matrix4<T>::getData() const {
	return this->values.data();
}

template <typename T>
Matrix4<T> Matrix4<T>::identity() {
	return Matrix4{};
}

template <typename T>
Matrix4<T> Matrix4<T>::fromColumnMajor(const T* values) {
	Matrix4 result;
	for (std::size_t i = 0; i < 16; i++) {
		result.values[i] = values[i];
	}
	return result;
}

template <typename T>
Matrix4<T> Matrix4<T>::fromAffine(const Affine2<T>& affine) {
	// x' = a x + c y + tx and y' = b x + d y + ty, with z and w passed through.
	Matrix4 result;
	result.values[0] = affine.a;
	result.values[1] = affine.b;
	result.values[4] = affine.c;
	result.values[5] = affine.d;
	result.values[12] = affine.tx;
	result.values[13] = affine.ty;
	return result;
}

template <typename T>
Matrix4<T> Matrix4<T>::ortho(T left, T right, T bottom, T top, T zNear, T zFar) {
	Matrix4 result;
	result.values[0] = T(2) / (right - left);
	result.values[5] = T(2) / (top - bottom);
	result.values[10] = T(-2) / (zFar - zNear);
	result.values[12] = -(right + left) / (right - left);
	result.values[13] = -(top + bottom) / (top - bottom);
	result.values[14] = -(zFar + zNear) / (zFar - zNear);
	return result;
}

template <typename T>
Matrix4<T> Matrix4<T>::translate(T x, T y, T z) {
	Matrix4 result;
	result.values[12] = x;
	result.values[13] = y;
	result.values[14] = z;
	return result;
}

template <typename T>
Matrix4<T> Matrix4<T>::scale(T x, T y, T z) {
	Matrix4 result;
	result.values[0] = x;
	result.values[5] = y;
	result.values[10] = z;
	return result;
}

template <typename T>
Matrix4<T> Matrix4<T>::rotateZ(T radians) {
	// Counterclockwise when y is up, the way glRotate turns about +z.
	const T cosine = std::cos(radians);
	const T sine = std::sin(radians);
	Matrix4 result;
	result.values[0] = cosine;
	result.values[1] = sine;
	result.values[4] = -sine;
	result.values[5] = cosine;
	return result;
}

}
