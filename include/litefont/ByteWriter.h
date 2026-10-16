#pragma once

#include <litefont/Bytes.h>

#include <span>

namespace litefont {

/**
 * \class ByteWriter ByteWriter.h <litefont/ByteWriter.h>
 * @addtogroup IO
 *
 * ByteWriter - Appends big-endian values to Bytes it owns, the mirror of
 * ByteReader. Appending cannot fail, so those calls return nothing; patch()
 * overwrites a value written earlier (an offset known only later) and returns
 * false if the position is outside what has been written. Values must be
 * passed with their exact type (write(std::uint16_t{4})), which picks the overload.
 */
class ByteWriter {
public:
	void write(std::uint8_t value);
	void write(std::int8_t value);
	void write(std::uint16_t value);
	void write(std::int16_t value);
	void write(std::uint32_t value);
	void write(std::int32_t value);
	void write(std::uint64_t value);
	void write(std::int64_t value);
	void writeBuffer(std::span<const std::uint8_t> buffer);
	void align(std::size_t alignment);

	bool patch(std::size_t position, std::uint16_t value);
	bool patch(std::size_t position, std::uint32_t value);

	std::size_t getSize() const;
	const Bytes& getBytes() const;
	Bytes takeBytes();

private:
	template <typename T>
	void writeBigEndian(T value);

	template <typename T>
	bool patchBigEndian(std::size_t position, T value);

	Bytes bytes;
};

}
