#include <litefont/Utf8.h>
#include <litefont/Font.h>
#include <litefont/FontFamily.h>
#include <litefont/FontCatalog.h>
#include <litefont/Hinter.h>
#include <litefont/Text.h>
#include <litefont/TextLayout.h>
#include <litefont/TextCursor.h>
#include <litefont/GlyphSource.h>
#include <litefont/TextRenderer.h>

#include <algorithm>
#include <utility>

namespace litefont {

template <typename T>
void Text::change(T& member, T value) {
	if (member == value) {
		return;
	}
	member = std::move(value);
	this->revision++;
}

Text::Text(std::string content, const Font& font, float size) {
	this->content = std::move(content);
	this->family = FontFamily(font);
	this->size = size;
}

Text::Text(std::string content, const FontFamily& family, float size) {
	this->content = std::move(content);
	this->family = family;
	this->size = size;
}

void Text::setContent(std::string content) {
	this->change(this->content, std::move(content));
}

const std::string& Text::getContent() const {
	return this->content;
}

void Text::setFont(const Font& font) {
	this->change(this->family, FontFamily(font));
}

const Font* Text::getFont() const {
	return this->family.getRegular();
}

void Text::setFamily(const FontFamily& family) {
	this->change(this->family, family);
}

const FontFamily& Text::getFamily() const {
	return this->family;
}

void Text::setStyle(FontStyle style) {
	this->change(this->style, style);
}

FontStyle Text::getStyle() const {
	return this->style;
}

void Text::setStyleRuns(std::vector<TextStyleRun> styleRuns) {
	std::stable_sort(styleRuns.begin(), styleRuns.end(), [](const TextStyleRun& left, const TextStyleRun& right) {
		return left.firstByte < right.firstByte;
	});
	this->change(this->styleRuns, std::move(styleRuns));
}

const std::vector<TextStyleRun>& Text::getStyleRuns() const {
	return this->styleRuns;
}

void Text::setSize(float size) {
	this->change(this->size, size);
}

float Text::getSize() const {
	return this->size;
}

void Text::setPosition(Vector2f position) {
	this->change(this->position, position);
}

Vector2f Text::getPosition() const {
	return this->position;
}

void Text::setColor(Color4f color) {
	this->change(this->color, color);
}

Color4f Text::getColor() const {
	return this->color;
}

void Text::setColorRuns(std::vector<TextColorRun> colorRuns) {
	// Sorted by first byte, so a renderer can walk runs and glyphs together.
	std::stable_sort(colorRuns.begin(), colorRuns.end(), [](const TextColorRun& left, const TextColorRun& right) {
		return left.firstByte < right.firstByte;
	});
	this->change(this->colorRuns, std::move(colorRuns));
}

const std::vector<TextColorRun>& Text::getColorRuns() const {
	return this->colorRuns;
}

void Text::setGradient(const TextGradient& gradient) {
	if (this->shaded && this->gradient == gradient) {
		return;
	}
	this->gradient = gradient;
	this->shaded = true;
	this->revision++;
}

void Text::clearGradient() {
	if (!this->shaded) {
		return;
	}
	this->gradient = TextGradient{};
	this->shaded = false;
	this->revision++;
}

const TextGradient& Text::getGradient() const {
	return this->gradient;
}

bool Text::hasGradient() const {
	return this->shaded;
}

Color4f TextGradient::getColor(float u, float v) const {
	// Along the top and bottom edges first, then between them.
	const float x = std::clamp(u, 0.0f, 1.0f);
	const float y = std::clamp(v, 0.0f, 1.0f);
	auto mix = [](const Color4f& a, const Color4f& b, float t) {
		return Color4f{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
	};
	return mix(mix(this->topLeft, this->topRight, x), mix(this->bottomLeft, this->bottomRight, x), y);
}

TextGradient TextGradient::vertical(const Color4f& top, const Color4f& bottom) {
	return TextGradient{top, top, bottom, bottom};
}

TextGradient TextGradient::horizontal(const Color4f& left, const Color4f& right) {
	return TextGradient{left, right, left, right};
}

void Text::setOutline(const TextOutline& outline) {
	this->change(this->outline, outline);
}

const TextOutline& Text::getOutline() const {
	return this->outline;
}

void Text::setShadow(const TextShadow& shadow) {
	this->change(this->shadow, shadow);
}

const TextShadow& Text::getShadow() const {
	return this->shadow;
}

void Text::setMaxWidth(float maxWidth) {
	this->change(this->maxWidth, maxWidth);
}

float Text::getMaxWidth() const {
	return this->maxWidth;
}

void Text::setAlign(TextAlign align) {
	this->change(this->align, align);
}

TextAlign Text::getAlign() const {
	return this->align;
}

void Text::setLineSpacing(float lineSpacing) {
	this->change(this->lineSpacing, lineSpacing);
}

float Text::getLineSpacing() const {
	return this->lineSpacing;
}

void Text::setTabWidth(std::uint8_t tabWidth) {
	this->change(this->tabWidth, tabWidth);
}

std::uint8_t Text::getTabWidth() const {
	return this->tabWidth;
}

void Text::setPixelAlignedAdvances(bool pixelAlignedAdvances) {
	this->change(this->pixelAlignedAdvances, pixelAlignedAdvances);
}

bool Text::isPixelAlignedAdvances() const {
	return this->pixelAlignedAdvances;
}

void Text::setLetterSpacing(float letterSpacing) {
	this->change(this->letterSpacing, letterSpacing);
}

float Text::getLetterSpacing() const {
	return this->letterSpacing;
}

void Text::setMaxLines(std::uint32_t maxLines) {
	this->change(this->maxLines, maxLines);
}

std::uint32_t Text::getMaxLines() const {
	return this->maxLines;
}

void Text::setEllipsis(bool ellipsis) {
	this->change(this->ellipsis, ellipsis);
}

bool Text::hasEllipsis() const {
	return this->ellipsis;
}

void Text::setAnchor(Vector2f anchor) {
	this->change(this->anchor, anchor);
}

Vector2f Text::getAnchor() const {
	return this->anchor;
}

void Text::setRotation(TextRotation rotation) {
	this->change(this->rotation, rotation);
}

TextRotation Text::getRotation() const {
	return this->rotation;
}

void Text::setBackground(const TextBackground& background) {
	this->change(this->background, background);
}

const TextBackground& Text::getBackground() const {
	return this->background;
}

void Text::setOffset(Vector2f offset) {
	// Placement only: no new layout, so the revision stays.
	this->offset = offset;
}

Vector2f Text::getOffset() const {
	return this->offset;
}

void Text::setClip(const Rectf& clip) {
	// Placement only: no new layout, so the revision stays.
	this->clip = clip;
	this->clipped = true;
}

void Text::clearClip() {
	this->clip = Rectf{};
	this->clipped = false;
}

const Rectf& Text::getClip() const {
	return this->clip;
}

bool Text::isClipped() const {
	return this->clipped;
}

std::uint32_t Text::getRevision() const {
	return this->revision;
}

}
