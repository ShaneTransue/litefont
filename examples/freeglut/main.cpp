// litefont in an OpenGL program: declare, load once, draw.
//
//     litefont_freeglut <font.ttf> [mono.ttf]      an OpenGL 1.1 window: a triangle drawn with glBegin and text on top,
//                                                   with the code sample in the monospaced font when one is given
//     litefont_freeglut <font.ttf> --core          the text in an OpenGL 3.3 core profile context (no triangle:
//                                                   core profiles have no glBegin)
//     litefont_freeglut <font.ttf> --self-check    draws the labels through OpenGL in every space the device has
//                                                   and on the CPU, prints the differences and exits (0 when they match)
//     litefont_freeglut <font.ttf> --screenshot <file.ppm>   saves the fourth frame of the window and exits
//
// --core goes with either of the other two.

#include <litefont/litefont.h>

#include <GL/freeglut.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

/**
 * ExampleFeature - One item of the feature list the window shows: its words,
 * its color, and whether it is underlined or struck through.
 */
struct ExampleFeature {
	const char* name = "";
	litefont::Color4f color;
	bool underline = false;
	bool strikethrough = false;
};

class Example {
public:
	static int run(int argumentCount, char** arguments);

private:
	static void display();
	static void reshape(int width, int height);
	static void idle();
	static void drawTriangle(float angle);
	static litefont::OpenGLDevice& getDevice();
	static litefont::OpenGLRenderer& getRenderer();
	static std::vector<litefont::Text> getLabels();
	static std::vector<litefont::TextColorRun> highlight(const std::string& code);
	static int checkAgainstCpu();
	static bool checkStateRestored(const std::vector<litefont::Text>& labels);

	// (1) Declared once: a device for the program's context and a renderer
	// that draws through it. This example can run in either kind of context,
	// so it declares both pairs; a program declares the one it needs.
	static litefont::Font font;
	static litefont::Font mono;
	static std::string fontName;
	static litefont::OpenGL11Device legacyDevice;
	static litefont::OpenGLRenderer legacyRenderer;
	static litefont::OpenGL33Device coreDevice;
	static litefont::OpenGLRenderer coreRenderer;
	static litefont::Text tag;
	static int width;
	static int height;
	static int frame;
	static bool core;
	static bool selfCheck;
	static std::string screenshot;
	static std::chrono::steady_clock::time_point start;
};

litefont::Font Example::font;
litefont::Font Example::mono;
std::string Example::fontName;
litefont::OpenGL11Device Example::legacyDevice;
litefont::OpenGLRenderer Example::legacyRenderer(Example::legacyDevice);
litefont::OpenGL33Device Example::coreDevice;
litefont::OpenGLRenderer Example::coreRenderer(Example::coreDevice);
litefont::Text Example::tag;
int Example::width = 820;
int Example::height = 520;
int Example::frame = 0;
bool Example::core = false;
bool Example::selfCheck = false;
std::string Example::screenshot;
std::chrono::steady_clock::time_point Example::start;

int Example::run(int argumentCount, char** arguments) {
	if (argumentCount < 2) {
		std::printf("usage: litefont_freeglut <font.ttf> [mono.ttf] [--core] [--self-check | --screenshot <file.ppm>]\n");
		return 1;
	}
	std::string monoPath;
	for (int i = 2; i < argumentCount; i++) {
		const std::string argument = arguments[i];
		if (argument.rfind("--", 0) != 0 && monoPath.empty()) {
			monoPath = argument;
		}
		else if (argument == "--core") {
			Example::core = true;
		}
		else if (argument == "--self-check") {
			Example::selfCheck = true;
		}
		else if (argument == "--screenshot" && i + 1 < argumentCount) {
			Example::screenshot = arguments[++i];
		}
	}

	glutInit(&argumentCount, arguments);
	if (Example::core) {
		glutInitContextVersion(3, 3);
		glutInitContextProfile(GLUT_CORE_PROFILE);
	}
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(Example::width, Example::height);
	glutCreateWindow(Example::core ? "litefont + freeglut (OpenGL 3.3 core)" : "litefont + freeglut (OpenGL 1.1)");

	// (2) Created once: a font file. The renderer makes its OpenGL objects itself at the first draw.
	if (!Example::font.load(litefont::File::toPath(arguments[1]))) {
		std::printf("cannot load %s\n", arguments[1]);
		return 1;
	}
	if (!monoPath.empty() && !Example::mono.load(litefont::File::toPath(monoPath))) {
		std::printf("cannot load %s\n", monoPath.c_str());
		return 1;
	}
	Example::fontName = litefont::File::toPath(arguments[1]).filename().string();
	Example::tag = litefont::Text("Node 42  |  12.4 MPa", Example::font, 13.0f);
	Example::tag.setColor(litefont::Color4f{0.95f, 0.95f, 0.95f, 1.0f});
	Example::tag.setAnchor(litefont::Vector2f{0.5f, 1.0f});
	Example::tag.setBackground(litefont::TextBackground{litefont::Color4f{0.1f, 0.12f, 0.16f, 0.85f}, 4.0f});

	glutDisplayFunc(Example::display);
	glutReshapeFunc(Example::reshape);
	glutIdleFunc(Example::idle);
	Example::start = std::chrono::steady_clock::now();
	glutMainLoop();
	return 0;
}

void Example::display() {
	glClearColor(0.11f, 0.11f, 0.11f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	if (Example::selfCheck) {
		std::exit(Example::checkAgainstCpu());
	}
	litefont::OpenGLRenderer& renderer = Example::getRenderer();

	// The program's own drawing: one turn every two seconds, however fast
	// frames come (a screenshot always shows the same turn).
	const float seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - Example::start).count();
	const float angle = Example::screenshot.empty() ? seconds * 3.14159265f : -0.3f;
	if (!Example::core) {
		Example::drawTriangle(angle);
	}

	// (3) Drawn wherever the program draws: Texts with settings of their own,
	// and plain strings with drawText.
	for (const litefont::Text& label : Example::getLabels()) {
		renderer.draw(label);
	}
	const std::string counter = "frame " + std::to_string(Example::frame);
	renderer.drawText(Example::font, 12.0f, counter, 24.0f, static_cast<float>(Example::height) - 28.0f,
		litefont::Color4f{0.6f, 0.6f, 0.6f, 1.0f});

	// A label that follows the triangle's top corner: moving it costs no layout.
	const float x = (0.5f + 0.35f * std::cos(angle + 1.5708f) * 0.5f) * static_cast<float>(Example::width);
	const float y = (0.5f - 0.35f * std::sin(angle + 1.5708f) * 0.5f) * static_cast<float>(Example::height);
	renderer.draw(Example::tag, std::round(x), std::round(y) - 6.0f);

	// Frame 3: the triangle has been drawn after text three times, so a state
	// the renderer failed to restore would show in it.
	if (!Example::screenshot.empty() && Example::frame == 3) {
		const bool saved = Example::getDevice().readPixels().save(litefont::File::toPath(Example::screenshot));
		std::printf("%s %s\n", saved ? "saved" : "cannot write", Example::screenshot.c_str());
		std::exit(saved ? 0 : 1);
	}
	glutSwapBuffers();
	Example::frame++;
}

void Example::reshape(int width, int height) {
	Example::width = width;
	Example::height = height;
	glViewport(0, 0, width, height);
}

void Example::idle() {
	glutPostRedisplay();
}

void Example::drawTriangle(float angle) {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glBegin(GL_TRIANGLES);
	for (int corner = 0; corner < 3; corner++) {
		const float a = angle + 1.5708f + static_cast<float>(corner) * 2.0944f;
		glColor3f(corner == 0 ? 0.9f : 0.2f, corner == 1 ? 0.7f : 0.2f, corner == 2 ? 0.9f : 0.3f);
		glVertex2f(0.35f * std::cos(a), 0.35f * std::sin(a));
	}
	glEnd();
}

litefont::OpenGLDevice& Example::getDevice() {
	if (Example::core) {
		return Example::coreDevice;
	}
	return Example::legacyDevice;
}

litefont::OpenGLRenderer& Example::getRenderer() {
	return Example::core ? Example::coreRenderer : Example::legacyRenderer;
}

std::vector<litefont::Text> Example::getLabels() {
	// Texts with settings of their own, drawn with renderer.draw().
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
	litefont::Text title("Hello from litefont", Example::font, 32.0f);
	title.setPosition(litefont::Vector2f{24.0f, 20.0f});
	title.setGradient(litefont::TextGradient::vertical(Color4f{1.0f, 0.55f, 0.78f, 1.0f}, Color4f{0.55f, 0.82f, 1.0f, 1.0f}));
	labels.push_back(title);

	// Which device draws this window, in the top-right corner.
	litefont::Text mode(Example::core ? "OpenGL 3.3 core profile  ·  OpenGL33Device" : "OpenGL 1.1  ·  OpenGL11Device", Example::font, 12.0f);
	mode.setPosition(litefont::Vector2f{796.0f, 26.0f});
	mode.setAnchor(litefont::Vector2f{1.0f, 0.0f});
	mode.setColor(Color4f{0.55f, 0.55f, 0.55f, 1.0f});
	labels.push_back(mode);

	// What litefont is: a wrapped paragraph.
	litefont::Text paragraph("litefont reads TrueType and OpenType fonts itself, renders hinted, antialiased glyphs into a texture "
		"atlas, and lays out paragraphs and labels. It draws through OpenGL 1.1 or a 3.3 core profile, finds its own OpenGL "
		"functions, and puts the program's state back.", Example::font, 15.0f);
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
	litefont::Text features(list, Example::font, 14.0f);
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
	litefont::Text equations(math, Example::font, 18.0f);
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
	std::string sample = line(std::string("litefont::") + (Example::core ? "OpenGL33Device" : "OpenGL11Device") + " device;", "// (1) declare")
		+ line("litefont::OpenGLRenderer renderer(device);", nullptr)
		+ line("font.load(\"" + Example::fontName + "\");", "// (2) load once")
		+ line("renderer.drawText(font, 16.0f, \"Hi\", x, y);", "// (3) draw");
	sample.pop_back();
	litefont::Text code(sample, Example::mono.isLoaded() ? Example::mono : Example::font, 13.0f);
	code.setPosition(litefont::Vector2f{32.0f, 382.0f});
	code.setColor(Color4f{0.85f, 0.85f, 0.85f, 1.0f});
	code.setColorRuns(Example::highlight(sample));
	code.setBackground(litefont::TextBackground{Color4f{0.07f, 0.08f, 0.10f, 1.0f}, 8.0f});
	labels.push_back(code);

	// A label held to two lines, cut with an ellipsis.
	litefont::Text cut("Labels can stop after a set number of lines and end with an ellipsis when the text runs longer "
		"than the space it was given.", Example::font, 13.0f);
	cut.setPosition(litefont::Vector2f{568.0f, 382.0f});
	cut.setMaxWidth(200.0f);
	cut.setMaxLines(2);
	cut.setEllipsis(true);
	cut.setColor(Color4f{0.75f, 0.75f, 0.75f, 1.0f});
	cut.setBackground(litefont::TextBackground{Color4f{0.07f, 0.08f, 0.10f, 1.0f}, 8.0f});
	labels.push_back(cut);

	litefont::Text outlined("Outlined and shadowed", Example::font, 22.0f);
	outlined.setPosition(litefont::Vector2f{24.0f, 230.0f});
	outlined.setStyle(litefont::FontStyle::Bold);
	outlined.setColor(litefont::Color4f{0.95f, 0.78f, 0.35f, 1.0f});
	outlined.setOutline(litefont::TextOutline{1.0f, litefont::Color4f{0.0f, 0.0f, 0.0f, 1.0f}});
	outlined.setShadow(litefont::TextShadow{litefont::Vector2f{2.0f, 2.0f}, litefont::Color4f{0.0f, 0.0f, 0.0f, 0.6f}, 3.0f});
	// A color in each corner: gold and pink along the top, green and cyan along the bottom.
	outlined.setGradient(litefont::TextGradient{gold, pink, green, cyan});
	labels.push_back(outlined);

	litefont::Text rotated("Rotated axis label", Example::font, 14.0f);
	rotated.setPosition(litefont::Vector2f{780.0f, 260.0f});
	rotated.setAnchor(litefont::Vector2f{0.5f, 0.5f});
	rotated.setRotation(litefont::TextRotation::CounterClockwise);
	rotated.setColor(litefont::Color4f{0.6f, 0.8f, 0.95f, 1.0f});
	labels.push_back(rotated);
	return labels;
}

std::vector<litefont::TextColorRun> Example::highlight(const std::string& code) {
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

int Example::checkAgainstCpu() {
	// The same Texts through OpenGL and through SoftwareDevice. OpenGL draws
	// them in each space the device has: pixels from the viewport, and pixel
	// matrices with y down and y up (and the matrix stack, in OpenGL 1.1),
	// which must all give the same picture.
	const litefont::Color4f clear{0.11f, 0.11f, 0.11f, 1.0f};
	std::vector<litefont::Text> labels = Example::getLabels();
	litefont::Text placedTag = Example::tag;
	placedTag.setPosition(litefont::Vector2f{410.0f, 300.0f});
	labels.push_back(placedTag);

	// The CPU picture, placed the way OpenGLRenderer places text on pixels:
	// laid out at the origin and moved by an offset.
	litefont::SoftwareDevice software;
	litefont::GlyphRasterizer glyphs;
	litefont::TextRenderer cpuRenderer(software, glyphs);
	std::deque<litefont::Text> copies;
	software.beginFrame(litefont::Size2i{Example::width, Example::height}, clear);
	for (const litefont::Text& label : labels) {
		litefont::Text& copy = copies.emplace_back(label);
		copy.setPosition(litefont::Vector2f{});
		copy.setOffset(label.getPosition());
		cpuRenderer.drawNow(cpuRenderer.add(copy), 1.0f);
	}
	const litefont::Image cpu = software.readPixels();

	// The y-up passes draw each Text at its mirrored height, from where it
	// grows down toward smaller y.
	const float w = static_cast<float>(Example::width);
	const float h = static_cast<float>(Example::height);
	std::vector<litefont::Text> raised = labels;
	for (litefont::Text& label : raised) {
		label.setPosition(litefont::Vector2f{label.getPosition().x, h - label.getPosition().y});
	}

	litefont::OpenGLDevice& device = Example::getDevice();
	litefont::OpenGLRenderer& renderer = Example::getRenderer();
	const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
	const GLenum contextProfileMask = 0x9126;
	GLint profile = 0;
	glGetIntegerv(contextProfileMask, &profile);
	while (glGetError() != GL_NO_ERROR) {
	}
	std::printf("OpenGL %s, %s profile, %s\n", version != nullptr ? version : "(unknown)", (profile & 1) != 0 ? "core" : "compatibility",
		Example::core ? "OpenGL33Device" : "OpenGL11Device");
	bool good = true;
	for (int pass = 0; pass < (Example::core ? 3 : 4); pass++) {
		glClearColor(clear.r, clear.g, clear.b, clear.a);
		glClear(GL_COLOR_BUFFER_BIT);
		const char* name = "viewport pixels";
		const std::vector<litefont::Text>* drawn = &labels;
		if (pass == 0) {
			device.useViewport();
		}
		else if (pass == 1) {
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
		else {
			name = "matrix stack, y up";
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0.0, w, 0.0, h, -1.0, 1.0);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			Example::legacyDevice.useMatrixStack();
			device.setYUp(true);
			drawn = &raised;
		}
		for (const litefont::Text& label : *drawn) {
			renderer.draw(label);
		}
		const litefont::Image gpu = device.readPixels();
		device.useViewport();

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
		good = good && matches;
	}
	const bool restored = Example::checkStateRestored(labels);
	std::printf("%-20s %s\n", "OpenGL state", restored ? "restored after drawing" : "changed by drawing");
	if (device.getError() != litefont::OpenGLError::None) {
		std::printf("device error %d\n", static_cast<int>(device.getError()));
	}
	return good && restored ? 0 : 1;
}

bool Example::checkStateRestored(const std::vector<litefont::Text>& labels) {
	// A state unlike the one litefont draws with, which drawing must leave as
	// it found it. Core profiles reach the newer functions through litefont's
	// own function table, the way the device does.
	using GL = litefont::OpenGL33Functions;
	GL gl;
	const bool newer = Example::core && gl.load();
	GLuint texture = 0;
	GLuint vertexArray = 0;
	GLuint buffer = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_SCISSOR_TEST);
	glScissor(1, 2, 3, 4);
	glDisable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ZERO);
	if (newer) {
		gl.glGenVertexArrays(1, &vertexArray);
		gl.glBindVertexArray(vertexArray);
		gl.glGenBuffers(1, &buffer);
		gl.glBindBuffer(GL::ARRAY_BUFFER, buffer);
		gl.glActiveTexture(GL::TEXTURE0 + 1);
	}
	auto read = [newer]() {
		std::vector<GLint> values;
		for (const GLenum capability : {GL_DEPTH_TEST, GL_CULL_FACE, GL_SCISSOR_TEST, GL_BLEND}) {
			values.push_back(glIsEnabled(capability));
		}
		GLint box[4] = {0, 0, 0, 0};
		glGetIntegerv(GL_SCISSOR_BOX, box);
		values.insert(values.end(), std::begin(box), std::end(box));
		std::vector<GLenum> names = {GL::BLEND_SRC_RGB, GL::BLEND_DST_RGB, GL_TEXTURE_BINDING_2D};
		if (newer) {
			names.insert(names.end(), {GL::VERTEX_ARRAY_BINDING, GL::ARRAY_BUFFER_BINDING, GL::ACTIVE_TEXTURE, GL::CURRENT_PROGRAM});
		}
		for (const GLenum name : names) {
			GLint value = 0;
			glGetIntegerv(name, &value);
			values.push_back(value);
		}
		return values;
	};
	const std::vector<GLint> before = read();
	for (const litefont::Text& label : labels) {
		Example::getRenderer().draw(label);
	}
	const std::vector<GLint> after = read();

	// Back to the defaults for whatever is drawn next.
	if (newer) {
		gl.glActiveTexture(GL::TEXTURE0);
		gl.glBindBuffer(GL::ARRAY_BUFFER, 0);
		gl.glDeleteBuffers(1, &buffer);
		gl.glBindVertexArray(0);
		gl.glDeleteVertexArrays(1, &vertexArray);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	glDeleteTextures(1, &texture);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	return before == after;
}

int main(int argumentCount, char** arguments) {
	return Example::run(argumentCount, arguments);
}
