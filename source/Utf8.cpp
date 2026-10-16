#include <litefont/Utf8.h>

namespace litefont {

Utf8::Utf8(std::string_view text) {
	this->text = text;
}

bool Utf8::next(CodePoint& codePoint) {
	codePoint = CodePoint{};
	codePoint.offset = this->offset;
	if (this->offset >= this->text.size()) {
		return false;
	}
	bool valid = false;
	codePoint.length = Utf8::decode(this->text, this->offset, codePoint.value, valid);
	this->offset += codePoint.length;
	return true;
}

std::uint8_t Utf8::encode(char32_t value, std::array<char, 4>& bytes) {
	if (value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
		value = 0xFFFD;
	}
	if (value < 0x80) {
		bytes[0] = static_cast<char>(value);
		return 1;
	}
	if (value < 0x800) {
		bytes[0] = static_cast<char>(0xC0 | (value >> 6));
		bytes[1] = static_cast<char>(0x80 | (value & 0x3F));
		return 2;
	}
	if (value < 0x10000) {
		bytes[0] = static_cast<char>(0xE0 | (value >> 12));
		bytes[1] = static_cast<char>(0x80 | ((value >> 6) & 0x3F));
		bytes[2] = static_cast<char>(0x80 | (value & 0x3F));
		return 3;
	}
	bytes[0] = static_cast<char>(0xF0 | (value >> 18));
	bytes[1] = static_cast<char>(0x80 | ((value >> 12) & 0x3F));
	bytes[2] = static_cast<char>(0x80 | ((value >> 6) & 0x3F));
	bytes[3] = static_cast<char>(0x80 | (value & 0x3F));
	return 4;
}

bool Utf8::isValid(std::string_view text) {
	std::size_t offset = 0;
	while (offset < text.size()) {
		char32_t value = 0;
		bool valid = false;
		offset += Utf8::decode(text, offset, value, valid);
		if (!valid) {
			return false;
		}
	}
	return true;
}

std::string Utf8::fromU8(std::u8string_view text) {
	// Byte by byte: char8_t may not alias char, so no reinterpret_cast.
	std::string result(text.size(), '\0');
	for (std::size_t i = 0; i < text.size(); i++) {
		result[i] = static_cast<char>(text[i]);
	}
	return result;
}

std::u8string Utf8::toU8(std::string_view text) {
	std::u8string result(text.size(), u8'\0');
	for (std::size_t i = 0; i < text.size(); i++) {
		result[i] = static_cast<char8_t>(text[i]);
	}
	return result;
}

std::uint8_t Utf8::decode(std::string_view text, std::size_t offset, char32_t& value, bool& valid) {
	const std::uint8_t lead = static_cast<std::uint8_t>(text[offset]);
	value = 0xFFFD;
	valid = false;
	if (lead < 0x80) {
		value = lead;
		valid = true;
		return 1;
	}

	// Well-formed sequences (Unicode table 3-7): the lead byte fixes the
	// length and the range of the second byte, which rules out overlong
	// forms, surrogates and values past U+10FFFF.
	std::uint8_t length = 0;
	std::uint8_t secondLow = 0x80;
	std::uint8_t secondHigh = 0xBF;
	char32_t bits = 0;
	if (lead >= 0xC2 && lead <= 0xDF) {
		length = 2;
		bits = lead & 0x1F;
	}
	else if (lead >= 0xE0 && lead <= 0xEF) {
		length = 3;
		bits = lead & 0x0F;
		secondLow = lead == 0xE0 ? 0xA0 : 0x80;
		secondHigh = lead == 0xED ? 0x9F : 0xBF;
	}
	else if (lead >= 0xF0 && lead <= 0xF4) {
		length = 4;
		bits = lead & 0x07;
		secondLow = lead == 0xF0 ? 0x90 : 0x80;
		secondHigh = lead == 0xF4 ? 0x8F : 0xBF;
	}
	else {
		return 1;
	}

	// Take continuation bytes while they fit; the first that does not is left
	// for the next code point, so one ill-formed subpart gives one U+FFFD.
	std::uint8_t taken = 1;
	while (taken < length && offset + taken < text.size()) {
		const std::uint8_t byte = static_cast<std::uint8_t>(text[offset + taken]);
		const std::uint8_t low = taken == 1 ? secondLow : 0x80;
		const std::uint8_t high = taken == 1 ? secondHigh : 0xBF;
		if (byte < low || byte > high) {
			break;
		}
		bits = (bits << 6) | (byte & 0x3F);
		taken++;
	}
	if (taken == length) {
		value = bits;
		valid = true;
	}
	return taken;
}

}
