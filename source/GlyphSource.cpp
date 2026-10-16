#include <litefont/GlyphSource.h>
#include <litefont/Hash.h>

#include <array>

namespace litefont {

std::uint64_t GlyphKey::getHash() const {
	// Every field, little-endian, so the hash is the same on every platform.
	std::array<std::uint8_t, 20> bytes{};
	for (std::size_t i = 0; i < 8; i++) {
		bytes[i] = static_cast<std::uint8_t>(this->font >> (8 * i));
	}
	bytes[8] = static_cast<std::uint8_t>(this->glyph);
	bytes[9] = static_cast<std::uint8_t>(this->glyph >> 8);
	for (std::size_t i = 0; i < 4; i++) {
		bytes[10 + i] = static_cast<std::uint8_t>(this->sizePx64 >> (8 * i));
	}
	bytes[14] = this->subpixelBin;
	bytes[15] = static_cast<std::uint8_t>((this->simulatedBold ? 1 : 0) | (this->simulatedItalic ? 2 : 0));
	bytes[16] = static_cast<std::uint8_t>(this->outlineWidth16);
	bytes[17] = static_cast<std::uint8_t>(this->outlineWidth16 >> 8);
	bytes[18] = static_cast<std::uint8_t>(this->blurRadius16);
	bytes[19] = static_cast<std::uint8_t>(this->blurRadius16 >> 8);
	return Hash::fnv1a(bytes);
}

}
