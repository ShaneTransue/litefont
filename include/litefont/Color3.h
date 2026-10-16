#pragma once

#include <litefont/ColorChannel.h>

namespace litefont {

/**
 * \struct Color3 Color3.h <litefont/Color3.h>
 * @addtogroup Core
 *
 * Color3 - A red, green, blue color whose channel type sets its range through
 * ColorChannel: Color3<float> runs from 0 to 1, Color3<std::uint8_t> from 0 to 255.
 * convert<U>() rescales to another channel type.
 */
template <typename T>
struct Color3 {
	T r = T(0);
	T g = T(0);
	T b = T(0);

	template <typename U>
	Color3<U> convert() const;

	bool operator==(const Color3&) const = default;

	static constexpr Color3 black();
	static constexpr Color3 white();
	static constexpr Color3 red();
	static constexpr Color3 green();
	static constexpr Color3 blue();
};

using Color3f = Color3<float>;
using Color3d = Color3<double>;
using Color3i = Color3<std::int32_t>;
using Color3u8 = Color3<std::uint8_t>;

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------
template <typename T>
template <typename U>
Color3<U> Color3<T>::convert() const {
	Color3<U> color;
	color.r = ChannelConversion::convert<U>(this->r);
	color.g = ChannelConversion::convert<U>(this->g);
	color.b = ChannelConversion::convert<U>(this->b);
	return color;
}

template <typename T>
constexpr Color3<T> Color3<T>::black() {
	return Color3{T(0), T(0), T(0)};
}

template <typename T>
constexpr Color3<T> Color3<T>::white() {
	constexpr T max = ColorChannel<T>::max;
	return Color3{max, max, max};
}

template <typename T>
constexpr Color3<T> Color3<T>::red() {
	return Color3{ColorChannel<T>::max, T(0), T(0)};
}

template <typename T>
constexpr Color3<T> Color3<T>::green() {
	return Color3{T(0), ColorChannel<T>::max, T(0)};
}

template <typename T>
constexpr Color3<T> Color3<T>::blue() {
	return Color3{T(0), T(0), ColorChannel<T>::max};
}

}
