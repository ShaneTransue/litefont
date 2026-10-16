#pragma once

#include <string>
#include <string_view>
#include <cstddef>
#include <cstdint>

#include <array>

namespace litefont {

/**
 * \struct CodePoint Utf8.h <litefont/Utf8.h>
 * @addtogroup Text
 *
 * CodePoint - One decoded code point and the bytes it came from: offset is
 * where its sequence starts in the text, length how many bytes it took.
 */
struct CodePoint {
	char32_t value = 0;
	std::uint32_t offset = 0;
	std::uint8_t length = 0;
};

/**
 * \class Utf8 Utf8.h <litefont/Utf8.h>
 * @addtogroup Text
 *
 * Utf8 - Walks the code points of UTF-8 text and encodes code points back.
 * Ill-formed input never stops the walk: each maximal ill-formed subpart
 * (a bad lead byte, a stray continuation byte, a truncated, overlong or
 * surrogate sequence, or one past U+10FFFF) becomes one U+FFFD, the way the
 * Unicode standard recommends and Python, browsers and ICU decode. Offsets
 * are 32-bit, so texts stay under 4 GB.
 *
 * A Utf8 walker views its text without copying it, like std::string_view: the text
 * must outlive the walker, so never construct one from a temporary string.
 * fromU8 and toU8 convert to and from std::u8string for code that uses it.
 */
class Utf8 {
public:
	explicit Utf8(std::string_view text);

	bool next(CodePoint& codePoint);

	static std::uint8_t encode(char32_t value, std::array<char, 4>& bytes);
	static bool isValid(std::string_view text);
	static std::string fromU8(std::u8string_view text);
	static std::u8string toU8(std::string_view text);

private:
	static std::uint8_t decode(std::string_view text, std::size_t offset, char32_t& value, bool& valid);

	std::string_view text;
	std::uint32_t offset = 0;
};

}
