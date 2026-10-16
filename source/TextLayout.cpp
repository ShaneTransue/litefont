#include <litefont/TextLayout.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace litefont {

void TextLayout::build(const Text& text, float deviceScale) {
	this->glyphs.clear();
	this->lines.clear();
	this->decorations.clear();
	this->lineHeight = 0.0f;
	this->truncated = false;

	const std::string& content = text.getContent();
	const std::uint32_t contentSize = static_cast<std::uint32_t>(content.size());
	const Font* font = text.getFont();
	const bool pixelAligned = text.isPixelAlignedAdvances();
	const Vector2f origin = text.getPosition() * deviceScale;
	const float left = pixelAligned ? std::round(origin.x) : origin.x;

	if (font == nullptr || !font->isLoaded() || !(text.getSize() > 0.0f) || !(deviceScale > 0.0f)) {
		TextLine line;
		line.left = left;
		line.top = origin.y;
		line.baseline = origin.y;
		line.byteCount = contentSize;
		this->lines.push_back(line);
		this->bounds = Rectf{left, origin.y, 0.0f, 0.0f};
		return;
	}

	const FontMetrics& metrics = font->getMetrics();
	const float textSize = text.getSize() * deviceScale;
	const float scale = textSize / static_cast<float>(metrics.unitsPerEm);
	auto toPixels = [](float fontUnits, const Font& face, float size) {
		return fontUnits * size / static_cast<float>(face.getMetrics().unitsPerEm);
	};
	// Pixel-aligned text rounds where each glyph starts and ends, never an
	// advance on its own: lines keep their exact length as the size changes
	// smoothly, and a column lands on the same pixel on every line. Pens add
	// up in double precision, so a tab stop and the spaces that reach it agree.
	auto snap = [pixelAligned](double position) {
		return static_cast<float>(pixelAligned ? std::round(position) : position);
	};

	// Decode once: every code point with its face, glyph, size, baseline
	// shift and advance. The style and script come from the latest-starting
	// style run that covers the byte, or the Text's style on the baseline;
	// the family then picks the face that has the glyph, falling back to
	// other families or the replacement character.
	std::vector<CodePoint> codePoints;
	Utf8 walker(content);
	CodePoint codePoint;
	while (walker.next(codePoint)) {
		codePoints.push_back(codePoint);
	}
	const std::size_t count = codePoints.size();
	const FontFace primary{font, false, false};
	const GlyphId spaceGlyph = font->getGlyph(U' ');
	const std::vector<TextStyleRun>& runs = text.getStyleRuns();
	std::size_t run = 0;
	std::vector<FontFace> faces(count);
	std::vector<GlyphId> glyphIds(count);
	std::vector<float> advances(count);
	std::vector<float> sizes(count, textSize);
	std::vector<float> shifts(count, 0.0f);
	for (std::size_t i = 0; i < count; i++) {
		if (codePoints[i].value == U'\t') {
			faces[i] = primary;
			glyphIds[i] = spaceGlyph;
			continue;
		}
		const std::uint32_t offset = codePoints[i].offset;
		while (run + 1 < runs.size() && runs[run + 1].firstByte <= offset) {
			run++;
		}
		FontStyle style = text.getStyle();
		TextScript script = TextScript::Normal;
		if (run < runs.size() && runs[run].firstByte <= offset && offset - runs[run].firstByte < runs[run].byteCount) {
			style = runs[run].style;
			script = runs[run].script;
		}
		const FontGlyph shown = text.getFamily().resolve(style, codePoints[i].value);
		faces[i] = shown.face;
		glyphIds[i] = shown.glyph;
		if (faces[i].font == nullptr || !faces[i].font->isLoaded()) {
			faces[i] = FontFace{font, FontFamily::isBold(style), FontFamily::isItalic(style)};
			glyphIds[i] = font->getGlyph(codePoints[i].value);
		}
		const Font& face = *faces[i].font;
		if (script != TextScript::Normal) {
			float sizeRatio = 1.0f;
			float offsetUnits = 0.0f;
			TextLayout::getScript(face, script, sizeRatio, offsetUnits);
			sizes[i] = textSize * sizeRatio;
			const float shift = std::round(toPixels(offsetUnits, face, textSize));
			shifts[i] = script == TextScript::Superscript ? -shift : shift;
		}

		// A simulated bold widens the advance by the strength it thickens stems by.
		float advance = static_cast<float>(face.getGlyphMetrics(glyphIds[i]).advance);
		if (faces[i].simulatedBold) {
			advance += FontFamily::getBoldStrength(face);
		}
		advances[i] = toPixels(advance, face, sizes[i]);
	}

	const float fontLineHeight = static_cast<float>(metrics.ascender) - static_cast<float>(metrics.descender) + static_cast<float>(metrics.lineGap);
	// Lines are spaced by the exact line height and each baseline rounds to a
	// whole device pixel on its own: baselines stay on the pixel grid while a
	// paragraph's height follows its size smoothly, lines differing by a pixel.
	this->lineHeight = std::max(1.0f, fontLineHeight * scale * text.getLineSpacing());
	const double firstBaseline = static_cast<double>(origin.y) + static_cast<double>(metrics.ascender) * scale;
	const float wrapWidth = text.getMaxWidth() > 0.0f ? text.getMaxWidth() * deviceScale : 0.0f;
	const double tabStop = static_cast<double>(std::max<std::uint8_t>(text.getTabWidth(), 1))
		* toPixels(static_cast<float>(font->getGlyphMetrics(spaceGlyph).advance), *font, textSize);
	const double letterSpacing = static_cast<double>(text.getLetterSpacing()) * deviceScale;
	constexpr std::size_t noBreak = std::numeric_limits<std::size_t>::max();
	constexpr float tolerance = 1e-3f;
	std::uint32_t cutByte = contentSize;

	std::size_t start = 0;
	while (true) {
		const double lineOffset = static_cast<double>(this->lines.size()) * this->lineHeight;
		TextLine line;
		line.top = static_cast<float>(origin.y + lineOffset);
		line.baseline = static_cast<float>(std::round(firstBaseline + lineOffset));
		line.firstGlyph = static_cast<std::uint32_t>(this->glyphs.size());
		line.firstByte = start < count ? codePoints[start].offset : contentSize;

		auto place = [this, &line](const FontFace& face, GlyphId glyph, float x, float shift, float advance, float size, std::uint32_t byteOffset) {
			PlacedGlyph placed;
			placed.face = face;
			placed.glyph = glyph;
			placed.pen = Vector2f{x, line.baseline + shift};
			placed.advance = advance;
			placed.size = size;
			placed.byteOffset = byteOffset;
			this->glyphs.push_back(placed);
		};

		double x = 0.0;
		double contentEnd = 0.0;
		GlyphId previous = 0;
		const Font* previousFont = nullptr;
		float previousSize = 0.0f;
		bool kernWithPrevious = false;
		// The last place this line may break: after code point breakAfter,
		// keeping that many glyphs and that much width.
		std::size_t breakAfter = noBreak;
		std::size_t glyphsAtBreak = 0;
		double contentEndAtBreak = 0.0;
		std::size_t next = count;
		bool more = false;

		for (std::size_t i = start; i < count; i++) {
			const char32_t value = codePoints[i].value;
			if (value == U'\n' || value == U'\r') {
				next = i + 1;
				if (value == U'\r' && next < count && codePoints[next].value == U'\n') {
					next++;
				}
				more = true;
				break;
			}
			if (value == U'\t') {
				const double stop = tabStop > 0.0 ? (std::floor(x / tabStop + tolerance) + 1.0) * tabStop : x;
				place(primary, spaceGlyph, snap(x), 0.0f, snap(stop) - snap(x), textSize, codePoints[i].offset);
				x = stop;
				kernWithPrevious = false;
				breakAfter = i;
				glyphsAtBreak = this->glyphs.size();
				contentEndAtBreak = contentEnd;
				continue;
			}

			// Kerning pairs belong to one font at one size, so they never cross
			// a change of face or a superscript.
			const GlyphId glyph = glyphIds[i];
			const Font& face = *faces[i].font;
			const bool space = value == U' ';
			double kerning = 0.0;
			if (kernWithPrevious && previousFont == &face && previousSize == sizes[i]) {
				kerning = toPixels(static_cast<float>(face.getKerning(previous, glyph)), face, sizes[i]);
			}
			if (wrapWidth > 0.0f && !space && i > start && x + kerning + advances[i] > wrapWidth + tolerance) {
				if (breakAfter != noBreak) {
					this->glyphs.resize(glyphsAtBreak);
					contentEnd = contentEndAtBreak;
					next = breakAfter + 1;
				}
				else {
					next = i;
				}
				more = true;
				break;
			}

			// Letter spacing follows every glyph but never counts as content.
			const double glyphStart = x + kerning;
			const double glyphEnd = glyphStart + advances[i];
			x = glyphEnd + letterSpacing;
			place(faces[i], glyph, snap(glyphStart), shifts[i], snap(x) - snap(glyphStart), sizes[i], codePoints[i].offset);
			previous = glyph;
			previousFont = &face;
			previousSize = sizes[i];
			kernWithPrevious = true;
			if (!space) {
				contentEnd = glyphEnd;
			}
			if (space || value == U'-' || value == 0x2010) {
				breakAfter = i;
				glyphsAtBreak = this->glyphs.size();
				contentEndAtBreak = contentEnd;
			}
		}

		line.glyphCount = static_cast<std::uint32_t>(this->glyphs.size()) - line.firstGlyph;
		line.width = snap(contentEnd);
		line.byteCount = (next < count ? codePoints[next].offset : contentSize) - line.firstByte;
		this->lines.push_back(line);
		if (!more) {
			break;
		}
		if (text.getMaxLines() > 0 && this->lines.size() >= text.getMaxLines()) {
			// Only text that would show counts as cut, not an empty last line.
			this->truncated = next < count;
			cutByte = next < count ? codePoints[next].offset : contentSize;
			break;
		}
		start = next;
	}

	if (this->truncated && text.hasEllipsis()) {
		// The ellipsis takes the style of the last glyph kept: U+2026 when the
		// family has it, else three periods.
		TextLine& line = this->lines.back();
		FontStyle style = text.getStyle();
		if (line.glyphCount > 0) {
			const TextStyleRun* lastRun = TextLayout::findStyleRun(runs, this->glyphs.back().byteOffset);
			style = lastRun != nullptr ? lastRun->style : style;
		}
		FontGlyph mark = text.getFamily().resolve(style, 0x2026);
		const bool single = mark.face.font != nullptr && mark.face.font->isLoaded() && mark.glyph != 0 && mark.face.font->getGlyph(0x2026) == mark.glyph;
		if (!single) {
			mark = text.getFamily().resolve(style, U'.');
		}
		if (mark.face.font == nullptr || !mark.face.font->isLoaded()) {
			mark = FontGlyph{FontFace{font, FontFamily::isBold(style), FontFamily::isItalic(style)}, font->getGlyph(single ? 0x2026 : U'.')};
		}
		const Font& markFont = *mark.face.font;
		float markUnits = static_cast<float>(markFont.getGlyphMetrics(mark.glyph).advance);
		if (mark.face.simulatedBold) {
			markUnits += FontFamily::getBoldStrength(markFont);
		}
		const double markAdvance = toPixels(markUnits, markFont, textSize);
		const std::int32_t marks = single ? 1 : 3;
		const double ellipsisWidth = markAdvance * marks + letterSpacing * (marks - 1);

		// Trailing spaces go, then glyphs until the ellipsis fits the wrap width.
		std::uint32_t cut = cutByte;
		while (line.glyphCount > 0) {
			const PlacedGlyph& last = this->glyphs.back();
			const char value = content[last.byteOffset];
			const bool blank = value == ' ' || value == '\t';
			const bool fits = wrapWidth <= 0.0f || static_cast<double>(last.pen.x) + last.advance + ellipsisWidth <= wrapWidth + tolerance;
			if (!blank && fits) {
				break;
			}
			cut = last.byteOffset;
			this->glyphs.pop_back();
			line.glyphCount--;
		}
		double x = line.glyphCount > 0 ? static_cast<double>(this->glyphs.back().pen.x) + this->glyphs.back().advance : 0.0;
		double end = x;
		for (std::int32_t k = 0; k < marks; k++) {
			const double markStart = x;
			end = markStart + markAdvance;
			x = end + (k + 1 < marks ? letterSpacing : 0.0);
			PlacedGlyph placed;
			placed.face = mark.face;
			placed.glyph = mark.glyph;
			placed.pen = Vector2f{snap(markStart), line.baseline};
			placed.advance = snap(x) - snap(markStart);
			placed.size = textSize;
			placed.byteOffset = cut;
			this->glyphs.push_back(placed);
			line.glyphCount++;
		}
		line.width = snap(end);
		line.byteCount = cut - line.firstByte;    // the line ends where the text was cut
	}

	this->alignLines(text.getAlign(), left, wrapWidth, pixelAligned);
	this->addDecorations(text, *font, scale);

	// The anchor point of the bounds lands on the position, by whole pixels.
	const Vector2f anchor = text.getAnchor();
	if (anchor.x != 0.0f || anchor.y != 0.0f) {
		const float dx = std::round(origin.x - (this->bounds.x + anchor.x * this->bounds.width));
		const float dy = std::round(origin.y - (this->bounds.y + anchor.y * this->bounds.height));
		this->moveBy(Vector2f{dx, dy});
	}
}

const std::vector<PlacedGlyph>& TextLayout::getGlyphs() const {
	return this->glyphs;
}

const std::vector<TextLine>& TextLayout::getLines() const {
	return this->lines;
}

const std::vector<TextDecoration>& TextLayout::getDecorations() const {
	return this->decorations;
}

float TextLayout::getLineHeight() const {
	return this->lineHeight;
}

Rectf TextLayout::getBounds() const {
	return this->bounds;
}

bool TextLayout::isTruncated() const {
	return this->truncated;
}

void TextLayout::alignLines(TextAlign align, float left, float wrapWidth, bool pixelAligned) {
	// Without a wrap width, lines align with each other inside the widest one.
	float alignWidth = wrapWidth;
	if (alignWidth <= 0.0f) {
		for (const TextLine& line : this->lines) {
			alignWidth = std::max(alignWidth, line.width);
		}
	}

	float minimum = std::numeric_limits<float>::max();
	float maximum = std::numeric_limits<float>::lowest();
	for (TextLine& line : this->lines) {
		const float slack = alignWidth - line.width;
		float offset = 0.0f;
		if (align == TextAlign::Center) {
			offset = slack * 0.5f;
		}
		else if (align == TextAlign::Right) {
			offset = slack;
		}
		line.left = left + (pixelAligned ? std::round(offset) : offset);
		for (std::uint32_t i = line.firstGlyph; i < line.firstGlyph + line.glyphCount; i++) {
			this->glyphs[i].pen.x += line.left;
		}
		minimum = std::min(minimum, line.left);
		maximum = std::max(maximum, line.left + line.width);
	}

	const float top = this->lines.front().top;
	this->bounds = Rectf{minimum, top, maximum - minimum, static_cast<float>(this->lines.size()) * this->lineHeight};
}

void TextLayout::addDecorations(const Text& text, const Font& font, float scale) {
	const std::vector<TextStyleRun>& styleRuns = text.getStyleRuns();
	if (styleRuns.empty()) {
		return;
	}
	// The primary font's recommendations, or ordinary values when it has none.
	const FontMetrics& metrics = font.getMetrics();
	const float em = static_cast<float>(metrics.unitsPerEm);
	const float underlineThickness = metrics.underlineThickness > 0 ? static_cast<float>(metrics.underlineThickness) : 0.05f * em;
	const float underlinePosition = metrics.underlinePosition != 0 ? static_cast<float>(metrics.underlinePosition) : -0.1f * em;
	const float strikeoutSize = metrics.strikeoutSize > 0 ? static_cast<float>(metrics.strikeoutSize) : underlineThickness;
	const float xHeight = metrics.xHeight > 0 ? static_cast<float>(metrics.xHeight) : 0.5f * em;
	const float strikeoutPosition = metrics.strikeoutPosition > 0 ? static_cast<float>(metrics.strikeoutPosition) : (xHeight + strikeoutSize) * 0.5f;
	const std::vector<TextColorRun>& colorRuns = text.getColorRuns();

	for (const TextDecorationKind kind : {TextDecorationKind::Underline, TextDecorationKind::Strikethrough}) {
		const bool underline = kind == TextDecorationKind::Underline;
		const float position = underline ? underlinePosition : strikeoutPosition;
		const float thickness = std::max(1.0f, std::round((underline ? underlineThickness : strikeoutSize) * scale));
		for (const TextLine& line : this->lines) {
			// One span per run of decorated glyphs of one color, never past
			// the line's last visible glyph.
			const float top = std::round(line.baseline - position * scale);
			const float contentEnd = line.left + line.width;
			bool open = false;
			float spanLeft = 0.0f;
			float spanRight = 0.0f;
			std::int32_t spanColor = -1;
			std::uint32_t spanByte = 0;
			auto close = [&]() {
				if (open && spanRight > spanLeft) {
					this->decorations.push_back(TextDecoration{kind, Rectf{spanLeft, top, spanRight - spanLeft, thickness}, spanByte});
				}
				open = false;
			};
			for (std::uint32_t i = line.firstGlyph; i < line.firstGlyph + line.glyphCount; i++) {
				const PlacedGlyph& glyph = this->glyphs[i];
				const TextStyleRun* glyphRun = TextLayout::findStyleRun(styleRuns, glyph.byteOffset);
				const bool wanted = glyphRun != nullptr && (underline ? glyphRun->underline : glyphRun->strikethrough);
				if (!wanted || glyph.pen.x >= contentEnd - 1e-3f) {
					close();
					continue;
				}
				const float glyphLeft = std::round(glyph.pen.x);
				const float glyphRight = std::round(std::min(glyph.pen.x + glyph.advance, contentEnd));
				const std::int32_t color = TextLayout::findColorRun(colorRuns, glyph.byteOffset);
				if (open && color == spanColor) {
					spanRight = std::max(spanRight, glyphRight);
					continue;
				}
				close();
				open = true;
				spanLeft = glyphLeft;
				spanRight = glyphRight;
				spanColor = color;
				spanByte = glyph.byteOffset;
			}
			close();
		}
	}
}

void TextLayout::moveBy(Vector2f delta) {
	for (PlacedGlyph& glyph : this->glyphs) {
		glyph.pen = glyph.pen + delta;
	}
	for (TextLine& line : this->lines) {
		line.left += delta.x;
		line.top += delta.y;
		line.baseline += delta.y;
	}
	for (TextDecoration& decoration : this->decorations) {
		decoration.rect.x += delta.x;
		decoration.rect.y += delta.y;
	}
	this->bounds.x += delta.x;
	this->bounds.y += delta.y;
}

const TextStyleRun* TextLayout::findStyleRun(const std::vector<TextStyleRun>& runs, std::uint32_t byteOffset) {
	// The latest-starting run at or before the byte, when it covers the byte.
	const auto after = std::upper_bound(runs.begin(), runs.end(), byteOffset, [](std::uint32_t offset, const TextStyleRun& run) {
		return offset < run.firstByte;
	});
	if (after == runs.begin()) {
		return nullptr;
	}
	const TextStyleRun& run = *(after - 1);
	return byteOffset - run.firstByte < run.byteCount ? &run : nullptr;
}

std::int32_t TextLayout::findColorRun(const std::vector<TextColorRun>& runs, std::uint32_t byteOffset) {
	// The same rule as style runs; -1 means the Text's own color.
	const auto after = std::upper_bound(runs.begin(), runs.end(), byteOffset, [](std::uint32_t offset, const TextColorRun& run) {
		return offset < run.firstByte;
	});
	if (after == runs.begin()) {
		return -1;
	}
	const std::size_t index = static_cast<std::size_t>(after - runs.begin()) - 1;
	return byteOffset - runs[index].firstByte < runs[index].byteCount ? static_cast<std::int32_t>(index) : -1;
}

void TextLayout::getScript(const Font& face, TextScript script, float& sizeRatio, float& offsetUnits) {
	// Values outside a fifth of an em to a whole em, or offsets of an em or
	// more, are not trusted; both offsets are stored as positive numbers.
	const FontMetrics& metrics = face.getMetrics();
	const float em = static_cast<float>(metrics.unitsPerEm);
	const bool superscript = script == TextScript::Superscript;
	const float size = std::fabs(static_cast<float>(superscript ? metrics.superscriptSize : metrics.subscriptSize));
	const float offset = std::fabs(static_cast<float>(superscript ? metrics.superscriptOffset : metrics.subscriptOffset));
	const bool trusted = size >= 0.2f * em && size <= em && offset < em;
	sizeRatio = trusted ? size / em : 0.65f;
	offsetUnits = trusted ? offset : (superscript ? 0.4f : 0.15f) * em;
}

}
