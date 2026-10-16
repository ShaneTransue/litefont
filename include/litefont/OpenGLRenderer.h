#pragma once

#include <litefont/GlyphRasterizer.h>
#include <litefont/OpenGLDevice.h>
#include <litefont/TextRenderer.h>

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string_view>
#include <unordered_map>

namespace litefont {

/**
 * \struct OpenGLCachedText OpenGLRenderer.h <litefont/OpenGLRenderer.h>
 * @addtogroup Text
 *
 * OpenGLCachedText - A Text an OpenGLRenderer keeps between draws: the copy
 * it draws (laid out at the origin and moved by its offset), its handle in
 * the TextRenderer, and the key it is found by.
 */
struct OpenGLCachedText {
	Text text;
	TextHandle handle;
	std::uint64_t key = 0;
};

/**
 * \class OpenGLRenderer OpenGLRenderer.h <litefont/OpenGLRenderer.h>
 * @addtogroup Text
 *
 * OpenGLRenderer - Text in an OpenGL program in three steps: declare it,
 * load a Font once, and call drawText() wherever the program draws. It needs
 * nothing at construction, makes its OpenGL objects at the first draw (with
 * the context current), and draws next to glBegin and glEnd code; the
 * OpenGLDevice it draws through says which state it touches and restores.
 *
 * drawText() is immediate: every call draws right away, in call order with
 * whatever else the program draws. Behind it, each distinct string with its
 * font, size, style and color keeps a laid-out Text and its glyphs in the
 * atlas, so drawing it again, anywhere, costs no layout and no glyph
 * rendering: the position only moves its quads, by whole device pixels.
 * When more strings than the cache capacity (1024 by default) are kept, the
 * least recently drawn one goes. draw() does the same for a Text with every
 * setting of its own (style runs, wrapping, outlines, shadows, rotation and
 * the rest), placed at its position and offset plus the point given; the
 * renderer draws its own copy, so the Text may change or go at any time.
 *
 * Positions are in pixels at 100 percent scale from the top-left corner of
 * the viewport; setScale() sets the device pixels per pixel (the ratio of a
 * high-density screen). measureText() gives the box a string takes and
 * needs no context. Fonts are not owned and must outlive the strings drawn
 * with them, or call clearCache() first. Change the hinting before the
 * first draw: glyphs already in the atlas keep the setting they were
 * rendered with. Call release() with the context current to delete the
 * OpenGL textures; the destructor cannot, since the context may be gone.
 */
class OpenGLRenderer {
public:
	OpenGLRenderer();
	OpenGLRenderer(const OpenGLRenderer&) = delete;
	OpenGLRenderer& operator=(const OpenGLRenderer&) = delete;

	void drawText(const Font& font, float size, std::string_view text, float x, float y, const Color4f& color = Color4f{1.0f, 1.0f, 1.0f, 1.0f});
	void drawText(const FontFamily& family, FontStyle style, float size, std::string_view text, float x, float y,
		const Color4f& color = Color4f{1.0f, 1.0f, 1.0f, 1.0f});
	void draw(const Text& text, float x = 0.0f, float y = 0.0f);
	Rectf measureText(const Font& font, float size, std::string_view text) const;
	void preload(const Font& font, float size, std::string_view characters);

	void setScale(float scale);
	float getScale() const;
	void setHinting(bool hinting);
	bool isHinting() const;
	void setSubpixelBins(std::uint8_t bins);
	void setRestoreState(bool restore);
	void setCacheCapacity(std::size_t capacity);
	std::size_t getCacheCapacity() const;
	std::size_t getCachedCount() const;
	void clearCache();
	void release();

	OpenGLDevice& getDevice();
	TextRenderer& getTextRenderer();

private:
	OpenGLCachedText& find(std::uint64_t key, const Text& text);

	static std::uint64_t getKey(const Text& text);
	static std::uint64_t getKey(const FontFamily& family, FontStyle style, float size, const Color4f& color, std::string_view text);
	static std::uint64_t mixFamily(const FontFamily& family, std::uint64_t hash);
	static std::uint64_t mixText(std::uint64_t hash, std::string_view text);

	template <typename T>
	static std::uint64_t mixValue(std::uint64_t hash, const T& value);

	OpenGLDevice device;
	GlyphRasterizer glyphs;
	std::unique_ptr<TextRenderer> renderer;
	std::list<OpenGLCachedText> cache;
	std::unordered_map<std::uint64_t, std::list<OpenGLCachedText>::iterator> index;
	std::size_t capacity = 1024;
	float scale = 1.0f;
};

}
