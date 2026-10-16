#include <litefont/Hash.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <string_view>
#include <vector>

namespace litefont::tests {

class HashTests {
	static inline test::Test matchesFnv1aVectors{"Hash: fnv1a matches the published 64-bit test vectors", [](test::Context& context) {
		auto hashOf = [](std::string_view text) {
			const std::vector<std::uint8_t> bytes(text.begin(), text.end());
			return Hash::fnv1a(bytes);
		};

		context.checkEqual(hashOf(""), std::uint64_t{0xcbf29ce484222325ull});
		context.checkEqual(hashOf("a"), std::uint64_t{0xaf63dc4c8601ec8cull});
		context.checkEqual(hashOf("foobar"), std::uint64_t{0x85944171f73967e8ull});
	}};
};

}
