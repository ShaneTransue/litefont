#pragma once

#include <litefont/Bytes.h>
#include <litefont/Color4.h>
#include <litefont/Rect.h>
#include <litefont/Size2.h>
#include <string>
#include <string_view>

#include <filesystem>
#include <span>
#include <vector>

namespace litefont {

class ByteReader;

/**
 * @addtogroup Image
 *
 * PixelFormat - How an Image stores one pixel: 8 bits per channel, channels
 * interleaved in R, G, B, A order.
 */
enum class PixelFormat : std::uint8_t {
	Gray8,
	RGB8,
	RGBA8
};

/**
 * \class Image Image.h <litefont/Image.h>
 * @addtogroup Image
 *
 * Image - A width by height grid of pixels that owns its memory. Rows are
 * tightly packed from top to bottom, so row y starts y * getRowSize() bytes
 * in. Gray images store a color's luma; RGB images drop its alpha.
 *
 * Images save and load themselves as binary netpbm files: PGM (P5) for gray,
 * PPM (P6) for RGB and PAM (P7) for RGBA, always with a maximum value of 255.
 * Width and height are limited to getMaximumSize(); a size outside that range
 * makes an empty image.
 */
class Image {
public:
	Image() = default;
	Image(std::int32_t width, std::int32_t height, PixelFormat format);

	bool isEmpty() const;
	std::int32_t getWidth() const;
	std::int32_t getHeight() const;
	Size2i getSize() const;
	Recti getBounds() const;
	PixelFormat getFormat() const;
	std::int32_t getChannelCount() const;
	std::size_t getRowSize() const;

	std::uint8_t* getRow(std::int32_t y);
	const std::uint8_t* getRow(std::int32_t y) const;
	std::span<std::uint8_t> getPixels();
	std::span<const std::uint8_t> getPixels() const;

	Color4u8 getPixel(std::int32_t x, std::int32_t y) const;
	void setPixel(std::int32_t x, std::int32_t y, const Color4u8& color);

	void fill(const Color4f& color);
	void fill(const Recti& region, const Color4f& color);
	void copyFrom(const Image& source, std::int32_t x, std::int32_t y);
	void copyFrom(const Image& source, const Recti& region, std::int32_t x, std::int32_t y);

	bool save(const std::filesystem::path& path) const;
	bool load(const std::filesystem::path& path);
	bool encode(Bytes& bytes) const;
	bool decode(const Bytes& bytes);

	bool operator==(const Image&) const = default;

	static std::int32_t getChannelCount(PixelFormat format);
	static std::int32_t getMaximumSize();

private:
	void storePixel(std::uint8_t* pixel, const Color4u8& color) const;

	static bool readToken(ByteReader& reader, std::string& token);
	static bool readNumber(ByteReader& reader, std::int32_t& value);
	static bool readPamHeader(ByteReader& reader, std::int32_t& width, std::int32_t& height, std::int32_t& depth, std::int32_t& maximum);

	std::int32_t width = 0;
	std::int32_t height = 0;
	PixelFormat format = PixelFormat::Gray8;
	std::vector<std::uint8_t> pixels;
};

}
