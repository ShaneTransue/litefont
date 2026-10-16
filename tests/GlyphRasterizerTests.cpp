#include <litefont/GlyphRasterizer.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include "BlockFont.h"

#include <cmath>

namespace litefont::tests {

class GlyphRasterizerTests {
	static GlyphKey makeKey(const Font& font, char32_t codePoint, float size, std::uint8_t bin = 0) {
		GlyphKey key;
		key.font = font.getId();
		key.glyph = font.getGlyph(codePoint);
		key.sizePx64 = static_cast<std::uint32_t>(std::lround(size * 64.0f));
		key.subpixelBin = bin;
		return key;
	}

	static std::int32_t getRowSum(const Image& image, std::int32_t row) {
		std::int32_t sum = 0;
		for (std::int32_t x = 0; x < image.getWidth(); x++) {
			sum += image.getRow(row)[x];
		}
		return sum;
	}

	// An edge row is solid when it is at least 95 percent as covered as the
	// row next to it. Stems that straddle two columns cover both rows alike,
	// so this measures only where the horizontal edge falls.
	static bool isSolidEdge(const Image& image, std::int32_t edge, std::int32_t neighbor) {
		return getRowSum(image, edge) * 100 >= getRowSum(image, neighbor) * 95;
	}

	static inline test::Test keys{"GlyphKey: equal keys hash equally and every field changes the hash", [](test::Context& context) {
		const GlyphKey key{0x1234567890ABCDEFull, 42, 13 * 64, 0};
		GlyphKey same = key;
		context.check(same == key && same.getHash() == key.getHash(), "equal keys");
		GlyphKey other = key;
		other.font ^= 1;
		context.check(other.getHash() != key.getHash(), "font");
		other = key;
		other.glyph = 43;
		context.check(other.getHash() != key.getHash(), "glyph");
		other = key;
		other.sizePx64 += 1;
		context.check(other.getHash() != key.getHash(), "size");
		other = key;
		other.subpixelBin = 2;
		context.check(other.getHash() != key.getHash(), "subpixel bin");
		other = key;
		other.simulatedBold = true;
		context.check(other.getHash() != key.getHash(), "simulated bold");
		other = key;
		other.simulatedItalic = true;
		context.check(other.getHash() != key.getHash(), "simulated italic");
		other = key;
		other.outlineWidth16 = 16;
		context.check(other.getHash() != key.getHash(), "outline width");
	}};

	static inline test::Test simulations{"GlyphRasterizer: simulated bold thickens stems, simulated italic slants by 12 degrees", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer glyphs;
		glyphs.setHinting(false);

		// x is a 10 x 10 px block at 20 px.
		GlyphKey key = makeKey(font, U'x', 20.0f);
		key.simulatedBold = true;
		Sprite sprite;
		context.check(glyphs.render(font, key, sprite), "bold renders");
		context.check(sprite.origin == Vector2f{0.0f, -10.0f} && sprite.image.getSize() == Size2i{11, 10}, "1/24 em wider, as tall");
		// The last column holds the extra 0.83 px.
		context.check(sprite.image.getRow(5)[10] >= 211 && sprite.image.getRow(5)[10] <= 213, "the widened edge");

		key.simulatedBold = false;
		key.simulatedItalic = true;
		context.check(glyphs.render(font, key, sprite), "italic renders");
		context.check(sprite.origin.x == 0.0f && sprite.image.getWidth() == 13, "the top leans 2.1 px right");
		context.checkEqual(sprite.image.getRow(9)[1], std::uint8_t{255});
		context.checkEqual(sprite.image.getRow(0)[0], std::uint8_t{0});
		context.checkEqual(sprite.image.getRow(0)[3], std::uint8_t{255});
	}};

	static inline test::Test outlines{"GlyphRasterizer: an outline key renders the glyph grown by the outline width", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer glyphs;
		glyphs.setHinting(false);
		// a at 20 px covers x 1 to 9 and y -10 to 0; a 1 px border adds a ring.
		GlyphKey key = makeKey(font, U'a', 20.0f);
		key.outlineWidth16 = 16;
		Sprite sprite;
		context.check(glyphs.render(font, key, sprite), "the outline renders");
		context.check(sprite.origin == Vector2f{-1.0f, -12.0f} && sprite.image.getSize() == Size2i{12, 14}, "two pixels of room on every side");
		context.checkEqual(sprite.image.getRow(7)[1], std::uint8_t{255});
		context.checkEqual(sprite.image.getRow(7)[0], std::uint8_t{0});
		context.checkEqual(sprite.image.getRow(1)[1], std::uint8_t{202});
		context.checkEqual(sprite.image.getRow(7)[5], std::uint8_t{255});
	}};

	static inline test::Test conformance{"GlyphRasterizer: empty glyphs give empty sprites, missing glyphs fail, origins match the outline", [](test::Context& context) {
		Font font;
		context.check(font.load(BlockFont::build()), "the block font loads");
		GlyphRasterizer glyphs;
		glyphs.setHinting(false);
		Sprite sprite;
		context.check(glyphs.render(font, makeKey(font, U' ', 20.0f), sprite) && sprite.image.isEmpty(), "space is empty");
		GlyphKey missing = makeKey(font, U'a', 20.0f);
		missing.glyph = 500;
		context.check(!glyphs.render(font, missing, sprite), "a glyph past the end fails");
		context.check(!glyphs.render(Font(), makeKey(font, U'a', 20.0f), sprite), "an unloaded font fails");
		GlyphKey sizeless = makeKey(font, U'a', 20.0f);
		sizeless.sizePx64 = 0;
		context.check(!glyphs.render(font, sizeless, sprite), "size 0 fails");

		// a at 20 px is the block from (1, -10) to (9, 0) relative to the pen.
		context.check(glyphs.render(font, makeKey(font, U'a', 20.0f), sprite), "a renders");
		context.check(sprite.origin == Vector2f{1.0f, -10.0f}, "the origin is the block's top-left");
		context.checkEqual(sprite.image.getSize(), Size2i{8, 10});
		bool solid = true;
		for (const std::uint8_t value : sprite.image.getPixels()) {
			solid = solid && value == 255;
		}
		context.check(solid, "a whole-pixel block is fully covered");

		// A quarter-pixel shift moves the left edge into the first column.
		context.check(glyphs.render(font, makeKey(font, U'a', 20.0f, 1), sprite), "shifted");
		context.check(sprite.origin.x == 1.0f && sprite.image.getWidth() == 9, "one more column");
		context.checkEqual(sprite.image.getRow(0)[0], std::uint8_t{191});
		context.checkEqual(sprite.image.getRow(0)[8], std::uint8_t{64});

		context.check(glyphs.render(font, makeKey(font, U'p', 20.0f), sprite), "p renders");
		context.check(sprite.origin.y == -10.0f && sprite.image.getHeight() == 14, "p reaches 4 px below the baseline");
		context.check(glyphs.getFormat() == PixelFormat::Gray8 && glyphs.getEncoding() == SpriteEncoding::Coverage, "coverage sprites");
	}};

	static inline test::Test crisp{"GlyphRasterizer: with hinting, the baseline and top rows of x, z, E and H are solid from 8 to 16 px", [](test::Context& context) {
		Font font;
		if (!font.load(test::Context::getFontFolder() / "segoeui.ttf")) {
			context.note("segoeui is not in the fonts folder; skipped");
			return;
		}
		std::int32_t hintedSoft = 0;
		std::int32_t unhintedSoft = 0;
		for (const bool hinting : {true, false}) {
			GlyphRasterizer glyphs;
			glyphs.setHinting(hinting);
			for (float size = 8.0f; size <= 16.0f; size += 1.0f) {
				for (const char32_t letter : {U'x', U'z', U'E', U'H'}) {
					Sprite sprite;
					glyphs.render(font, makeKey(font, letter, size), sprite);
					const std::int32_t last = sprite.image.getHeight() - 1;
					const bool sharp = isSolidEdge(sprite.image, 0, 1) && isSolidEdge(sprite.image, last, last - 1);
					(hinting ? hintedSoft : unhintedSoft) += sharp ? 0 : 1;
				}
			}
		}
		context.checkEqual(hintedSoft, 0);
		context.check(unhintedSoft > 0, "without hinting some rows are soft");
		context.note(std::format("unhinted: {} of 36 glyph images have a soft top or bottom row", unhintedSoft));
	}};
};

}
