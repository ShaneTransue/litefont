#include <litefont/Image.h>
#include <litefont/Sprite.h>
#include <litefont/Blur.h>
#include <litefont/ByteReader.h>
#include <litefont/ByteWriter.h>
#include <litefont/File.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <format>
#include <utility>

namespace litefont {

Image::Image(std::int32_t width, std::int32_t height, PixelFormat format) {
	this->format = format;
	if (width <= 0 || height <= 0 || width > Image::getMaximumSize() || height > Image::getMaximumSize()) {
		return;
	}
	this->width = width;
	this->height = height;
	this->pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * static_cast<std::size_t>(Image::getChannelCount(format)), 0);
}

bool Image::isEmpty() const {
	return this->pixels.empty();
}

std::int32_t Image::getWidth() const {
	return this->width;
}

std::int32_t Image::getHeight() const {
	return this->height;
}

Size2i Image::getSize() const {
	return Size2i{this->width, this->height};
}

Recti Image::getBounds() const {
	return Recti{0, 0, this->width, this->height};
}

PixelFormat Image::getFormat() const {
	return this->format;
}

std::int32_t Image::getChannelCount() const {
	return Image::getChannelCount(this->format);
}

std::size_t Image::getRowSize() const {
	return static_cast<std::size_t>(this->width) * static_cast<std::size_t>(this->getChannelCount());
}

std::uint8_t* Image::getRow(std::int32_t y) {
	if (y < 0 || y >= this->height) {
		return nullptr;
	}
	return this->pixels.data() + static_cast<std::size_t>(y) * this->getRowSize();
}

const std::uint8_t* Image::getRow(std::int32_t y) const {
	if (y < 0 || y >= this->height) {
		return nullptr;
	}
	return this->pixels.data() + static_cast<std::size_t>(y) * this->getRowSize();
}

std::span<std::uint8_t> Image::getPixels() {
	return this->pixels;
}

std::span<const std::uint8_t> Image::getPixels() const {
	return this->pixels;
}

Color4u8 Image::getPixel(std::int32_t x, std::int32_t y) const {
	if (x < 0 || x >= this->width || y < 0 || y >= this->height) {
		return Color4u8{0, 0, 0, 0};
	}
	const std::uint8_t* pixel = this->getRow(y) + static_cast<std::size_t>(x) * static_cast<std::size_t>(this->getChannelCount());
	switch (this->format) {
	case PixelFormat::Gray8:
		return Color4u8{pixel[0], pixel[0], pixel[0], 255};
	case PixelFormat::RGB8:
		return Color4u8{pixel[0], pixel[1], pixel[2], 255};
	case PixelFormat::RGBA8:
		return Color4u8{pixel[0], pixel[1], pixel[2], pixel[3]};
	}
	return Color4u8{0, 0, 0, 0};
}

void Image::setPixel(std::int32_t x, std::int32_t y, const Color4u8& color) {
	if (x < 0 || x >= this->width || y < 0 || y >= this->height) {
		return;
	}
	this->storePixel(this->getRow(y) + static_cast<std::size_t>(x) * static_cast<std::size_t>(this->getChannelCount()), color);
}

void Image::fill(const Color4f& color) {
	this->fill(this->getBounds(), color);
}

void Image::fill(const Recti& region, const Color4f& color) {
	// 64-bit edges, so a region far outside the image cannot overflow.
	const std::int64_t left = std::max<std::int64_t>(region.x, 0);
	const std::int64_t top = std::max<std::int64_t>(region.y, 0);
	const std::int64_t right = std::min<std::int64_t>(std::int64_t{region.x} + region.width, this->width);
	const std::int64_t bottom = std::min<std::int64_t>(std::int64_t{region.y} + region.height, this->height);
	if (left >= right || top >= bottom) {
		return;
	}

	const Color4u8 bytes = color.convert<std::uint8_t>();
	const std::size_t channels = static_cast<std::size_t>(this->getChannelCount());
	std::uint8_t pattern[4] = {};
	this->storePixel(pattern, bytes);

	for (std::int64_t y = top; y < bottom; y++) {
		std::uint8_t* row = this->getRow(static_cast<std::int32_t>(y));
		for (std::int64_t x = left; x < right; x++) {
			std::memcpy(row + static_cast<std::size_t>(x) * channels, pattern, channels);
		}
	}
}

void Image::copyFrom(const Image& source, std::int32_t x, std::int32_t y) {
	this->copyFrom(source, source.getBounds(), x, y);
}

void Image::copyFrom(const Image& source, const Recti& region, std::int32_t x, std::int32_t y) {
	// Clip the region to the source, moving its destination along, then clip
	// the destination to this image. 64-bit edges keep far-off values safe.
	const std::int64_t sourceLeft = std::max<std::int64_t>(region.x, 0);
	const std::int64_t sourceTop = std::max<std::int64_t>(region.y, 0);
	const std::int64_t sourceRight = std::min<std::int64_t>(std::int64_t{region.x} + region.width, source.width);
	const std::int64_t sourceBottom = std::min<std::int64_t>(std::int64_t{region.y} + region.height, source.height);
	if (sourceLeft >= sourceRight || sourceTop >= sourceBottom) {
		return;
	}
	const std::int64_t targetX = std::int64_t{x} + (sourceLeft - region.x);
	const std::int64_t targetY = std::int64_t{y} + (sourceTop - region.y);
	const std::int64_t left = std::max<std::int64_t>(targetX, 0);
	const std::int64_t top = std::max<std::int64_t>(targetY, 0);
	const std::int64_t right = std::min<std::int64_t>(targetX + (sourceRight - sourceLeft), this->width);
	const std::int64_t bottom = std::min<std::int64_t>(targetY + (sourceBottom - sourceTop), this->height);
	if (left >= right || top >= bottom) {
		return;
	}

	const std::size_t channels = static_cast<std::size_t>(this->getChannelCount());
	const std::size_t count = static_cast<std::size_t>(right - left);
	for (std::int64_t row = top; row < bottom; row++) {
		const std::int32_t sourceY = static_cast<std::int32_t>(sourceTop + (row - targetY));
		const std::int32_t sourceX = static_cast<std::int32_t>(sourceLeft + (left - targetX));
		std::uint8_t* target = this->getRow(static_cast<std::int32_t>(row)) + static_cast<std::size_t>(left) * channels;
		if (source.format == this->format) {
			std::memcpy(target, source.getRow(sourceY) + static_cast<std::size_t>(sourceX) * channels, count * channels);
			continue;
		}
		for (std::size_t i = 0; i < count; i++) {
			this->storePixel(target + i * channels, source.getPixel(sourceX + static_cast<std::int32_t>(i), sourceY));
		}
	}
}

bool Image::save(const std::filesystem::path& path) const {
	Bytes bytes;
	if (!this->encode(bytes)) {
		return false;
	}
	return File::write(path, bytes);
}

bool Image::load(const std::filesystem::path& path) {
	Bytes bytes;
	if (!File::read(path, bytes)) {
		*this = Image();
		return false;
	}
	return this->decode(bytes);
}

bool Image::encode(Bytes& bytes) const {
	bytes.data.clear();
	if (this->isEmpty()) {
		return false;
	}

	std::string header;
	switch (this->format) {
	case PixelFormat::Gray8:
		header = std::format("P5\n{} {}\n255\n", this->width, this->height);
		break;
	case PixelFormat::RGB8:
		header = std::format("P6\n{} {}\n255\n", this->width, this->height);
		break;
	case PixelFormat::RGBA8:
		header = std::format("P7\nWIDTH {}\nHEIGHT {}\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n", this->width, this->height);
		break;
	}

	ByteWriter writer;
	writer.writeBuffer(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(header.data()), header.size()));
	writer.writeBuffer(this->pixels);
	bytes = writer.takeBytes();
	return true;
}

bool Image::decode(const Bytes& bytes) {
	*this = Image();

	ByteReader reader(bytes.data);
	std::string magic;
	if (!Image::readToken(reader, magic)) {
		return false;
	}

	std::int32_t width = 0;
	std::int32_t height = 0;
	std::int32_t depth = 0;
	std::int32_t maximum = 0;
	if (magic == "P5" || magic == "P6") {
		depth = magic == "P5" ? 1 : 3;
		if (!Image::readNumber(reader, width) || !Image::readNumber(reader, height) || !Image::readNumber(reader, maximum)) {
			return false;
		}
	}
	else if (magic == "P7") {
		if (!Image::readPamHeader(reader, width, height, depth, maximum)) {
			return false;
		}
	}
	else {
		return false;
	}

	PixelFormat format = PixelFormat::Gray8;
	switch (depth) {
	case 1:
		format = PixelFormat::Gray8;
		break;
	case 3:
		format = PixelFormat::RGB8;
		break;
	case 4:
		format = PixelFormat::RGBA8;
		break;
	default:
		return false;
	}
	if (maximum != 255 || width <= 0 || height <= 0 || width > Image::getMaximumSize() || height > Image::getMaximumSize()) {
		return false;
	}

	// Check the size before allocating, so a header that lies cannot make the
	// image reserve more memory than the file holds.
	const std::size_t size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * static_cast<std::size_t>(depth);
	if (reader.getRemaining() < size) {
		return false;
	}
	Image image(width, height, format);
	if (!reader.readBuffer(image.pixels)) {
		return false;
	}
	*this = std::move(image);
	return true;
}

std::int32_t Image::getChannelCount(PixelFormat format) {
	switch (format) {
	case PixelFormat::Gray8:
		return 1;
	case PixelFormat::RGB8:
		return 3;
	case PixelFormat::RGBA8:
		return 4;
	}
	return 1;
}

std::int32_t Image::getMaximumSize() {
	return 32768;
}

void Image::storePixel(std::uint8_t* pixel, const Color4u8& color) const {
	switch (this->format) {
	case PixelFormat::Gray8:
		// Rec. 601 luma with weights that sum to 256, so white stays 255.
		pixel[0] = static_cast<std::uint8_t>((77u * color.r + 150u * color.g + 29u * color.b + 128u) >> 8);
		break;
	case PixelFormat::RGB8:
		pixel[0] = color.r;
		pixel[1] = color.g;
		pixel[2] = color.b;
		break;
	case PixelFormat::RGBA8:
		pixel[0] = color.r;
		pixel[1] = color.g;
		pixel[2] = color.b;
		pixel[3] = color.a;
		break;
	}
}

bool Image::readToken(ByteReader& reader, std::string& token) {
	constexpr std::size_t longestToken = 32;
	auto isSpace = [](std::uint8_t character) {
		return character == ' ' || character == '\t' || character == '\n' || character == '\r' || character == '\v' || character == '\f';
	};

	token.clear();
	std::uint8_t character = 0;
	while (true) {
		if (!reader.read(character)) {
			return false;
		}
		if (character == '#') {
			while (character != '\n' && character != '\r') {
				if (!reader.read(character)) {
					return false;
				}
			}
			continue;
		}
		if (!isSpace(character)) {
			break;
		}
	}

	// The whitespace that ends the token is consumed with it: in PGM and PPM
	// exactly one whitespace byte separates the header from the pixels.
	while (!isSpace(character)) {
		token.push_back(static_cast<char>(character));
		if (token.size() > longestToken || !reader.read(character)) {
			return false;
		}
	}
	return true;
}

bool Image::readNumber(ByteReader& reader, std::int32_t& value) {
	value = 0;
	std::string token;
	if (!Image::readToken(reader, token)) {
		return false;
	}
	const char* end = token.data() + token.size();
	const std::from_chars_result result = std::from_chars(token.data(), end, value);
	return result.ec == std::errc() && result.ptr == end;
}

bool Image::readPamHeader(ByteReader& reader, std::int32_t& width, std::int32_t& height, std::int32_t& depth, std::int32_t& maximum) {
	std::string key;
	while (true) {
		if (!Image::readToken(reader, key)) {
			return false;
		}
		if (key == "ENDHDR") {
			return true;
		}

		bool good = false;
		if (key == "WIDTH") {
			good = Image::readNumber(reader, width);
		}
		else if (key == "HEIGHT") {
			good = Image::readNumber(reader, height);
		}
		else if (key == "DEPTH") {
			good = Image::readNumber(reader, depth);
		}
		else if (key == "MAXVAL") {
			good = Image::readNumber(reader, maximum);
		}
		else if (key == "TUPLTYPE") {
			// The depth already says how many channels there are.
			std::string tupleType;
			good = Image::readToken(reader, tupleType);
		}
		if (!good) {
			return false;
		}
	}
}

}
