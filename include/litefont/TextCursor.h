#pragma once

#include <litefont/Rect.h>

#include <litefont/TextLayout.h>

#include <vector>

namespace litefont {

/**
 * @addtogroup Text
 *
 * TextMovement - Where a caret moves: by a character (a CR LF pair counts as
 * one), to the start of the previous or next word, to the start or end of
 * its line, a line up or down at the same x, or to the start or end of the
 * text.
 */
enum class TextMovement : std::uint8_t {
	PreviousCharacter,
	NextCharacter,
	PreviousWord,
	NextWord,
	LineStart,
	LineEnd,
	LineUp,
	LineDown,
	TextStart,
	TextEnd
};

/**
 * \struct CaretBox TextCursor.h <litefont/TextCursor.h>
 * @addtogroup Text
 *
 * CaretBox - Where a caret is drawn in a layout: its x and the top and
 * bottom of its line, in the layout's device pixels, and the line's index.
 */
struct CaretBox {
	float x = 0.0f;
	float top = 0.0f;
	float bottom = 0.0f;
	std::uint32_t line = 0;
};

/**
 * \class TextCursor TextCursor.h <litefont/TextCursor.h>
 * @addtogroup Text
 *
 * TextCursor - A caret and selection in a Text, as byte offsets: the
 * position is where the caret is, the anchor where the selection started
 * (the same when nothing is selected). It answers the geometry an editor
 * needs from a TextLayout: the caret box, the selection rectangles, which
 * position a point hits, and where the caret moves. Editing the text, undo,
 * focus and scrolling belong to whatever owns the cursor.
 *
 * Positions sit between code points, from 0 to the end of the visible text
 * (getTextEnd: the content's size, or where a line limit cut the text). A
 * position that starts a line belongs to that line, so the end of a line is
 * just before its line break, or before the spaces a wrap left at its end.
 * A line broken inside a word or after a hyphen has no position of its own
 * at its end: its end is the next line's start.
 *
 * Geometry is in the layout's frame (device pixels, anchor applied, before
 * the offset and rotation a renderer adds). toLayout() takes a point from
 * the drawn frame into it, and toDrawn() takes a rectangle back out, the
 * way TextRenderer places quads. Vertical moves keep the x the first of
 * them started from, so moving through a short line returns to the same
 * column; any other move or placement forgets it.
 */
class TextCursor {
public:
	void place(std::uint32_t position);
	void select(std::uint32_t anchor, std::uint32_t position);
	void move(const Text& text, const TextLayout& layout, TextMovement movement, bool extend);

	std::uint32_t getPosition() const;
	std::uint32_t getAnchor() const;
	bool hasSelection() const;
	std::uint32_t getSelectionStart() const;
	std::uint32_t getSelectionEnd() const;
	CaretBox getCaret(const TextLayout& layout) const;
	std::vector<Rectf> getSelectionRects(const Text& text, const TextLayout& layout) const;

	static std::uint32_t hitTest(const Text& text, const TextLayout& layout, Vector2f point);
	static CaretBox getCaretBox(const TextLayout& layout, std::uint32_t position);
	static std::uint32_t getTextEnd(const TextLayout& layout);
	static Vector2f toLayout(const Text& text, float deviceScale, Vector2f drawnPoint);
	static Rectf toDrawn(const Text& text, float deviceScale, const Rectf& rect);

private:
	static std::uint32_t findLine(const TextLayout& layout, std::uint32_t position);
	static std::uint32_t getLineEnd(const Text& text, const TextLayout& layout, std::uint32_t line);
	static float getCaretX(const TextLayout& layout, std::uint32_t line, std::uint32_t position);
	static float getGlyphsEnd(const TextLayout& layout, std::uint32_t line);
	static std::uint32_t hitLine(const Text& text, const TextLayout& layout, std::uint32_t line, float x);
	static Vector2f rotate(Vector2f point, Vector2f pivot, TextRotation rotation, bool inverse);
	static std::uint8_t getKind(char32_t codePoint);

	std::uint32_t position = 0;
	std::uint32_t anchor = 0;
	float goalX = 0.0f;
	bool hasGoal = false;
};

}
