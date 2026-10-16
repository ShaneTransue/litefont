#pragma once

#include <litefont/Rect.h>

#include <litefont/Text.h>
#include <litefont/Utf8.h>

#include <vector>

namespace litefont {

/**
 * \struct PlacedGlyph TextLayout.h <litefont/TextLayout.h>
 * @addtogroup Text
 *
 * PlacedGlyph - One glyph of a laid-out Text: the face it comes from (glyph
 * ids belong to one font), its origin and its advance (kerning included in
 * the position, letter spacing in the advance), in device pixels, the size
 * it renders at in device pixels, and the byte offset of the code point it
 * shows. Superscripts and subscripts are smaller and their origin sits off
 * the baseline. Tabs show the primary font's space glyph with the advance to
 * the next tab stop; line breaks have no glyph. An ellipsis at a cut carries
 * the byte offset where the text was cut.
 */
struct PlacedGlyph {
	FontFace face;
	GlyphId glyph = 0;
	Vector2f pen;
	float advance = 0.0f;
	float size = 0.0f;
	std::uint32_t byteOffset = 0;
};

/**
 * @addtogroup Text
 *
 * TextDecorationKind - A line drawn through a run of glyphs.
 */
enum class TextDecorationKind : std::uint8_t {
	Underline,
	Strikethrough
};

/**
 * \struct TextDecoration TextLayout.h <litefont/TextLayout.h>
 * @addtogroup Text
 *
 * TextDecoration - An underline or strikethrough span: its rectangle in whole
 * device pixels and the byte offset of its first glyph, whose color it takes.
 */
struct TextDecoration {
	TextDecorationKind kind = TextDecorationKind::Underline;
	Rectf rect;
	std::uint32_t byteOffset = 0;
};

/**
 * \struct TextLine TextLayout.h <litefont/TextLayout.h>
 * @addtogroup Text
 *
 * TextLine - One line of a laid-out Text in device pixels: where it starts
 * after alignment, the top of its box, its baseline, and its width without
 * trailing spaces and tabs. Its glyph and byte ranges cover the line's break
 * and any spaces absorbed at a wrap, so the byte ranges of all lines join up,
 * from the start of the content to its end, or to where a line limit cut it
 * (after an ellipsis, the bytes it replaced are past the last line).
 */
struct TextLine {
	float left = 0.0f;
	float top = 0.0f;
	float baseline = 0.0f;
	float width = 0.0f;
	std::uint32_t firstGlyph = 0;
	std::uint32_t glyphCount = 0;
	std::uint32_t firstByte = 0;
	std::uint32_t byteCount = 0;
};

/**
 * \class TextLayout TextLayout.h <litefont/TextLayout.h>
 * @addtogroup Text
 *
 * TextLayout - Places the glyphs of one Text, in device pixels, following a
 * few predictable paragraph rules:
 *
 * - Each byte's style (from the style runs, else the Text's style) picks a
 *   face from the family, or from a fallback family when the family's face
 *   lacks the code point (see FontFamily). Glyphs advance by their face's
 *   advances plus kerning; a simulated bold widens the advance by its
 *   strength. Kerning never crosses a line break, a wrap, a tab or a change
 *   of font.
 * - LF, CR LF and CR each end a line once. Empty text still has one line,
 *   and text ending in a break has an empty last line, so a caret can sit
 *   there.
 * - With a wrap width, lines break after spaces, tabs and hyphens, and a word
 *   longer than the line breaks between characters. Spaces at a wrap stay at
 *   the end of the line they follow, and trailing spaces and tabs never count
 *   toward a line's width or its alignment.
 * - Tabs advance to the next stop, every tab-width spaces from the line start.
 * - Line metrics, tab stops and alignment come from the family's regular
 *   (primary) font, so styles never move lines off the pixel grid.
 * - The line height is the font's ascender minus descender plus line gap,
 *   times the line spacing, kept exact. Each baseline is the top plus the
 *   ascender plus its line's offset, rounded on its own, so every baseline
 *   sits on a whole device pixel while a paragraph's height follows its size
 *   smoothly; at sizes between whole-pixel line heights, gaps between lines
 *   differ by one pixel.
 * - With pixel-aligned advances, each glyph's start and end round to whole
 *   device pixels, as do the start of each line and alignment offsets, while
 *   advances, kerning and tab stops stay exact. Every glyph starts on a
 *   pixel, monospace columns line up from line to line, and a line stays
 *   within half a pixel of its exact length, so text grows smoothly as its
 *   size or scale changes; at sizes between whole-pixel advances, cells of a
 *   monospace font differ by one pixel.
 * - Letter spacing goes between glyphs: it widens every advance but never
 *   counts after a line's last glyph.
 * - Superscripts and subscripts take the face's recommended size and offset
 *   (OS/2), or 0.65 em raised 0.4 em and lowered 0.15 em when the font has
 *   none; the offset rounds to whole device pixels. Kerning never crosses a
 *   change of size.
 * - With a line limit, layout stops after that many lines. If text was cut
 *   and the Text asks for an ellipsis, the last line's trailing spaces go
 *   and, with a wrap width, glyphs come off its end until the ellipsis fits;
 *   the ellipsis is U+2026, or three periods when the family has none.
 * - Underlines and strikethroughs follow the runs that ask for them, one span
 *   per line and color, never over a line's trailing spaces. Their position
 *   and thickness come from the primary font (post and OS/2), or 0.1 em
 *   below the baseline, 0.05 em thick and through the middle of the x-height
 *   when it has none, rounded to whole device pixels, at least one thick.
 * - Finally the whole layout moves by whole device pixels so the anchor
 *   point of its bounds lands on the position.
 *
 * findStyleRun() and findColorRun() apply the run rule every user of runs
 * shares: the latest-starting run at or before a byte, if it covers the byte
 * (a color run index of -1 means the Text's own color).
 *
 * The device scale multiplies the Text's position, size, wrap width and
 * letter spacing. A Text without a loaded font or with a size of 0 gets one
 * empty line. Rotation, offset, clip and background are drawing matters, so
 * the layout ignores them.
 */
class TextLayout {
public:
	void build(const Text& text, float deviceScale);

	const std::vector<PlacedGlyph>& getGlyphs() const;
	const std::vector<TextLine>& getLines() const;
	const std::vector<TextDecoration>& getDecorations() const;
	float getLineHeight() const;
	Rectf getBounds() const;
	bool isTruncated() const;

	static const TextStyleRun* findStyleRun(const std::vector<TextStyleRun>& runs, std::uint32_t byteOffset);
	static std::int32_t findColorRun(const std::vector<TextColorRun>& runs, std::uint32_t byteOffset);

private:
	void alignLines(TextAlign align, float left, float wrapWidth, bool pixelAligned);
	void addDecorations(const Text& text, const Font& font, float scale);
	void moveBy(Vector2f delta);

	static void getScript(const Font& face, TextScript script, float& sizeRatio, float& offsetUnits);

	std::vector<PlacedGlyph> glyphs;
	std::vector<TextLine> lines;
	std::vector<TextDecoration> decorations;
	float lineHeight = 0.0f;
	Rectf bounds;
	bool truncated = false;
};

}
