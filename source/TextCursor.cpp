#include <litefont/TextCursor.h>
#include <litefont/Utf8.h>

#include <algorithm>
#include <cmath>

namespace litefont {

void TextCursor::place(std::uint32_t position) {
	this->position = position;
	this->anchor = position;
	this->hasGoal = false;
}

void TextCursor::select(std::uint32_t anchor, std::uint32_t position) {
	this->anchor = anchor;
	this->position = position;
	this->hasGoal = false;
}

void TextCursor::move(const Text& text, const TextLayout& layout, TextMovement movement, bool extend) {
	constexpr std::uint8_t spaceKind = 1;
	constexpr std::uint8_t breakKind = 2;
	const std::uint32_t end = TextCursor::getTextEnd(layout);
	const std::uint32_t current = std::min(this->position, end);
	const bool vertical = movement == TextMovement::LineUp || movement == TextMovement::LineDown;
	if (!vertical) {
		this->hasGoal = false;
	}

	// Without extending, a character move collapses a selection to its edge.
	const bool characterMove = movement == TextMovement::PreviousCharacter || movement == TextMovement::NextCharacter;
	if (!extend && characterMove && this->hasSelection()) {
		const std::uint32_t edge = movement == TextMovement::PreviousCharacter ? this->getSelectionStart() : this->getSelectionEnd();
		this->position = std::min(edge, end);
		this->anchor = this->position;
		return;
	}

	// The visible text's code points, where they start and what they are.
	const std::string_view visible = std::string_view(text.getContent()).substr(0, end);
	std::vector<CodePoint> points;
	Utf8 walker(visible);
	CodePoint codePoint;
	while (walker.next(codePoint)) {
		points.push_back(codePoint);
	}
	const std::size_t count = points.size();
	auto indexOf = [&points](std::uint32_t offset) {
		return static_cast<std::size_t>(std::lower_bound(points.begin(), points.end(), offset, [](const CodePoint& point, std::uint32_t value) {
			return point.offset < value;
		}) - points.begin());
	};
	auto offsetOf = [&points, count, end](std::size_t index) {
		return index < count ? points[index].offset : end;
	};
	auto isPair = [&points, count](std::size_t index) {
		return index + 1 < count && points[index].value == U'\r' && points[index + 1].value == U'\n';
	};

	std::uint32_t target = current;
	const std::vector<TextLine>& lines = layout.getLines();
	switch (movement) {
	case TextMovement::PreviousCharacter: {
		std::size_t i = indexOf(current);
		if (i > 0) {
			i--;
			if (i > 0 && isPair(i - 1)) {
				i--;
			}
			target = offsetOf(i);
		}
		break;
	}
	case TextMovement::NextCharacter: {
		const std::size_t i = indexOf(current);
		if (i < count) {
			target = offsetOf(i + (isPair(i) ? 2 : 1));
		}
		break;
	}
	case TextMovement::NextWord: {
		// Past the rest of this word (or run of punctuation), then past spaces;
		// a line break is a stop of its own.
		std::size_t i = indexOf(current);
		if (i < count) {
			const std::uint8_t kind = TextCursor::getKind(points[i].value);
			if (kind == breakKind) {
				i += isPair(i) ? 2 : 1;
			}
			else {
				while (kind != spaceKind && i < count && TextCursor::getKind(points[i].value) == kind) {
					i++;
				}
				while (i < count && TextCursor::getKind(points[i].value) == spaceKind) {
					i++;
				}
			}
			target = offsetOf(i);
		}
		break;
	}
	case TextMovement::PreviousWord: {
		// Back over spaces, then to the start of the word (or run of
		// punctuation) before them; a line break is a stop of its own.
		std::size_t i = indexOf(current);
		while (i > 0 && TextCursor::getKind(points[i - 1].value) == spaceKind) {
			i--;
		}
		if (i > 0) {
			const std::uint8_t kind = TextCursor::getKind(points[i - 1].value);
			if (kind == breakKind) {
				i -= i > 1 && isPair(i - 2) ? 2 : 1;
			}
			else {
				while (i > 0 && TextCursor::getKind(points[i - 1].value) == kind) {
					i--;
				}
			}
		}
		target = offsetOf(i);
		break;
	}
	case TextMovement::LineStart:
		target = lines[TextCursor::findLine(layout, current)].firstByte;
		break;
	case TextMovement::LineEnd:
		target = TextCursor::getLineEnd(text, layout, TextCursor::findLine(layout, current));
		break;
	case TextMovement::LineUp:
	case TextMovement::LineDown: {
		// From the first line up goes to the start, from the last down to the end.
		const std::uint32_t line = TextCursor::findLine(layout, current);
		if (!this->hasGoal) {
			this->goalX = TextCursor::getCaretX(layout, line, current);
			this->hasGoal = true;
		}
		if (movement == TextMovement::LineUp) {
			target = line == 0 ? 0 : TextCursor::hitLine(text, layout, line - 1, this->goalX);
		}
		else {
			target = line + 1 >= lines.size() ? end : TextCursor::hitLine(text, layout, line + 1, this->goalX);
		}
		break;
	}
	case TextMovement::TextStart:
		target = 0;
		break;
	case TextMovement::TextEnd:
		target = end;
		break;
	}

	this->position = std::min(target, end);
	if (!extend) {
		this->anchor = this->position;
	}
}

std::uint32_t TextCursor::getPosition() const {
	return this->position;
}

std::uint32_t TextCursor::getAnchor() const {
	return this->anchor;
}

bool TextCursor::hasSelection() const {
	return this->position != this->anchor;
}

std::uint32_t TextCursor::getSelectionStart() const {
	return std::min(this->position, this->anchor);
}

std::uint32_t TextCursor::getSelectionEnd() const {
	return std::max(this->position, this->anchor);
}

CaretBox TextCursor::getCaret(const TextLayout& layout) const {
	return TextCursor::getCaretBox(layout, this->position);
}

std::vector<Rectf> TextCursor::getSelectionRects(const Text& text, const TextLayout& layout) const {
	std::vector<Rectf> rects;
	const std::uint32_t textEnd = TextCursor::getTextEnd(layout);
	const std::uint32_t start = std::min(this->getSelectionStart(), textEnd);
	const std::uint32_t end = std::min(this->getSelectionEnd(), textEnd);
	if (start >= end) {
		return rects;
	}
	const std::vector<TextLine>& lines = layout.getLines();
	const float height = layout.getLineHeight();
	const std::uint32_t first = TextCursor::findLine(layout, start);
	std::uint32_t last = TextCursor::findLine(layout, end);
	if (last > first && lines[last].firstByte == end) {
		last--;    // the selection ends where that line starts
	}
	for (std::uint32_t line = first; line <= last; line++) {
		const TextLine& current = lines[line];
		const std::uint32_t lineEnd = TextCursor::getLineEnd(text, layout, line);
		const float left = TextCursor::getCaretX(layout, line, std::max(start, current.firstByte));
		float right = TextCursor::getCaretX(layout, line, std::min(end, lineEnd));
		if (end > lineEnd && line + 1 < lines.size()) {
			// The selection goes on past this line: it covers the trailing
			// spaces, and a quarter of a line height stands for the break.
			right = std::max(right, TextCursor::getGlyphsEnd(layout, line)) + height * 0.25f;
		}
		if (right > left) {
			rects.push_back(Rectf{left, current.top, right - left, height});
		}
	}
	return rects;
}

std::uint32_t TextCursor::hitTest(const Text& text, const TextLayout& layout, Vector2f point) {
	// Lines are evenly spaced: the point's height picks one, clamped to the text.
	const std::vector<TextLine>& lines = layout.getLines();
	const float height = layout.getLineHeight();
	float row = height > 0.0f ? std::floor((point.y - lines.front().top) / height) : 0.0f;
	row = std::clamp(row, 0.0f, static_cast<float>(lines.size() - 1));
	return TextCursor::hitLine(text, layout, static_cast<std::uint32_t>(row), point.x);
}

CaretBox TextCursor::getCaretBox(const TextLayout& layout, std::uint32_t position) {
	const std::uint32_t clamped = std::min(position, TextCursor::getTextEnd(layout));
	const std::uint32_t line = TextCursor::findLine(layout, clamped);
	const TextLine& current = layout.getLines()[line];
	return CaretBox{TextCursor::getCaretX(layout, line, clamped), current.top, current.top + layout.getLineHeight(), line};
}

std::uint32_t TextCursor::getTextEnd(const TextLayout& layout) {
	const TextLine& last = layout.getLines().back();
	return last.firstByte + last.byteCount;
}

Vector2f TextCursor::toLayout(const Text& text, float deviceScale, Vector2f drawnPoint) {
	const Vector2f offset{std::round(text.getOffset().x * deviceScale), std::round(text.getOffset().y * deviceScale)};
	const Vector2f pivot{std::round(text.getPosition().x * deviceScale), std::round(text.getPosition().y * deviceScale)};
	return TextCursor::rotate(drawnPoint - offset, pivot, text.getRotation(), true);
}

Rectf TextCursor::toDrawn(const Text& text, float deviceScale, const Rectf& rect) {
	const Vector2f offset{std::round(text.getOffset().x * deviceScale), std::round(text.getOffset().y * deviceScale)};
	const Vector2f pivot{std::round(text.getPosition().x * deviceScale), std::round(text.getPosition().y * deviceScale)};
	const Vector2f first = TextCursor::rotate(Vector2f{rect.x, rect.y}, pivot, text.getRotation(), false);
	const Vector2f second = TextCursor::rotate(Vector2f{rect.getRight(), rect.getBottom()}, pivot, text.getRotation(), false);
	return Rectf{std::min(first.x, second.x) + offset.x, std::min(first.y, second.y) + offset.y,
		std::fabs(second.x - first.x), std::fabs(second.y - first.y)};
}

std::uint32_t TextCursor::findLine(const TextLayout& layout, std::uint32_t position) {
	// The last line starting at or before the position.
	const std::vector<TextLine>& lines = layout.getLines();
	const auto after = std::upper_bound(lines.begin(), lines.end(), position, [](std::uint32_t value, const TextLine& line) {
		return value < line.firstByte;
	});
	return after == lines.begin() ? 0 : static_cast<std::uint32_t>(after - lines.begin()) - 1;
}

std::uint32_t TextCursor::getLineEnd(const Text& text, const TextLayout& layout, std::uint32_t line) {
	// Before the line break, or before the spaces a wrap left at the end.
	const std::vector<TextLine>& lines = layout.getLines();
	if (line + 1 >= lines.size()) {
		return TextCursor::getTextEnd(layout);
	}
	const std::string& content = text.getContent();
	const TextLine& current = lines[line];
	std::uint32_t end = std::min(current.firstByte + current.byteCount, static_cast<std::uint32_t>(content.size()));
	if (end > current.firstByte && (content[end - 1] == '\n' || content[end - 1] == '\r')) {
		end--;
		if (end > current.firstByte && content[end] == '\n' && content[end - 1] == '\r') {
			end--;
		}
		return end;
	}
	while (end > current.firstByte && (content[end - 1] == ' ' || content[end - 1] == '\t')) {
		end--;
	}
	return end;
}

float TextCursor::getCaretX(const TextLayout& layout, std::uint32_t line, std::uint32_t position) {
	// Before the first glyph at or after the position, else after the last.
	const TextLine& current = layout.getLines()[line];
	const std::vector<PlacedGlyph>& glyphs = layout.getGlyphs();
	for (std::uint32_t i = current.firstGlyph; i < current.firstGlyph + current.glyphCount; i++) {
		if (glyphs[i].byteOffset >= position) {
			return glyphs[i].pen.x;
		}
	}
	return TextCursor::getGlyphsEnd(layout, line);
}

float TextCursor::getGlyphsEnd(const TextLayout& layout, std::uint32_t line) {
	const TextLine& current = layout.getLines()[line];
	if (current.glyphCount == 0) {
		return current.left;
	}
	const PlacedGlyph& last = layout.getGlyphs()[current.firstGlyph + current.glyphCount - 1];
	return last.pen.x + last.advance;
}

std::uint32_t TextCursor::hitLine(const Text& text, const TextLayout& layout, std::uint32_t line, float x) {
	// The glyph whose first half holds x; past every glyph, the line's end.
	const TextLine& current = layout.getLines()[line];
	const std::vector<PlacedGlyph>& glyphs = layout.getGlyphs();
	for (std::uint32_t i = current.firstGlyph; i < current.firstGlyph + current.glyphCount; i++) {
		if (x < glyphs[i].pen.x + glyphs[i].advance * 0.5f) {
			return glyphs[i].byteOffset;
		}
	}
	return TextCursor::getLineEnd(text, layout, line);
}

Vector2f TextCursor::rotate(Vector2f point, Vector2f pivot, TextRotation rotation, bool inverse) {
	// TextRenderer's quarter turns: with y down, clockwise takes right to down.
	TextRotation turn = rotation;
	if (inverse) {
		turn = rotation == TextRotation::Clockwise ? TextRotation::CounterClockwise
			: (rotation == TextRotation::CounterClockwise ? TextRotation::Clockwise : TextRotation::None);
	}
	const float dx = point.x - pivot.x;
	const float dy = point.y - pivot.y;
	if (turn == TextRotation::Clockwise) {
		return Vector2f{pivot.x - dy, pivot.y + dx};
	}
	if (turn == TextRotation::CounterClockwise) {
		return Vector2f{pivot.x + dy, pivot.y - dx};
	}
	return point;
}

std::uint8_t TextCursor::getKind(char32_t codePoint) {
	// 0 a word character (letters, digits, underscore, anything beyond ASCII),
	// 1 a space, 2 a line break, 3 punctuation.
	if (codePoint == U' ' || codePoint == U'\t' || codePoint == 0xA0 || codePoint == 0x3000) {
		return 1;
	}
	if (codePoint == U'\n' || codePoint == U'\r') {
		return 2;
	}
	const bool word = (codePoint >= U'0' && codePoint <= U'9') || (codePoint >= U'A' && codePoint <= U'Z')
		|| (codePoint >= U'a' && codePoint <= U'z') || codePoint == U'_' || codePoint >= 0x80;
	return word ? 0 : 3;
}

}
