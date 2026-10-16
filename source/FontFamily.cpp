#include <litefont/FontFamily.h>

#include <algorithm>

namespace litefont {

FontFamily::FontFamily(const Font& regular) {
	this->faces[static_cast<std::size_t>(FontStyle::Regular)] = &regular;
}

bool FontFamily::addFace(const Font& face) {
	if (!face.isLoaded()) {
		return false;
	}
	const FontTraits& traits = face.getTraits();
	this->faces[static_cast<std::size_t>(FontFamily::getStyle(traits.bold, traits.italic))] = &face;
	return true;
}

void FontFamily::setFace(FontStyle style, const Font& face) {
	this->faces[static_cast<std::size_t>(style)] = &face;
}

const Font* FontFamily::getFace(FontStyle style) const {
	const Font* face = this->faces[static_cast<std::size_t>(style)];
	return face != nullptr && face->isLoaded() ? face : nullptr;
}

const Font* FontFamily::getRegular() const {
	// A family without a regular face still has a primary font: the first one.
	for (const FontStyle style : {FontStyle::Regular, FontStyle::Bold, FontStyle::Italic, FontStyle::BoldItalic}) {
		if (this->faces[static_cast<std::size_t>(style)] != nullptr) {
			return this->faces[static_cast<std::size_t>(style)];
		}
	}
	return nullptr;
}

FontFace FontFamily::resolve(FontStyle style) const {
	// The faces to try, best first.
	std::array<FontStyle, 4> order{};
	switch (style) {
	case FontStyle::Regular:
		order = {FontStyle::Regular, FontStyle::Italic, FontStyle::Bold, FontStyle::BoldItalic};
		break;
	case FontStyle::Bold:
		order = {FontStyle::Bold, FontStyle::Regular, FontStyle::BoldItalic, FontStyle::Italic};
		break;
	case FontStyle::Italic:
		order = {FontStyle::Italic, FontStyle::Regular, FontStyle::BoldItalic, FontStyle::Bold};
		break;
	case FontStyle::BoldItalic:
		order = {FontStyle::BoldItalic, FontStyle::Bold, FontStyle::Italic, FontStyle::Regular};
		break;
	}

	FontFace face;
	for (const FontStyle candidate : order) {
		face.font = this->getFace(candidate);
		if (face.font != nullptr) {
			break;
		}
	}
	if (face.font == nullptr) {
		face.font = this->getRegular();
	}
	if (face.font != nullptr) {
		const FontTraits& traits = face.font->getTraits();
		face.simulatedBold = FontFamily::isBold(style) && !traits.bold;
		face.simulatedItalic = FontFamily::isItalic(style) && !traits.italic;
	}
	return face;
}

FontGlyph FontFamily::resolve(FontStyle style, char32_t codePoint) const {
	FontGlyph found;
	if (FontFamily::isControl(codePoint)) {
		found.face = this->resolve(style);
		found.glyph = found.face.font != nullptr ? found.face.font->getGlyph(codePoint) : GlyphId(0);
		return found;
	}
	if (this->findGlyph(style, codePoint, found)) {
		return found;
	}
	for (const char32_t substitute : {this->replacementCharacter, char32_t{U'?'}}) {
		if (substitute != codePoint && substitute != 0 && this->findGlyph(style, substitute, found)) {
			return found;
		}
	}
	found.face = this->resolve(style);
	found.glyph = 0;
	return found;
}

bool FontFamily::addFallback(const FontFamily& fallback) {
	if (&fallback == this || std::find(this->fallbacks.begin(), this->fallbacks.end(), &fallback) != this->fallbacks.end()) {
		return false;
	}
	this->fallbacks.push_back(&fallback);
	return true;
}

void FontFamily::clearFallbacks() {
	this->fallbacks.clear();
}

const std::vector<const FontFamily*>& FontFamily::getFallbacks() const {
	return this->fallbacks;
}

void FontFamily::setReplacementCharacter(char32_t codePoint) {
	this->replacementCharacter = codePoint;
}

char32_t FontFamily::getReplacementCharacter() const {
	return this->replacementCharacter;
}

bool FontFamily::findGlyph(FontStyle style, char32_t codePoint, FontGlyph& found) const {
	auto search = [style, codePoint, &found](const FontFamily& family) {
		const FontFace face = family.resolve(style);
		if (face.font == nullptr || !face.font->isLoaded()) {
			return false;
		}
		const GlyphId glyph = face.font->getGlyph(codePoint);
		if (glyph == 0) {
			return false;
		}
		found = FontGlyph{face, glyph};
		return true;
	};
	if (search(*this)) {
		return true;
	}
	for (const FontFamily* fallback : this->fallbacks) {
		if (search(*fallback)) {
			return true;
		}
	}
	return false;
}

bool FontFamily::isControl(char32_t codePoint) {
	return codePoint < 0x20 || (codePoint >= 0x7F && codePoint < 0xA0);
}

bool FontFamily::isBold(FontStyle style) {
	return style == FontStyle::Bold || style == FontStyle::BoldItalic;
}

bool FontFamily::isItalic(FontStyle style) {
	return style == FontStyle::Italic || style == FontStyle::BoldItalic;
}

FontStyle FontFamily::getStyle(bool bold, bool italic) {
	if (bold) {
		return italic ? FontStyle::BoldItalic : FontStyle::Bold;
	}
	return italic ? FontStyle::Italic : FontStyle::Regular;
}

float FontFamily::getBoldStrength(const Font& font) {
	// FreeType's emboldening: stems thicken, and the advance grows, by 1/24 em.
	return static_cast<float>(font.getMetrics().unitsPerEm) / 24.0f;
}

float FontFamily::getItalicSlant() {
	// tan(12 degrees), FreeType's oblique and the angle of most italic faces.
	return 0.21255656f;
}

}
