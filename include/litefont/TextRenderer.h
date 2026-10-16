#pragma once

#include <litefont/AtlasPacker.h>
#include <litefont/SpriteAtlas.h>
#include <litefont/Vertex.h>
#include <litefont/Geometry.h>
#include <litefont/GraphicsDevice.h>

#include <litefont/GlyphSource.h>
#include <litefont/TextLayout.h>

#include <vector>

namespace litefont {

/**
 * \struct TextHandle TextRenderer.h <litefont/TextRenderer.h>
 * @addtogroup Text
 *
 * TextHandle - A Text registered with a TextRenderer; 0 is none.
 */
struct TextHandle {
	std::uint32_t value = 0;

	bool operator==(const TextHandle&) const = default;
};

/**
 * @addtogroup Text
 *
 * TextLayer - What a quad draws, in drawing order: backgrounds first, then
 * shadows, outlines (borders), and the glyphs themselves with their
 * underlines and strikethroughs.
 */
enum class TextLayer : std::uint8_t {
	Background,
	Shadow,
	Outline,
	Fill
};

/**
 * \struct TextEntry TextRenderer.h <litefont/TextRenderer.h>
 * @addtogroup Text
 *
 * TextEntry - One registered Text and what the renderer keeps for it: its
 * layout, the revision and device scale it was built at, its quads (four
 * vertices each, in
 * place but before the offset) with the atlas page and layer of each quad,
 * the set of pages it uses, and the placement its geometry was joined with:
 * the offset in whole device pixels and the clip in device pixels.
 */
struct TextEntry {
	const Text* text = nullptr;
	TextLayout layout;
	std::uint32_t revision = 0;
	float scale = 0.0f;
	bool stale = true;
	std::vector<SpriteVertex> vertices;
	std::vector<std::uint16_t> quadPages;
	std::vector<TextLayer> quadLayers;
	std::uint64_t pageMask = 0;
	Vector2f offset;
	Recti clip;
	bool clipped = false;
};

/**
 * \class TextRenderer TextRenderer.h <litefont/TextRenderer.h>
 * @addtogroup Text
 *
 * TextRenderer - Retained text drawing. add() registers a Text once.
 * update() rebuilds only the Texts whose revision changed (or all of them
 * when the device scale changes): it lays each one out, finds its glyphs in
 * the sprite atlas (rendering missing ones through the GlyphSource) and keeps
 * its quads. It then uploads the atlas regions that changed and, if any Text
 * changed, joins every Text's quads into the shared geometry, one batch per
 * atlas page, and uploads that. draw() replays the geometry with one device
 * call per frame. An update with nothing changed does no layout, no glyph
 * lookup and no upload.
 *
 * Glyphs are drawn at whole device pixels by default; with 2 or 4 subpixel
 * bins a glyph is rendered shifted by half or quarter pixels instead, which
 * spaces fractional advances more truly at the cost of more atlas entries.
 * Glyph colors come from the Text's color runs per vertex, so syntax colors
 * cost no atlas space; where runs overlap, the one starting later wins.
 * Each glyph comes from the face its style resolved to, simulated where the
 * family lacks a face. A Text with an outline adds a grown copy of each
 * glyph in the outline color, and one with a shadow adds an offset copy (of
 * the outline when there is one), blurred when the shadow asks for it;
 * geometry is ordered backgrounds, shadows, outlines, glyphs, so no border
 * paints over a neighboring glyph.
 *
 * Backgrounds, underlines and strikethroughs are solid quads: they sample a
 * small white sprite the renderer keeps in the atlas, so they cost no extra
 * texture or draw call. Underlines and strikethroughs take the color of
 * their first glyph and cast no shadow. A rotated Text turns all its quads a
 * quarter turn about its position, exactly on the pixel grid.
 *
 * The offset and clip are applied when quads are joined into the geometry:
 * a change to either costs no layout and no glyph lookup, only joining and
 * uploading the geometry again. Texts that share a clip share a batch, and a
 * Text clipped to nothing draws nothing. getLayout() is the layout before
 * offset and rotation.
 *
 * drawNow() draws one Text right away instead, for immediate-mode use: it
 * brings that Text up to date at the given scale (a new layout only when
 * its revision or the scale changed), uploads new glyphs and draws just its
 * quads, so text drawn this way lands in call order between whatever else
 * the program draws. prepare() does the same work without drawing, to fill
 * the atlas ahead of time. Both work alongside update() and draw().
 * Remove a Text before destroying it.
 */
class TextRenderer {
public:
	TextRenderer(GraphicsDevice& device, GlyphSource& glyphs, std::uint16_t atlasPageSize = 1024, std::uint16_t maxAtlasPages = 8);

	TextHandle add(const Text& text);
	void remove(TextHandle handle);
	void update(float deviceScale);
	void draw() const;
	void drawNow(TextHandle handle, float deviceScale);
	void prepare(TextHandle handle, float deviceScale);

	void setSubpixelBins(std::uint8_t bins);
	std::uint8_t getSubpixelBins() const;

	const TextLayout* getLayout(TextHandle handle) const;
	const SpriteAtlas& getAtlas() const;
	const Geometry<SpriteVertex>& getGeometry() const;
	std::span<const SpriteBatch> getBatches() const;
	std::uint64_t getRebuildCount() const;

private:
	void rebuild(TextEntry& entry);
	void addQuad(TextEntry& entry, const AtlasEntry& sprite, float x, float y, std::uint32_t rgba, TextLayer layer, std::uint64_t& pageMask) const;
	void addSolid(TextEntry& entry, const Rectf& rect, std::uint32_t rgba, TextLayer layer, std::uint64_t& pageMask);
	void getPlacement(const Text& text, Vector2f& offset, Recti& clip, bool& clipped) const;
	void releasePages(TextEntry& entry);
	void uploadAtlas();
	void rebuildGeometry();
	bool refresh(TextEntry& entry, float deviceScale);
	void appendQuads(const TextEntry& entry, TextLayer layer, std::uint16_t page, Geometry<SpriteVertex>& target) const;

	static void rotate(std::span<SpriteVertex> vertices, Vector2f pivot, TextRotation rotation);
	static std::uint64_t getSolidKey();

	GraphicsDevice& device;
	GlyphSource& glyphs;
	SpriteAtlas atlas;
	Geometry<SpriteVertex> geometry;
	std::vector<SpriteBatch> batches;
	GeometryHandle deviceGeometry;
	Geometry<SpriteVertex> single;
	std::vector<SpriteBatch> singleBatches;
	GeometryHandle singleGeometry;
	std::vector<TextureHandle> pageTextures;
	std::vector<TextEntry> entries;
	std::vector<std::uint32_t> freeEntries;
	float deviceScale = 0.0f;
	std::uint8_t subpixelBins = 1;
	bool geometryChanged = false;
	std::uint64_t rebuilds = 0;
};

}
