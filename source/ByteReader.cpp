#include <litefont/ByteReader.h>

#include <algorithm>
#include <bit>
#include <type_traits>

namespace litefont {

ByteReader::ByteReader(std::span<const std::uint8_t> data) {
	this->data = data;
}

bool ByteReader::read(std::uint8_t& value) {
	return this->readBigEndian(value);
}

bool ByteReader::read(std::int8_t& value) {
	return this->readBigEndian(value);
}

bool ByteReader::read(std::uint16_t& value) {
	return this->readBigEndian(value);
}

bool ByteReader::read(std::int16_t& value) {
	return this->readBigEndian(value);
}

bool ByteReader::read(std::uint32_t& value) {
	return this->readBigEndian(value);
}

bool ByteReader::read(std::int32_t& value) {
	return this->readBigEndian(value);
}

bool ByteReader::read(std::uint64_t& value) {
	return this->readBigEndian(value);
}

bool ByteReader::read(std::int64_t& value) {
	return this->readBigEndian(value);
}

bool ByteReader::readBuffer(std::span<std::uint8_t> buffer) {
	if (!this->good || !this->contains(this->position, buffer.size())) {
		std::fill(buffer.begin(), buffer.end(), std::uint8_t(0));
		return this->fail();
	}
	const std::span<const std::uint8_t> source = this->data.subspan(this->position, buffer.size());
	std::copy(source.begin(), source.end(), buffer.begin());
	this->position += buffer.size();
	return true;
}

bool ByteReader::skip(std::size_t count) {
	if (!this->good || !this->contains(this->position, count)) {
		return this->fail();
	}
	this->position += count;
	return true;
}

bool ByteReader::seek(std::size_t position) {
	if (!this->good || position > this->data.size()) {
		return this->fail();
	}
	this->position = position;
	return true;
}

bool ByteReader::slice(std::size_t offset, std::size_t size, ByteReader& reader) const {
	if (!this->contains(offset, size)) {
		reader = ByteReader();
		return false;
	}
	reader = ByteReader(this->data.subspan(offset, size));
	return true;
}

bool ByteReader::contains(std::size_t offset, std::size_t size) const {
	// Written so that neither side can overflow, whatever offset and size are.
	return offset <= this->data.size() && size <= this->data.size() - offset;
}

bool ByteReader::isGood() const {
	return this->good;
}

std::size_t ByteReader::getPosition() const {
	return this->position;
}

std::size_t ByteReader::getRemaining() const {
	return this->data.size() - this->position;
}

std::size_t ByteReader::getSize() const {
	return this->data.size();
}

template <typename T>
bool ByteReader::readBigEndian(T& value) {
	if (!this->good || !this->contains(this->position, sizeof(T))) {
		value = T(0);
		return this->fail();
	}

	using Unsigned = std::make_unsigned_t<T>;
	Unsigned result = 0;
	for (std::size_t i = 0; i < sizeof(T); i++) {
		result = static_cast<Unsigned>((result << 8) | this->data[this->position + i]);
	}
	value = std::bit_cast<T>(result);
	this->position += sizeof(T);
	return true;
}

bool ByteReader::fail() {
	this->good = false;
	return false;
}

}
