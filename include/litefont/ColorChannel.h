#pragma once

#include <cstddef>
#include <cstdint>

#include <type_traits>

namespace litefont {

/**
 * \struct ColorChannel ColorChannel.h <litefont/ColorChannel.h>
 * @addtogroup Core
 *
 * ColorChannel - The range of one color channel for a given storage type: a
 * channel runs from 0 to max. Floating-point channels run to 1, 8-bit channels
 * to 255. Specialize it to allow a new channel type in Color3 and Color4.
 */
template <typename T>
struct ColorChannel {
	static_assert(sizeof(T) == 0, "ColorChannel has no specialization for this channel type");
};

template <>
struct ColorChannel<std::uint8_t> {
	static constexpr std::uint8_t max = 255;
};

template <>
struct ColorChannel<std::uint16_t> {
	static constexpr std::uint16_t max = 65535;
};

template <>
struct ColorChannel<std::int32_t> {
	static constexpr std::int32_t max = 255;      // integer colors use the 0-255 range
};

template <>
struct ColorChannel<float> {
	static constexpr float max = 1.0f;
};

template <>
struct ColorChannel<double> {
	static constexpr double max = 1.0;
};

/**
 * \class ChannelConversion ColorChannel.h <litefont/ColorChannel.h>
 * @addtogroup Core
 *
 * ChannelConversion - Converts one channel value between storage types,
 * rescaling from one ColorChannel range to the other. Conversions to integer
 * channels clamp to the range and round to the nearest level; NaN becomes 0.
 */
class ChannelConversion {
public:
	template <typename To, typename From>
	static constexpr To convert(From value);
};

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------
template <typename To, typename From>
constexpr To ChannelConversion::convert(From value) {
	if constexpr (std::is_same_v<To, From>) {
		return value;
	}
	else if constexpr (std::is_floating_point_v<From> && std::is_floating_point_v<To>) {
		return static_cast<To>(value);
	}
	else if constexpr (std::is_floating_point_v<From>) {
		// Floating point to integer: clamp to the range, then round to the nearest level.
		if (!(value > From(0))) {
			return To(0);               // also catches NaN
		}
		if (value >= ColorChannel<From>::max) {
			return ColorChannel<To>::max;
		}
		return static_cast<To>(value * static_cast<From>(ColorChannel<To>::max) + From(0.5));
	}
	else if constexpr (std::is_floating_point_v<To>) {
		// Integer to floating point: scale the level into the floating range.
		return static_cast<To>(value) / static_cast<To>(ColorChannel<From>::max) * ColorChannel<To>::max;
	}
	else {
		// Integer to integer: rescale between the two ranges and round.
		const double scaled = static_cast<double>(value) * static_cast<double>(ColorChannel<To>::max)
			/ static_cast<double>(ColorChannel<From>::max);
		if (!(scaled > 0.0)) {
			return To(0);
		}
		if (scaled >= static_cast<double>(ColorChannel<To>::max)) {
			return ColorChannel<To>::max;
		}
		return static_cast<To>(scaled + 0.5);
	}
}

}
