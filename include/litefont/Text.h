#pragma once

#include <litefont/Color4.h>
#include <litefont/Font.h>
#include <litefont/FontFamily.h>
#include <litefont/Rect.h>
#include <string>
#include <string_view>
#include <litefont/Vector2.h>

#include <vector>

namespace litefont {

/**
 * @addtogroup Text
 *
 * TextAlign - Where lines sit horizontally: against the left edge, centered
 * or against the right edge of the wrap width (or of the widest line when the
 * text does not wrap).
 */
enum class TextAlign : std::uint8_t {
	Left,
	Center,
	Right
};

/**
 * @addtogroup Text
 *
 * TextScript - Where a run sits on the line: on the baseline, raised and
 * smaller (superscript) or lowered and smaller (subscript), with the sizes
 * and offsets the font recommends.
 */
enum class TextScript : std::uint8_t {
	Normal,
	Superscript,
	Subscript
};

/**
 * @addtogroup Text
 *
 * TextRotation - How a Text turns about its position, in quarter turns:
 * not at all, clockwise (reading top to bottom) or counterclockwise (reading
 * bottom to top, as a plot's vertical axis label).
 */
enum class TextRotation : std::uint8_t {
	None,
	Clockwise,
	CounterClockwise
};

/**
 * \struct TextColorRun Text.h <litefont/Text.h>
 * @addtogroup Text
 *
 * TextColorRun - A color for a range of bytes of the content, for example
 * syntax highlighting. Bytes outside every run use the Text's color.
 */
struct TextColorRun {
	std::uint32_t firstByte = 0;
	std::uint32_t byteCount = 0;
	Color4f color;

	bool operator==(const TextColorRun&) const = default;
};

/**
 * \struct TextStyleRun Text.h <litefont/Text.h>
 * @addtogroup Text
 *
 * TextStyleRun - A style for a range of bytes of the content, for example a
 * bold word or italic comments in code: the face, the script (superscript or
 * subscript) and whether the bytes are underlined or struck through. Bytes
 * outside every run use the Text's style, on the baseline, undecorated; a
 * run sets all of these, so an underlined bold word names both.
 */
struct TextStyleRun {
	std::uint32_t firstByte = 0;
	std::uint32_t byteCount = 0;
	FontStyle style = FontStyle::Regular;
	TextScript script = TextScript::Normal;
	bool underline = false;
	bool strikethrough = false;

	bool operator==(const TextStyleRun&) const = default;
};

/**
 * \struct TextOutline Text.h <litefont/Text.h>
 * @addtogroup Text
 *
 * TextOutline - A border around every glyph: its width in pixels at 100
 * percent scale (0 for none) and its color.
 */
struct TextOutline {
	float width = 0.0f;
	Color4f color;

	bool operator==(const TextOutline&) const = default;
};

/**
 * \struct TextShadow Text.h <litefont/Text.h>
 * @addtogroup Text
 *
 * TextShadow - A copy of the text drawn behind it, moved by offset (pixels
 * at 100 percent scale), in color, and blurred by a radius in pixels at 100
 * percent scale (0 for a sharp shadow; the radius is twice the Gaussian's
 * standard deviation, as in CSS). A transparent color means no shadow.
 */
struct TextShadow {
	Vector2f offset;
	Color4f color = Color4f{0.0f, 0.0f, 0.0f, 0.0f};
	float blur = 0.0f;

	bool operator==(const TextShadow&) const = default;
};

/**
 * \struct TextBackground Text.h <litefont/Text.h>
 * @addtogroup Text
 *
 * TextBackground - A box drawn behind the whole Text: its laid-out bounds
 * grown by padding (pixels at 100 percent scale), in color. A transparent
 * color means no background.
 */
struct TextBackground {
	Color4f color = Color4f{0.0f, 0.0f, 0.0f, 0.0f};
	float padding = 0.0f;

	bool operator==(const TextBackground&) const = default;
};

/**
 * \struct TextGradient Text.h <litefont/Text.h>
 * @addtogroup Text
 *
 * TextGradient - A color for each corner of a Text's laid-out box, blended
 * across it. getColor() gives the blend at a point of the box, from (0, 0)
 * at the top-left corner to (1, 1) at the bottom-right: the top and bottom
 * edges blend left to right, and the result blends top to bottom.
 * vertical() and horizontal() make the two-color gradients; four different
 * corners blend both ways at once.
 */
struct TextGradient {
	Color4f topLeft;
	Color4f topRight;
	Color4f bottomLeft;
	Color4f bottomRight;

	Color4f getColor(float u, float v) const;
	bool operator==(const TextGradient&) const = default;

	static TextGradient vertical(const Color4f& top, const Color4f& bottom);
	static TextGradient horizontal(const Color4f& left, const Color4f& right);
};

/**
 * \class Text Text.h <litefont/Text.h>
 * @addtogroup Text
 *
 * Text - A piece of text as a concept: its UTF-8 content, the font and size
 * (in pixels at 100 percent scale) it uses, where its top-left corner sits,
 * its colors, and how its paragraphs are laid out. It holds data only;
 * TextLayout places its glyphs and a renderer draws them.
 *
 * Every setter that changes a value increments the revision, and setting the
 * value a Text already has does not, so a renderer rebuilds a Text only when
 * it really changed.
 *
 * A Text draws with a FontFamily; setFont() makes a family of one font,
 * whose other styles are then simulated. The style and style runs choose a
 * face per byte range, and the outline and shadow decorate every glyph. The
 * fonts are not owned and must outlive the Text.
 *
 * A gradient (setGradient) colors the glyphs, underlines and strikethroughs
 * by where they sit in the laid-out box, in place of the Text's color; color
 * runs still color their own ranges, and outlines, shadows and the
 * background keep their colors. clearGradient() goes back to one color.
 *
 * The anchor is the point of the laid-out box that lands on the position,
 * from (0, 0), the top-left corner (the default), to (1, 1), the
 * bottom-right; (0.5, 1) puts the bottom center there, as a label above a
 * point wants. A rotation turns the Text a quarter turn about its position.
 * Letter spacing adds pixels (at 100 percent scale) between glyphs. A line
 * limit stops the layout after that many lines, and with an ellipsis the
 * last line kept ends in one when text was cut.
 *
 * The offset and the clip are placement only: the offset moves the drawn
 * Text by whole device pixels (pixels at 100 percent scale, rounded), and
 * the clip (pixels at 100 percent scale) hides whatever falls outside it. A
 * renderer applies them without a new layout, so they are the two setters
 * that never change the revision: a label can follow a moving point every
 * frame for the cost of moving its quads.
 */
class Text {
public:
	Text() = default;
	Text(std::string content, const Font& font, float size);
	Text(std::string content, const FontFamily& family, float size);

	void setContent(std::string content);
	const std::string& getContent() const;
	void setFont(const Font& font);
	const Font* getFont() const;
	void setFamily(const FontFamily& family);
	const FontFamily& getFamily() const;
	void setStyle(FontStyle style);
	FontStyle getStyle() const;
	void setStyleRuns(std::vector<TextStyleRun> styleRuns);
	const std::vector<TextStyleRun>& getStyleRuns() const;
	void setSize(float size);
	float getSize() const;
	void setPosition(Vector2f position);
	Vector2f getPosition() const;

	void setColor(Color4f color);
	Color4f getColor() const;
	void setColorRuns(std::vector<TextColorRun> colorRuns);
	const std::vector<TextColorRun>& getColorRuns() const;
	void setGradient(const TextGradient& gradient);
	void clearGradient();
	const TextGradient& getGradient() const;
	bool hasGradient() const;
	void setOutline(const TextOutline& outline);
	const TextOutline& getOutline() const;
	void setShadow(const TextShadow& shadow);
	const TextShadow& getShadow() const;

	void setMaxWidth(float maxWidth);
	float getMaxWidth() const;
	void setAlign(TextAlign align);
	TextAlign getAlign() const;
	void setLineSpacing(float lineSpacing);
	float getLineSpacing() const;
	void setTabWidth(std::uint8_t tabWidth);
	std::uint8_t getTabWidth() const;
	void setPixelAlignedAdvances(bool pixelAlignedAdvances);
	bool isPixelAlignedAdvances() const;
	void setLetterSpacing(float letterSpacing);
	float getLetterSpacing() const;
	void setMaxLines(std::uint32_t maxLines);
	std::uint32_t getMaxLines() const;
	void setEllipsis(bool ellipsis);
	bool hasEllipsis() const;

	void setAnchor(Vector2f anchor);
	Vector2f getAnchor() const;
	void setRotation(TextRotation rotation);
	TextRotation getRotation() const;
	void setBackground(const TextBackground& background);
	const TextBackground& getBackground() const;

	void setOffset(Vector2f offset);
	Vector2f getOffset() const;
	void setClip(const Rectf& clip);
	void clearClip();
	const Rectf& getClip() const;
	bool isClipped() const;

	std::uint32_t getRevision() const;

private:
	template <typename T>
	void change(T& member, T value);

	std::string content;
	FontFamily family;
	FontStyle style = FontStyle::Regular;
	std::vector<TextStyleRun> styleRuns;
	float size = 16.0f;
	Vector2f position;
	Color4f color;
	std::vector<TextColorRun> colorRuns;
	TextGradient gradient;
	bool shaded = false;
	TextOutline outline;
	TextShadow shadow;
	float maxWidth = 0.0f;
	TextAlign align = TextAlign::Left;
	float lineSpacing = 1.0f;
	std::uint8_t tabWidth = 4;
	bool pixelAlignedAdvances = false;
	float letterSpacing = 0.0f;
	std::uint32_t maxLines = 0;
	bool ellipsis = false;
	Vector2f anchor;
	TextRotation rotation = TextRotation::None;
	TextBackground background;
	Vector2f offset;
	Rectf clip;
	bool clipped = false;
	std::uint32_t revision = 0;
};

}
