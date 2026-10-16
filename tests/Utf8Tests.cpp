#include <litefont/Utf8.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <fstream>
#include <sstream>
#include <vector>

namespace litefont::tests {

class Utf8Tests {
	static std::string makeText(std::initializer_list<std::uint8_t> bytes) {
		std::string text;
		for (const std::uint8_t byte : bytes) {
			text.push_back(static_cast<char>(byte));
		}
		return text;
	}

	static std::vector<char32_t> decodeAll(std::string_view text) {
		std::vector<char32_t> values;
		Utf8 walker(text);
		CodePoint codePoint;
		while (walker.next(codePoint)) {
			values.push_back(codePoint.value);
		}
		return values;
	}

	static inline test::Test offsets{"Utf8: walks code points with their byte offsets and lengths", [](test::Context& context) {
		const std::string text = "aé€\U0001F600";
		Utf8 walker(text);
		CodePoint codePoint;
		const char32_t values[] = {U'a', 0xE9, 0x20AC, 0x1F600};
		const std::uint32_t starts[] = {0, 1, 3, 6};
		const std::uint8_t lengths[] = {1, 2, 3, 4};
		for (std::size_t i = 0; i < 4; i++) {
			context.check(walker.next(codePoint), "a code point");
			context.checkEqual(static_cast<std::uint32_t>(codePoint.value), static_cast<std::uint32_t>(values[i]));
			context.checkEqual(codePoint.offset, starts[i]);
			context.checkEqual(codePoint.length, lengths[i]);
		}
		context.check(!walker.next(codePoint), "then the end");
		context.checkEqual(codePoint.offset, std::uint32_t{10});
		context.check(!Utf8("").next(codePoint), "empty text has no code points");
	}};

	static inline test::Test boundaries{"Utf8: decodes the first and last code point of every length", [](test::Context& context) {
		context.check(decodeAll(makeText({0x00, 0x7F})) == std::vector<char32_t>{0x00, 0x7F}, "one byte");
		context.check(decodeAll(makeText({0xC2, 0x80, 0xDF, 0xBF})) == std::vector<char32_t>{0x80, 0x7FF}, "two bytes");
		context.check(decodeAll(makeText({0xE0, 0xA0, 0x80, 0xEF, 0xBF, 0xBF})) == std::vector<char32_t>{0x800, 0xFFFF}, "three bytes");
		context.check(decodeAll(makeText({0xF0, 0x90, 0x80, 0x80, 0xF4, 0x8F, 0xBF, 0xBF})) == std::vector<char32_t>{0x10000, 0x10FFFF}, "four bytes");
		context.check(decodeAll(makeText({0xED, 0x9F, 0xBF, 0xEE, 0x80, 0x80})) == std::vector<char32_t>{0xD7FF, 0xE000}, "around the surrogates");
	}};

	static inline test::Test illFormed{"Utf8: each ill-formed subpart becomes one U+FFFD", [](test::Context& context) {
		const char32_t r = 0xFFFD;
		context.check(decodeAll(makeText({0x80})) == std::vector<char32_t>{r}, "stray continuation byte");
		context.check(decodeAll(makeText({0xFE, 0xFF})) == std::vector<char32_t>{r, r}, "impossible bytes");
		context.check(decodeAll(makeText({0xC0, 0xAF})) == std::vector<char32_t>{r, r}, "overlong two-byte slash");
		context.check(decodeAll(makeText({0xE0, 0x80, 0xAF})) == std::vector<char32_t>{r, r, r}, "overlong three-byte slash");
		context.check(decodeAll(makeText({0xF0, 0x80, 0x80, 0xAF})) == std::vector<char32_t>{r, r, r, r}, "overlong four-byte slash");
		context.check(decodeAll(makeText({0xED, 0xA0, 0x80})) == std::vector<char32_t>{r, r, r}, "a surrogate");
		context.check(decodeAll(makeText({0xF4, 0x90, 0x80, 0x80})) == std::vector<char32_t>{r, r, r, r}, "past U+10FFFF");
		context.check(decodeAll(makeText({0xE2, 0x82})) == std::vector<char32_t>{r}, "a truncated sequence is one subpart");
		context.check(decodeAll(makeText({0xE2, 0x82, 0x41})) == std::vector<char32_t>{r, U'A'}, "and the next byte starts over");

		const std::string truncated = makeText({0xF0, 0x9F, 0x98, 0x41});
		Utf8 walker(truncated);
		CodePoint codePoint;
		walker.next(codePoint);
		context.checkEqual(codePoint.length, std::uint8_t{3});
		walker.next(codePoint);
		context.checkEqual(codePoint.offset, std::uint32_t{3});
	}};

	static inline test::Test roundTrips{"Utf8: encoding then decoding round-trips every code point", [](test::Context& context) {
		std::int32_t wrong = 0;
		std::array<char, 4> bytes{};
		for (char32_t value = 0; value <= 0x10FFFF; value++) {
			if (value >= 0xD800 && value <= 0xDFFF) {
				continue;
			}
			const std::uint8_t length = Utf8::encode(value, bytes);
			const std::string_view text(bytes.data(), length);
			Utf8 walker(text);
			CodePoint codePoint;
			const bool decoded = walker.next(codePoint);
			wrong += decoded && codePoint.value == value && codePoint.length == length && Utf8::isValid(text) ? 0 : 1;
		}
		context.checkEqual(wrong, 0);
		context.checkEqual(Utf8::encode(0xD800, bytes), std::uint8_t{3});
		context.check(static_cast<std::uint8_t>(bytes[0]) == 0xEF && static_cast<std::uint8_t>(bytes[1]) == 0xBF && static_cast<std::uint8_t>(bytes[2]) == 0xBD,
			"a surrogate encodes as U+FFFD");
		context.checkEqual(Utf8::encode(0x110000, bytes), std::uint8_t{3});
	}};

	static inline test::Test convertsU8{"Utf8: fromU8 and toU8 keep the bytes", [](test::Context& context) {
		const std::u8string wide = u8"abcé";
		const std::string narrow = Utf8::fromU8(wide);
		context.checkEqual(narrow.size(), std::size_t{5});
		context.check(static_cast<std::uint8_t>(narrow[3]) == 0xC3 && static_cast<std::uint8_t>(narrow[4]) == 0xA9, "the same UTF-8 bytes");
		context.check(Utf8::toU8(narrow) == wide, "and back");
	}};

	static inline test::Test validates{"Utf8: isValid accepts a real U+FFFD and rejects ill-formed bytes", [](test::Context& context) {
		context.check(Utf8::isValid("plain � text"), "U+FFFD itself is valid");
		context.check(!Utf8::isValid(makeText({0x61, 0xC0, 0x80})), "an overlong NUL is not");
		context.check(!Utf8::isValid(makeText({0xE2, 0x82})), "a truncated sequence is not");
	}};

	static inline test::Test python{"Utf8: 3,000 random byte strings decode as CPython decodes them", [](test::Context& context) {
		std::ifstream stream("tests/utf8-cases.txt");
		if (!stream) {
			context.note("tests/utf8-cases.txt not found; run the tests from the project folder");
			return;
		}
		std::string line;
		std::int32_t cases = 0;
		std::int32_t wrong = 0;
		while (std::getline(stream, line)) {
			const std::size_t colon = line.find(" : ");
			std::istringstream bytesText(line.substr(0, colon));
			std::istringstream valuesText(line.substr(colon + 3));
			std::string text;
			std::string token;
			while (bytesText >> token) {
				text.push_back(static_cast<char>(std::stoul(token, nullptr, 16)));
			}
			std::vector<char32_t> expected;
			while (valuesText >> token) {
				expected.push_back(static_cast<char32_t>(std::stoul(token, nullptr, 16)));
			}
			wrong += decodeAll(text) == expected ? 0 : 1;
			cases++;
		}
		context.checkEqual(cases, 3000);
		context.checkEqual(wrong, 0);
	}};
};

}
