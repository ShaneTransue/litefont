// litefont in an old-style OpenGL program: declare, load once, draw.
//
//     litefont_freeglut <font.ttf>                the window
//     litefont_freeglut <font.ttf> --self-check   draws one frame through OpenGL and on the CPU,
//                                                  prints the difference and exits (0 when they match)
//     litefont_freeglut <font.ttf> --screenshot <file.ppm>   saves the fourth frame of the window and exits

#include <litefont/litefont.h>

#include <GL/freeglut.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <vector>

class Example {
public:
	static int run(int argumentCount, char** arguments);

private:
	static void display();
	static void reshape(int width, int height);
	static void idle();
	static void drawTriangle(float angle);
	static std::vector<litefont::Text> getLabels();
	static int checkAgainstCpu();

	// (1) Declared once.
	static litefont::Font font;
	static litefont::OpenGLRenderer renderer;
	static litefont::Text tag;
	static int width;
	static int height;
	static int frame;
	static bool selfCheck;
	static std::string screenshot;
	static std::chrono::steady_clock::time_point start;
};

litefont::Font Example::font;
litefont::OpenGLRenderer Example::renderer;
litefont::Text Example::tag;
int Example::width = 820;
int Example::height = 520;
int Example::frame = 0;
bool Example::selfCheck = false;
std::string Example::screenshot;
std::chrono::steady_clock::time_point Example::start;

int Example::run(int argumentCount, char** arguments) {
	if (argumentCount < 2) {
		std::printf("usage: litefont_freeglut <font.ttf> [--self-check]\n");
		return 1;
	}
	Example::selfCheck = argumentCount > 2 && std::string(arguments[2]) == "--self-check";
	if (argumentCount > 3 && std::string(arguments[2]) == "--screenshot") {
		Example::screenshot = arguments[3];
	}

	glutInit(&argumentCount, arguments);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(Example::width, Example::height);
	glutCreateWindow("litefont + freeglut");

	// (2) Created once: a font file. The renderer makes its OpenGL objects itself at the first draw.
	if (!Example::font.load(litefont::File::toPath(arguments[1]))) {
		std::printf("cannot load %s\n", arguments[1]);
		return 1;
	}
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

	// The program's own drawing, the old way: one turn every two seconds,
	// however fast frames come.
	const float seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - Example::start).count();
	const float angle = seconds * 3.14159265f;
	Example::drawTriangle(angle);

	// (3) Drawn wherever the program draws.
	Example::renderer.drawText(Example::font, 32.0f, "Hello from litefont", 24.0f, 20.0f);
	for (const litefont::Text& label : Example::getLabels()) {
		Example::renderer.draw(label);
	}
	const std::string counter = "frame " + std::to_string(Example::frame);
	Example::renderer.drawText(Example::font, 12.0f, counter, 24.0f, static_cast<float>(Example::height) - 28.0f,
		litefont::Color4f{0.6f, 0.6f, 0.6f, 1.0f});

	// A label that follows the triangle's top corner: moving it costs no layout.
	const float x = (0.5f + 0.35f * std::cos(angle + 1.5708f) * 0.5f) * static_cast<float>(Example::width);
	const float y = (0.5f - 0.35f * std::sin(angle + 1.5708f) * 0.5f) * static_cast<float>(Example::height);
	Example::renderer.draw(Example::tag, std::round(x), std::round(y) - 6.0f);

	// Frame 3: the triangle has been drawn after text three times, so a state
	// the renderer failed to restore would show in it.
	if (!Example::screenshot.empty() && Example::frame == 3) {
		const bool saved = Example::renderer.getDevice().readPixels().save(litefont::File::toPath(Example::screenshot));
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

std::vector<litefont::Text> Example::getLabels() {
	// Texts with settings of their own, drawn with renderer.draw().
	std::vector<litefont::Text> labels;
	litefont::Text paragraph("Old-style OpenGL draws the triangle with glBegin and glEnd; litefont draws the text "
		"on top through OpenGL 1.1, with no loader and no shaders, and puts the state back.", Example::font, 15.0f);
	paragraph.setPosition(litefont::Vector2f{24.0f, 70.0f});
	paragraph.setMaxWidth(360.0f);
	paragraph.setColor(litefont::Color4f{0.85f, 0.85f, 0.85f, 1.0f});
	labels.push_back(paragraph);

	const std::string formula = "E = mc2 and H2O, underlined";
	litefont::Text scripts(formula, Example::font, 18.0f);
	scripts.setPosition(litefont::Vector2f{24.0f, 170.0f});
	scripts.setColor(litefont::Color4f{0.95f, 0.85f, 0.5f, 1.0f});
	scripts.setStyleRuns({
		litefont::TextStyleRun{6, 1, litefont::FontStyle::Regular, litefont::TextScript::Superscript},
		litefont::TextStyleRun{13, 1, litefont::FontStyle::Regular, litefont::TextScript::Subscript},
		litefont::TextStyleRun{17, 10, litefont::FontStyle::Regular, litefont::TextScript::Normal, true, false}});
	labels.push_back(scripts);

	litefont::Text outlined("Outlined and shadowed", Example::font, 22.0f);
	outlined.setPosition(litefont::Vector2f{24.0f, 210.0f});
	outlined.setStyle(litefont::FontStyle::Bold);
	outlined.setColor(litefont::Color4f{0.95f, 0.78f, 0.35f, 1.0f});
	outlined.setOutline(litefont::TextOutline{1.0f, litefont::Color4f{0.0f, 0.0f, 0.0f, 1.0f}});
	outlined.setShadow(litefont::TextShadow{litefont::Vector2f{2.0f, 2.0f}, litefont::Color4f{0.0f, 0.0f, 0.0f, 0.6f}, 3.0f});
	labels.push_back(outlined);

	litefont::Text rotated("Rotated axis label", Example::font, 14.0f);
	rotated.setPosition(litefont::Vector2f{780.0f, 260.0f});
	rotated.setAnchor(litefont::Vector2f{0.5f, 0.5f});
	rotated.setRotation(litefont::TextRotation::CounterClockwise);
	rotated.setColor(litefont::Color4f{0.6f, 0.8f, 0.95f, 1.0f});
	labels.push_back(rotated);
	return labels;
}

int Example::checkAgainstCpu() {
	// The same Texts through OpenGL and through SoftwareDevice, placed the way
	// OpenGLRenderer places them: laid out at the origin and moved by an offset.
	const litefont::Color4f clear{0.11f, 0.11f, 0.11f, 1.0f};
	std::vector<litefont::Text> labels = Example::getLabels();
	litefont::Text greeting("Hello from litefont", Example::font, 32.0f);
	greeting.setColor(litefont::Color4f{1.0f, 1.0f, 1.0f, 1.0f});
	greeting.setPosition(litefont::Vector2f{24.0f, 20.0f});
	labels.insert(labels.begin(), greeting);
	litefont::Text placedTag = Example::tag;
	placedTag.setPosition(litefont::Vector2f{410.0f, 300.0f});
	labels.push_back(placedTag);

	for (const litefont::Text& label : labels) {
		Example::renderer.draw(label);
	}
	const litefont::Image gpu = Example::renderer.getDevice().readPixels();

	litefont::SoftwareDevice software;
	litefont::GlyphRasterizer glyphs;
	litefont::TextRenderer cpuRenderer(software, glyphs);
	std::deque<litefont::Text> copies;
	software.beginFrame(litefont::Size2i{gpu.getWidth(), gpu.getHeight()}, clear);
	for (const litefont::Text& label : labels) {
		litefont::Text& copy = copies.emplace_back(label);
		copy.setPosition(litefont::Vector2f{});
		copy.setOffset(label.getPosition());
		cpuRenderer.drawNow(cpuRenderer.add(copy), 1.0f);
	}
	const litefont::Image cpu = software.readPixels();

	int largest = 0;
	int over = 0;
	int lit = 0;
	for (int y = 0; y < gpu.getHeight(); y++) {
		for (int x = 0; x < gpu.getWidth(); x++) {
			const litefont::Color4u8 a = gpu.getPixel(x, y);
			const litefont::Color4u8 b = cpu.getPixel(x, y);
			const int difference = std::max({std::abs(a.r - b.r), std::abs(a.g - b.g), std::abs(a.b - b.b)});
			largest = std::max(largest, difference);
			over += difference > 1 ? 1 : 0;
			lit += b.r > 60 ? 1 : 0;
		}
	}
	// Two levels: each device rounds every blend, and the tag stacks translucent layers.
	const bool good = gpu.getWidth() > 0 && lit > 1000 && largest <= 2;
	std::printf("%d x %d px, %d lit pixels: largest difference %d levels, %d pixels over one level: %s\n",
		gpu.getWidth(), gpu.getHeight(), lit, largest, over, good ? "OpenGL matches the CPU" : "OpenGL differs from the CPU");
	return good ? 0 : 1;
}

int main(int argumentCount, char** arguments) {
	return Example::run(argumentCount, arguments);
}
