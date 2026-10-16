#pragma once

#include <litefont/Font.h>
#include <litefont/Geometry.h>
#include <litefont/Image.h>
#include <litefont/Sprite.h>
#include <litefont/Blur.h>

namespace litefont {

/**
 * \struct GlyphKey GlyphSource.h <litefont/GlyphSource.h>
 * @addtogroup Text
 *
 * GlyphKey - Which glyph image to render: the font (its content id), the
 * glyph, the size in 1/64 device pixel, the horizontal subpixel shift in
 * quarter pixels (0 to 3), the simulated styles, for an outline (border)
 * image its width in 1/16 device pixel (0 renders the glyph itself), and for
 * a soft shadow its blur radius in 1/16 device pixel (0 for none).
 * getHash() is the key a SpriteAtlas stores it under.
 */
struct GlyphKey {
	std::uint64_t font = 0;
	GlyphId glyph = 0;
	std::uint32_t sizePx64 = 0;
	std::uint8_t subpixelBin = 0;
	bool simulatedBold = false;
	bool simulatedItalic = false;
	std::uint16_t outlineWidth16 = 0;
	std::uint16_t blurRadius16 = 0;

	std::uint64_t getHash() const;
	bool operator==(const GlyphKey&) const = default;
};

/**
 * \class GlyphSource GlyphSource.h <litefont/GlyphSource.h>
 * @addtogroup Text
 *
 * GlyphSource - The concept of something that turns a glyph at a size into a
 * sprite: its image and the offset from the pen to its top-left pixel. An
 * empty glyph (a space) renders to an empty image and returns true; a glyph
 * the font cannot give returns false. Sources honor the key's simulated
 * styles (FontFamily gives their strength and slant), outline width and
 * blur radius.
 * Coverage rendering and MSDF (F7) are implementations, and TextRenderer
 * only knows this interface.
 */
class GlyphSource {
public:
	virtual ~GlyphSource() = default;

	virtual bool render(const Font& font, const GlyphKey& key, Sprite& sprite) = 0;
	virtual PixelFormat getFormat() const = 0;
	virtual SpriteEncoding getEncoding() const = 0;
};

}
