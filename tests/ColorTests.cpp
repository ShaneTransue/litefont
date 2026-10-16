#include <litefont/ColorChannel.h>
#include <litefont/Color3.h>
#include <litefont/Color4.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <limits>

namespace litefont::tests {

class ColorTests {
	static inline test::Test defaultsToOpaqueBlack{"Color: defaults to opaque black in every channel type", [](test::Context& context) {
		context.checkEqual(Color4f().a, 1.0f);
		context.checkEqual(Color4u8().a, std::uint8_t{255});
		context.checkEqual(Color4i().a, std::int32_t{255});
		context.checkEqual(Color4f().r, 0.0f);
	}};

	static inline test::Test packsRedLowAndAlphaHigh{"Color: packs red in the low byte and alpha in the high byte", [](test::Context& context) {
		context.checkEqual(Color4f::fromRGB(Color3f::red()).packRGBA8(), std::uint32_t{0xFF'00'00'FFu});

		Color4f green;
		green.g = 1.0f;
		green.a = 0.0f;
		context.checkEqual(green.packRGBA8(), std::uint32_t{0x00'00'FF'00u});
	}};

	static inline test::Test roundsToNearestLevel{"Color: converting float to 8-bit rounds to the nearest level", [](test::Context& context) {
		Color4f color;
		color.r = 0.5f;               // 127.5 rounds up to 128
		color.g = 0.25f;              // 63.75 rounds up to 64
		color.b = 1.0f / 255.0f;      // 1
		color.a = 254.4f / 255.0f;    // 254.4 rounds down to 254
		context.checkEqual(color.packRGBA8(), std::uint32_t{0xFE'01'40'80u});   // alpha, blue, green, red
	}};

	static inline test::Test clampsOutOfRange{"Color: converting to integers clamps out-of-range and NaN channels", [](test::Context& context) {
		Color4f color;
		color.r = 2.0f;
		color.g = -1.0f;
		color.b = std::numeric_limits<float>::quiet_NaN();
		context.checkEqual(color.packRGBA8(), std::uint32_t{0xFF'00'00'FFu});

		Color3i wide{300, -20, 128};
		context.checkEqual(wide.convert<std::uint8_t>(), Color3u8{255, 0, 128});
	}};

	static inline test::Test convertsBetweenRanges{"Color: converts between channel ranges", [](test::Context& context) {
		const Color4u8 bytes{255, 0, 51, 255};
		const Color4f floats = bytes.convert<float>();
		context.checkEqual(floats.r, 1.0f);
		context.checkEqual(floats.g, 0.0f);
		context.checkEqual(floats.b, 0.2f);

		const Color3<std::uint16_t> deep{65535, 257, 0};
		context.checkEqual(deep.convert<std::uint8_t>(), Color3u8{255, 1, 0});
		context.checkEqual(Color3u8{255, 1, 0}.convert<std::uint16_t>(), deep);

		context.checkEqual(Color3i::white().convert<double>(), Color3d::white());
	}};

	static inline test::Test roundTripsEveryLevel{"Color: fromRGBA8 then packRGBA8 returns every level unchanged", [](test::Context& context) {
		for (std::uint32_t level = 0; level < 256; level++) {
			const std::uint32_t packed = level | ((255u - level) << 8) | ((level ^ 0x5Au) << 16) | (level << 24);
			context.checkEqual(Color4f::fromRGBA8(packed).packRGBA8(), packed);
			context.checkEqual(Color4u8::fromRGBA8(packed).packRGBA8(), packed);
		}
	}};

	static inline test::Test splitsAndJoinsAlpha{"Color: getRGB and fromRGB split and join the alpha channel", [](test::Context& context) {
		const Color4u8 color{10, 20, 30, 40};
		context.checkEqual(color.getRGB(), Color3u8{10, 20, 30});
		context.checkEqual(Color4u8::fromRGB(Color3u8{10, 20, 30}, 40), color);
	}};
};

}
