#include "ExampleScene.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <initializer_list>
#include <string_view>
#include <system_error>

bool ExampleScene::loadFonts(const std::vector<std::string>& paths, const char* program, litefont::Font& font, litefont::Font& bold,
	litefont::Font& mono, std::string& fontName) {
	if (!paths.empty()) {
		if (!font.load(litefont::File::toPath(paths[0]))) {
			std::printf("cannot load %s\n", paths[0].c_str());
			return false;
		}
		if (paths.size() > 1 && !mono.load(litefont::File::toPath(paths[1]))) {
			std::printf("cannot load %s\n", paths[1].c_str());
			return false;
		}
		fontName = litefont::File::toPath(paths[0]).filename().string();
		return true;
	}

	// No fonts named: the fonts folder beside the program, or in the working folder.
	std::error_code error;
	std::filesystem::path folder = litefont::File::toPath(program).parent_path() / "fonts";
	if (!std::filesystem::is_directory(folder, error)) {
		folder = std::filesystem::current_path(error) / "fonts";
	}
	litefont::FontCatalog catalog;
	if (!std::filesystem::is_directory(folder, error) || catalog.scan(folder) == 0) {
		std::printf("no fonts: pass a .ttf, .otf or .ttc file, or put fonts in a folder named fonts beside the program\n");
		return false;
	}
	// The regular face of the first family found from a list, else of any family.
	auto pick = [&catalog](std::initializer_list<const char*> families, bool anyFamily) -> const litefont::FontCatalogEntry* {
		for (const char* family : families) {
			for (const litefont::FontCatalogEntry& entry : catalog.getEntries()) {
				if (litefont::FontCatalog::isSameName(entry.names.family, family) && !entry.traits.bold && !entry.traits.italic) {
					return &entry;
				}
			}
		}
		for (const litefont::FontCatalogEntry& entry : catalog.getEntries()) {
			if (anyFamily && !entry.traits.bold && !entry.traits.italic) {
				return &entry;
			}
		}
		return nullptr;
	};
	const litefont::FontCatalogEntry* text = pick({"Noto Sans", "Segoe UI", "DejaVu Sans", "Liberation Sans", "Arial", "Helvetica"}, true);
	const litefont::FontCatalogEntry* code = pick({"Noto Sans Mono", "Consolas", "DejaVu Sans Mono", "Cascadia Mono", "Liberation Mono", "Courier New"}, false);
	if (text == nullptr || !font.load(text->path, text->face)) {
		std::printf("cannot load a font from %s\n", folder.string().c_str());
		return false;
	}
	// The family's own bold face, so bold text is not simulated.
	for (const litefont::FontCatalogEntry& entry : catalog.getEntries()) {
		if (litefont::FontCatalog::isSameName(entry.names.family, text->names.family) && entry.traits.bold && !entry.traits.italic) {
			bold.load(entry.path, entry.face);
			break;
		}
	}
	if (code != nullptr) {
		mono.load(code->path, code->face);
	}
	fontName = text->path.filename().string();
	return true;
}

std::vector<litefont::Text> ExampleScene::getLabels(const litefont::FontFamily& family, const litefont::Font& mono, const std::string& fontName,
	const ExampleDevice& device) {
	// Texts with settings of their own, drawn with painter.draw().
	using litefont::Color4f;
	using litefont::FontStyle;
	using litefont::TextScript;
	const Color4f gold{0.95f, 0.80f, 0.40f, 1.0f};
	const Color4f cyan{0.45f, 0.80f, 0.95f, 1.0f};
	const Color4f pink{0.95f, 0.55f, 0.72f, 1.0f};
	const Color4f green{0.60f, 0.88f, 0.55f, 1.0f};
	const Color4f violet{0.72f, 0.62f, 0.98f, 1.0f};
	const Color4f orange{0.98f, 0.65f, 0.40f, 1.0f};
	std::vector<litefont::Text> labels;

	// The title, pink at the top and light blue at the bottom.
	litefont::Text title("Hello from litefont", family, 32.0f);
	title.setPosition(litefont::Vector2f{24.0f, 20.0f});
	title.setGradient(litefont::TextGradient::vertical(Color4f{1.0f, 0.55f, 0.78f, 1.0f}, Color4f{0.55f, 0.82f, 1.0f, 1.0f}));
	labels.push_back(title);

	// Which graphics API and device draw this window, in the top-right corner.
	litefont::Text mode(device.name, family, 12.0f);
	mode.setPosition(litefont::Vector2f{796.0f, 26.0f});
	mode.setAnchor(litefont::Vector2f{1.0f, 0.0f});
	mode.setColor(Color4f{0.55f, 0.55f, 0.55f, 1.0f});
	labels.push_back(mode);

	// What litefont is: a wrapped paragraph.
	litefont::Text paragraph("litefont is a small text rendering library: it reads TrueType and OpenType fonts, renders hinted, "
		"antialiased glyphs, lays out paragraphs and labels, and draws through OpenGL, Direct3D 11 or the CPU.", family, 15.0f);
	paragraph.setPosition(litefont::Vector2f{24.0f, 68.0f});
	paragraph.setMaxWidth(380.0f);
	paragraph.setColor(Color4f{0.85f, 0.85f, 0.85f, 1.0f});
	labels.push_back(paragraph);

	// What it does: a color per range, underlined and struck through where it says so.
	const ExampleFeature items[] = {{"Kerning", gold, false, false}, {"fallback fonts", cyan, false, false}, {"wrapping", pink, false, false},
		{"ellipses", green, false, false}, {"underlines", violet, true, false}, {"strikethrough", orange, false, true},
		{"outlines", gold, false, false}, {"soft shadows", cyan, false, false}, {"gradients", violet, false, false},
		{"a color per range of text", pink, false, false}};
	const std::size_t itemCount = std::size(items);
	std::string list;
	std::vector<litefont::TextColorRun> listColors;
	std::vector<litefont::TextStyleRun> listStyles;
	for (std::size_t i = 0; i < itemCount; i++) {
		const ExampleFeature& item = items[i];
		list += i == 0 ? "" : (i + 1 == itemCount ? " and " : ", ");
		const std::uint32_t first = static_cast<std::uint32_t>(list.size());
		const std::uint32_t count = static_cast<std::uint32_t>(std::string_view(item.name).size());
		list += item.name;
		listColors.push_back(litefont::TextColorRun{first, count, item.color});
		if (item.underline || item.strikethrough) {
			listStyles.push_back(litefont::TextStyleRun{first, count, FontStyle::Regular, TextScript::Normal, item.underline, item.strikethrough});
		}
	}
	list += ".";
	litefont::Text features(list, family, 14.0f);
	features.setPosition(litefont::Vector2f{440.0f, 70.0f});
	features.setMaxWidth(340.0f);
	features.setColor(Color4f{0.5f, 0.5f, 0.5f, 1.0f});
	features.setColorRuns(listColors);
	features.setStyleRuns(listStyles);
	labels.push_back(features);

	// Equations: superscripts and subscripts, a color each.
	std::string math;
	std::vector<litefont::TextColorRun> mathColors;
	std::vector<litefont::TextStyleRun> mathScripts;
	auto put = [&math, &mathColors, &mathScripts](std::string_view piece, const Color4f& color, TextScript script) {
		const std::uint32_t first = static_cast<std::uint32_t>(math.size());
		const std::uint32_t count = static_cast<std::uint32_t>(piece.size());
		math += piece;
		mathColors.push_back(litefont::TextColorRun{first, count, color});
		if (script != TextScript::Normal) {
			mathScripts.push_back(litefont::TextStyleRun{first, count, FontStyle::Regular, script, false, false});
		}
	};
	put("E = mc", gold, TextScript::Normal);
	put("2", gold, TextScript::Superscript);
	put("    H", cyan, TextScript::Normal);
	put("2", cyan, TextScript::Subscript);
	put("O", cyan, TextScript::Normal);
	put("    a", pink, TextScript::Normal);
	put("2", pink, TextScript::Superscript);
	put(" + b", pink, TextScript::Normal);
	put("2", pink, TextScript::Superscript);
	put(" = c", pink, TextScript::Normal);
	put("2", pink, TextScript::Superscript);
	put("    σ = F / A", green, TextScript::Normal);
	litefont::Text equations(math, family, 18.0f);
	equations.setPosition(litefont::Vector2f{24.0f, 190.0f});
	equations.setColorRuns(mathColors);
	equations.setStyleRuns(mathScripts);
	labels.push_back(equations);

	// The code that draws all this, in the monospaced font, a color per token.
	auto line = [](std::string code, const char* comment) {
		if (comment != nullptr) {
			code.resize(std::max<std::size_t>(code.size() + 2, 46), ' ');
			code += comment;
		}
		return code + "\n";
	};
	std::string sample = line(device.declaration, "// (1) declare")
		+ line("litefont::TextPainter painter(device);", nullptr)
		+ line("font.load(\"" + fontName + "\");", "// (2) load once")
		+ line("painter.drawText(font, 16.0f, \"Hi\", x, y);", "// (3) draw");
	sample.pop_back();
	litefont::Text code = mono.isLoaded() ? litefont::Text(sample, mono, 13.0f) : litefont::Text(sample, family, 13.0f);
	code.setPosition(litefont::Vector2f{32.0f, 382.0f});
	code.setColor(Color4f{0.85f, 0.85f, 0.85f, 1.0f});
	code.setColorRuns(ExampleScene::highlight(sample));
	code.setBackground(litefont::TextBackground{Color4f{0.07f, 0.08f, 0.10f, 1.0f}, 8.0f});
	labels.push_back(code);

	// A label held to two lines, cut with an ellipsis.
	litefont::Text cut("Labels can stop after a set number of lines and end with an ellipsis when the text runs longer "
		"than the space it was given.", family, 13.0f);
	cut.setPosition(litefont::Vector2f{568.0f, 382.0f});
	cut.setMaxWidth(200.0f);
	cut.setMaxLines(2);
	cut.setEllipsis(true);
	cut.setColor(Color4f{0.75f, 0.75f, 0.75f, 1.0f});
	cut.setBackground(litefont::TextBackground{Color4f{0.07f, 0.08f, 0.10f, 1.0f}, 8.0f});
	labels.push_back(cut);

	// Outlined and shadowed, with a color in each corner: gold and pink along
	// the top, green and cyan along the bottom.
	litefont::Text outlined("Outlined and shadowed", family, 22.0f);
	outlined.setPosition(litefont::Vector2f{24.0f, 230.0f});
	outlined.setStyle(FontStyle::Bold);
	outlined.setOutline(litefont::TextOutline{1.0f, Color4f{0.0f, 0.0f, 0.0f, 1.0f}});
	outlined.setShadow(litefont::TextShadow{litefont::Vector2f{2.0f, 2.0f}, Color4f{0.0f, 0.0f, 0.0f, 0.6f}, 3.0f});
	outlined.setGradient(litefont::TextGradient{gold, pink, green, cyan});
	labels.push_back(outlined);

	litefont::Text rotated("Rotated axis label", family, 14.0f);
	rotated.setPosition(litefont::Vector2f{780.0f, 260.0f});
	rotated.setAnchor(litefont::Vector2f{0.5f, 0.5f});
	rotated.setRotation(litefont::TextRotation::CounterClockwise);
	rotated.setColor(Color4f{0.6f, 0.8f, 0.95f, 1.0f});
	labels.push_back(rotated);
	return labels;
}

litefont::Text ExampleScene::getTag(const litefont::FontFamily& family) {
	// A label for a point, drawn above it on a translucent box.
	litefont::Text tag("Node 42  |  12.4 MPa", family, 13.0f);
	tag.setColor(litefont::Color4f{0.95f, 0.95f, 0.95f, 1.0f});
	tag.setAnchor(litefont::Vector2f{0.5f, 1.0f});
	tag.setBackground(litefont::TextBackground{litefont::Color4f{0.1f, 0.12f, 0.16f, 0.85f}, 4.0f});
	return tag;
}

litefont::Color4f ExampleScene::getBackground() {
	return litefont::Color4f{0.11f, 0.11f, 0.11f, 1.0f};
}

float ExampleScene::getAngle(float seconds, bool screenshot) {
	// One turn every two seconds, however fast frames come; a screenshot
	// always shows the same turn.
	return screenshot ? -0.3f : seconds * 3.14159265f;
}

litefont::Vector2f ExampleScene::getCorner(float angle, std::int32_t corner) {
	// The triangle's corners in clip space, counterclockwise from the top one.
	const float a = angle + 1.5708f + static_cast<float>(corner) * 2.0944f;
	return litefont::Vector2f{0.35f * std::cos(a), 0.35f * std::sin(a)};
}

litefont::Color4f ExampleScene::getCornerColor(std::int32_t corner) {
	return litefont::Color4f{corner == 0 ? 0.9f : 0.2f, corner == 1 ? 0.7f : 0.2f, corner == 2 ? 0.9f : 0.3f, 1.0f};
}

litefont::Vector2f ExampleScene::getTagPoint(float angle, std::int32_t width, std::int32_t height) {
	// The top corner in pixels, a little above it, on whole pixels.
	const litefont::Vector2f corner = ExampleScene::getCorner(angle, 0);
	return litefont::Vector2f{std::round((0.5f + corner.x * 0.5f) * static_cast<float>(width)),
		std::round((0.5f - corner.y * 0.5f) * static_cast<float>(height)) - 6.0f};
}

litefont::Image ExampleScene::drawOnCpu(const std::vector<litefont::Text>& labels, litefont::Size2i size) {
	// The same Texts on the CPU, through a TextPainter like the GPU frames.
	litefont::SoftwareDevice software;
	litefont::TextPainter painter(software);
	software.beginFrame(size, ExampleScene::getBackground());
	for (const litefont::Text& label : labels) {
		painter.draw(label);
	}
	software.endFrame();
	return software.readPixels();
}

bool ExampleScene::checkSpaces(litefont::GraphicsDevice& device, litefont::TextPainter& painter, const std::vector<litefont::Text>& labels,
	litefont::Size2i size, const std::function<void()>& clear, const std::function<bool()>& useStack) {
	// Pixels from the viewport, pixel matrices with y down and y up, and the
	// API's own transform where the device has one, must all give the CPU's
	// picture. The y-up passes draw each Text at its mirrored height, from
	// where it grows down toward smaller y.
	const litefont::Image cpu = ExampleScene::drawOnCpu(labels, size);
	const float w = static_cast<float>(size.width);
	const float h = static_cast<float>(size.height);
	std::vector<litefont::Text> raised = labels;
	for (litefont::Text& label : raised) {
		label.setPosition(litefont::Vector2f{label.getPosition().x, h - label.getPosition().y});
	}
	bool good = true;
	for (int pass = 0; pass < 4; pass++) {
		clear();
		const char* name = "viewport pixels";
		const std::vector<litefont::Text>* drawn = &labels;
		if (pass == 1) {
			name = "matrix, y down";
			device.setMatrix(litefont::Matrix4f::ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f));
			device.setYUp(false);
		}
		else if (pass == 2) {
			name = "matrix, y up";
			device.setMatrix(litefont::Matrix4f::ortho(0.0f, w, 0.0f, h, -1.0f, 1.0f));
			device.setYUp(true);
			drawn = &raised;
		}
		else if (pass == 3) {
			name = "matrix stack, y up";
			if (!useStack()) {
				break;
			}
			device.setYUp(true);
			drawn = &raised;
		}
		for (const litefont::Text& label : *drawn) {
			painter.draw(label);
		}
		const litefont::Image gpu = device.readPixels();
		device.useViewport();
		good = ExampleScene::compare(name, gpu, cpu) && good;
	}
	return good;
}

bool ExampleScene::compare(const char* name, const litefont::Image& gpu, const litefont::Image& cpu) {
	int largest = 0;
	int over = 0;
	int lit = 0;
	for (int y = 0; y < gpu.getHeight() && y < cpu.getHeight(); y++) {
		for (int x = 0; x < gpu.getWidth() && x < cpu.getWidth(); x++) {
			const litefont::Color4u8 a = gpu.getPixel(x, y);
			const litefont::Color4u8 b = cpu.getPixel(x, y);
			const int difference = std::max({std::abs(a.r - b.r), std::abs(a.g - b.g), std::abs(a.b - b.b)});
			largest = std::max(largest, difference);
			over += difference > 1 ? 1 : 0;
			lit += b.r > 60 ? 1 : 0;
		}
	}
	// Two levels: each device rounds every blend, and the tag stacks translucent layers.
	const bool matches = gpu.getWidth() == cpu.getWidth() && gpu.getHeight() == cpu.getHeight() && lit > 1000 && largest <= 2;
	std::printf("%-20s %d x %d px, %d lit pixels: largest difference %d levels, %d pixels over one level: %s\n", name,
		gpu.getWidth(), gpu.getHeight(), lit, largest, over, matches ? "matches the CPU" : "differs from the CPU");
	return matches;
}

std::vector<litefont::TextColorRun> ExampleScene::highlight(const std::string& code) {
	// Enough syntax colors for the sample: comments, strings, numbers,
	// litefont's types and the methods called on objects.
	const litefont::Color4f comment{0.45f, 0.62f, 0.45f, 1.0f};
	const litefont::Color4f quoted{0.92f, 0.62f, 0.45f, 1.0f};
	const litefont::Color4f number{0.70f, 0.85f, 0.55f, 1.0f};
	const litefont::Color4f type{0.35f, 0.80f, 0.75f, 1.0f};
	const litefont::Color4f method{0.88f, 0.86f, 0.60f, 1.0f};
	std::vector<litefont::TextColorRun> runs;
	auto add = [&runs](std::size_t first, std::size_t end, const litefont::Color4f& color) {
		runs.push_back(litefont::TextColorRun{static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(end - first), color});
	};
	auto isWord = [](char c) {
		return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
	};
	std::size_t i = 0;
	while (i < code.size()) {
		std::size_t end = i + 1;
		if (code.compare(i, 2, "//") == 0) {
			end = std::min(code.find('\n', i), code.size());
			add(i, end, comment);
		}
		else if (code[i] == '"') {
			const std::size_t close = code.find('"', i + 1);
			end = close == std::string::npos ? code.size() : close + 1;
			add(i, end, quoted);
		}
		else if (std::isdigit(static_cast<unsigned char>(code[i])) != 0) {
			while (end < code.size() && (std::isdigit(static_cast<unsigned char>(code[end])) != 0 || code[end] == '.' || code[end] == 'f')) {
				end++;
			}
			add(i, end, number);
		}
		else if (isWord(code[i])) {
			while (end < code.size() && (isWord(code[end]) || code.compare(end, 2, "::") == 0)) {
				end += code.compare(end, 2, "::") == 0 ? 2 : 1;
			}
			if (code.substr(i, end - i).find("::") != std::string::npos) {
				add(i, end, type);
			}
			else if (i > 0 && code[i - 1] == '.' && end < code.size() && code[end] == '(') {
				add(i, end, method);
			}
		}
		i = end;
	}
	return runs;
}
