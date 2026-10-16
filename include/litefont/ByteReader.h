#pragma once

#include <cstddef>
#include <cstdint>

#include <span>

namespace litefont {

/**
 * \class ByteReader ByteReader.h <litefont/ByteReader.h>
 * @addtogroup IO
 *
 * ByteReader - Reads big-endian values in sequence from bytes it does not own,
 * the way font files store them. Every call checks its range first and returns
 * false instead of touching memory outside the view. Failure is sticky: after
 * one failed read, skip or seek, every later call also returns false and reads
 * give 0, so a block of reads can be checked once with isGood(). Use slice()
 * to get an independent reader for one table, with its own offsets.
 */
class ByteReader {
public:
	ByteReader() = default;
	explicit ByteReader(std::span<const std::uint8_t> data);

	bool read(std::uint8_t& value);
	bool read(std::int8_t& value);
	bool read(std::uint16_t& value);
	bool read(std::int16_t& value);
	bool read(std::uint32_t& value);
	bool read(std::int32_t& value);
	bool read(std::uint64_t& value);
	bool read(std::int64_t& value);
	bool readBuffer(std::span<std::uint8_t> buffer);

	bool skip(std::size_t count);
	bool seek(std::size_t position);
	bool slice(std::size_t offset, std::size_t size, ByteReader& reader) const;

	bool contains(std::size_t offset, std::size_t size) const;
	bool isGood() const;
	std::size_t getPosition() const;
	std::size_t getRemaining() const;
	std::size_t getSize() const;

private:
	template <typename T>
	bool readBigEndian(T& value);

	bool fail();

	std::span<const std::uint8_t> data;
	std::size_t position = 0;
	bool good = true;
};

}
