#include <litefont/File.h>
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

#include <charconv>
#include <cmath>
#include <format>
#include <iostream>

namespace litefont {

/**
 * \struct RasterizeOptions Rasterize.cpp
 *
 * RasterizeOptions - What the command line asked for.
 */
struct RasterizeOptions {
	std::string fontPath;
	std::string text;
	float size = 0.0f;
	std::string outputPath;
	float scale = 1.0f;
	float width = 0.0f;
	TextAlign align = TextAlign::Left;
	bool hinting = true;
	std::uint8_t bins = 1;
	bool light = false;
	bool pixelAdvances = false;
	FontStyle style = FontStyle::Regular;
	float outline = 0.0f;
	bool shadow = false;
};

/**
 * \class RasterizeProgram Rasterize.cpp
 *
 * RasterizeProgram - Renders text to an image file with no window and no
 * GPU: Text, TextRenderer, GlyphRasterizer and SoftwareDevice, the whole CPU
 * pipeline. Writes PPM (RGB), PAM (RGBA) or PGM (gray) by extension, then
 * updates once more with nothing changed and reports that it did no work.
 *
 *     rasterize font.ttf "Hello\nworld" 13 out.ppm --scale 1.5 --width 200
 *
 * Options: --scale S, --width W (wrap, in 100 percent pixels), --align
 * left|center|right, --hinting on|off, --bins 1|2|4, --theme dark|light,
 * --pixel-advances, --style regular|bold|italic|bolditalic (simulated unless
 * the font already is that face), --outline W (a border W pixels wide),
 * --shadow. In the text, \n is a line break and \t a tab.
 */
class RasterizeProgram {
public:
	static int run(int argumentCount, char** arguments);

private:
	static bool parse(int argumentCount, char** arguments, RasterizeOptions& options);
	static bool parseNumber(std::string_view text, float& value);
	static std::string unescape(std::string_view text);
};

int RasterizeProgram::run(int argumentCount, char** arguments) {
	RasterizeOptions options;
	if (!RasterizeProgram::parse(argumentCount, arguments, options)) {
		std::cerr << "usage: litefont_rasterize font.ttf text size out.ppm [--scale S] [--width W] [--align left|center|right]\n"
			"                 [--hinting on|off] [--bins 1|2|4] [--theme dark|light] [--pixel-advances]\n"
			"                 [--style regular|bold|italic|bolditalic] [--outline W] [--shadow]\n";
		return 2;
	}
	Font font;
	if (!font.load(File::toPath(options.fontPath))) {
		std::cerr << "cannot load " << options.fontPath << "\n";
		return 1;
	}

	const Color4f background = options.light ? Color4f{1.0f, 1.0f, 1.0f, 1.0f} : Color4f{0.11f, 0.11f, 0.11f, 1.0f};
	const Color4f foreground = options.light ? Color4f{0.13f, 0.13f, 0.13f, 1.0f} : Color4f{0.80f, 0.80f, 0.80f, 1.0f};
	constexpr float margin = 8.0f;
	Text text(options.text, font, options.size);
	text.setPosition(Vector2f{margin, margin});
	text.setMaxWidth(options.width);
	text.setAlign(options.align);
	text.setColor(foreground);
	text.setPixelAlignedAdvances(options.pixelAdvances);
	text.setStyle(options.style);
	if (options.outline > 0.0f) {
		const Color4f border = options.light ? Color4f{1.0f, 1.0f, 1.0f, 1.0f} : Color4f{0.0f, 0.0f, 0.0f, 1.0f};
		text.setOutline(TextOutline{options.outline, border});
	}
	if (options.shadow) {
		text.setShadow(TextShadow{Vector2f{1.0f, 1.0f}, Color4f{0.0f, 0.0f, 0.0f, options.light ? 0.25f : 0.6f}});
	}

	SoftwareDevice device;
	GlyphRasterizer glyphs;
	glyphs.setHinting(options.hinting);
	TextRenderer renderer(device, glyphs, 1024, 8);
	renderer.setSubpixelBins(options.bins);
	const TextHandle handle = renderer.add(text);
	renderer.update(options.scale);

	const Rectf bounds = renderer.getLayout(handle)->getBounds();
	const float alignWidth = options.width > 0.0f ? options.width * options.scale : bounds.width;
	const Size2i size{
		static_cast<std::int32_t>(std::ceil(margin * options.scale * 2.0f + alignWidth)),
		static_cast<std::int32_t>(std::ceil(bounds.getBottom() + margin * options.scale)),
	};
	device.beginFrame(size, background);
	renderer.draw();
	device.endFrame();

	// A second frame with nothing changed: retained drawing does no work.
	const std::uint64_t rebuilds = renderer.getRebuildCount();
	const std::uint64_t lookups = renderer.getAtlas().getLookupCount();
	renderer.update(options.scale);
	device.beginFrame(size, background);
	renderer.draw();
	device.endFrame();

	const Image frame = device.readPixels();
	const std::filesystem::path outputPath = File::toPath(options.outputPath);
	const std::string extension = File::fromPath(outputPath.extension());
	Image output;
	if (extension == ".pam") {
		output = frame;
	}
	else {
		output = Image(frame.getWidth(), frame.getHeight(), extension == ".pgm" ? PixelFormat::Gray8 : PixelFormat::RGB8);
		output.copyFrom(frame, 0, 0);
	}
	if (!output.save(outputPath)) {
		std::cerr << "cannot write " << options.outputPath << "\n";
		return 1;
	}
	std::cout << std::format("{}: {} x {} px, {} glyphs on {} lines, {} atlas page(s)\n", options.outputPath, size.width, size.height,
		renderer.getLayout(handle)->getGlyphs().size(), renderer.getLayout(handle)->getLines().size(), renderer.getAtlas().getPageCount());
	std::cout << std::format("second frame: {} layouts, {} glyph lookups\n", renderer.getRebuildCount() - rebuilds,
		renderer.getAtlas().getLookupCount() - lookups);
	return 0;
}

bool RasterizeProgram::parse(int argumentCount, char** arguments, RasterizeOptions& options) {
	if (argumentCount < 5) {
		return false;
	}
	options.fontPath = arguments[1];
	options.text = RasterizeProgram::unescape(arguments[2]);
	options.outputPath = arguments[4];
	if (!RasterizeProgram::parseNumber(arguments[3], options.size) || !(options.size > 0.0f)) {
		return false;
	}
	for (int i = 5; i < argumentCount; i++) {
		const std::string_view name = arguments[i];
		if (name == "--pixel-advances" || name == "--shadow") {
			options.pixelAdvances = options.pixelAdvances || name == "--pixel-advances";
			options.shadow = options.shadow || name == "--shadow";
			continue;
		}
		if (i + 1 >= argumentCount) {
			return false;
		}
		const std::string_view value = arguments[++i];
		float number = 0.0f;
		if (name == "--scale" && RasterizeProgram::parseNumber(value, number) && number > 0.0f) {
			options.scale = number;
		}
		else if (name == "--width" && RasterizeProgram::parseNumber(value, number) && number >= 0.0f) {
			options.width = number;
		}
		else if (name == "--bins" && RasterizeProgram::parseNumber(value, number) && (number == 1.0f || number == 2.0f || number == 4.0f)) {
			options.bins = static_cast<std::uint8_t>(number);
		}
		else if (name == "--align" && (value == "left" || value == "center" || value == "right")) {
			options.align = value == "left" ? TextAlign::Left : (value == "center" ? TextAlign::Center : TextAlign::Right);
		}
		else if (name == "--hinting" && (value == "on" || value == "off")) {
			options.hinting = value == "on";
		}
		else if (name == "--theme" && (value == "dark" || value == "light")) {
			options.light = value == "light";
		}
		else if (name == "--style" && (value == "regular" || value == "bold" || value == "italic" || value == "bolditalic")) {
			options.style = FontFamily::getStyle(value == "bold" || value == "bolditalic", value == "italic" || value == "bolditalic");
		}
		else if (name == "--outline" && RasterizeProgram::parseNumber(value, number) && number >= 0.0f && number <= 16.0f) {
			options.outline = number;
		}
		else {
			return false;
		}
	}
	return true;
}

bool RasterizeProgram::parseNumber(std::string_view text, float& value) {
	const char* end = text.data() + text.size();
	const std::from_chars_result result = std::from_chars(text.data(), end, value);
	return result.ec == std::errc() && result.ptr == end;
}

std::string RasterizeProgram::unescape(std::string_view text) {
	std::string result;
	for (std::size_t i = 0; i < text.size(); i++) {
		if (text[i] == '\\' && i + 1 < text.size() && (text[i + 1] == 'n' || text[i + 1] == 't')) {
			result.push_back(text[i + 1] == 'n' ? '\n' : '\t');
			i++;
			continue;
		}
		result.push_back(text[i]);
	}
	return result;
}

}

int main(int argumentCount, char** arguments) {
	return litefont::RasterizeProgram::run(argumentCount, arguments);
}
