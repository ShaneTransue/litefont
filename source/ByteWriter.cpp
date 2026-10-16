#include <litefont/ByteWriter.h>

#include <bit>
#include <type_traits>
#include <utility>

namespace litefont {

void ByteWriter::write(std::uint8_t value) {
	this->writeBigEndian(value);
}

void ByteWriter::write(std::int8_t value) {
	this->writeBigEndian(value);
}

void ByteWriter::write(std::uint16_t value) {
	this->writeBigEndian(value);
}

void ByteWriter::write(std::int16_t value) {
	this->writeBigEndian(value);
}

void ByteWriter::write(std::uint32_t value) {
	this->writeBigEndian(value);
}

void ByteWriter::write(std::int32_t value) {
	this->writeBigEndian(value);
}

void ByteWriter::write(std::uint64_t value) {
	this->writeBigEndian(value);
}

void ByteWriter::write(std::int64_t value) {
	this->writeBigEndian(value);
}

void ByteWriter::writeBuffer(std::span<const std::uint8_t> buffer) {
	this->bytes.data.insert(this->bytes.data.end(), buffer.begin(), buffer.end());
}

void ByteWriter::align(std::size_t alignment) {
	if (alignment < 2) {
		return;
	}
	const std::size_t remainder = this->bytes.data.size() % alignment;
	if (remainder != 0) {
		this->bytes.data.resize(this->bytes.data.size() + (alignment - remainder), std::uint8_t(0));
	}
}

bool ByteWriter::patch(std::size_t position, std::uint16_t value) {
	return this->patchBigEndian(position, value);
}

bool ByteWriter::patch(std::size_t position, std::uint32_t value) {
	return this->patchBigEndian(position, value);
}

std::size_t ByteWriter::getSize() const {
	return this->bytes.data.size();
}

const Bytes& ByteWriter::getBytes() const {
	return this->bytes;
}

Bytes ByteWriter::takeBytes() {
	return std::exchange(this->bytes, Bytes());
}

template <typename T>
void ByteWriter::writeBigEndian(T value) {
	using Unsigned = std::make_unsigned_t<T>;
	const Unsigned bits = std::bit_cast<Unsigned>(value);
	for (std::size_t i = sizeof(T); i > 0; i--) {
		this->bytes.data.push_back(static_cast<std::uint8_t>((bits >> ((i - 1) * 8)) & 0xFFu));
	}
}

template <typename T>
bool ByteWriter::patchBigEndian(std::size_t position, T value) {
	const std::size_t size = this->bytes.data.size();
	if (position > size || sizeof(T) > size - position) {
		return false;
	}
	using Unsigned = std::make_unsigned_t<T>;
	const Unsigned bits = std::bit_cast<Unsigned>(value);
	for (std::size_t i = 0; i < sizeof(T); i++) {
		const std::size_t shift = (sizeof(T) - 1 - i) * 8;
		this->bytes.data[position + i] = static_cast<std::uint8_t>((bits >> shift) & 0xFFu);
	}
	return true;
}

}
