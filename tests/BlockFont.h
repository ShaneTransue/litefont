#pragma once

#include "TestFontBuilder.h"

namespace litefont::tests {

/**
 * \class BlockFont BlockFont.h
 *
 * BlockFont - A synthetic font of rectangles for renderer tests, so every
 * glyph image is known exactly: 1000 units per em, ascender 800, descender
 * -200. a is 400 x 500 units, b 300 x 700, p reaches 200 below the baseline,
 * x is a 500 x 500 block, W a 900 x 700 block, and space is empty. The bold
 * face's blocks and advances are 100 units wider and it says it is bold.
 */
class BlockFont {
public:
	static Bytes build(bool bold = false) {
		TestFontBuilder builder;
		builder.setLineMetrics(1000, 800, -200, 0);
		if (bold) {
			builder.setStyle(700, true, false, 0);
		}
		const std::int16_t extra = bold ? 100 : 0;
		auto add = [&builder, extra](char32_t codePoint, std::int16_t left, std::int16_t bottom, std::int16_t right, std::int16_t top, std::uint16_t advance) {
			TestGlyph glyph;
			glyph.advance = static_cast<std::uint16_t>(advance + extra);
			glyph.leftSideBearing = left;
			if (right > left) {
				glyph.contours = {{{left, bottom, true}, {left, top, true}, {static_cast<std::int16_t>(right + extra), top, true},
					{static_cast<std::int16_t>(right + extra), bottom, true}}};
			}
			builder.map(codePoint, builder.addGlyph(glyph));
		};
		add(U'a', 50, 0, 450, 500, 500);
		add(U'b', 50, 0, 350, 700, 400);
		add(U'p', 50, -200, 350, 500, 400);
		add(U'x', 0, 0, 500, 500, 550);
		add(U'W', 0, 0, 900, 700, 1000);
		add(U' ', 0, 0, 0, 0, 250);
		return builder.build();
	}
};

}
