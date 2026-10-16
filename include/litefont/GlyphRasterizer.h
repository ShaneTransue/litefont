#pragma once

#include <litefont/Blur.h>
#include <litefont/Dilation.h>
#include <litefont/Rasterizer.h>

#include <litefont/FontFamily.h>
#include <litefont/GlyphSource.h>
#include <litefont/Hinter.h>

#include <unordered_map>

namespace litefont {

/**
 * \class GlyphRasterizer GlyphRasterizer.h <litefont/GlyphRasterizer.h>
 * @addtogroup Text
 *
 * GlyphRasterizer - A GlyphSource that renders coverage sprites: the glyph's
 * outline from the font, hinted vertically (or only scaled with hinting
 * off), shifted by the key's subpixel bin, flattened and filled with exact
 * area coverage. The sprite's origin is the whole-pixel offset from the pen
 * on the baseline to its top-left pixel, so a sprite drawn at a whole-pixel
 * pen lands exactly where the outline is. Each font's hinting zones are
 * measured once and kept by font id.
 *
 * Simulated bold thickens stems horizontally only (vertical extents stay on
 * their hinting zones) and moves the glyph right by half the strength, so
 * the extra advance is shared on both sides. Simulated italic slants by 12
 * degrees around the baseline. An outline key renders the glyph grown by
 * the outline width (Dilation): drawn underneath the glyph in another color
 * it makes a border. A blur key renders the glyph (or its outline image)
 * blurred with a Gaussian whose standard deviation is half the radius, as
 * CSS blurs shadows, in a sprite grown to hold the whole blur.
 *
 * Change settings before the first glyph is rendered: sprites already in an
 * atlas keep the settings they were rendered with.
 */
class GlyphRasterizer : public GlyphSource {
public:
	bool render(const Font& font, const GlyphKey& key, Sprite& sprite) override;
	PixelFormat getFormat() const override;
	SpriteEncoding getEncoding() const override;

	void setHinting(bool hinting);
	bool isHinting() const;

	static std::int32_t getLargestSprite();

private:
	const Hinter& getHinter(const Font& font);

	Rasterizer rasterizer;
	std::unordered_map<std::uint64_t, Hinter> hinters;
	bool hinting = true;
};

}
