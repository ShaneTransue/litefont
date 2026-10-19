#pragma once

#include <litefont/litefont.h>

#include <array>
#include <functional>
#include <string>
#include <vector>

/**
 * ExampleFeature - One item of the feature list the scene shows: its words,
 * its color, and whether it is underlined or struck through.
 */
struct ExampleFeature {
	const char* name = "";
	litefont::Color4f color;
	bool underline = false;
	bool strikethrough = false;
};

/**
 * ExampleDevice - What the scene says about the program drawing it: the line
 * naming its graphics API and litefont device, and the line of code that
 * declares the device.
 */
struct ExampleDevice {
	std::string name;
	std::string declaration;
};

/**
 * ExampleScene - The picture every example draws, whatever its graphics API:
 * the labels (a gradient title, a paragraph about litefont, a feature list,
 * equations, the code that draws it, an outlined label, a label cut with an
 * ellipsis and a rotated axis label), a name tag that follows the corner of
 * the program's own spinning triangle, and the background color. It also
 * holds what the examples' self-checks share: the same labels drawn on the
 * CPU, and a comparison of a GPU frame against them in every space a device
 * has.
 *
 * loadFonts() loads the fonts named on the command line (a font, then an
 * optional monospaced one for the code sample). Given none, as when the
 * program is started from a file manager, it reads the fonts folder beside
 * the program (or in the working folder) and picks a common sans-serif
 * family, with its bold face when there is one, and a monospaced family from
 * it by name; it never looks anywhere else.
 */
class ExampleScene {
public:
	static constexpr std::int32_t width = 820;
	static constexpr std::int32_t height = 520;

	static bool loadFonts(const std::vector<std::string>& paths, const char* program, litefont::Font& font, litefont::Font& bold,
		litefont::Font& mono, std::string& fontName);

	static std::vector<litefont::Text> getLabels(const litefont::FontFamily& family, const litefont::Font& mono, const std::string& fontName,
		const ExampleDevice& device);
	static litefont::Text getTag(const litefont::FontFamily& family);
	static litefont::Color4f getBackground();
	static float getAngle(float seconds, bool screenshot);
	static litefont::Vector2f getCorner(float angle, std::int32_t corner);
	static litefont::Color4f getCornerColor(std::int32_t corner);
	static litefont::Vector2f getTagPoint(float angle, std::int32_t width, std::int32_t height);

	static litefont::Image drawOnCpu(const std::vector<litefont::Text>& labels, litefont::Size2i size);
	static bool checkSpaces(litefont::GraphicsDevice& device, litefont::TextPainter& painter, const std::vector<litefont::Text>& labels,
		litefont::Size2i size, const std::function<void()>& clear, const std::function<bool()>& useStack);

private:
	static bool compare(const char* name, const litefont::Image& gpu, const litefont::Image& cpu);
	static std::vector<litefont::TextColorRun> highlight(const std::string& code);
};
