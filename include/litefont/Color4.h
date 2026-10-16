#pragma once

#include <litefont/Color3.h>

namespace litefont {

/**
 * \struct Color4 Color4.h <litefont/Color4.h>
 * @addtogroup Core
 *
 * Color4 - A red, green, blue, alpha color with straight (not premultiplied)
 * alpha, opaque by default. The channel type sets the range through
 * ColorChannel. Packs to 8 bits per channel with red in bits 0-7 and alpha in
 * bits 24-31, which is R, G, B, A in memory on little-endian machines.
 */
template <typename T>
struct Color4 {
	T r = T(0);
	T g = T(0);
	T b = T(0);
	T a = ColorChannel<T>::max;

	template <typename U>
	Color4<U> convert() const;

	Color3<T> getRGB() const;
	std::uint32_t packRGBA8() const;

	bool operator==(const Color4&) const = default;

	static Color4 fromRGB(const Color3<T>& rgb, T alpha = ColorChannel<T>::max);
	static Color4 fromRGBA8(std::uint32_t rgba);
};

template <typename T>
using Color = Color4<T>;

using Color4f = Color4<float>;
using Color4d = Color4<double>;
using Color4i = Color4<std::int32_t>;
using Color4u8 = Color4<std::uint8_t>;

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------
template <typename T>
template <typename U>
Color4<U> Color4<T>::convert() const {
	Color4<U> color;
	color.r = ChannelConversion::convert<U>(this->r);
	color.g = ChannelConversion::convert<U>(this->g);
	color.b = ChannelConversion::convert<U>(this->b);
	color.a = ChannelConversion::convert<U>(this->a);
	return color;
}

template <typename T>
Color3<T> Color4<T>::getRGB() const {
	return Color3<T>{this->r, this->g, this->b};
}

template <typename T>
std::uint32_t Color4<T>::packRGBA8() const {
	const Color4<std::uint8_t> bytes = this->template convert<std::uint8_t>();
	return std::uint32_t{bytes.r}
		| (std::uint32_t{bytes.g} << 8)
		| (std::uint32_t{bytes.b} << 16)
		| (std::uint32_t{bytes.a} << 24);
}

template <typename T>
Color4<T> Color4<T>::fromRGB(const Color3<T>& rgb, T alpha) {
	return Color4{rgb.r, rgb.g, rgb.b, alpha};
}

template <typename T>
Color4<T> Color4<T>::fromRGBA8(std::uint32_t rgba) {
	Color4<std::uint8_t> bytes;
	bytes.r = static_cast<std::uint8_t>(rgba & 0xFFu);
	bytes.g = static_cast<std::uint8_t>((rgba >> 8) & 0xFFu);
	bytes.b = static_cast<std::uint8_t>((rgba >> 16) & 0xFFu);
	bytes.a = static_cast<std::uint8_t>((rgba >> 24) & 0xFFu);
	return bytes.template convert<T>();
}

}
