#pragma once

#include <litefont/Font.h>

#include <array>
#include <vector>

namespace litefont {

/**
 * @addtogroup Text
 *
 * FontStyle - The style a run of text asks for.
 */
enum class FontStyle : std::uint8_t {
	Regular,
	Bold,
	Italic,
	BoldItalic
};

/**
 * \struct FontFace FontFamily.h <litefont/FontFamily.h>
 * @addtogroup Text
 *
 * FontFace - The font that draws a style, and what has to be simulated
 * because the family has no face of its own for it: bold by thickening the
 * outlines, italic by slanting them.
 */
struct FontFace {
	const Font* font = nullptr;
	bool simulatedBold = false;
	bool simulatedItalic = false;

	bool operator==(const FontFace&) const = default;
};

/**
 * \struct FontGlyph FontFamily.h <litefont/FontFamily.h>
 * @addtogroup Text
 *
 * FontGlyph - The glyph that shows a code point and the face it comes from.
 */
struct FontGlyph {
	FontFace face;
	GlyphId glyph = 0;
};

/**
 * \class FontFamily FontFamily.h <litefont/FontFamily.h>
 * @addtogroup Text
 *
 * FontFamily - Up to four faces of one typeface: regular, bold, italic and
 * bold italic (Segoe UI's segoeui, segoeuib, segoeuii and segoeuiz). The
 * fonts are not owned and must outlive the family.
 *
 * resolve() works the way CSS and DirectWrite match fonts: a style uses its
 * own face when the family has it. Otherwise bold italic falls back to the
 * bold face slanted, then the italic face thickened; bold and italic fall
 * back to the regular face. Whatever the chosen face lacks, according to
 * its own traits, is simulated, so a font that is already bold is never
 * thickened again. A family made from one font treats it as regular and
 * simulates every other style.
 *
 * Fallback families fill in the code points this family's fonts lack, so
 * text in Latin can carry Chinese or symbols from another typeface. For a
 * code point, resolve() tries this family's face for the style, then each
 * fallback family's face for the style, in the order they were added; the
 * fallbacks' own fallbacks are not followed. A code point nobody has shows
 * the replacement character (U+FFFD unless set otherwise), else a question
 * mark, else the face's missing glyph. Control characters never fall back.
 * Fallback families are not owned and must outlive the family.
 */
class FontFamily {
public:
	FontFamily() = default;
	explicit FontFamily(const Font& regular);

	bool addFace(const Font& face);
	void setFace(FontStyle style, const Font& face);
	const Font* getFace(FontStyle style) const;
	const Font* getRegular() const;
	FontFace resolve(FontStyle style) const;
	FontGlyph resolve(FontStyle style, char32_t codePoint) const;

	bool addFallback(const FontFamily& fallback);
	void clearFallbacks();
	const std::vector<const FontFamily*>& getFallbacks() const;
	void setReplacementCharacter(char32_t codePoint);
	char32_t getReplacementCharacter() const;

	bool operator==(const FontFamily&) const = default;

	static bool isBold(FontStyle style);
	static bool isItalic(FontStyle style);
	static FontStyle getStyle(bool bold, bool italic);
	static float getBoldStrength(const Font& font);
	static float getItalicSlant();

private:
	bool findGlyph(FontStyle style, char32_t codePoint, FontGlyph& found) const;

	static bool isControl(char32_t codePoint);

	std::array<const Font*, 4> faces{};
	std::vector<const FontFamily*> fallbacks;
	char32_t replacementCharacter = 0xFFFD;
};

}
