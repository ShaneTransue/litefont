#include <litefont/ByteReader.h>
#include <litefont/ByteWriter.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <array>
#include <limits>

namespace litefont::tests {

class ByteWriterTests {
	static inline test::Test writesBigEndian{"ByteWriter: writes values big-endian", [](test::Context& context) {
		ByteWriter writer;
		writer.write(std::uint16_t{0x1234});
		writer.write(std::int16_t{-2});
		writer.write(std::uint32_t{0xCAFEBABEu});

		const std::array<std::uint8_t, 8> expected{0x12, 0x34, 0xFF, 0xFE, 0xCA, 0xFE, 0xBA, 0xBE};
		const Bytes& bytes = writer.getBytes();
		context.checkEqual(bytes.data.size(), expected.size());
		for (std::size_t i = 0; i < expected.size() && i < bytes.data.size(); i++) {
			context.checkEqual(bytes.data[i], expected[i]);
		}
	}};

	static inline test::Test roundTripsEveryType{"ByteWriter: every type reads back unchanged through ByteReader", [](test::Context& context) {
		ByteWriter writer;
		writer.write(std::numeric_limits<std::uint8_t>::max());
		writer.write(std::numeric_limits<std::int8_t>::min());
		writer.write(std::numeric_limits<std::uint16_t>::max());
		writer.write(std::numeric_limits<std::int16_t>::min());
		writer.write(std::numeric_limits<std::uint32_t>::max());
		writer.write(std::numeric_limits<std::int32_t>::min());
		writer.write(std::numeric_limits<std::uint64_t>::max());
		writer.write(std::numeric_limits<std::int64_t>::min());

		const Bytes bytes = writer.takeBytes();
		ByteReader reader(bytes.data);
		std::uint8_t u8 = 0;
		std::int8_t i8 = 0;
		std::uint16_t u16 = 0;
		std::int16_t i16 = 0;
		std::uint32_t u32 = 0;
		std::int32_t i32 = 0;
		std::uint64_t u64 = 0;
		std::int64_t i64 = 0;
		reader.read(u8);
		reader.read(i8);
		reader.read(u16);
		reader.read(i16);
		reader.read(u32);
		reader.read(i32);
		reader.read(u64);
		reader.read(i64);

		context.check(reader.isGood() && reader.getRemaining() == 0, "every byte read, nothing left over");
		context.checkEqual(u8, std::numeric_limits<std::uint8_t>::max());
		context.checkEqual(i8, std::numeric_limits<std::int8_t>::min());
		context.checkEqual(u16, std::numeric_limits<std::uint16_t>::max());
		context.checkEqual(i16, std::numeric_limits<std::int16_t>::min());
		context.checkEqual(u32, std::numeric_limits<std::uint32_t>::max());
		context.checkEqual(i32, std::numeric_limits<std::int32_t>::min());
		context.checkEqual(u64, std::numeric_limits<std::uint64_t>::max());
		context.checkEqual(i64, std::numeric_limits<std::int64_t>::min());
	}};

	static inline test::Test alignsWithZeros{"ByteWriter: align pads with zeros to the next multiple", [](test::Context& context) {
		ByteWriter writer;
		const std::array<std::uint8_t, 3> three{7, 7, 7};
		writer.writeBuffer(three);
		writer.align(4);
		context.checkEqual(writer.getSize(), std::size_t{4});
		context.checkEqual(writer.getBytes().data[3], std::uint8_t{0});

		writer.align(4);
		context.checkEqual(writer.getSize(), std::size_t{4});
		writer.align(0);
		writer.align(1);
		context.checkEqual(writer.getSize(), std::size_t{4});
	}};

	static inline test::Test patchesEarlierValues{"ByteWriter: patch overwrites a value written earlier", [](test::Context& context) {
		ByteWriter writer;
		writer.write(std::uint32_t{0});            // placeholder for an offset known later
		writer.write(std::uint16_t{0xAAAA});

		context.check(writer.patch(0, std::uint32_t{0x00000010u}), "patch inside the written range");
		context.check(writer.patch(4, std::uint16_t{0x1234}), "patch the last two bytes");
		context.check(!writer.patch(5, std::uint16_t{1}), "patch straddling the end fails");
		context.check(!writer.patch(std::numeric_limits<std::size_t>::max(), std::uint32_t{1}), "patch at a huge position fails");

		ByteReader reader(writer.getBytes().data);
		std::uint32_t offset = 0;
		std::uint16_t tail = 0;
		reader.read(offset);
		reader.read(tail);
		context.checkEqual(offset, std::uint32_t{0x10u});
		context.checkEqual(tail, std::uint16_t{0x1234});
	}};

	static inline test::Test takeBytesEmptiesTheWriter{"ByteWriter: takeBytes hands over the bytes and leaves the writer empty", [](test::Context& context) {
		ByteWriter writer;
		writer.write(std::uint16_t{1});
		const Bytes taken = writer.takeBytes();
		context.checkEqual(taken.data.size(), std::size_t{2});
		context.checkEqual(writer.getSize(), std::size_t{0});
	}};
};

}
