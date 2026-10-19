#include <litefont/TextPainter.h>

#include <litefont/Hash.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace litefont {

TextPainter::TextPainter(GraphicsDevice& device) : device(device) {
	this->renderer = std::make_unique<TextRenderer>(this->device, this->glyphs, std::uint16_t{1024}, std::uint16_t{8});
}

void TextPainter::drawText(const Font& font, float size, std::string_view text, float x, float y, const Color4f& color) {
	this->drawText(FontFamily(font), FontStyle::Regular, size, text, x, y, color);
}

void TextPainter::drawText(const FontFamily& family, FontStyle style, float size, std::string_view text, float x, float y, const Color4f& color) {
	// The key comes from the arguments, so a cached string costs no allocation.
	const std::uint64_t key = TextPainter::getKey(family, style, size, color, text);
	TextPainterEntry* entry = nullptr;
	const auto found = this->index.find(key);
	if (found != this->index.end()) {
		this->cache.splice(this->cache.begin(), this->cache, found->second);
		entry = &this->cache.front();
	}
	else {
		Text made(std::string(text), family, size);
		made.setStyle(style);
		made.setColor(color);
		entry = &this->find(key, made);
	}
	this->drawEntry(*entry, Vector2f{x, y});
}

void TextPainter::draw(const Text& text, float x, float y) {
	TextPainterEntry& entry = this->find(TextPainter::getKey(text), text);
	if (text.isClipped() && this->device.getSpace() == SpriteSpace::Viewport) {
		entry.text.setClip(text.getClip());
	}
	else {
		entry.text.clearClip();
	}
	this->drawEntry(entry, text.getPosition() + text.getOffset() + Vector2f{x, y});
}

Rectf TextPainter::measureText(const Font& font, float size, std::string_view text) const {
	// Pixels at 100 percent scale, from the point drawText would be given.
	const Text measured(std::string(text), font, size);
	TextLayout layout;
	layout.build(measured, 1.0f);
	return layout.getBounds();
}

void TextPainter::preload(const Font& font, float size, std::string_view characters) {
	const FontFamily family(font);
	const Color4f white{1.0f, 1.0f, 1.0f, 1.0f};
	Text made(std::string(characters), family, size);
	made.setColor(white);
	TextPainterEntry& entry = this->find(TextPainter::getKey(family, FontStyle::Regular, size, white, characters), made);
	this->renderer->prepare(entry.handle, this->scale);
}

void TextPainter::setScale(float scale) {
	this->scale = scale > 0.0f ? scale : 1.0f;
}

float TextPainter::getScale() const {
	return this->scale;
}

void TextPainter::setHinting(bool hinting) {
	this->glyphs.setHinting(hinting);
}

bool TextPainter::isHinting() const {
	return this->glyphs.isHinting();
}

void TextPainter::setSubpixelBins(std::uint8_t bins) {
	this->renderer->setSubpixelBins(bins);
}

void TextPainter::setCacheCapacity(std::size_t capacity) {
	this->capacity = std::max<std::size_t>(capacity, 1);
	while (this->cache.size() > this->capacity) {
		const TextPainterEntry& oldest = this->cache.back();
		this->renderer->remove(oldest.handle);
		this->index.erase(oldest.key);
		this->cache.pop_back();
	}
}

std::size_t TextPainter::getCacheCapacity() const {
	return this->capacity;
}

std::size_t TextPainter::getCachedCount() const {
	return this->cache.size();
}

void TextPainter::clearCache() {
	for (const TextPainterEntry& entry : this->cache) {
		this->renderer->remove(entry.handle);
	}
	this->cache.clear();
	this->index.clear();
}

void TextPainter::release() {
	// Everything starts over: a later draw makes new textures.
	this->cache.clear();
	this->index.clear();
	this->renderer.reset();
	this->device.release();
	this->renderer = std::make_unique<TextRenderer>(this->device, this->glyphs, std::uint16_t{1024}, std::uint16_t{8});
}

GraphicsDevice& TextPainter::getDevice() {
	return this->device;
}

TextRenderer& TextPainter::getTextRenderer() {
	return *this->renderer;
}

float TextPainter::getRasterScale(float pixelScale, float size) {
	// Sixths of an octave, so glyphs are never drawn more than 6 percent
	// larger or smaller than rendered; no glyph is rendered over 256 pixels,
	// which an atlas page always holds, and larger text is magnified.
	if (!(pixelScale > 0.0f) || !std::isfinite(pixelScale)) {
		return 1.0f;
	}
	const float steps = std::round(std::log2(pixelScale) * 6.0f);
	const float stepped = std::exp2(steps / 6.0f);
	const float largest = 256.0f / std::max(size, 1e-6f);
	return std::max(std::min(stepped, largest), 1e-6f);
}

void TextPainter::drawEntry(TextPainterEntry& entry, Vector2f origin) {
	if (this->device.getSpace() == SpriteSpace::Viewport) {
		// Pixels: laid out once at the screen's scale, moved by whole device pixels.
		entry.text.setOffset(origin);
		this->device.setPlacement(Affine2f::identity());
		this->renderer->drawNow(entry.handle, this->scale);
		return;
	}
	// Under a matrix: laid out and rendered at about the size it shows, then
	// scaled back to the matrix's units and moved to the origin there, with y
	// turned over when y is up so the text still reads top to bottom.
	const float rasterScale = TextPainter::getRasterScale(this->device.getPixelScale(origin), entry.text.getSize());
	const float unit = 1.0f / rasterScale;
	entry.text.setOffset(Vector2f{});
	this->device.setPlacement(Affine2f::scale(unit, this->device.isYUp() ? -unit : unit).then(Affine2f::translate(origin.x, origin.y)));
	this->renderer->drawNow(entry.handle, rasterScale);
}

TextPainterEntry& TextPainter::find(std::uint64_t key, const Text& text) {
	const auto found = this->index.find(key);
	if (found != this->index.end()) {
		this->cache.splice(this->cache.begin(), this->cache, found->second);
		return this->cache.front();
	}
	// A new string: a copy laid out at the origin, moved by its offset.
	this->cache.push_front(TextPainterEntry{});
	TextPainterEntry& entry = this->cache.front();
	entry.text = text;
	entry.text.setPosition(Vector2f{});
	entry.text.setOffset(Vector2f{});
	entry.text.clearClip();
	entry.key = key;
	entry.handle = this->renderer->add(entry.text);
	this->index[key] = this->cache.begin();
	while (this->cache.size() > this->capacity) {
		const TextPainterEntry& oldest = this->cache.back();
		this->renderer->remove(oldest.handle);
		this->index.erase(oldest.key);
		this->cache.pop_back();
	}
	return entry;
}

std::uint64_t TextPainter::getKey(const Text& text) {
	// Everything that shapes how the Text looks; not where it is drawn.
	std::uint64_t hash = TextPainter::mixValue(Hash::fnv1a(std::span<const std::uint8_t>()), std::uint8_t{1});
	hash = TextPainter::mixText(hash, text.getContent());
	hash = TextPainter::mixFamily(text.getFamily(), hash);
	hash = TextPainter::mixValue(hash, text.getStyle());
	for (const TextStyleRun& run : text.getStyleRuns()) {
		hash = TextPainter::mixValue(hash, run.firstByte);
		hash = TextPainter::mixValue(hash, run.byteCount);
		hash = TextPainter::mixValue(hash, run.style);
		hash = TextPainter::mixValue(hash, run.script);
		hash = TextPainter::mixValue(hash, run.underline);
		hash = TextPainter::mixValue(hash, run.strikethrough);
	}
	hash = TextPainter::mixValue(hash, text.getSize());
	hash = TextPainter::mixValue(hash, text.getColor().packRGBA8());
	for (const TextColorRun& run : text.getColorRuns()) {
		hash = TextPainter::mixValue(hash, run.firstByte);
		hash = TextPainter::mixValue(hash, run.byteCount);
		hash = TextPainter::mixValue(hash, run.color.packRGBA8());
	}
	hash = TextPainter::mixValue(hash, text.hasGradient());
	if (text.hasGradient()) {
		const TextGradient& gradient = text.getGradient();
		for (const Color4f& corner : {gradient.topLeft, gradient.topRight, gradient.bottomLeft, gradient.bottomRight}) {
			hash = TextPainter::mixValue(hash, corner.packRGBA8());
		}
	}
	hash = TextPainter::mixValue(hash, text.getOutline().width);
	hash = TextPainter::mixValue(hash, text.getOutline().color.packRGBA8());
	hash = TextPainter::mixValue(hash, text.getShadow().offset.x);
	hash = TextPainter::mixValue(hash, text.getShadow().offset.y);
	hash = TextPainter::mixValue(hash, text.getShadow().color.packRGBA8());
	hash = TextPainter::mixValue(hash, text.getShadow().blur);
	hash = TextPainter::mixValue(hash, text.getMaxWidth());
	hash = TextPainter::mixValue(hash, text.getAlign());
	hash = TextPainter::mixValue(hash, text.getLineSpacing());
	hash = TextPainter::mixValue(hash, text.getTabWidth());
	hash = TextPainter::mixValue(hash, text.isPixelAlignedAdvances());
	hash = TextPainter::mixValue(hash, text.getLetterSpacing());
	hash = TextPainter::mixValue(hash, text.getMaxLines());
	hash = TextPainter::mixValue(hash, text.hasEllipsis());
	hash = TextPainter::mixValue(hash, text.getAnchor().x);
	hash = TextPainter::mixValue(hash, text.getAnchor().y);
	hash = TextPainter::mixValue(hash, text.getRotation());
	hash = TextPainter::mixValue(hash, text.getBackground().color.packRGBA8());
	return TextPainter::mixValue(hash, text.getBackground().padding);
}

std::uint64_t TextPainter::getKey(const FontFamily& family, FontStyle style, float size, const Color4f& color, std::string_view text) {
	// A different tag from the Text key, so the two never meet.
	std::uint64_t hash = TextPainter::mixValue(Hash::fnv1a(std::span<const std::uint8_t>()), std::uint8_t{2});
	hash = TextPainter::mixText(hash, text);
	hash = TextPainter::mixFamily(family, hash);
	hash = TextPainter::mixValue(hash, style);
	hash = TextPainter::mixValue(hash, size);
	return TextPainter::mixValue(hash, color.packRGBA8());
}

std::uint64_t TextPainter::mixFamily(const FontFamily& family, std::uint64_t hash) {
	// The fonts themselves (where they live and what they hold) and the fallbacks.
	for (const FontStyle style : {FontStyle::Regular, FontStyle::Bold, FontStyle::Italic, FontStyle::BoldItalic}) {
		const Font* face = family.getFace(style);
		hash = TextPainter::mixValue(hash, reinterpret_cast<std::uintptr_t>(face));
		hash = TextPainter::mixValue(hash, face != nullptr ? face->getId() : 0);
	}
	const Font* regular = family.getRegular();
	hash = TextPainter::mixValue(hash, reinterpret_cast<std::uintptr_t>(regular));
	for (const FontFamily* fallback : family.getFallbacks()) {
		hash = TextPainter::mixValue(hash, reinterpret_cast<std::uintptr_t>(fallback));
		hash = TextPainter::mixValue(hash, fallback->getRegular() != nullptr ? fallback->getRegular()->getId() : 0);
	}
	return TextPainter::mixValue(hash, family.getReplacementCharacter());
}

template <typename T>
std::uint64_t TextPainter::mixValue(std::uint64_t hash, const T& value) {
	// Continues an FNV-1a hash over the bytes of a value.
	std::uint8_t bytes[sizeof(T)];
	std::memcpy(bytes, &value, sizeof(T));
	return Hash::fnv1a(std::span<const std::uint8_t>(bytes, sizeof(T)), hash);
}

std::uint64_t TextPainter::mixText(std::uint64_t hash, std::string_view text) {
	hash = TextPainter::mixValue(hash, text.size());
	return Hash::fnv1a(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()), hash);
}

}
