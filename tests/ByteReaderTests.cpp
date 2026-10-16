#include <litefont/Bytes.h>
#include <litefont/ByteReader.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <array>
#include <limits>
#include <random>
#include <vector>

namespace litefont::tests {

class ByteReaderTests {
	static inline test::Test readsInSequence{"ByteReader: reads big-endian values in sequence", [](test::Context& context) {
		const Bytes bytes{{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE}};
		ByteReader reader(bytes.data);

		std::uint8_t first = 0;
		std::uint16_t second = 0;
		std::uint32_t third = 0;
		context.check(reader.read(first) && reader.read(second) && reader.read(third), "all three reads succeed");
		context.checkEqual(first, std::uint8_t{0x12});
		context.checkEqual(second, std::uint16_t{0x3456});
		context.checkEqual(third, std::uint32_t{0x789ABCDEu});
		context.checkEqual(reader.getPosition(), std::size_t{7});
		context.checkEqual(reader.getRemaining(), std::size_t{0});
	}};

	static inline test::Test readsSigned{"ByteReader: reads signed values as two's complement", [](test::Context& context) {
		const std::array<std::uint8_t, 7> bytes{0xFF, 0xFF, 0xFE, 0x80, 0x00, 0x00, 0x00};
		ByteReader reader(bytes);

		std::int8_t minusOne = 0;
		std::int16_t minusTwo = 0;
		std::int32_t minimum = 0;
		reader.read(minusOne);
		reader.read(minusTwo);
		reader.read(minimum);
		context.check(reader.isGood(), "reader is still good");
		context.checkEqual(minusOne, std::int8_t{-1});
		context.checkEqual(minusTwo, std::int16_t{-2});
		context.checkEqual(minimum, std::numeric_limits<std::int32_t>::min());
	}};

	static inline test::Test failureIsSticky{"ByteReader: a failed read zeroes the value and fails every later call", [](test::Context& context) {
		const std::array<std::uint8_t, 3> bytes{1, 2, 3};
		ByteReader reader(bytes);

		std::uint16_t value = 0;
		context.check(reader.read(value), "first two bytes");
		std::uint16_t pastEnd = 0xBEEF;
		context.check(!reader.read(pastEnd), "u16 straddling the end fails");
		context.checkEqual(pastEnd, std::uint16_t{0});
		context.check(!reader.isGood(), "reader is no longer good");

		std::uint8_t lastByte = 0;
		context.check(!reader.read(lastByte), "a read that would fit still fails after a failure");
		context.check(!reader.seek(0), "seek fails after a failure");
	}};

	static inline test::Test seeksAndSkips{"ByteReader: seek and skip move within the view and fail beyond it", [](test::Context& context) {
		const std::array<std::uint8_t, 8> bytes{0, 1, 2, 3, 4, 5, 6, 7};
		ByteReader reader(bytes);

		std::uint8_t value = 0;
		context.check(reader.seek(5) && reader.read(value), "seek then read");
		context.checkEqual(value, std::uint8_t{5});
		context.check(reader.seek(1) && reader.skip(2) && reader.read(value), "seek, skip, read");
		context.checkEqual(value, std::uint8_t{3});
		context.check(reader.seek(8), "seeking to the end is allowed");

		ByteReader beyond(bytes);
		context.check(!beyond.seek(9), "seeking past the end fails");
		ByteReader overskip(bytes);
		context.check(!overskip.skip(9), "skipping past the end fails");
	}};

	static inline test::Test survivesOverflowingRanges{"ByteReader: offsets and sizes near the maximum never wrap around", [](test::Context& context) {
		const std::array<std::uint8_t, 8> bytes{};
		constexpr std::size_t maximum = std::numeric_limits<std::size_t>::max();

		ByteReader seeker(bytes);
		context.check(!seeker.seek(maximum), "seek to the maximum");
		ByteReader skipper(bytes);
		skipper.skip(4);
		context.check(!skipper.skip(maximum - 1), "skip that would wrap");

		const ByteReader reader(bytes);
		ByteReader slice;
		context.check(!reader.slice(4, maximum, slice), "slice size that would wrap");
		context.check(!reader.slice(maximum, 0, slice), "slice at the maximum offset");
		context.check(!reader.contains(1, maximum), "contains with a huge size");
	}};

	static inline test::Test slicesHaveTheirOwnOffsets{"ByteReader: slices read from their own start and stop at their own end", [](test::Context& context) {
		const std::array<std::uint8_t, 8> bytes{0, 1, 2, 3, 4, 5, 6, 7};
		const ByteReader reader(bytes);

		ByteReader table;
		context.check(reader.slice(2, 4, table), "slice inside the view");
		context.checkEqual(table.getSize(), std::size_t{4});

		std::uint8_t first = 0;
		std::uint16_t middle = 0;
		table.read(first);
		table.skip(1);
		table.read(middle);
		context.checkEqual(first, std::uint8_t{2});
		context.checkEqual(middle, std::uint16_t{0x0405});
		std::uint8_t beyond = 0;
		context.check(!table.read(beyond), "slice end is enforced even though the parent continues");

		ByteReader empty;
		context.check(reader.slice(8, 0, empty), "empty slice at the end is allowed");
		context.check(!reader.slice(6, 3, empty), "slice past the end fails");
	}};

	static inline test::Test readsBuffers{"ByteReader: readBuffer copies bytes and fails without reading past the end", [](test::Context& context) {
		const std::array<std::uint8_t, 5> bytes{9, 8, 7, 6, 5};
		ByteReader reader(bytes);

		std::array<std::uint8_t, 3> buffer{};
		context.check(reader.readBuffer(buffer), "three bytes fit");
		context.checkEqual(buffer[2], std::uint8_t{7});
		context.check(!reader.readBuffer(buffer), "three more do not");
		context.checkEqual(buffer[0], std::uint8_t{0});
	}};

	static inline test::Test emptyReaderFailsReads{"ByteReader: an empty reader fails every read", [](test::Context& context) {
		ByteReader reader;
		std::uint32_t value = 1;
		context.checkEqual(reader.getSize(), std::size_t{0});
		context.check(!reader.read(value), "read from an empty reader");
		context.checkEqual(value, std::uint32_t{0});
	}};

	static inline test::Test randomReadsStayInBounds{"ByteReader: random positions are accepted exactly when in range", [](test::Context& context) {
		// The view sits in the middle of a larger buffer, so a read past its end
		// would return real bytes instead of crashing; the values prove it did not.
		std::vector<std::uint8_t> storage(64);
		for (std::size_t i = 0; i < storage.size(); i++) {
			storage[i] = static_cast<std::uint8_t>(i * 7 + 3);
		}
		constexpr std::size_t viewStart = 16;
		constexpr std::size_t viewSize = 32;
		const std::span<const std::uint8_t> view = std::span<const std::uint8_t>(storage).subspan(viewStart, viewSize);

		std::mt19937 random(20261015u);
		std::uniform_int_distribution<std::size_t> positions(0, viewSize + 8);

		for (std::int32_t i = 0; i < 20000; i++) {
			const std::size_t position = positions(random);
			ByteReader reader(view);

			std::uint32_t value = 0;
			const bool read = reader.seek(position) && reader.read(value);
			context.check(read == (position + 4 <= viewSize), "seek and read accept exactly the in-range positions");
			if (read) {
				const std::size_t at = viewStart + position;
				const std::uint32_t expected = (std::uint32_t{storage[at]} << 24) | (std::uint32_t{storage[at + 1]} << 16)
					| (std::uint32_t{storage[at + 2]} << 8) | std::uint32_t{storage[at + 3]};
				context.checkEqual(value, expected);
			}
		}
	}};
};

}
