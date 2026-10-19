#pragma once

#include <litefont/GlyphRasterizer.h>
#include <litefont/GraphicsDevice.h>
#include <litefont/TextRenderer.h>

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string_view>
#include <unordered_map>

namespace litefont {

/**
 * \struct TextPainterEntry TextPainter.h <litefont/TextPainter.h>
 * @addtogroup Text
 *
 * TextPainterEntry - A Text a TextPainter keeps between draws: the copy it
 * draws (laid out at the origin and moved by its offset), its handle in the
 * TextRenderer, and the key it is found by.
 */
struct TextPainterEntry {
	Text text;
	TextHandle handle;
	std::uint64_t key = 0;
};

/**
 * \class TextPainter TextPainter.h <litefont/TextPainter.h>
 * @addtogroup Text
 *
 * TextPainter - Immediate text on any GraphicsDevice, in three steps:
 * declare it with the device for the program's graphics API (OpenGL11Device,
 * OpenGL33Device, D3D11Device, or SoftwareDevice for the CPU), load a Font
 * once, and call drawText() wherever the program draws. It needs no graphics
 * context at construction; the device makes its objects at the first draw
 * and says which state it touches and restores. The device is not owned and
 * must outlive the painter.
 *
 * drawText() is immediate: every call draws right away, in call order with
 * whatever else the program draws. Behind it, each distinct string with its
 * font, size, style and color keeps a laid-out Text and its glyphs in the
 * atlas, so drawing it again, anywhere, costs no layout and no glyph
 * rendering: the position only moves its quads. When more strings than the
 * cache capacity (1024 by default) are kept, the least recently drawn one
 * goes. draw() does the same for a Text with every setting of its own
 * (style runs, wrapping, outlines, shadows, gradients, rotation and the
 * rest), placed at its position and offset plus the point given; the
 * painter draws its own copy, so the Text may change or go at any time.
 *
 * By default positions are pixels at 100 percent scale from the top-left
 * corner of the viewport, moved by whole device pixels, and setScale() sets
 * the device pixels per pixel (the ratio of a high-density screen). When the
 * device draws through a matrix (GraphicsDevice::setMatrix, or
 * OpenGL11Device::useMatrixStack), positions and sizes are in the matrix's
 * units instead, and setScale() and clips do not apply: each Text is laid out
 * and rendered at the size it shows on screen, rounded to a sixth of an
 * octave (getRasterScale), and the matrix carries it the rest of the way, so
 * moving, turning or zooming it costs no layout until the size crosses a
 * step. measureText() gives the box a string takes and needs no device.
 *
 * Fonts are not owned and must outlive the strings drawn with them, or call
 * clearCache() first. Change the hinting before the first draw: glyphs
 * already in the atlas keep the setting they were rendered with. release()
 * deletes the device's objects (for a GPU, with its context current) and
 * starts over; the destructor cannot, since a context may be gone.
 */
class TextPainter {
public:
	explicit TextPainter(GraphicsDevice& device);
	TextPainter(const TextPainter&) = delete;
	TextPainter& operator=(const TextPainter&) = delete;

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
	void setCacheCapacity(std::size_t capacity);
	std::size_t getCacheCapacity() const;
	std::size_t getCachedCount() const;
	void clearCache();
	void release();

	GraphicsDevice& getDevice();
	TextRenderer& getTextRenderer();

	static float getRasterScale(float pixelScale, float size);

private:
	TextPainterEntry& find(std::uint64_t key, const Text& text);
	void drawEntry(TextPainterEntry& entry, Vector2f origin);

	static std::uint64_t getKey(const Text& text);
	static std::uint64_t getKey(const FontFamily& family, FontStyle style, float size, const Color4f& color, std::string_view text);
	static std::uint64_t mixFamily(const FontFamily& family, std::uint64_t hash);
	static std::uint64_t mixText(std::uint64_t hash, std::string_view text);

	template <typename T>
	static std::uint64_t mixValue(std::uint64_t hash, const T& value);

	GraphicsDevice& device;
	GlyphRasterizer glyphs;
	std::unique_ptr<TextRenderer> renderer;
	std::list<TextPainterEntry> cache;
	std::unordered_map<std::uint64_t, std::list<TextPainterEntry>::iterator> index;
	std::size_t capacity = 1024;
	float scale = 1.0f;
};

}
