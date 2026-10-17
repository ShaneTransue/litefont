#include <litefont/GlyphRasterizer.h>
#include <litefont/SoftwareDevice.h>
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

#include "BlockFont.h"

#include <cmath>
#include <deque>
#include <random>

namespace litefont::tests {

/**
 * GlyphSource wrapper that counts renders, to prove unchanged frames look
 * nothing up.
 */
class CountingGlyphSource : public GlyphSource {
public:
	explicit CountingGlyphSource(GlyphSource& inner) : inner(inner) {
	}

	bool render(const Font& font, const GlyphKey& key, Sprite& sprite) override {
		this->renders++;
		return this->inner.render(font, key, sprite);
	}

	PixelFormat getFormat() const override {
		return this->inner.getFormat();
	}

	SpriteEncoding getEncoding() const override {
		return this->inner.getEncoding();
	}

	std::uint32_t getRenderCount() const {
		return this->renders;
	}

private:
	GlyphSource& inner;
	std::uint32_t renders = 0;
};

class TextRendererTests {
	static Image drawFrame(GraphicsDevice& device, const TextRenderer& renderer, Size2i size, Color4f clear) {
		device.beginFrame(size, clear);
		renderer.draw();
		device.endFrame();
		return device.readPixels();
	}

	static inline test::Test retained{"TextRenderer: with 1,000 Texts, an unchanged update does no layout, glyph lookup or upload", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer rasterizer;
		CountingGlyphSource glyphs(rasterizer);
		test::RecordingDevice device;
		TextRenderer renderer(device, glyphs, 256, 4);

		std::deque<Text> texts;
		for (std::int32_t i = 0; i < 1000; i++) {
			texts.emplace_back(std::format("ab{} pa", i % 7 == 0 ? "x" : "W"), font, 10.0f + static_cast<float>(i % 3));
			texts.back().setPosition(Vector2f{static_cast<float>(i % 40) * 30.0f, static_cast<float>(i / 40) * 14.0f});
			renderer.add(texts.back());
		}
		renderer.update(1.0f);
		const std::uint64_t rebuilds = renderer.getRebuildCount();
		const std::uint64_t lookups = renderer.getAtlas().getLookupCount();
		const std::uint32_t renders = glyphs.getRenderCount();
		const test::DeviceCallCounts before = device.getCounts();
		context.checkEqual(rebuilds, std::uint64_t{1000});
		context.checkEqual(renders, std::uint32_t{6 * 3});

		renderer.update(1.0f);
		renderer.draw();
		const test::DeviceCallCounts& after = device.getCounts();
		context.checkEqual(renderer.getRebuildCount(), rebuilds);
		context.checkEqual(renderer.getAtlas().getLookupCount(), lookups);
		context.checkEqual(glyphs.getRenderCount(), renders);
		context.checkEqual(after.updateTexture, before.updateTexture);
		context.checkEqual(after.updateGeometry, before.updateGeometry);
		context.checkEqual(after.createTexture, before.createTexture);
		context.checkEqual(after.drawSprites, before.drawSprites + 1);

		// Changing one Text rebuilds that Text alone and uploads the geometry once.
		texts[500].setContent("changed");
		renderer.update(1.0f);
		context.checkEqual(renderer.getRebuildCount(), rebuilds + 1);
		context.checkEqual(device.getCounts().updateGeometry, before.updateGeometry + 1);

		// Setting the same content changes nothing.
		texts[500].setContent("changed");
		renderer.update(1.0f);
		context.checkEqual(renderer.getRebuildCount(), rebuilds + 1);
	}};

	static inline test::Test quads{"TextRenderer: quads sit at the pens plus sprite origins, and their UVs address the sprite's atlas pixels", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer glyphs;
		test::RecordingDevice device;
		TextRenderer renderer(device, glyphs, 128, 2);
		Text text("ab pW\nxa", font, 13.0f);
		text.setPosition(Vector2f{3.0f, 2.0f});
		const TextHandle handle = renderer.add(text);
		renderer.update(1.5f);

		const TextLayout& layout = *renderer.getLayout(handle);
		const std::span<const SpriteVertex> vertices = renderer.getGeometry().getVertices();
		const SpriteAtlas& atlas = renderer.getAtlas();
		const float pageSize = static_cast<float>(atlas.getPageSize());
		std::size_t quad = 0;
		std::int32_t wrong = 0;
		for (const PlacedGlyph& placed : layout.getGlyphs()) {
			GlyphKey key{font.getId(), placed.glyph, static_cast<std::uint32_t>(std::lround(13.0f * 1.5f * 64.0f)), 0};
			Sprite sprite;
			glyphs.render(font, key, sprite);
			if (sprite.image.isEmpty()) {
				continue;
			}
			const SpriteVertex& topLeft = vertices[quad * 4];
			const SpriteVertex& bottomRight = vertices[quad * 4 + 2];
			wrong += topLeft.x == std::round(placed.pen.x) + sprite.origin.x ? 0 : 1;
			wrong += topLeft.y == placed.pen.y + sprite.origin.y ? 0 : 1;
			wrong += bottomRight.x - topLeft.x == static_cast<float>(sprite.image.getWidth()) ? 0 : 1;
			const std::int32_t atlasX = static_cast<std::int32_t>(std::lround(topLeft.u * pageSize));
			const std::int32_t atlasY = static_cast<std::int32_t>(std::lround(topLeft.v * pageSize));
			wrong += static_cast<std::int32_t>(std::lround(bottomRight.u * pageSize)) - atlasX == sprite.image.getWidth() ? 0 : 1;
			for (std::int32_t y = 0; y < sprite.image.getHeight(); y++) {
				for (std::int32_t x = 0; x < sprite.image.getWidth(); x++) {
					wrong += atlas.getPage(0).getRow(atlasY + y)[atlasX + x] == sprite.image.getRow(y)[x] ? 0 : 1;
				}
			}
			quad++;
		}
		context.checkEqual(wrong, 0);
		context.checkEqual(quad, vertices.size() / 4);
		context.checkEqual(renderer.getBatches().size(), std::size_t{1});
	}};

	static inline test::Test colors{"TextRenderer: color runs color exactly their byte ranges", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer glyphs;
		test::RecordingDevice device;
		TextRenderer renderer(device, glyphs);
		const Color4f base{1.0f, 1.0f, 1.0f, 1.0f};
		const Color4f red{1.0f, 0.0f, 0.0f, 1.0f};
		const Color4f blue{0.0f, 0.0f, 1.0f, 1.0f};
		Text text("ababab", font, 12.0f);
		text.setColor(base);
		text.setColorRuns({TextColorRun{1, 2, red}, TextColorRun{4, 1, blue}});
		renderer.add(text);
		renderer.update(1.0f);
		const std::span<const SpriteVertex> vertices = renderer.getGeometry().getVertices();
		const Color4f expected[] = {base, red, red, base, blue, base};
		bool right = vertices.size() == 24;
		for (std::size_t i = 0; right && i < 6; i++) {
			for (std::size_t corner = 0; corner < 4; corner++) {
				right = right && vertices[i * 4 + corner].rgba == expected[i].packRGBA8();
			}
		}
		context.check(right, "per-glyph colors");
	}};

	static inline test::Test gradients{"TextRenderer: a gradient colors each corner by where it sits in the box, and color runs still win", [](test::Context& context) {
		const Color4f red{1.0f, 0.0f, 0.0f, 1.0f};
		const Color4f green{0.0f, 1.0f, 0.0f, 1.0f};
		const Color4f blue{0.0f, 0.0f, 1.0f, 1.0f};
		const Color4f white{1.0f, 1.0f, 1.0f, 1.0f};
		const TextGradient corners{red, green, blue, white};
		context.check(corners.getColor(0.0f, 0.0f) == red && corners.getColor(1.0f, 0.0f) == green, "top corners");
		context.check(corners.getColor(0.0f, 1.0f) == blue && corners.getColor(1.0f, 1.0f) == white, "bottom corners");
		context.check(corners.getColor(0.5f, 0.5f) == Color4f{0.5f, 0.5f, 0.5f, 1.0f}, "the middle blends all four");
		context.check(TextGradient::vertical(red, blue) == TextGradient{red, red, blue, blue}, "vertical");

		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer glyphs;
		test::RecordingDevice device;
		TextRenderer renderer(device, glyphs);
		Text text("abab", font, 12.0f);
		text.setColor(white);
		const std::uint32_t revision = text.getRevision();
		const TextGradient sideways = TextGradient::horizontal(red, blue);
		text.setGradient(sideways);
		text.setGradient(sideways);
		context.check(text.hasGradient() && text.getRevision() == revision + 1, "setting a gradient changes the revision once");
		text.setColorRuns({TextColorRun{2, 1, green}});
		const TextHandle handle = renderer.add(text);
		renderer.update(1.0f);

		const Rectf box = renderer.getLayout(handle)->getBounds();
		const std::span<const SpriteVertex> vertices = renderer.getGeometry().getVertices();
		bool right = vertices.size() == 16;
		for (std::size_t i = 0; right && i < vertices.size(); i++) {
			const Color4f expected = i / 4 == 2 ? green : sideways.getColor((vertices[i].x - box.x) / box.width, 0.0f);
			right = vertices[i].rgba == expected.packRGBA8();
		}
		context.check(right, "corner colors");
		context.check(vertices.size() == 16 && vertices[0].rgba != vertices[1].rgba, "the color changes across a glyph");

		text.clearGradient();
		renderer.update(1.0f);
		bool plain = renderer.getGeometry().getVertices().size() == 16;
		for (std::size_t i = 0; plain && i < 16; i++) {
			plain = renderer.getGeometry().getVertices()[i].rgba == (i / 4 == 2 ? green : white).packRGBA8();
		}
		context.check(plain, "cleared, the Text's color again");
	}};

	static inline test::Test removes{"TextRenderer: remove drops a Text's quads and its handle is reused", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer glyphs;
		test::RecordingDevice device;
		TextRenderer renderer(device, glyphs);
		Text first("aaa", font, 12.0f);
		Text second("bb", font, 12.0f);
		const TextHandle a = renderer.add(first);
		renderer.add(second);
		renderer.update(1.0f);
		context.checkEqual(renderer.getGeometry().getVertices().size(), std::size_t{20});
		renderer.remove(a);
		renderer.remove(a);
		renderer.update(1.0f);
		context.checkEqual(renderer.getGeometry().getVertices().size(), std::size_t{8});
		context.check(renderer.getLayout(a) == nullptr, "the handle is gone");
		Text third("x", font, 12.0f);
		context.check(renderer.add(third) == a, "and reused");
	}};

	static inline test::Test rescales{"TextRenderer: a device scale change rebuilds every Text at the new size", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer glyphs;
		test::RecordingDevice device;
		TextRenderer renderer(device, glyphs);
		Text first("ab", font, 10.0f);
		Text second("pa", font, 10.0f);
		const TextHandle handle = renderer.add(first);
		renderer.add(second);
		renderer.update(1.0f);
		const float pen = renderer.getLayout(handle)->getGlyphs()[1].pen.x;
		const std::size_t quads = renderer.getGeometry().getVertices().size();
		renderer.update(2.0f);
		context.checkEqual(renderer.getRebuildCount(), std::uint64_t{4});
		context.checkEqual(renderer.getLayout(handle)->getGlyphs()[1].pen.x, 2.0f * pen);
		context.checkEqual(renderer.getLayout(handle)->getLineHeight(), 20.0f);
		context.checkEqual(renderer.getGeometry().getVertices().size(), quads);
	}};

	static inline test::Test tinyAtlas{"TextRenderer: a full atlas skips glyphs it cannot fit and reuses pages no Text draws from", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer glyphs;
		test::RecordingDevice device;
		// One 32 px page holds four 10 x 10 blocks (11 px with padding).
		TextRenderer renderer(device, glyphs, 32, 1);
		Text sizes("x", font, 20.0f);
		Text more("x", font, 20.5f);
		Text evenMore("x", font, 21.0f);
		Text another("x", font, 19.5f);
		Text fifth("x", font, 19.0f);
		const TextHandle handles[] = {renderer.add(sizes), renderer.add(more), renderer.add(evenMore), renderer.add(another), renderer.add(fifth)};
		renderer.update(1.0f);
		const std::size_t drawn = renderer.getGeometry().getVertices().size() / 4;
		context.check(drawn < 5, "not every block fits");
		context.checkEqual(renderer.getAtlas().getEvictionCount(), std::uint32_t{0});

		for (const TextHandle handle : handles) {
			renderer.remove(handle);
		}
		renderer.update(1.0f);
		Text later("x", font, 30.0f);
		renderer.add(later);
		renderer.update(1.0f);
		context.checkEqual(renderer.getGeometry().getVertices().size(), std::size_t{4});
		context.checkEqual(renderer.getAtlas().getEvictionCount(), std::uint32_t{1});
	}};

	static inline test::Test subpixel{"TextRenderer: subpixel bins render shifted glyphs at whole-pixel quads", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer glyphs;
		test::RecordingDevice device;
		TextRenderer renderer(device, glyphs);
		renderer.setSubpixelBins(4);
		Text text("aaaa", font, 11.0f);
		renderer.add(text);
		renderer.update(1.0f);
		std::int32_t fractional = 0;
		for (const SpriteVertex& vertex : renderer.getGeometry().getVertices()) {
			fractional += vertex.x == std::round(vertex.x) ? 0 : 1;
		}
		context.checkEqual(fractional, 0);
		context.check(renderer.getAtlas().getLookupCount() == 4, "four glyph lookups");
		context.checkEqual(renderer.getSubpixelBins(), std::uint8_t{4});
		renderer.setSubpixelBins(3);
		context.checkEqual(renderer.getSubpixelBins(), std::uint8_t{1});
	}};

	static inline test::Test layers{"TextRenderer: shadows, then outlines, then glyphs, each in its own color", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer glyphs;
		test::RecordingDevice device;
		TextRenderer renderer(device, glyphs);
		const Color4f fill{0.9f, 0.9f, 0.9f, 1.0f};
		const Color4f border{0.0f, 0.0f, 0.0f, 1.0f};
		const Color4f shade{0.0f, 0.0f, 0.0f, 0.5f};
		Text text("ab", font, 12.0f);
		text.setColor(fill);
		text.setOutline(TextOutline{1.0f, border});
		text.setShadow(TextShadow{Vector2f{2.0f, 1.0f}, shade});
		renderer.add(text);
		renderer.update(1.0f);

		const std::span<const SpriteVertex> vertices = renderer.getGeometry().getVertices();
		context.checkEqual(vertices.size(), std::size_t{24});
		context.checkEqual(renderer.getBatches().size(), std::size_t{3});
		const std::uint32_t expected[] = {shade.packRGBA8(), border.packRGBA8(), fill.packRGBA8()};
		bool ordered = vertices.size() == 24;
		for (std::size_t quad = 0; ordered && quad < 6; quad++) {
			ordered = vertices[quad * 4].rgba == expected[quad / 2];
		}
		context.check(ordered, "two shadow quads, two outline quads, then two glyph quads");
		// The shadow copies the outline, moved by (2, 1); the outline is a pixel wider than the glyph all round.
		context.checkEqual(vertices[0].x - vertices[8].x, 2.0f);
		context.checkEqual(vertices[0].y - vertices[8].y, 1.0f);
		context.checkEqual(vertices[16].x - vertices[8].x, 2.0f);

		// Without a visible outline or shadow only the glyphs remain.
		text.setOutline(TextOutline{});
		text.setShadow(TextShadow{});
		renderer.update(1.0f);
		context.checkEqual(renderer.getGeometry().getVertices().size(), std::size_t{8});
	}};

	static inline test::Test styles{"TextRenderer: real, simulated, outlined and shadowed styles match their golden images", [](test::Context& context) {
		Font regular;
		Font bold;
		regular.load(BlockFont::build());
		bold.load(BlockFont::build(true));
		FontFamily family(regular);
		family.addFace(bold);
		GlyphRasterizer glyphs;
		SoftwareDevice device;
		TextRenderer renderer(device, glyphs, 256, 2);
		// "ab" regular, "pa" from the bold face, "xW" slanted (simulated italic), then simulated bold italic.
		Text text("ab pa xW bap", family, 11.0f);
		text.setPosition(Vector2f{6.0f, 6.0f});
		text.setColor(Color4f{0.95f, 0.85f, 0.4f, 1.0f});
		text.setStyleRuns({TextStyleRun{3, 2, FontStyle::Bold}, TextStyleRun{6, 2, FontStyle::Italic}, TextStyleRun{9, 3, FontStyle::BoldItalic}});
		text.setOutline(TextOutline{1.0f, Color4f{0.05f, 0.05f, 0.05f, 1.0f}});
		text.setShadow(TextShadow{Vector2f{1.0f, 1.0f}, Color4f{0.0f, 0.0f, 0.0f, 0.6f}});
		renderer.add(text);
		renderer.update(1.0f);
		context.checkImage(drawFrame(device, renderer, Size2i{104, 28}, Color4f{0.3f, 0.45f, 0.6f, 1.0f}), "tests/golden/textrenderer-styles.pam");

		Font segoe[4];
		const char* names[] = {"segoeui", "segoeuib", "segoeuii", "segoeuiz"};
		FontFamily installed;
		for (std::size_t i = 0; i < 4; i++) {
			if (!segoe[i].load(test::Context::getFontFolder() / (std::string(names[i]) + ".ttf"))) {
				context.note("Segoe UI's faces are not in the fonts folder; the local golden is skipped");
				return;
			}
			installed.addFace(segoe[i]);
		}
		SoftwareDevice local;
		TextRenderer real(local, glyphs, 512, 2);
		Text label("Regular, bold, italic and bold italic in one Text", installed, 13.0f);
		label.setPosition(Vector2f{6.0f, 4.0f});
		label.setColor(Color4f{0.9f, 0.9f, 0.9f, 1.0f});
		label.setStyleRuns({TextStyleRun{9, 4, FontStyle::Bold}, TextStyleRun{15, 6, FontStyle::Italic}, TextStyleRun{26, 11, FontStyle::BoldItalic}});
		label.setOutline(TextOutline{1.0f, Color4f{0.0f, 0.0f, 0.0f, 1.0f}});
		real.add(label);
		real.update(1.0f);
		context.checkImage(drawFrame(local, real, Size2i{340, 28}, Color4f{0.35f, 0.5f, 0.65f, 1.0f}), "out/golden/textrenderer-styles-installed.pam");
	}};

	static inline test::Test goldens{"TextRenderer: paragraphs drawn through SoftwareDevice match their golden images", [](test::Context& context) {
		Font blocks;
		blocks.load(BlockFont::build());
		GlyphRasterizer glyphs;
		SoftwareDevice device;
		TextRenderer renderer(device, glyphs, 256, 2);
		Text paragraph("ab pa xW\nbap ab\tx", blocks, 11.0f);
		paragraph.setPosition(Vector2f{4.0f, 4.0f});
		paragraph.setMaxWidth(60.0f);
		paragraph.setColor(Color4f{0.9f, 0.9f, 0.8f, 1.0f});
		paragraph.setColorRuns({TextColorRun{3, 2, Color4f{1.0f, 0.4f, 0.2f, 1.0f}}});
		renderer.add(paragraph);
		renderer.update(1.0f);
		context.checkImage(drawFrame(device, renderer, Size2i{72, 56}, Color4f{0.11f, 0.11f, 0.11f, 1.0f}), "tests/golden/textrenderer-blocks.pam");

		Font bahnschrift;
		if (!bahnschrift.load(test::Context::getFontFolder() / "bahnschrift.ttf")) {
			context.note("bahnschrift is not in the fonts folder; its local golden is skipped");
			return;
		}
		// The local fonts change between Windows versions, so this golden is local.
		SoftwareDevice local;
		TextRenderer real(local, glyphs, 512, 2);
		Text label("M4A1  AK-74N  Salewa\nthe quick brown fox jumps", bahnschrift, 11.0f);
		label.setPosition(Vector2f{6.0f, 4.0f});
		label.setColor(Color4f{0.8f, 0.8f, 0.8f, 1.0f});
		label.setPixelAlignedAdvances(true);
		real.add(label);
		real.update(1.0f);
		context.checkImage(drawFrame(local, real, Size2i{180, 40}, Color4f{0.11f, 0.11f, 0.11f, 1.0f}), "out/golden/textrenderer-bahnschrift.pam");
	}};

	static inline test::Test damagedFonts{"TextRenderer: damaged fonts run through the whole CPU pipeline without crashing", [](test::Context& context) {
		const Bytes valid = BlockFont::build();
		std::mt19937 random(31u);
		GlyphRasterizer glyphs;
		SoftwareDevice device;
		std::int32_t loaded = 0;
		for (std::int32_t trial = 0; trial < 400; trial++) {
			Bytes damaged = valid;
			const std::size_t count = 1 + random() % 8;
			for (std::size_t i = 0; i < count; i++) {
				damaged.data[random() % damaged.data.size()] = static_cast<std::uint8_t>(random());
			}
			Font font;
			if (!font.load(std::move(damaged))) {
				continue;
			}
			loaded++;
			TextRenderer renderer(device, glyphs, 64, 2);
			Text text("ab pa\nxW bap", font, 14.0f);
			text.setMaxWidth(40.0f);
			renderer.add(text);
			renderer.update(1.25f);
			device.beginFrame(Size2i{64, 48}, Color4f{});
			renderer.draw();
			device.endFrame();
		}
		context.note(std::format("{} of 400 damaged fonts loaded and were drawn", loaded));
	}};

	static inline test::Test offsets{"TextRenderer: an offset moves a Text's quads with no layout, glyph lookup or new revision", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer rasterizer;
		CountingGlyphSource glyphs(rasterizer);
		test::RecordingDevice device;
		TextRenderer renderer(device, glyphs);
		Text text("ab", font, 12.0f);
		text.setPosition(Vector2f{3.0f, 4.0f});
		renderer.add(text);
		renderer.update(1.5f);
		const std::vector<SpriteVertex> before(renderer.getGeometry().getVertices().begin(), renderer.getGeometry().getVertices().end());
		const std::uint64_t rebuilds = renderer.getRebuildCount();
		const std::uint32_t renders = glyphs.getRenderCount();
		const std::uint64_t lookups = renderer.getAtlas().getLookupCount();
		const std::uint32_t revision = text.getRevision();

		text.setOffset(Vector2f{5.0f, 3.0f});
		renderer.update(1.5f);
		const std::span<const SpriteVertex> moved = renderer.getGeometry().getVertices();
		bool shifted = moved.size() == before.size();
		for (std::size_t i = 0; shifted && i < moved.size(); i++) {
			shifted = moved[i].x == before[i].x + 8.0f && moved[i].y == before[i].y + 5.0f && moved[i].u == before[i].u;
		}
		context.check(shifted, "every vertex moves by (5, 3) at 150 percent, rounded to (8, 5)");
		context.checkEqual(renderer.getRebuildCount(), rebuilds);
		context.checkEqual(glyphs.getRenderCount(), renders);
		context.checkEqual(renderer.getAtlas().getLookupCount(), lookups);
		context.checkEqual(text.getRevision(), revision);

		text.setOffset(Vector2f{});
		renderer.update(1.5f);
		context.check(std::equal(before.begin(), before.end(), renderer.getGeometry().getVertices().begin(), [](const SpriteVertex& a, const SpriteVertex& b) {
			return a.x == b.x && a.y == b.y;
		}), "and back");
	}};

	static inline test::Test clips{"TextRenderer: Texts that share a clip share a batch, and a Text clipped to nothing draws nothing", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer glyphs;
		test::RecordingDevice device;
		TextRenderer renderer(device, glyphs);
		Text first("ab", font, 12.0f);
		Text second("ab", font, 12.0f);
		Text free("ab", font, 12.0f);
		Text hidden("ab", font, 12.0f);
		first.setClip(Rectf{0.0f, 0.0f, 20.0f, 10.0f});
		second.setClip(Rectf{0.0f, 0.0f, 20.0f, 10.0f});
		hidden.setClip(Rectf{5.0f, 5.0f, 0.0f, 10.0f});
		for (const Text* text : {&first, &second, &free, &hidden}) {
			renderer.add(*text);
		}
		renderer.update(1.5f);
		const std::span<const SpriteBatch> batches = renderer.getBatches();
		context.checkEqual(batches.size(), std::size_t{2});
		context.check(batches.size() == 2 && batches[0].clip == Recti{0, 0, 30, 15} && batches[1].clip.isEmpty(), "one clipped batch, one free");
		context.checkEqual(renderer.getGeometry().getVertices().size(), std::size_t{24});

		const std::uint64_t rebuilds = renderer.getRebuildCount();
		first.setClip(Rectf{0.2f, 0.0f, 10.0f, 10.0f});
		renderer.update(1.5f);
		context.checkEqual(renderer.getBatches().size(), std::size_t{3});
		context.check(renderer.getBatches()[0].clip == Recti{0, 0, 16, 15}, "the clip covers every pixel it touches");
		context.checkEqual(renderer.getRebuildCount(), rebuilds);

		// Drawn: nothing lands outside the clip.
		SoftwareDevice software;
		TextRenderer drawn(software, glyphs);
		Text wide("WWW", font, 20.0f);
		wide.setColor(Color4f{1.0f, 1.0f, 1.0f, 1.0f});
		wide.setClip(Rectf{0.0f, 0.0f, 25.0f, 30.0f});
		drawn.add(wide);
		drawn.update(1.0f);
		const Image frame = drawFrame(software, drawn, Size2i{70, 30}, Color4f{0.0f, 0.0f, 0.0f, 1.0f});
		std::int32_t outside = 0;
		std::int32_t inside = 0;
		for (std::int32_t y = 0; y < 30; y++) {
			for (std::int32_t x = 0; x < 70; x++) {
				const bool lit = frame.getPixel(x, y).r > 0;
				outside += lit && x >= 25 ? 1 : 0;
				inside += lit && x < 25 ? 1 : 0;
			}
		}
		context.check(outside == 0 && inside > 0, "drawn inside the clip only");
	}};

	static inline test::Test rotations{"TextRenderer: a rotated Text is exactly the upright one turned a quarter about its position", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer glyphs;
		SoftwareDevice device;
		TextRenderer renderer(device, glyphs);
		Text text("Wab", font, 20.0f);
		text.setPosition(Vector2f{40.0f, 40.0f});
		text.setColor(Color4f{1.0f, 0.8f, 0.4f, 1.0f});
		text.setStyleRuns({TextStyleRun{1, 2, FontStyle::Regular, TextScript::Normal, true, false}});
		renderer.add(text);
		const Color4f clear{0.1f, 0.1f, 0.1f, 1.0f};
		renderer.update(1.0f);
		const Image upright = drawFrame(device, renderer, Size2i{80, 80}, clear);
		text.setRotation(TextRotation::CounterClockwise);
		renderer.update(1.0f);
		const Image left = drawFrame(device, renderer, Size2i{80, 80}, clear);
		text.setRotation(TextRotation::Clockwise);
		renderer.update(1.0f);
		const Image right = drawFrame(device, renderer, Size2i{80, 80}, clear);

		// A pixel (x, y) moves to (y, 79 - x) counterclockwise and to (79 - y, x) clockwise.
		std::int32_t wrong = 0;
		std::int32_t lit = 0;
		for (std::int32_t y = 0; y < 80; y++) {
			for (std::int32_t x = 0; x < 80; x++) {
				const Color4u8 pixel = upright.getPixel(x, y);
				wrong += left.getPixel(y, 79 - x) == pixel ? 0 : 1;
				wrong += right.getPixel(79 - y, x) == pixel ? 0 : 1;
				lit += pixel.r > 30 ? 1 : 0;
			}
		}
		context.checkEqual(wrong, 0);
		context.check(lit > 100, "the upright text is drawn");
	}};

	static inline test::Test solids{"TextRenderer: backgrounds and underlines are solid quads in their layers", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer glyphs;
		test::RecordingDevice recording;
		TextRenderer renderer(recording, glyphs);
		const Color4f ink{0.9f, 0.9f, 0.9f, 1.0f};
		const Color4f box{0.8f, 0.1f, 0.1f, 1.0f};
		Text text("ab", font, 10.0f);
		text.setColor(ink);
		text.setBackground(TextBackground{box, 2.0f});
		text.setStyleRuns({TextStyleRun{0, 2, FontStyle::Regular, TextScript::Normal, true, false}});
		renderer.add(text);
		renderer.update(1.0f);
		const std::span<const SpriteVertex> vertices = renderer.getGeometry().getVertices();
		context.checkEqual(vertices.size(), std::size_t{16});
		context.check(vertices.size() == 16 && vertices[0].rgba == box.packRGBA8() && vertices[0].x == -2.0f && vertices[0].y == -2.0f
			&& vertices[2].x == 11.0f && vertices[2].y == 12.0f, "the background comes first: the bounds grown by 2 px");
		context.check(vertices.size() == 16 && vertices[0].u == vertices[2].u && vertices[0].v == vertices[2].v, "a solid quad samples one texel");
		context.check(vertices.size() == 16 && vertices[12].rgba == ink.packRGBA8() && vertices[12].y == 9.0f && vertices[14].y == 10.0f, "the underline after the glyphs");

		SoftwareDevice software;
		TextRenderer drawn(software, glyphs);
		drawn.add(text);
		drawn.update(1.0f);
		software.beginFrame(Size2i{20, 20}, Color4f{0.0f, 0.0f, 0.0f, 1.0f});
		drawn.draw();
		software.endFrame();
		const Image frame = software.readPixels();
		context.check(frame.getPixel(10, 1).r == 204 && frame.getPixel(10, 1).g == 26, "the background is solid");
		context.check(frame.getPixel(4, 9).r > 200 && frame.getPixel(4, 9).g > 200, "the underline is solid ink");
	}};

	static inline test::Test softShadows{"TextRenderer: a blurred shadow is a larger, softer sprite of its own", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer glyphs;
		test::RecordingDevice recording;
		TextRenderer renderer(recording, glyphs);
		Text text("x", font, 20.0f);
		text.setShadow(TextShadow{Vector2f{}, Color4f{0.0f, 0.0f, 0.0f, 1.0f}, 4.0f});
		renderer.add(text);
		renderer.update(1.0f);
		const std::span<const SpriteVertex> vertices = renderer.getGeometry().getVertices();
		context.checkEqual(vertices.size(), std::size_t{8});
		const float shadowWidth = vertices[1].x - vertices[0].x;
		const float glyphWidth = vertices[5].x - vertices[4].x;
		context.checkEqual(shadowWidth - glyphWidth, 14.0f);    // radius 4 px: sigma 2, cut at 6 px, plus a pixel each side

		GlyphKey sharp;
		sharp.font = font.getId();
		sharp.glyph = font.getGlyph(U'x');
		sharp.sizePx64 = 20 * 64;
		GlyphKey soft = sharp;
		soft.blurRadius16 = 64;
		context.check(sharp.getHash() != soft.getHash(), "the blur is part of the key");

		SoftwareDevice software;
		TextRenderer drawn(software, glyphs);
		text.setPosition(Vector2f{10.0f, 4.0f});
		drawn.add(text);
		drawn.update(1.0f);
		const Image frame = drawFrame(software, drawn, Size2i{40, 34}, Color4f{1.0f, 1.0f, 1.0f, 1.0f});
		const std::uint8_t beside = frame.getPixel(8, 15).r;
		context.check(beside > 0 && beside < 255, "the shadow fades out beside the glyph");
	}};

	static inline test::Test smallerScripts{"TextRenderer: superscripts render smaller and above the baseline", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer glyphs;
		test::RecordingDevice recording;
		TextRenderer renderer(recording, glyphs);
		Text text("xx", font, 20.0f);
		text.setStyleRuns({TextStyleRun{1, 1, FontStyle::Regular, TextScript::Superscript}});
		renderer.add(text);
		renderer.update(1.0f);
		const std::span<const SpriteVertex> vertices = renderer.getGeometry().getVertices();
		context.checkEqual(vertices.size(), std::size_t{8});
		const float normalHeight = vertices[2].y - vertices[0].y;
		const float raisedHeight = vertices[6].y - vertices[4].y;
		context.check(raisedHeight < normalHeight && vertices[6].y < vertices[2].y, "smaller, and its bottom sits higher");
	}};

	static inline test::Test immediate{"TextRenderer: drawNow draws one Text at once, as update and draw would, and lays it out only when needed", [](test::Context& context) {
		Font font;
		font.load(BlockFont::build());
		GlyphRasterizer rasterizer;
		CountingGlyphSource glyphs(rasterizer);
		const Color4f clear{0.1f, 0.1f, 0.1f, 1.0f};
		Text first("ab Wx", font, 14.0f);
		first.setPosition(Vector2f{4.0f, 3.0f});
		first.setColor(Color4f{0.9f, 0.8f, 0.3f, 1.0f});
		first.setOutline(TextOutline{1.0f, Color4f{0.0f, 0.0f, 0.0f, 1.0f}});
		Text second("pab", font, 11.0f);
		second.setPosition(Vector2f{10.0f, 24.0f});
		second.setBackground(TextBackground{Color4f{0.2f, 0.4f, 0.6f, 1.0f}, 2.0f});

		// Retained: update, then draw everything.
		SoftwareDevice retainedDevice;
		TextRenderer retained(retainedDevice, rasterizer);
		retained.add(first);
		retained.add(second);
		retained.update(1.5f);
		const Image expected = drawFrame(retainedDevice, retained, Size2i{90, 64}, clear);

		// Immediate: one drawNow per Text, in the same order.
		SoftwareDevice immediateDevice;
		TextRenderer immediate(immediateDevice, glyphs);
		const TextHandle firstHandle = immediate.add(first);
		const TextHandle secondHandle = immediate.add(second);
		immediateDevice.beginFrame(Size2i{90, 64}, clear);
		immediate.drawNow(firstHandle, 1.5f);
		immediate.drawNow(secondHandle, 1.5f);
		context.check(immediateDevice.readPixels() == expected, "the same pixels as update and draw");
		context.checkEqual(immediate.getRebuildCount(), std::uint64_t{2});

		// Moving by the offset, or drawing again, lays nothing out and renders nothing.
		const std::uint32_t renders = glyphs.getRenderCount();
		first.setOffset(Vector2f{6.0f, 0.0f});
		immediateDevice.beginFrame(Size2i{90, 64}, clear);
		immediate.drawNow(firstHandle, 1.5f);
		immediate.drawNow(firstHandle, 1.5f);
		context.checkEqual(immediate.getRebuildCount(), std::uint64_t{2});
		context.checkEqual(glyphs.getRenderCount(), renders);

		// A new scale lays out only the Text drawn at it.
		immediate.drawNow(firstHandle, 2.0f);
		context.checkEqual(immediate.getRebuildCount(), std::uint64_t{3});

		// prepare renders the glyphs ahead of time without drawing.
		SoftwareDevice untouched;
		TextRenderer prepared(untouched, glyphs);
		const std::uint32_t before = glyphs.getRenderCount();
		prepared.prepare(prepared.add(second), 1.5f);
		context.check(glyphs.getRenderCount() > before, "the glyphs are rendered");
		context.check(untouched.readPixels().isEmpty(), "and nothing is drawn");
	}};
};

}
