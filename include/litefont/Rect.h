#pragma once

#include <litefont/Vector2.h>

#include <algorithm>

namespace litefont {

/**
 * \struct Rect Rect.h <litefont/Rect.h>
 * @addtogroup Math
 *
 * Rect - An axis-aligned rectangle: x, y is the corner with the smallest
 * coordinates. The left and top edges are inside, the right and bottom edges
 * outside, so rectangles that share an edge do not overlap.
 */
template <typename T>
struct Rect {
	T x = T(0);
	T y = T(0);
	T width = T(0);
	T height = T(0);

	T getRight() const;
	T getBottom() const;
	bool isEmpty() const;
	bool contains(const Vector2<T>& point) const;
	Rect united(const Rect& other) const;
	Rect intersected(const Rect& other) const;
	bool operator==(const Rect&) const = default;

	template <typename U>
	Rect<U> convert() const;

	static Rect fromCorners(const Vector2<T>& first, const Vector2<T>& second);
};

using Rectf = Rect<float>;
using Recti = Rect<std::int32_t>;

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------
template <typename T>
T Rect<T>::getRight() const {
	return this->x + this->width;
}

template <typename T>
T Rect<T>::getBottom() const {
	return this->y + this->height;
}

template <typename T>
bool Rect<T>::isEmpty() const {
	return !(this->width > T(0)) || !(this->height > T(0));
}

template <typename T>
bool Rect<T>::contains(const Vector2<T>& point) const {
	return point.x >= this->x && point.x < this->getRight()
		&& point.y >= this->y && point.y < this->getBottom();
}

template <typename T>
Rect<T> Rect<T>::united(const Rect& other) const {
	if (this->isEmpty()) {
		return other;
	}
	if (other.isEmpty()) {
		return *this;
	}
	const T left = std::min(this->x, other.x);
	const T top = std::min(this->y, other.y);
	const T right = std::max(this->getRight(), other.getRight());
	const T bottom = std::max(this->getBottom(), other.getBottom());
	return Rect{left, top, right - left, bottom - top};
}

template <typename T>
Rect<T> Rect<T>::intersected(const Rect& other) const {
	const T left = std::max(this->x, other.x);
	const T top = std::max(this->y, other.y);
	const T right = std::min(this->getRight(), other.getRight());
	const T bottom = std::min(this->getBottom(), other.getBottom());
	if (!(right > left) || !(bottom > top)) {
		return Rect{};
	}
	return Rect{left, top, right - left, bottom - top};
}

template <typename T>
template <typename U>
Rect<U> Rect<T>::convert() const {
	return Rect<U>{static_cast<U>(this->x), static_cast<U>(this->y),
		static_cast<U>(this->width), static_cast<U>(this->height)};
}

template <typename T>
Rect<T> Rect<T>::fromCorners(const Vector2<T>& first, const Vector2<T>& second) {
	const T left = std::min(first.x, second.x);
	const T top = std::min(first.y, second.y);
	return Rect{left, top, std::max(first.x, second.x) - left, std::max(first.y, second.y) - top};
}

}
