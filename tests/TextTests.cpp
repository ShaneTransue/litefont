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
#include "TestKit.h"
#include "RecordingDevice.h"

#include <functional>

namespace litefont::tests {

class TextTests {
	static inline test::Test defaults{"Text: defaults to no wrapping, left aligned, line spacing 1 and tab width 4", [](test::Context& context) {
		const Text text;
		context.check(text.getContent().empty(), "no content");
		context.check(text.getFont() == nullptr, "no font");
		context.checkEqual(text.getSize(), 16.0f);
		context.checkEqual(text.getMaxWidth(), 0.0f);
		context.check(text.getAlign() == TextAlign::Left, "left aligned");
		context.checkEqual(text.getLineSpacing(), 1.0f);
		context.checkEqual(text.getTabWidth(), std::uint8_t{4});
		context.check(!text.isPixelAlignedAdvances(), "fractional advances");
		context.check(text.getColor() == Color4f{0.0f, 0.0f, 0.0f, 1.0f}, "opaque black");
		context.checkEqual(text.getRevision(), std::uint32_t{0});
		context.check(text.getStyle() == FontStyle::Regular && text.getStyleRuns().empty(), "regular style");
		context.check(text.getOutline().width == 0.0f && text.getShadow().color.a == 0.0f, "no outline and no shadow");
	}};

	static inline test::Test revisions{"Text: every change bumps the revision once, and setting the same value does not", [](test::Context& context) {
		Font font;
		Font other;
		Text text("start", font, 12.0f);
		const std::function<void(Text&)> setters[] = {
			[](Text& t) { t.setContent("changed"); },
			[&other](Text& t) { t.setFont(other); },
			[](Text& t) { t.setSize(13.0f); },
			[](Text& t) { t.setPosition(Vector2f{1.0f, 2.0f}); },
			[](Text& t) { t.setColor(Color4f{1.0f, 0.5f, 0.0f, 1.0f}); },
			[](Text& t) { t.setColorRuns({TextColorRun{2, 3, Color4f{0.0f, 1.0f, 0.0f, 1.0f}}}); },
			[](Text& t) { t.setMaxWidth(120.0f); },
			[](Text& t) { t.setAlign(TextAlign::Center); },
			[](Text& t) { t.setLineSpacing(1.25f); },
			[](Text& t) { t.setTabWidth(8); },
			[](Text& t) { t.setPixelAlignedAdvances(true); },
			[](Text& t) { t.setStyle(FontStyle::Bold); },
			[](Text& t) { t.setStyleRuns({TextStyleRun{0, 2, FontStyle::Italic}}); },
			[&font, &other](Text& t) {
				FontFamily family(font);
				family.setFace(FontStyle::Bold, other);
				t.setFamily(family);
			},
			[](Text& t) { t.setOutline(TextOutline{1.0f, Color4f{0.0f, 0.0f, 0.0f, 1.0f}}); },
			[](Text& t) { t.setShadow(TextShadow{Vector2f{1.0f, 1.0f}, Color4f{0.0f, 0.0f, 0.0f, 0.5f}}); },
		};
		std::int32_t wrong = 0;
		for (const std::function<void(Text&)>& set : setters) {
			const std::uint32_t before = text.getRevision();
			set(text);
			wrong += text.getRevision() == before + 1 ? 0 : 1;
			set(text);
			wrong += text.getRevision() == before + 1 ? 0 : 1;
		}
		context.checkEqual(wrong, 0);
		context.checkEqual(text.getRevision(), std::uint32_t{16});
		context.check(text.getFont() == &font, "the family's regular face is the primary font");
		FontFamily expected(font);
		expected.setFace(FontStyle::Bold, other);
		context.check(text.getFamily() == expected, "the family holds the fonts it was given, not copies");
	}};

	static inline test::Test sortsRuns{"Text: color runs are kept sorted by first byte", [](test::Context& context) {
		Text text;
		const Color4f red{1.0f, 0.0f, 0.0f, 1.0f};
		const Color4f blue{0.0f, 0.0f, 1.0f, 1.0f};
		text.setColorRuns({TextColorRun{9, 2, red}, TextColorRun{0, 4, blue}, TextColorRun{5, 1, red}});
		const std::vector<TextColorRun>& runs = text.getColorRuns();
		context.checkEqual(runs.size(), std::size_t{3});
		context.check(runs[0].firstByte == 0 && runs[1].firstByte == 5 && runs[2].firstByte == 9, "sorted");
		context.check(runs[0].color == blue, "each run keeps its color");
	}};

	static inline test::Test placement{"Text: layout settings bump the revision, while the offset and clip never do", [](test::Context& context) {
		Text text;
		const std::function<void(Text&)> setters[] = {
			[](Text& t) { t.setLetterSpacing(1.5f); },
			[](Text& t) { t.setMaxLines(2); },
			[](Text& t) { t.setEllipsis(true); },
			[](Text& t) { t.setAnchor(Vector2f{0.5f, 1.0f}); },
			[](Text& t) { t.setRotation(TextRotation::CounterClockwise); },
			[](Text& t) { t.setBackground(TextBackground{Color4f{0.0f, 0.0f, 0.0f, 0.5f}, 3.0f}); },
			[](Text& t) { t.setShadow(TextShadow{Vector2f{1.0f, 1.0f}, Color4f{0.0f, 0.0f, 0.0f, 0.5f}, 4.0f}); },
			[](Text& t) { t.setStyleRuns({TextStyleRun{0, 2, FontStyle::Regular, TextScript::Superscript, true, false}}); },
		};
		std::int32_t wrong = 0;
		for (const std::function<void(Text&)>& set : setters) {
			const std::uint32_t before = text.getRevision();
			set(text);
			wrong += text.getRevision() == before + 1 ? 0 : 1;
			set(text);
			wrong += text.getRevision() == before + 1 ? 0 : 1;
		}
		context.checkEqual(wrong, 0);

		const std::uint32_t revision = text.getRevision();
		text.setOffset(Vector2f{12.0f, -3.0f});
		text.setClip(Rectf{0.0f, 0.0f, 50.0f, 20.0f});
		context.checkEqual(text.getRevision(), revision);
		context.check(text.getOffset() == Vector2f{12.0f, -3.0f} && text.isClipped() && text.getClip().width == 50.0f, "placement is kept");
		text.clearClip();
		context.check(!text.isClipped(), "and the clip can go");
		context.checkEqual(text.getRevision(), revision);

		const Text fresh;
		context.check(fresh.getAnchor() == Vector2f{} && fresh.getRotation() == TextRotation::None && fresh.getMaxLines() == 0
			&& !fresh.hasEllipsis() && fresh.getLetterSpacing() == 0.0f && fresh.getBackground().color.a == 0.0f && !fresh.isClipped(),
			"defaults: top-left anchor, no rotation, no line limit, no spacing, no background, no clip");
	}};
};

}
