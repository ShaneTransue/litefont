#include <litefont/GlyphRasterizer.h>

#include <cmath>

namespace litefont {

bool GlyphRasterizer::render(const Font& font, const GlyphKey& key, Sprite& sprite) {
	sprite = Sprite{};
	Outline outline;
	if (!font.isLoaded() || key.glyph >= font.getGlyphCount() || !font.getOutline(key.glyph, outline)) {
		return false;
	}
	const float size = static_cast<float>(key.sizePx64) / 64.0f;
	if (!(size > 0.0f)) {
		return false;
	}
	if (outline.isEmpty()) {
		return true;
	}

	if (key.simulatedBold) {
		const float strength = FontFamily::getBoldStrength(font);
		outline = outline.getEmboldened(strength, 0.0f).getTransformed(Affine2f::translate(strength * 0.5f, 0.0f));
	}
	if (key.simulatedItalic) {
		Affine2f slant;
		slant.c = FontFamily::getItalicSlant();
		outline = outline.getTransformed(slant);
	}

	const float scale = size / static_cast<float>(font.getMetrics().unitsPerEm);
	const Outline pixels = this->hinting ? this->getHinter(font).apply(outline, size) : outline.getTransformed(Affine2f::scale(scale, scale));
	const float shift = static_cast<float>(key.subpixelBin & 3) * 0.25f;
	const Outline placed = pixels.getTransformed(Affine2f::scale(1.0f, -1.0f).then(Affine2f::translate(shift, 0.0f)));

	// The sprite covers the whole pixels the outline touches, with y down,
	// plus room for the border of an outline image and the spread of a blur.
	const float border = static_cast<float>(key.outlineWidth16) / 16.0f;
	const float sigma = static_cast<float>(key.blurRadius16) / 32.0f;
	const float spread = sigma > 0.0f ? static_cast<float>(Blur::getRadius(sigma)) + 1.0f : 0.0f;
	const float margin = (border > 0.0f ? std::ceil(border) + 1.0f : 0.0f) + spread;
	const Rectf bounds = placed.getBounds();
	const float left = std::floor(bounds.x) - margin;
	const float top = std::floor(bounds.y) - margin;
	const float width = std::ceil(bounds.getRight()) + margin - left;
	const float height = std::ceil(bounds.getBottom()) + margin - top;
	const float largest = static_cast<float>(GlyphRasterizer::getLargestSprite());
	if (!(width >= 0.0f && height >= 0.0f) || width > largest || height > largest || !std::isfinite(left) || !std::isfinite(top)) {
		return false;
	}
	if (width == 0.0f || height == 0.0f) {
		return true;
	}

	sprite.image = Image(static_cast<std::int32_t>(width), static_cast<std::int32_t>(height), PixelFormat::Gray8);
	sprite.origin = Vector2f{left, top};
	const Polylines contours = placed.getTransformed(Affine2f::translate(-left, -top)).flatten(0.02f);
	this->rasterizer.fill(contours, FillRule::NonZero, sprite.image);
	if (border > 0.0f) {
		Dilation::apply(contours, border, sprite.image);
	}
	Blur::gaussian(sprite.image, sigma);
	return true;
}

PixelFormat GlyphRasterizer::getFormat() const {
	return PixelFormat::Gray8;
}

SpriteEncoding GlyphRasterizer::getEncoding() const {
	return SpriteEncoding::Coverage;
}

void GlyphRasterizer::setHinting(bool hinting) {
	this->hinting = hinting;
}

bool GlyphRasterizer::isHinting() const {
	return this->hinting;
}

std::int32_t GlyphRasterizer::getLargestSprite() {
	// Bigger glyphs belong to MSDF (F7); this also stops a damaged font from
	// asking for an enormous image.
	return 2048;
}

const Hinter& GlyphRasterizer::getHinter(const Font& font) {
	const auto found = this->hinters.find(font.getId());
	if (found != this->hinters.end()) {
		return found->second;
	}
	return this->hinters.emplace(font.getId(), Hinter(Hinter::findZones(font), font.getMetrics().unitsPerEm)).first->second;
}

}
