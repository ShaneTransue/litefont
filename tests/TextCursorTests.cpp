#include <litefont/GlyphRasterizer.h>
#include "TestKit.h"
#include "RecordingDevice.h"
#include <litefont/TextCursor.h>
#include <litefont/TextRenderer.h>
#include <litefont/Utf8.h>

#include "TestFontBuilder.h"

#include <cmath>
#include <format>

namespace litefont::tests {

/**
 * Caret geometry and movement on a synthetic font whose numbers are easy to
 * follow: 1000 units per em, so at 10 px a line is 10 px tall, a, b, c, q, x,
 * underscore and e-acute advance 5 px, space 2.5, period 2, hyphen 3, W 10,
 * and U+2026 6.
 */
class TextCursorTests {
	static Bytes makeFont() {
		TestFontBuilder builder;
		builder.setLineMetrics(1000, 800, -200, 0);
		auto add = [&builder](char32_t codePoint, std::uint16_t advance) {
			TestGlyph glyph;
			glyph.advance = advance;
			builder.map(codePoint, builder.addGlyph(glyph));
		};
		for (const char32_t letter : {U'a', U'b', U'c', U'f', U'o', U'r', U'z', U'q', U'x', U'_', char32_t{0xE9}}) {
			add(letter, 500);
		}
		add(U' ', 250);
		add(U'.', 200);
		add(U',', 200);
		add(U'!', 200);
		add(U'-', 300);
		add(U'W', 1000);
		add(0x2026, 600);
		return builder.build();
	}

	static TextLayout layOut(const Text& text, float scale = 1.0f) {
		TextLayout layout;
		layout.build(text, scale);
		return layout;
	}

	// Every caret position: code point boundaries up to the visible end, but not inside a CR LF pair.
	static std::vector<std::uint32_t> getPositions(const Text& text, const TextLayout& layout) {
		std::vector<std::uint32_t> positions;
		const std::uint32_t end = TextCursor::getTextEnd(layout);
		const std::string& content = text.getContent();
		Utf8 walker(content);
		CodePoint point;
		while (walker.next(point) && point.offset < end) {
			const bool insidePair = point.offset > 0 && content[point.offset] == '\n' && content[point.offset - 1] == '\r';
			if (!insidePair) {
				positions.push_back(point.offset);
			}
		}
		positions.push_back(end);
		return positions;
	}

	static inline test::Test roundTrips{"TextCursor: every caret position hit-tests back to itself", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		struct Fixture {
			const char* content;
			float maxWidth;
			float scale;
			TextAlign align;
			bool pixelAligned;
			float letterSpacing;
		};
		const Fixture fixtures[] = {
			{"ab c\ta\xC3\xA9\r\nb\n\nca", 0.0f, 1.0f, TextAlign::Left, false, 0.0f},
			{"aaaa  bbbb cc-ab WWWWWWWW", 30.0f, 1.0f, TextAlign::Left, false, 0.0f},
			{"aaaa  bbbb cc-ab WWWWWWWW", 30.0f, 1.25f, TextAlign::Center, true, 1.5f},
			{"foo bar\r\n\tbaz qux", 40.0f, 1.5f, TextAlign::Right, true, 0.0f},
			{"", 0.0f, 1.0f, TextAlign::Left, false, 0.0f},
		};
		std::int32_t wrong = 0;
		std::int32_t checked = 0;
		for (const Fixture& fixture : fixtures) {
			Text text(fixture.content, font, 10.0f);
			text.setMaxWidth(fixture.maxWidth);
			text.setAlign(fixture.align);
			text.setPixelAlignedAdvances(fixture.pixelAligned);
			text.setLetterSpacing(fixture.letterSpacing);
			text.setStyleRuns({TextStyleRun{1, 1, FontStyle::Regular, TextScript::Superscript}});
			const TextLayout layout = layOut(text, fixture.scale);
			for (const std::uint32_t position : getPositions(text, layout)) {
				const CaretBox box = TextCursor::getCaretBox(layout, position);
				const std::uint32_t hit = TextCursor::hitTest(text, layout, Vector2f{box.x, (box.top + box.bottom) * 0.5f});
				checked++;
				if (hit != position && ++wrong <= 5) {
					context.check(false, std::format("\"{}\": position {} hits {}", fixture.content, position, hit));
				}
			}
		}
		context.checkEqual(wrong, 0);
		context.note(std::format("{} caret positions checked", checked));
	}};

	static inline test::Test clicks{"TextCursor: clicks beyond a line land at its ends, and above or below the text on its first or last line", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		Text text("abc\nab\nabcab", font, 10.0f);
		text.setPosition(Vector2f{20.0f, 30.0f});
		const TextLayout layout = layOut(text);
		context.checkEqual(TextCursor::hitTest(text, layout, Vector2f{0.0f, 35.0f}), std::uint32_t{0});
		context.checkEqual(TextCursor::hitTest(text, layout, Vector2f{500.0f, 35.0f}), std::uint32_t{3});      // before the break
		context.checkEqual(TextCursor::hitTest(text, layout, Vector2f{500.0f, 45.0f}), std::uint32_t{6});
		context.checkEqual(TextCursor::hitTest(text, layout, Vector2f{22.6f, 45.0f}), std::uint32_t{5});      // past the middle of a
		context.checkEqual(TextCursor::hitTest(text, layout, Vector2f{22.4f, 45.0f}), std::uint32_t{4});
		context.checkEqual(TextCursor::hitTest(text, layout, Vector2f{31.0f, -100.0f}), std::uint32_t{2});    // above: the first line
		context.checkEqual(TextCursor::hitTest(text, layout, Vector2f{500.0f, 900.0f}), std::uint32_t{12});   // below: the last line's end

		const CaretBox caret = TextCursor::getCaretBox(layout, 5);
		context.check(caret.x == 25.0f && caret.top == 40.0f && caret.bottom == 50.0f && caret.line == 1, "the caret box sits on its line");
		context.check(TextCursor::getCaretBox(layout, 999).x == 45.0f, "positions past the end clamp to it");
	}};

	static inline test::Test characters{"TextCursor: character and word moves step over code points, CR LF pairs, words and punctuation", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		// f0 o1 o2 sp3 sp4 b5 a6 r7 .8 b9 a10 z11 CR12 LF13 q14 e-acute15 sp17 x18, end 19
		Text text("foo  bar.baz\r\nq\xC3\xA9 x", font, 10.0f);
		const TextLayout layout = layOut(text);
		auto moved = [&text, &layout](std::uint32_t from, TextMovement movement) {
			TextCursor cursor;
			cursor.place(from);
			cursor.move(text, layout, movement, false);
			return cursor.getPosition();
		};
		const std::pair<std::uint32_t, std::uint32_t> next[] = {{11, 12}, {12, 14}, {14, 15}, {15, 17}, {19, 19}};
		for (const auto& [from, to] : next) {
			context.check(moved(from, TextMovement::NextCharacter) == to, std::format("next character from {} is {}", from, to));
		}
		const std::pair<std::uint32_t, std::uint32_t> previous[] = {{14, 12}, {17, 15}, {1, 0}, {0, 0}};
		for (const auto& [from, to] : previous) {
			context.check(moved(from, TextMovement::PreviousCharacter) == to, std::format("previous character from {} is {}", from, to));
		}
		const std::pair<std::uint32_t, std::uint32_t> nextWords[] = {{0, 5}, {5, 8}, {8, 9}, {9, 12}, {12, 14}, {14, 18}, {18, 19}, {3, 5}};
		for (const auto& [from, to] : nextWords) {
			context.check(moved(from, TextMovement::NextWord) == to, std::format("next word from {} is {}", from, to));
		}
		const std::pair<std::uint32_t, std::uint32_t> previousWords[] = {{19, 18}, {18, 14}, {14, 12}, {12, 9}, {9, 8}, {8, 5}, {5, 0}, {0, 0}};
		for (const auto& [from, to] : previousWords) {
			context.check(moved(from, TextMovement::PreviousWord) == to, std::format("previous word from {} is {}", from, to));
		}
		context.checkEqual(moved(17, TextMovement::LineStart), std::uint32_t{14});
		context.checkEqual(moved(2, TextMovement::LineEnd), std::uint32_t{12});
		context.checkEqual(moved(14, TextMovement::LineEnd), std::uint32_t{19});
		context.checkEqual(moved(9, TextMovement::TextStart), std::uint32_t{0});
		context.checkEqual(moved(9, TextMovement::TextEnd), std::uint32_t{19});
	}};

	static inline test::Test lines{"TextCursor: line moves keep their column through short lines, and wrapped lines end before their spaces", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		// Lines start at 0, 9 and 12; the caret at 7 is 35 px in.
		Text text("aaaaaaaa\nab\naaaaaaaa", font, 10.0f);
		const TextLayout layout = layOut(text);
		TextCursor cursor;
		cursor.place(7);
		cursor.move(text, layout, TextMovement::LineDown, false);
		context.checkEqual(cursor.getPosition(), std::uint32_t{11});    // the short line's end
		cursor.move(text, layout, TextMovement::LineDown, false);
		context.checkEqual(cursor.getPosition(), std::uint32_t{19});    // back to 35 px
		cursor.move(text, layout, TextMovement::LineUp, false);
		cursor.move(text, layout, TextMovement::LineUp, false);
		context.checkEqual(cursor.getPosition(), std::uint32_t{7});
		cursor.move(text, layout, TextMovement::LineUp, false);
		context.checkEqual(cursor.getPosition(), std::uint32_t{0});
		cursor.place(14);
		cursor.move(text, layout, TextMovement::LineDown, false);
		context.checkEqual(cursor.getPosition(), std::uint32_t{20});

		// "aaaa  " wraps before "bbbb": its end is before the two spaces.
		Text wrapped("aaaa  bbbb", font, 10.0f);
		wrapped.setMaxWidth(25.0f);
		const TextLayout wrappedLayout = layOut(wrapped);
		context.checkEqual(wrappedLayout.getLines().size(), std::size_t{2});
		cursor.place(1);
		cursor.move(wrapped, wrappedLayout, TextMovement::LineEnd, false);
		context.checkEqual(cursor.getPosition(), std::uint32_t{4});
		cursor.move(wrapped, wrappedLayout, TextMovement::NextCharacter, false);
		cursor.move(wrapped, wrappedLayout, TextMovement::NextCharacter, false);
		context.check(cursor.getPosition() == 6 && cursor.getCaret(wrappedLayout).line == 1, "a wrapped line's next position starts the next line");
	}};

	static inline test::Test selections{"TextCursor: selections extend and collapse, and give one rectangle per line", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		// a0-3 LF4 b5 b6 LF7 c8-11
		Text text("aaaa\nbb\ncccc", font, 10.0f);
		const TextLayout layout = layOut(text);
		TextCursor cursor;
		cursor.place(2);
		for (std::int32_t i = 0; i < 7; i++) {
			cursor.move(text, layout, TextMovement::NextCharacter, true);
		}
		context.check(cursor.hasSelection() && cursor.getAnchor() == 2 && cursor.getPosition() == 9, "seven characters selected");
		const std::vector<Rectf> rects = cursor.getSelectionRects(text, layout);
		context.checkEqual(rects.size(), std::size_t{3});
		context.check(rects.size() == 3 && rects[0] == Rectf{10.0f, 0.0f, 12.5f, 10.0f}, "the first line from a to its end and break");
		context.check(rects.size() == 3 && rects[1] == Rectf{0.0f, 10.0f, 12.5f, 10.0f}, "the whole middle line with its break");
		context.check(rects.size() == 3 && rects[2] == Rectf{0.0f, 20.0f, 5.0f, 10.0f}, "one c of the last line");

		cursor.select(2, 8);
		context.checkEqual(cursor.getSelectionRects(text, layout).size(), std::size_t{2});    // ending where a line starts
		cursor.move(text, layout, TextMovement::PreviousCharacter, false);
		context.check(!cursor.hasSelection() && cursor.getPosition() == 2, "a character move collapses to the start");
		cursor.select(2, 8);
		cursor.move(text, layout, TextMovement::NextCharacter, false);
		context.checkEqual(cursor.getPosition(), std::uint32_t{8});
		cursor.place(4);
		context.check(cursor.getSelectionRects(text, layout).empty(), "no selection, no rectangles");
	}};

	static inline test::Test truncation{"TextCursor: a cut text ends at the ellipsis", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		Text text("aaaa aaaa aaaa", font, 10.0f);
		text.setMaxWidth(24.0f);
		text.setMaxLines(1);
		text.setEllipsis(true);
		const TextLayout layout = layOut(text);
		context.checkEqual(TextCursor::getTextEnd(layout), std::uint32_t{3});
		TextCursor cursor;
		cursor.move(text, layout, TextMovement::TextEnd, false);
		context.checkEqual(cursor.getPosition(), std::uint32_t{3});
		context.checkEqual(TextCursor::getCaretBox(layout, 3).x, 15.0f);
		context.checkEqual(TextCursor::hitTest(text, layout, Vector2f{100.0f, 5.0f}), std::uint32_t{3});
		context.checkEqual(TextCursor::hitTest(text, layout, Vector2f{19.0f, 5.0f}), std::uint32_t{3});    // on the ellipsis
	}};

	static inline test::Test drawnFrame{"TextCursor: toDrawn and toLayout follow the renderer's offset and rotation", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		GlyphRasterizer glyphs;
		test::RecordingDevice device;
		TextRenderer renderer(device, glyphs);
		Text text("abc", font, 10.0f);
		text.setPosition(Vector2f{40.0f, 40.0f});
		text.setOffset(Vector2f{5.0f, -2.0f});
		text.setBackground(TextBackground{Color4f{0.0f, 0.0f, 0.0f, 1.0f}, 0.0f});
		renderer.add(text);
		for (const TextRotation rotation : {TextRotation::None, TextRotation::Clockwise, TextRotation::CounterClockwise}) {
			text.setRotation(rotation);
			renderer.update(1.5f);
			const TextLayout& layout = *renderer.getLayout(TextHandle{1});
			const Rectf bounds = layout.getBounds();
			const Rectf box{std::round(bounds.x), std::round(bounds.y), std::round(bounds.width), std::round(bounds.height)};
			const Rectf drawn = TextCursor::toDrawn(text, 1.5f, box);
			const std::span<const SpriteVertex> quad = renderer.getGeometry().getVertices().first(4);
			float left = quad[0].x;
			float top = quad[0].y;
			float right = quad[0].x;
			float bottom = quad[0].y;
			for (const SpriteVertex& vertex : quad) {
				left = std::min(left, vertex.x);
				top = std::min(top, vertex.y);
				right = std::max(right, vertex.x);
				bottom = std::max(bottom, vertex.y);
			}
			context.check(drawn == Rectf{left, top, right - left, bottom - top}, "toDrawn matches the background quad the renderer drew");

			// A point in the drawn second glyph comes back to it.
			const CaretBox caret = TextCursor::getCaretBox(layout, 1);
			const Rectf point = TextCursor::toDrawn(text, 1.5f, Rectf{caret.x + 1.0f, caret.top + 3.0f, 0.0f, 0.0f});
			const Vector2f back = TextCursor::toLayout(text, 1.5f, Vector2f{point.x, point.y});
			context.check(back == Vector2f{caret.x + 1.0f, caret.top + 3.0f}, "toLayout undoes toDrawn");
			context.checkEqual(TextCursor::hitTest(text, layout, back), std::uint32_t{1});
		}
	}};
};

}
