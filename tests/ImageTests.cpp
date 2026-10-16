#include <litefont/Image.h>
#include <litefont/Sprite.h>
#include <litefont/Blur.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <limits>
#include <random>
#include <string>

namespace litefont::tests {

class ImageTests {
	static std::filesystem::path makeTemporaryPath(const char* name) {
		std::error_code error;
		return std::filesystem::temp_directory_path(error) / name;
	}

	static Bytes makeBytes(std::string_view text) {
		Bytes bytes;
		bytes.data.assign(text.begin(), text.end());
		return bytes;
	}

	static Image makePattern(std::int32_t width, std::int32_t height, PixelFormat format, std::uint8_t seed) {
		Image image(width, height, format);
		const std::span<std::uint8_t> pixels = image.getPixels();
		for (std::size_t i = 0; i < pixels.size(); i++) {
			pixels[i] = static_cast<std::uint8_t>(seed + i * 37);
		}
		return image;
	}

	static inline test::Test constructs{"Image: constructs zeroed, tightly packed rows", [](test::Context& context) {
		Image image(5, 3, PixelFormat::RGB8);
		context.checkEqual(image.getWidth(), 5);
		context.checkEqual(image.getHeight(), 3);
		context.checkEqual(image.getChannelCount(), 3);
		context.checkEqual(image.getRowSize(), std::size_t{15});
		context.checkEqual(image.getPixels().size(), std::size_t{45});
		context.check(image.getRow(1) == image.getRow(0) + 15, "rows are packed");
		context.check(image.getRow(-1) == nullptr && image.getRow(3) == nullptr, "rows outside the image are null");

		bool zeroed = true;
		for (const std::uint8_t value : image.getPixels()) {
			zeroed = zeroed && value == 0;
		}
		context.check(zeroed, "new pixels are zero");
	}};

	static inline test::Test invalidSize{"Image: a size outside the limits makes an empty image", [](test::Context& context) {
		context.check(Image(0, 4, PixelFormat::Gray8).isEmpty(), "zero width");
		context.check(Image(4, -1, PixelFormat::Gray8).isEmpty(), "negative height");
		context.check(Image(Image::getMaximumSize() + 1, 1, PixelFormat::Gray8).isEmpty(), "too wide");
		const Image empty(0, 0, PixelFormat::RGBA8);
		context.checkEqual(empty.getWidth(), 0);
		context.checkEqual(empty.getHeight(), 0);
		context.check(empty.getFormat() == PixelFormat::RGBA8, "keeps its format");
	}};

	static inline test::Test pixels{"Image: getPixel and setPixel convert between formats", [](test::Context& context) {
		Image gray(2, 2, PixelFormat::Gray8);
		gray.setPixel(0, 0, Color4u8{255, 255, 255, 255});
		gray.setPixel(1, 0, Color4u8{255, 0, 0, 255});
		gray.setPixel(5, 5, Color4u8{255, 255, 255, 255});
		context.checkEqual(gray.getRow(0)[0], std::uint8_t{255});
		context.checkEqual(gray.getRow(0)[1], std::uint8_t{77});
		context.check(gray.getPixel(1, 0) == Color4u8{77, 77, 77, 255}, "gray reads back as opaque gray");
		context.check(gray.getPixel(-1, 0) == Color4u8{0, 0, 0, 0}, "outside reads transparent black");

		Image rgb(1, 1, PixelFormat::RGB8);
		rgb.setPixel(0, 0, Color4u8{10, 20, 30, 40});
		context.check(rgb.getPixel(0, 0) == Color4u8{10, 20, 30, 255}, "RGB drops alpha");

		Image rgba(1, 1, PixelFormat::RGBA8);
		rgba.setPixel(0, 0, Color4u8{10, 20, 30, 40});
		context.check(rgba.getPixel(0, 0) == Color4u8{10, 20, 30, 40}, "RGBA keeps alpha");
	}};

	static inline test::Test fills{"Image: fill clips its region to the image", [](test::Context& context) {
		Image image(4, 4, PixelFormat::Gray8);
		image.fill(Recti{-2, 2, 4, 10}, Color4f{1.0f, 1.0f, 1.0f, 1.0f});
		std::int32_t lit = 0;
		for (std::int32_t y = 0; y < 4; y++) {
			for (std::int32_t x = 0; x < 4; x++) {
				const bool inside = x < 2 && y >= 2;
				const std::uint8_t value = image.getRow(y)[x];
				context.check(value == (inside ? 255 : 0), "only the clipped region is filled");
				lit += value == 255 ? 1 : 0;
			}
		}
		context.checkEqual(lit, 4);

		image.fill(Recti{std::numeric_limits<std::int32_t>::max() - 1, 0, 100, 100}, Color4f{});
		image.fill(Color4f{0.5f, 0.5f, 0.5f, 1.0f});
		context.checkEqual(image.getRow(3)[3], std::uint8_t{128});
	}};

	static inline test::Test copiesWithClipping{"Image: copyFrom clips at every edge and never writes outside", [](test::Context& context) {
		const Image source = makePattern(4, 3, PixelFormat::RGBA8, 1);
		const std::int32_t big = std::numeric_limits<std::int32_t>::max();
		const std::int32_t small = std::numeric_limits<std::int32_t>::min();
		const std::int32_t positions[] = {small, -5, -4, -3, -1, 0, 1, 3, 5, 6, 7, big};

		for (const std::int32_t x : positions) {
			for (const std::int32_t y : positions) {
				Image target(6, 5, PixelFormat::RGBA8);
				target.copyFrom(source, x, y);
				bool correct = true;
				for (std::int32_t ty = 0; ty < 5; ty++) {
					for (std::int32_t tx = 0; tx < 6; tx++) {
						const std::int64_t sx = std::int64_t{tx} - x;
						const std::int64_t sy = std::int64_t{ty} - y;
						const bool inside = sx >= 0 && sx < 4 && sy >= 0 && sy < 3;
						const Color4u8 expected = inside ? source.getPixel(static_cast<std::int32_t>(sx), static_cast<std::int32_t>(sy)) : Color4u8{0, 0, 0, 0};
						correct = correct && target.getPixel(tx, ty) == expected;
					}
				}
				context.check(correct, "copied region matches the clipped source");
			}
		}
	}};

	static inline test::Test copiesRegions{"Image: copyFrom with a region clips the region and the destination", [](test::Context& context) {
		const Image source = makePattern(5, 4, PixelFormat::Gray8, 9);
		const Recti regions[] = {Recti{1, 1, 3, 2}, Recti{-2, -1, 4, 3}, Recti{3, 2, 9, 9}, Recti{0, 0, 5, 4}};
		const std::int32_t positions[] = {-3, 0, 2, 5};
		std::int32_t wrong = 0;
		for (const Recti& region : regions) {
			for (const std::int32_t x : positions) {
				for (const std::int32_t y : positions) {
					Image target(6, 6, PixelFormat::Gray8);
					target.copyFrom(source, region, x, y);
					for (std::int32_t ty = 0; ty < 6; ty++) {
						for (std::int32_t tx = 0; tx < 6; tx++) {
							const std::int32_t sx = region.x + (tx - x);
							const std::int32_t sy = region.y + (ty - y);
							const bool inside = region.contains(Vector2i{sx, sy}) && source.getBounds().contains(Vector2i{sx, sy});
							const std::uint8_t expected = inside ? source.getRow(sy)[sx] : 0;
							wrong += target.getRow(ty)[tx] == expected ? 0 : 1;
						}
					}
				}
			}
		}
		context.checkEqual(wrong, 0);
	}};

	static inline test::Test convertsOnCopy{"Image: copyFrom converts between pixel formats", [](test::Context& context) {
		Image source(1, 1, PixelFormat::RGB8);
		source.setPixel(0, 0, Color4u8{0, 255, 0, 255});
		Image gray(1, 1, PixelFormat::Gray8);
		gray.copyFrom(source, 0, 0);
		context.checkEqual(gray.getRow(0)[0], std::uint8_t{149});

		Image rgba(1, 1, PixelFormat::RGBA8);
		rgba.copyFrom(gray, 0, 0);
		context.check(rgba.getPixel(0, 0) == Color4u8{149, 149, 149, 255}, "gray expands to opaque RGBA");
	}};

	static inline test::Test roundTrips{"Image: PGM, PPM and PAM files round trip", [](test::Context& context) {
		const PixelFormat formats[] = {PixelFormat::Gray8, PixelFormat::RGB8, PixelFormat::RGBA8};
		for (const PixelFormat format : formats) {
			const Image written = makePattern(7, 5, format, 3);
			const std::filesystem::path path = makeTemporaryPath("px_image_round_trip.pnm");
			context.check(written.save(path), "save");
			Image read;
			context.check(read.load(path), "load");
			context.check(read == written, "pixels, size and format survive");
			std::error_code error;
			std::filesystem::remove(path, error);
		}

		Bytes bytes;
		context.check(Image(3, 2, PixelFormat::Gray8).encode(bytes), "encode");
		const std::string header(bytes.data.begin(), bytes.data.begin() + 11);
		context.checkEqual(header, std::string("P5\n3 2\n255\n"));
		context.checkEqual(bytes.data.size(), std::size_t{17});
		context.check(!Image().encode(bytes), "an empty image cannot be encoded");
	}};

	static inline test::Test parsesHeaders{"Image: decodes comments and PAM headers and rejects malformed files", [](test::Context& context) {
		Image image;
		context.check(image.decode(makeBytes("P5 # a comment\n2 # another\n1\n255\nab")), "comments between tokens");
		context.checkEqual(image.getWidth(), 2);
		context.checkEqual(image.getRow(0)[1], std::uint8_t{'b'});

		context.check(image.decode(makeBytes("P7\nWIDTH 1\nHEIGHT 1\nDEPTH 3\nMAXVAL 255\nTUPLTYPE RGB\nENDHDR\nxyz")), "PAM");
		context.check(image.getFormat() == PixelFormat::RGB8, "PAM depth 3 is RGB");

		const char* malformed[] = {
			"",
			"P4\n1 1\n255\n\x01",
			"P5\n2 2\n255\nabc",
			"P5\n1 1\n65535\nab",
			"P5\n-1 1\n255\na",
			"P5\n1x 1\n255\na",
			"P5\n40000 1\n255\na",
			"P5\n30000 30000\n255\nabcd",
			"P7\nWIDTH 1\nHEIGHT 1\nDEPTH 2\nMAXVAL 255\nENDHDR\nab",
			"P7\nWIDTH 1\nHEIGHT 1\nDEPTH 1\nMAXVAL 255\na",
			"P7\nWIDTH 1\nCOLOR 1\nENDHDR\na",
		};
		for (const char* text : malformed) {
			context.check(!image.decode(makeBytes(text)), "malformed file is rejected");
			context.check(image.isEmpty(), "a failed decode leaves an empty image");
		}
		context.check(!image.load(makeTemporaryPath("px_image_does_not_exist.pgm")), "missing file");
	}};

	static inline test::Test survivesCorruption{"Image: corrupted files fail cleanly or decode consistently", [](test::Context& context) {
		Bytes valid;
		makePattern(9, 4, PixelFormat::RGBA8, 5).encode(valid);
		std::mt19937 random(11u);
		std::int32_t decoded = 0;
		for (std::int32_t trial = 0; trial < 2000; trial++) {
			Bytes corrupted = valid;
			const std::size_t count = 1 + random() % 4;
			for (std::size_t i = 0; i < count; i++) {
				corrupted.data[random() % 64 % corrupted.data.size()] = static_cast<std::uint8_t>(random());
			}
			if (trial % 7 == 0) {
				corrupted.data.resize(random() % corrupted.data.size());
			}
			Image image;
			if (image.decode(corrupted)) {
				decoded++;
				context.check(image.getPixels().size() == image.getRowSize() * static_cast<std::size_t>(image.getHeight()), "size is consistent");
			}
		}
		context.note(std::format("{} of 2000 corrupted files still decoded", decoded));
	}};
};

}
