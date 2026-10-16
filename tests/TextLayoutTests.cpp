#include <litefont/TextLayout.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include "BlockFont.h"
#include "TestFontBuilder.h"

#include <cmath>
#include <vector>

namespace litefont::tests {

/**
 * Layout tests with a synthetic font whose numbers are easy to follow: 1000
 * units per em, ascender 800, descender -200, so at 10 px a line is 10 px
 * tall with its baseline 8 px down. Advances at 10 px: a, b, c 5 px, space
 * 2.5 px, hyphen 3 px, W 10 px, A and V 6 px, and A V kerns by -1 px.
 * Every expected position below is worked out by hand from those numbers.
 */
class TextLayoutTests {
	static Bytes makeFont(std::int16_t lineGap = 0) {
		TestFontBuilder builder;
		builder.setLineMetrics(1000, 800, -200, lineGap);
		auto add = [&builder](char32_t codePoint, std::uint16_t advance) {
			TestGlyph glyph;
			glyph.advance = advance;
			const std::uint16_t id = builder.addGlyph(glyph);
			builder.map(codePoint, id);
			return id;
		};
		const std::uint16_t a = add(U'a', 500);
		add(U'b', 500);
		add(U'c', 500);
		add(U' ', 250);
		add(U'-', 300);
		add(U'W', 1000);
		const std::uint16_t capitalA = add(U'A', 600);
		const std::uint16_t capitalV = add(U'V', 600);
		builder.map(0x00E9, a);
		builder.addKerning(capitalA, capitalV, -100);
		return builder.build();
	}

	static std::vector<float> getPenX(const TextLayout& layout) {
		std::vector<float> values;
		for (const PlacedGlyph& glyph : layout.getGlyphs()) {
			values.push_back(glyph.pen.x);
		}
		return values;
	}

	static TextLayout layOut(const Font& font, std::string content, float maxWidth = 0.0f, float deviceScale = 1.0f) {
		Text text(std::move(content), font, 10.0f);
		text.setMaxWidth(maxWidth);
		TextLayout layout;
		layout.build(text, deviceScale);
		return layout;
	}

	static bool isWhole(float value) {
		return value == std::round(value);
	}

	static inline test::Test advances{"TextLayout: glyphs advance by their advances plus kerning along the baseline", [](test::Context& context) {
		Font font;
		context.check(font.load(makeFont()), "the test font loads");
		const TextLayout layout = layOut(font, "aAVb");
		context.check(getPenX(layout) == std::vector<float>{0.0f, 5.0f, 10.0f, 16.0f}, "A V kerns by -1 px");
		context.checkEqual(layout.getLines().size(), std::size_t{1});
		context.checkEqual(layout.getLines()[0].width, 21.0f);
		context.checkEqual(layout.getLines()[0].baseline, 8.0f);
		context.checkEqual(layout.getGlyphs()[2].pen.y, 8.0f);
		context.checkEqual(layout.getGlyphs()[3].byteOffset, std::uint32_t{3});
		context.checkEqual(layout.getLineHeight(), 10.0f);

		const TextLayout accented = layOut(font, "\u00E9a");
		context.checkEqual(accented.getGlyphs()[1].byteOffset, std::uint32_t{2});
		context.checkEqual(accented.getLines()[0].byteCount, std::uint32_t{3});
	}};

	static inline test::Test breaks{"TextLayout: LF, CR LF and CR each end a line once; empty text has one line", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		const TextLayout layout = layOut(font, "a\nb\r\nc\rd");
		const std::vector<TextLine>& lines = layout.getLines();
		context.checkEqual(lines.size(), std::size_t{4});
		const std::uint32_t firstBytes[] = {0, 2, 5, 7};
		const std::uint32_t byteCounts[] = {2, 3, 2, 1};
		for (std::size_t i = 0; i < lines.size() && i < 4; i++) {
			context.checkEqual(lines[i].firstByte, firstBytes[i]);
			context.checkEqual(lines[i].byteCount, byteCounts[i]);
			context.checkEqual(lines[i].glyphCount, std::uint32_t{1});
			context.checkEqual(lines[i].top, static_cast<float>(i) * 10.0f);
			context.checkEqual(lines[i].baseline, 8.0f + static_cast<float>(i) * 10.0f);
		}

		const TextLayout empty = layOut(font, "");
		context.checkEqual(empty.getLines().size(), std::size_t{1});
		context.checkEqual(empty.getGlyphs().size(), std::size_t{0});
		context.checkEqual(empty.getLines()[0].baseline, 8.0f);

		const TextLayout trailing = layOut(font, "a\n");
		context.checkEqual(trailing.getLines().size(), std::size_t{2});
		context.checkEqual(trailing.getLines()[1].firstByte, std::uint32_t{2});
		context.checkEqual(trailing.getLines()[1].byteCount, std::uint32_t{0});
	}};

	static inline test::Test noKerningAcrossLines{"TextLayout: kerning never crosses a line break or a wrap", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		context.checkEqual(layOut(font, "A\nV").getGlyphs()[1].pen.x, 0.0f);
		const TextLayout wrapped = layOut(font, "AV", 8.0f);
		context.checkEqual(wrapped.getLines().size(), std::size_t{2});
		context.checkEqual(wrapped.getGlyphs()[1].pen.x, 0.0f);
		context.checkEqual(wrapped.getLines()[1].width, 6.0f);
	}};

	static inline test::Test wrapsAtSpaces{"TextLayout: lines wrap after spaces, which stay behind and do not count", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		const TextLayout layout = layOut(font, "aa bb cc", 14.0f);
		const std::vector<TextLine>& lines = layout.getLines();
		context.checkEqual(lines.size(), std::size_t{3});
		if (lines.size() == 3) {
			context.check(lines[0].glyphCount == 3 && lines[1].glyphCount == 3 && lines[2].glyphCount == 2, "aa_, bb_, cc");
			context.check(lines[0].width == 10.0f && lines[1].width == 10.0f && lines[2].width == 10.0f, "the spaces do not count");
			context.check(lines[1].firstByte == 3 && lines[1].byteCount == 3 && lines[2].firstByte == 6, "byte ranges join up");
			context.checkEqual(layout.getGlyphs()[3].pen.x, 0.0f);
		}

		const TextLayout spaces = layOut(font, "aa   bb", 12.0f);
		context.checkEqual(spaces.getLines().size(), std::size_t{2});
		context.checkEqual(spaces.getLines()[0].glyphCount, std::uint32_t{5});
		context.checkEqual(spaces.getLines()[0].width, 10.0f);
		context.checkEqual(spaces.getLines()[1].firstByte, std::uint32_t{5});
	}};

	static inline test::Test wrapsWords{"TextLayout: lines wrap after hyphens, and long words break between characters", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		const TextLayout hyphen = layOut(font, "aa-bb", 14.0f);
		context.checkEqual(hyphen.getLines().size(), std::size_t{2});
		context.checkEqual(hyphen.getLines()[0].width, 13.0f);
		context.checkEqual(hyphen.getLines()[1].firstByte, std::uint32_t{3});

		const TextLayout word = layOut(font, "aaaaaaa", 12.0f);
		context.checkEqual(word.getLines().size(), std::size_t{4});
		bool counts = word.getLines().size() == 4;
		for (std::size_t i = 0; counts && i < 4; i++) {
			counts = word.getLines()[i].glyphCount == (i < 3 ? 2u : 1u);
		}
		context.check(counts, "two glyphs fit per line");

		const TextLayout wide = layOut(font, "WW", 4.0f);
		context.checkEqual(wide.getLines().size(), std::size_t{2});
		context.checkEqual(wide.getLines()[0].glyphCount, std::uint32_t{1});
	}};

	static inline test::Test tabs{"TextLayout: tabs advance to the next stop", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		context.check(getPenX(layOut(font, "a\tb")) == std::vector<float>{0.0f, 5.0f, 10.0f}, "from 5 to the stop at 10");
		context.check(getPenX(layOut(font, "aa\tb")) == std::vector<float>{0.0f, 5.0f, 10.0f, 20.0f}, "on a stop, to the next one");
		context.check(getPenX(layOut(font, "\t\tb")) == std::vector<float>{0.0f, 10.0f, 20.0f}, "two tabs");
		context.checkEqual(layOut(font, "a\tb").getGlyphs()[1].advance, 5.0f);
		context.checkEqual(layOut(font, "a\tb").getGlyphs()[1].glyph, font.getGlyph(U' '));

		Text narrow("a\tb", font, 10.0f);
		narrow.setTabWidth(2);
		TextLayout layout;
		layout.build(narrow, 1.0f);
		context.checkEqual(layout.getGlyphs()[2].pen.x, 10.0f);
	}};

	static inline test::Test trailingSpaces{"TextLayout: trailing spaces change neither the width nor the alignment", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		for (const TextAlign align : {TextAlign::Right, TextAlign::Center}) {
			Text plain("ab", font, 10.0f);
			plain.setMaxWidth(30.0f);
			plain.setAlign(align);
			Text spaced("ab   ", font, 10.0f);
			spaced.setMaxWidth(30.0f);
			spaced.setAlign(align);
			TextLayout first;
			first.build(plain, 1.0f);
			TextLayout second;
			second.build(spaced, 1.0f);
			const float expected = align == TextAlign::Right ? 20.0f : 10.0f;
			context.checkEqual(second.getLines()[0].width, 10.0f);
			context.checkEqual(second.getLines()[0].left, expected);
			context.checkEqual(second.getGlyphs()[0].pen.x, first.getGlyphs()[0].pen.x);
		}
	}};

	static inline test::Test alignsLines{"TextLayout: without a wrap width, lines align inside the widest one", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		const TextAlign aligns[] = {TextAlign::Left, TextAlign::Center, TextAlign::Right};
		const float expected[] = {0.0f, 5.0f, 10.0f};
		for (std::size_t i = 0; i < 3; i++) {
			Text text("aaaa\nab", font, 10.0f);
			text.setAlign(aligns[i]);
			text.setPosition(Vector2f{100.0f, 50.0f});
			TextLayout layout;
			layout.build(text, 1.0f);
			context.checkEqual(layout.getLines()[1].left, 100.0f + expected[i]);
			context.checkEqual(layout.getGlyphs()[4].pen.x, 100.0f + expected[i]);
			context.checkEqual(layout.getBounds(), Rectf{100.0f, 50.0f, 20.0f, 20.0f});
		}
	}};

	static inline test::Test pixelGrid{"TextLayout: baselines are whole device pixels within half a pixel of exact line spacing", [](test::Context& context) {
		Font font;
		font.load(makeFont(70));
		std::int32_t wrong = 0;
		for (const float scale : {1.0f, 1.25f, 1.5f, 2.0f}) {
			for (const float spacing : {1.0f, 1.3f}) {
				Text text("a\nb\nc", font, 11.0f);
				text.setPosition(Vector2f{0.0f, 3.3f});
				text.setLineSpacing(spacing);
				TextLayout layout;
				layout.build(text, scale);
				const float lineHeight = 1070.0f * 0.011f * spacing * scale;
				wrong += std::fabs(layout.getLineHeight() - lineHeight) < 1e-3f ? 0 : 1;
				for (std::size_t i = 0; i < layout.getLines().size(); i++) {
					const float exact = 3.3f * scale + 800.0f * 0.011f * scale + static_cast<float>(i) * lineHeight;
					const float baseline = layout.getLines()[i].baseline;
					wrong += isWhole(baseline) && std::fabs(baseline - exact) <= 0.5f + 1e-3f ? 0 : 1;
				}
			}
		}
		context.checkEqual(wrong, 0);

		// A tall block follows its size within a pixel at every step, never
		// holding still and then jumping by a pixel per line. The first and last
		// baselines round on their own, so the span may dip by one pixel.
		float previousSpan = 0.0f;
		bool smooth = true;
		for (std::int32_t step = 0; step <= 200; step++) {
			const float size = 10.0f + 0.05f * static_cast<float>(step);
			Text block("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12", font, size);
			TextLayout layout;
			layout.build(block, 1.0f);
			const float span = layout.getLines().back().baseline - layout.getLines().front().baseline;
			smooth = smooth && span >= previousSpan - 1.0f && std::fabs(span - 11.0f * layout.getLineHeight()) <= 1.0f + 1e-3f;
			previousSpan = span;
		}
		context.check(smooth, "the block's height follows its size within a pixel");
	}};

	static inline test::Test pixelAligned{"TextLayout: pixel-aligned advances start every glyph on a whole device pixel", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		std::int32_t wrong = 0;
		for (const float scale : {1.0f, 1.25f, 1.5f, 2.0f}) {
			Text text("abc\tab\nc a-b", font, 11.0f);
			text.setPosition(Vector2f{0.4f, 0.0f});
			text.setPixelAlignedAdvances(true);
			text.setAlign(TextAlign::Center);
			text.setMaxWidth(23.3f);
			TextLayout layout;
			layout.build(text, scale);
			for (const PlacedGlyph& glyph : layout.getGlyphs()) {
				wrong += isWhole(glyph.pen.x) && isWhole(glyph.advance) ? 0 : 1;
			}
		}
		context.checkEqual(wrong, 0);

		// Positions round, not advances: 0, 5.5 and 11 px land on 0, 6 and 11.
		Text text("abc", font, 11.0f);
		text.setPixelAlignedAdvances(true);
		TextLayout layout;
		layout.build(text, 1.0f);
		context.check(getPenX(layout) == std::vector<float>{0.0f, 6.0f, 11.0f}, "whole-pixel positions");
		context.checkEqual(layout.getGlyphs()[1].advance, 5.0f);    // from 6 to 11
	}};

	static inline test::Test pixelAlignedGrowth{"TextLayout: pixel-aligned lines grow smoothly with the size, and columns line up", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		// Every glyph stays within half a pixel of its exact position, so a line
		// never holds still and then jumps as the size changes.
		float farthest = 0.0f;
		float previousEnd = 0.0f;
		bool monotonic = true;
		for (std::int32_t step = 0; step <= 200; step++) {
			const float size = 10.0f + 0.05f * static_cast<float>(step);
			Text text("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", font, size);
			text.setPixelAlignedAdvances(true);
			TextLayout layout;
			layout.build(text, 1.0f);
			const std::vector<PlacedGlyph>& glyphs = layout.getGlyphs();
			for (std::size_t i = 0; i < glyphs.size(); i++) {
				farthest = std::max(farthest, std::fabs(glyphs[i].pen.x - 0.5f * size * static_cast<float>(i)));
			}
			const float end = glyphs.back().pen.x + glyphs.back().advance;
			monotonic = monotonic && end >= previousEnd;
			previousEnd = end;
		}
		context.check(farthest <= 0.5f + 1e-3f, "within half a pixel of the exact position");
		context.check(monotonic, "the line never shrinks as the size grows");

		// A tab and the spaces that reach the same stop put the next column on the same pixel.
		for (const float scale : {1.0f, 1.21f, 1.5f, 1.77f}) {
			Text text("\tab\n    ab", font, 13.0f);
			text.setPixelAlignedAdvances(true);
			text.setTabWidth(4);    // four spaces of 0.25 em
			TextLayout layout;
			layout.build(text, scale);
			const std::vector<PlacedGlyph>& glyphs = layout.getGlyphs();
			context.check(glyphs.size() == 9 && glyphs[1].pen.x == glyphs[7].pen.x && glyphs[2].pen.x == glyphs[8].pen.x,
				std::format("columns line up at {:.0f} percent", scale * 100.0f));
		}
	}};

	static inline test::Test deviceScale{"TextLayout: the device scale multiplies position, size and wrap width", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		Text text("aa bb cc", font, 10.0f);
		text.setMaxWidth(14.0f);
		text.setPosition(Vector2f{3.0f, 4.0f});
		TextLayout single;
		single.build(text, 1.0f);
		TextLayout twice;
		twice.build(text, 2.0f);
		context.checkEqual(twice.getLines().size(), single.getLines().size());
		bool doubled = twice.getGlyphs().size() == single.getGlyphs().size();
		for (std::size_t i = 0; doubled && i < single.getGlyphs().size(); i++) {
			doubled = twice.getGlyphs()[i].pen.x == 2.0f * single.getGlyphs()[i].pen.x;
		}
		context.check(doubled, "every pen doubles");
		context.checkEqual(twice.getLineHeight(), 20.0f);
		context.checkEqual(twice.getLines()[0].baseline, 24.0f);
	}};

	static inline test::Test noFont{"TextLayout: without a font or a size there is one empty line", [](test::Context& context) {
		Text text;
		text.setContent("abc");
		text.setPosition(Vector2f{5.0f, 6.0f});
		TextLayout layout;
		layout.build(text, 1.0f);
		context.checkEqual(layout.getLines().size(), std::size_t{1});
		context.checkEqual(layout.getGlyphs().size(), std::size_t{0});
		context.checkEqual(layout.getLines()[0].byteCount, std::uint32_t{3});
		context.checkEqual(layout.getBounds(), Rectf{5.0f, 6.0f, 0.0f, 0.0f});

		Font font;
		font.load(makeFont());
		Text sizeless("abc", font, 0.0f);
		layout.build(sizeless, 1.0f);
		context.checkEqual(layout.getGlyphs().size(), std::size_t{0});
	}};

	static inline test::Test styleRuns{"TextLayout: style runs pick faces from the family and simulate missing ones", [](test::Context& context) {
		Font regular;
		Font bold;
		regular.load(BlockFont::build());
		bold.load(BlockFont::build(true));
		FontFamily family(regular);
		family.addFace(bold);

		// a is 5 px in the regular face and 6 px in the bold face at 10 px.
		Text text("aaa", family, 10.0f);
		text.setStyleRuns({TextStyleRun{1, 1, FontStyle::Bold}});
		TextLayout layout;
		layout.build(text, 1.0f);
		context.check(getPenX(layout) == std::vector<float>{0.0f, 5.0f, 11.0f}, "the bold a is wider");
		context.check(layout.getGlyphs()[1].face == FontFace{&bold, false, false}, "from the bold face");
		context.check(layout.getGlyphs()[2].face == FontFace{&regular, false, false}, "and back to regular");
		context.checkEqual(layout.getLineHeight(), 10.0f);

		// A lone font simulates bold: the advance grows by 1/24 em.
		Text simulated("aaa", regular, 10.0f);
		simulated.setStyleRuns({TextStyleRun{1, 1, FontStyle::Bold}, TextStyleRun{2, 1, FontStyle::Italic}});
		layout.build(simulated, 1.0f);
		context.check(std::fabs(layout.getGlyphs()[1].advance - (5.0f + 10.0f / 24.0f)) < 1e-4f, "simulated bold widens the advance");
		context.check(layout.getGlyphs()[1].face == FontFace{&regular, true, false}, "simulated bold");
		context.check(layout.getGlyphs()[2].face == FontFace{&regular, false, true} && layout.getGlyphs()[2].advance == 5.0f, "simulated italic keeps the advance");

		Text whole("ab", regular, 10.0f);
		whole.setStyle(FontStyle::BoldItalic);
		layout.build(whole, 1.0f);
		context.check(layout.getGlyphs()[0].face == FontFace{&regular, true, true}, "the Text's own style applies outside runs");
	}};

	static inline test::Test kerningFaces{"TextLayout: kerning never crosses a change of font", [](test::Context& context) {
		Font regular;
		Font copy;
		regular.load(makeFont());
		copy.load(makeFont());
		FontFamily family(regular);
		family.setFace(FontStyle::Bold, copy);
		Text same("AV", family, 10.0f);
		same.setStyleRuns({TextStyleRun{1, 1, FontStyle::Italic}});
		TextLayout layout;
		layout.build(same, 1.0f);
		context.checkEqual(layout.getGlyphs()[1].pen.x, 5.0f);
		Text other("AV", family, 10.0f);
		other.setStyleRuns({TextStyleRun{1, 1, FontStyle::Bold}});
		layout.build(other, 1.0f);
		context.checkEqual(layout.getGlyphs()[1].pen.x, 6.0f);
	}};

	static inline test::Test fallbackGlyphs{"TextLayout: missing code points take glyph and advance from a fallback family", [](test::Context& context) {
		Font latin;
		latin.load(makeFont());
		TestFontBuilder builder;
		builder.setLineMetrics(2048, 1800, -248, 0);
		TestGlyph wide;
		wide.advance = 2048;
		builder.map(0x4E2D, builder.addGlyph(wide));
		builder.map(U'V', builder.addGlyph(wide));
		builder.addKerning(1, 2, -1024);
		Font chinese;
		chinese.load(builder.build());
		const FontFamily fallback(chinese);
		FontFamily family(latin);
		family.addFallback(fallback);

		Text text("A\xE4\xB8\xAD" "Va", family, 10.0f);
		TextLayout layout;
		layout.build(text, 1.0f);
		const std::vector<PlacedGlyph>& glyphs = layout.getGlyphs();
		context.checkEqual(glyphs.size(), std::size_t{4});
		context.check(glyphs[1].face.font == &chinese && glyphs[1].glyph == 1, "U+4E2D comes from the fallback");
		context.check(glyphs[2].face.font == &latin, "V is the family's own");
		context.checkEqual(glyphs[1].pen.x, 6.0f);           // no kerning into the fallback
		context.checkEqual(glyphs[1].advance, 10.0f);        // 2048 of 2048 units at 10 px
		context.checkEqual(glyphs[2].pen.x, 16.0f);          // nor out of it, though the fallback kerns that pair
		context.checkEqual(layout.getLineHeight(), 10.0f);   // line metrics stay the primary font's
	}};

	static inline test::Test installedFonts{"TextLayout: on the local fonts, pens are the sums of the font's advances and kerning", [](test::Context& context) {
		const std::string sample = "AVATAR To Wave, Lay Yearly \u00C9t\u00E9 7.62x39";
		for (const char* name : {"segoeui", "bahnschrift", "segoeuib"}) {
			Font font;
			if (!font.load(test::Context::getFontFolder() / (std::string(name) + ".ttf"))) {
				context.note(std::format("{} is not in the fonts folder; skipped", name));
				continue;
			}
			const float size = 13.0f;
			const float scale = size / static_cast<float>(font.getMetrics().unitsPerEm);
			Text text(sample, font, size);
			TextLayout layout;
			layout.build(text, 1.0f);

			Utf8 walker(sample);
			CodePoint codePoint;
			float pen = 0.0f;
			GlyphId previous = 0;
			std::size_t i = 0;
			std::int32_t kerned = 0;
			float largestError = 0.0f;
			while (walker.next(codePoint)) {
				const GlyphId glyph = font.getGlyph(codePoint.value);
				const std::int16_t kerning = i > 0 ? font.getKerning(previous, glyph) : std::int16_t{0};
				kerned += kerning != 0 ? 1 : 0;
				pen += static_cast<float>(kerning) * scale;
				if (i < layout.getGlyphs().size()) {
					largestError = std::fmax(largestError, std::fabs(layout.getGlyphs()[i].pen.x - pen));
					context.check(layout.getGlyphs()[i].glyph == glyph, "the mapped glyph");
				}
				pen += static_cast<float>(font.getGlyphMetrics(glyph).advance) * scale;
				previous = glyph;
				i++;
			}
			context.checkEqual(layout.getGlyphs().size(), i);
			context.check(largestError < 1e-3f, "pens match");
			context.check(kerned > 0, "the sample exercises kerning");
			context.note(std::format("{}: {} glyphs, {} kerned pairs, largest difference {} px", name, i, kerned, largestError));
		}

		Font mono;
		if (mono.load(test::Context::getFontFolder() / "consola.ttf")) {
			std::int32_t wrong = 0;
			for (const float scale : {1.0f, 1.25f, 1.5f}) {
				Text code("for (std::size_t i = 0; i < count; i++) {\n\tsum += values[i];\t// add\n}", mono, 13.0f);
				code.setPixelAlignedAdvances(true);
				TextLayout layout;
				layout.build(code, scale);
				for (const PlacedGlyph& glyph : layout.getGlyphs()) {
					wrong += isWhole(glyph.pen.x) ? 0 : 1;
				}
			}
			context.checkEqual(wrong, 0);
		}
	}};

	// a 500, space 250, period 200 and, when asked, U+2026 600 units: at 10 px
	// that is 5, 2.5, 2 and 6 px.
	static Bytes makeEllipsisFont(bool withEllipsis) {
		TestFontBuilder builder;
		builder.setLineMetrics(1000, 800, -200, 0);
		auto add = [&builder](char32_t codePoint, std::uint16_t advance) {
			TestGlyph glyph;
			glyph.advance = advance;
			builder.map(codePoint, builder.addGlyph(glyph));
		};
		add(U'a', 500);
		add(U' ', 250);
		add(U'.', 200);
		if (withEllipsis) {
			add(0x2026, 600);
		}
		return builder.build();
	}

	static inline test::Test letterSpacing{"TextLayout: letter spacing goes between glyphs, never after the last", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		Text text("abc", font, 10.0f);
		text.setLetterSpacing(2.0f);
		TextLayout layout;
		layout.build(text, 1.0f);
		context.check(getPenX(layout) == std::vector<float>{0.0f, 7.0f, 14.0f}, "7 px apart");
		context.checkEqual(layout.getGlyphs()[0].advance, 7.0f);
		context.checkEqual(layout.getLines()[0].width, 19.0f);
		layout.build(text, 2.0f);
		context.checkEqual(layout.getGlyphs()[1].pen.x, 14.0f);    // spacing scales with the device
	}};

	static inline test::Test lineLimit{"TextLayout: a line limit cuts the text, and an ellipsis takes the place of what no longer fits", [](test::Context& context) {
		Font withEllipsis;
		Font withoutEllipsis;
		withEllipsis.load(makeEllipsisFont(true));
		withoutEllipsis.load(makeEllipsisFont(false));
		const GlyphId ellipsis = withEllipsis.getGlyph(0x2026);
		auto layOutLimited = [](const Font& font, std::string_view content, float maxWidth, std::uint32_t maxLines, bool ellipsized) {
			Text text(std::string(content), font, 10.0f);
			text.setMaxWidth(maxWidth);
			text.setMaxLines(maxLines);
			text.setEllipsis(ellipsized);
			TextLayout layout;
			layout.build(text, 1.0f);
			return layout;
		};

		// The trailing space goes and the ellipsis fits after the second word.
		const TextLayout wide = layOutLimited(withEllipsis, "aaaa aaaa aaaa", 50.0f, 1, true);
		context.check(wide.isTruncated() && wide.getLines().size() == 1, "one line, cut");
		context.checkEqual(wide.getGlyphs().size(), std::size_t{10});
		context.check(wide.getGlyphs().back().glyph == ellipsis && wide.getGlyphs().back().pen.x == 42.5f, "U+2026 after aaaa aaaa");
		context.checkEqual(wide.getGlyphs().back().byteOffset, std::uint32_t{9});
		context.checkEqual(wide.getLines()[0].width, 48.5f);

		// Narrower: a letter comes off too, so the ellipsis fits within 24 px.
		const TextLayout narrow = layOutLimited(withEllipsis, "aaaa aaaa aaaa", 24.0f, 1, true);
		context.check(getPenX(narrow) == std::vector<float>{0.0f, 5.0f, 10.0f, 15.0f}, "aaa then the ellipsis");
		context.checkEqual(narrow.getGlyphs().back().byteOffset, std::uint32_t{3});
		context.checkEqual(narrow.getLines()[0].width, 21.0f);

		const TextLayout periods = layOutLimited(withoutEllipsis, "aaaa aaaa aaaa", 24.0f, 1, true);
		context.check(getPenX(periods) == std::vector<float>{0.0f, 5.0f, 10.0f, 15.0f, 17.0f, 19.0f}, "three periods without U+2026");

		const TextLayout cutOnly = layOutLimited(withEllipsis, "aaaa aaaa aaaa", 50.0f, 1, false);
		context.check(cutOnly.isTruncated() && cutOnly.getGlyphs().size() == 10 && cutOnly.getGlyphs().back().glyph != ellipsis, "no ellipsis unless asked");
		const TextLayout fits = layOutLimited(withEllipsis, "aaaa aaaa", 24.0f, 2, true);
		context.check(!fits.isTruncated() && fits.getLines().size() == 2, "text within the limit is not cut");
		const TextLayout emptyLast = layOutLimited(withEllipsis, "a\n", 0.0f, 1, true);
		context.check(!emptyLast.isTruncated() && emptyLast.getLines().size() == 1 && emptyLast.getGlyphs().size() == 1, "an empty last line is not cut text");
	}};

	static inline test::Test scripts{"TextLayout: superscripts and subscripts are smaller and off the baseline, and never kern", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		Text text("aba", font, 10.0f);
		text.setStyleRuns({TextStyleRun{1, 1, FontStyle::Regular, TextScript::Superscript}});
		TextLayout layout;
		layout.build(text, 1.0f);
		const std::vector<PlacedGlyph>& raised = layout.getGlyphs();
		context.check(raised[1].size == 6.5f && raised[1].pen.y == 4.0f && raised[0].pen.y == 8.0f, "0.65 em, 0.4 em up by default");
		context.checkEqual(raised[2].pen.x, 8.25f);
		context.checkEqual(raised[0].size, 10.0f);

		text.setStyleRuns({TextStyleRun{1, 1, FontStyle::Regular, TextScript::Subscript}});
		layout.build(text, 1.0f);
		context.check(layout.getGlyphs()[1].pen.y == 10.0f, "0.15 em down, rounded to a whole pixel");

		Text kerned("AV", font, 10.0f);
		kerned.setStyleRuns({TextStyleRun{1, 1, FontStyle::Regular, TextScript::Superscript}});
		layout.build(kerned, 1.0f);
		context.checkEqual(layout.getGlyphs()[1].pen.x, 6.0f);    // A V kerns by -1 px only at one size

		FontMetrics recommended;
		recommended.superscriptSize = 700;
		recommended.superscriptOffset = 300;
		recommended.subscriptSize = 600;
		recommended.subscriptOffset = 140;
		TestFontBuilder builder;
		builder.setLineMetrics(1000, 800, -200, 0).setDecorationMetrics(recommended);
		TestGlyph glyph;
		builder.map(U'a', builder.addGlyph(glyph));
		Font own;
		own.load(builder.build());
		context.check(own.getMetrics().superscriptSize == 700 && own.getMetrics().subscriptOffset == 140, "the font's values are read");
		Text both("aaa", own, 10.0f);
		both.setStyleRuns({TextStyleRun{0, 1, FontStyle::Regular, TextScript::Superscript}, TextStyleRun{2, 1, FontStyle::Regular, TextScript::Subscript}});
		layout.build(both, 1.0f);
		context.check(layout.getGlyphs()[0].size == 7.0f && layout.getGlyphs()[0].pen.y == 5.0f, "the font's superscript");
		context.check(layout.getGlyphs()[2].size == 6.0f && layout.getGlyphs()[2].pen.y == 9.0f, "the font's subscript");
	}};

	static inline test::Test decorations{"TextLayout: underlines and strikethroughs span their runs per line and color", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		auto decorate = [&font](std::string_view content, std::vector<TextStyleRun> runs, std::vector<TextColorRun> colors) {
			Text text(std::string(content), font, 10.0f);
			text.setStyleRuns(std::move(runs));
			text.setColorRuns(std::move(colors));
			TextLayout layout;
			layout.build(text, 1.0f);
			return layout.getDecorations();
		};
		const Rectf underline{0.0f, 9.0f, 10.0f, 1.0f};
		const std::vector<TextDecoration> under = decorate("abc", {TextStyleRun{0, 2, FontStyle::Regular, TextScript::Normal, true, false}}, {});
		context.check(under.size() == 1 && under[0].kind == TextDecorationKind::Underline && under[0].rect == underline, "0.1 em below, one pixel thick");

		const std::vector<TextDecoration> struck = decorate("abc", {TextStyleRun{0, 3, FontStyle::Regular, TextScript::Normal, false, true}}, {});
		context.check(struck.size() == 1 && struck[0].kind == TextDecorationKind::Strikethrough && struck[0].rect == Rectf{0.0f, 5.0f, 15.0f, 1.0f}, "through the middle");

		const std::vector<TextDecoration> spaced = decorate("ab c", {TextStyleRun{0, 4, FontStyle::Regular, TextScript::Normal, true, false}}, {});
		context.check(spaced.size() == 1 && spaced[0].rect.width == 18.0f, "one span across a space");
		const std::vector<TextDecoration> trailing = decorate("ab ", {TextStyleRun{0, 3, FontStyle::Regular, TextScript::Normal, true, false}}, {});
		context.check(trailing.size() == 1 && trailing[0].rect.width == 10.0f, "never over trailing spaces");
		const std::vector<TextDecoration> colored = decorate("ab", {TextStyleRun{0, 2, FontStyle::Regular, TextScript::Normal, true, false}},
			{TextColorRun{0, 1, Color4f{1.0f, 0.0f, 0.0f, 1.0f}}});
		context.check(colored.size() == 2 && colored[0].rect.width == 5.0f && colored[1].byteOffset == 1, "split where the color changes");
		context.check(decorate("abc", {}, {}).empty(), "nothing without runs");

		FontMetrics recommended;
		recommended.underlinePosition = -150;
		recommended.underlineThickness = 100;
		recommended.strikeoutSize = 80;
		recommended.strikeoutPosition = 300;
		TestFontBuilder builder;
		builder.setLineMetrics(1000, 800, -200, 0).setDecorationMetrics(recommended);
		TestGlyph glyph;
		glyph.advance = 500;
		builder.map(U'a', builder.addGlyph(glyph));
		Font own;
		own.load(builder.build());
		Text text("aa", own, 20.0f);
		text.setStyleRuns({TextStyleRun{0, 2, FontStyle::Regular, TextScript::Normal, true, true}});
		TextLayout layout;
		layout.build(text, 1.0f);
		const std::vector<TextDecoration>& lines = layout.getDecorations();
		context.check(lines.size() == 2 && lines[0].rect == Rectf{0.0f, 19.0f, 20.0f, 2.0f} && lines[1].rect == Rectf{0.0f, 10.0f, 20.0f, 2.0f},
			"the font's underline and strikeout at 20 px");
	}};

	static inline test::Test anchors{"TextLayout: the anchor point of the bounds lands on the position", [](test::Context& context) {
		Font font;
		font.load(makeFont());
		Text text("ab", font, 10.0f);
		text.setPosition(Vector2f{100.0f, 50.0f});
		text.setAnchor(Vector2f{0.5f, 1.0f});
		text.setStyleRuns({TextStyleRun{0, 2, FontStyle::Regular, TextScript::Normal, true, false}});
		TextLayout layout;
		layout.build(text, 1.0f);
		context.check(layout.getBounds() == Rectf{95.0f, 40.0f, 10.0f, 10.0f}, "bottom center on the position");
		context.check(getPenX(layout) == std::vector<float>{95.0f, 100.0f} && layout.getGlyphs()[0].pen.y == 48.0f, "glyphs move with it");
		context.check(layout.getLines()[0].top == 40.0f && layout.getDecorations()[0].rect.x == 95.0f, "lines and decorations too");

		text.setAnchor(Vector2f{1.0f, 0.5f});
		layout.build(text, 1.0f);
		context.check(layout.getBounds().x == 90.0f && layout.getBounds().y == 45.0f, "right middle");
	}};
};

}
