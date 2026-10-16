#pragma once

#include <cstddef>
#include <cstdint>

#include <cmath>
#include <concepts>

namespace litefont {

/**
 * \struct Vector2 Vector2.h <litefont/Vector2.h>
 * @addtogroup Math
 *
 * Vector2 - A 2D vector or point, templated on its scalar type. Arithmetic is
 * component-wise; scalar * vector works through a hidden friend.
 */
template <typename T>
struct Vector2 {
	T x = T(0);
	T y = T(0);

	Vector2 operator+(const Vector2& other) const;
	Vector2 operator-(const Vector2& other) const;
	Vector2 operator-() const;
	Vector2 operator*(T scale) const;
	Vector2 operator/(T scale) const;
	Vector2& operator+=(const Vector2& other);
	Vector2& operator-=(const Vector2& other);
	bool operator==(const Vector2&) const = default;

	T dot(const Vector2& other) const;
	T cross(const Vector2& other) const;
	T getLengthSquared() const;
	T getLength() const requires std::floating_point<T>;

	template <typename U>
	Vector2<U> convert() const;

	friend Vector2 operator*(T scale, const Vector2& vector) {
		return vector * scale;
	}
};

using Vector2f = Vector2<float>;
using Vector2d = Vector2<double>;
using Vector2i = Vector2<std::int32_t>;

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------
template <typename T>
Vector2<T> Vector2<T>::operator+(const Vector2& other) const {
	return Vector2{this->x + other.x, this->y + other.y};
}

template <typename T>
Vector2<T> Vector2<T>::operator-(const Vector2& other) const {
	return Vector2{this->x - other.x, this->y - other.y};
}

template <typename T>
Vector2<T> Vector2<T>::operator-() const {
	return Vector2{-this->x, -this->y};
}

template <typename T>
Vector2<T> Vector2<T>::operator*(T scale) const {
	return Vector2{this->x * scale, this->y * scale};
}

template <typename T>
Vector2<T> Vector2<T>::operator/(T scale) const {
	return Vector2{this->x / scale, this->y / scale};
}

template <typename T>
Vector2<T>& Vector2<T>::operator+=(const Vector2& other) {
	this->x += other.x;
	this->y += other.y;
	return *this;
}

template <typename T>
Vector2<T>& Vector2<T>::operator-=(const Vector2& other) {
	this->x -= other.x;
	this->y -= other.y;
	return *this;
}

template <typename T>
T Vector2<T>::dot(const Vector2& other) const {
	return this->x * other.x + this->y * other.y;
}

template <typename T>
T Vector2<T>::cross(const Vector2& other) const {
	return this->x * other.y - this->y * other.x;
}

template <typename T>
T Vector2<T>::getLengthSquared() const {
	return this->dot(*this);
}

template <typename T>
T Vector2<T>::getLength() const requires std::floating_point<T> {
	return std::sqrt(this->getLengthSquared());
}

template <typename T>
template <typename U>
Vector2<U> Vector2<T>::convert() const {
	return Vector2<U>{static_cast<U>(this->x), static_cast<U>(this->y)};
}

}
