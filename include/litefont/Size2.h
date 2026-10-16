#pragma once

#include <cstddef>
#include <cstdint>

namespace litefont {

/**
 * \struct Size2 Size2.h <litefont/Size2.h>
 * @addtogroup Math
 *
 * Size2 - A width and a height, templated on the scalar type.
 */
template <typename T>
struct Size2 {
	T width = T(0);
	T height = T(0);

	bool isEmpty() const;
	bool operator==(const Size2&) const = default;

	template <typename U>
	Size2<U> convert() const;
};

using Size2f = Size2<float>;
using Size2i = Size2<std::int32_t>;

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------
template <typename T>
bool Size2<T>::isEmpty() const {
	return !(this->width > T(0)) || !(this->height > T(0));
}

template <typename T>
template <typename U>
Size2<U> Size2<T>::convert() const {
	return Size2<U>{static_cast<U>(this->width), static_cast<U>(this->height)};
}

}
