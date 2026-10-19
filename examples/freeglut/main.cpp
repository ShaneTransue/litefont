// litefont in an OpenGL program: declare, load once, draw.
//
//     litefont_freeglut [font.ttf] [mono.ttf]      an OpenGL 1.1 window: a triangle drawn with glBegin and text on top,
//                                                   with the code sample in the monospaced font when one is given;
//                                                   with no fonts named, the fonts folder beside the program is used
//     litefont_freeglut <font.ttf> --core          the text in an OpenGL 3.3 core profile context (no triangle:
//                                                   core profiles have no glBegin)
//     litefont_freeglut <font.ttf> --self-check    draws the labels through OpenGL in every space the device has
//                                                   and on the CPU, prints the differences and exits (0 when they match)
//     litefont_freeglut <font.ttf> --screenshot <file.ppm>   saves the fourth frame of the window and exits
//
// --core goes with either of the other two.

#include "ExampleScene.h"

#include <litefont/litefont.h>

#include <GL/freeglut.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
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
	static litefont::OpenGLDevice& getDevice();
	static litefont::TextPainter& getPainter();
	static std::vector<litefont::Text> getLabels();
	static int checkAgainstCpu();
	static bool checkStateRestored(const std::vector<litefont::Text>& labels);

	// (1) Declared once: a device for the program's context and a painter
	// that draws through it. This example can run in either kind of context,
	// so it declares both pairs; a program declares the one it needs.
	static litefont::Font font;
	static litefont::Font bold;
	static litefont::Font mono;
	static litefont::FontFamily family;
	static std::string fontName;
	static litefont::OpenGL11Device legacyDevice;
	static litefont::TextPainter legacyPainter;
	static litefont::OpenGL33Device coreDevice;
	static litefont::TextPainter corePainter;
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
litefont::Font Example::bold;
litefont::Font Example::mono;
litefont::FontFamily Example::family;
std::string Example::fontName;
litefont::OpenGL11Device Example::legacyDevice;
litefont::TextPainter Example::legacyPainter(Example::legacyDevice);
litefont::OpenGL33Device Example::coreDevice;
litefont::TextPainter Example::corePainter(Example::coreDevice);
litefont::Text Example::tag;
int Example::width = ExampleScene::width;
int Example::height = ExampleScene::height;
int Example::frame = 0;
bool Example::core = false;
bool Example::selfCheck = false;
std::string Example::screenshot;
std::chrono::steady_clock::time_point Example::start;

int Example::run(int argumentCount, char** arguments) {
	std::vector<std::string> fontPaths;
	for (int i = 1; i < argumentCount; i++) {
		const std::string argument = arguments[i];
		if (argument.rfind("--", 0) != 0) {
			fontPaths.push_back(argument);
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

	// (2) Created once: a font file. The device makes its OpenGL objects itself at the first draw.
	if (!ExampleScene::loadFonts(fontPaths, arguments[0], Example::font, Example::bold, Example::mono, Example::fontName)) {
		std::printf("usage: litefont_freeglut [font.ttf] [mono.ttf] [--core] [--self-check | --screenshot <file.ppm>]\n");
		return 1;
	}
	Example::family = litefont::FontFamily(Example::font);
	if (Example::bold.isLoaded()) {
		Example::family.addFace(Example::bold);
	}
	Example::tag = ExampleScene::getTag(Example::family);

	glutDisplayFunc(Example::display);
	glutReshapeFunc(Example::reshape);
	glutIdleFunc(Example::idle);
	Example::start = std::chrono::steady_clock::now();
	glutMainLoop();
	return 0;
}

void Example::display() {
	const litefont::Color4f background = ExampleScene::getBackground();
	glClearColor(background.r, background.g, background.b, background.a);
	glClear(GL_COLOR_BUFFER_BIT);
	if (Example::selfCheck) {
		std::exit(Example::checkAgainstCpu());
	}
	litefont::TextPainter& painter = Example::getPainter();

	// The program's own drawing.
	const float seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - Example::start).count();
	const float angle = ExampleScene::getAngle(seconds, !Example::screenshot.empty());
	if (!Example::core) {
		Example::drawTriangle(angle);
	}

	// (3) Drawn wherever the program draws: Texts with settings of their own,
	// and plain strings with drawText.
	for (const litefont::Text& label : Example::getLabels()) {
		painter.draw(label);
	}
	const std::string counter = "frame " + std::to_string(Example::frame);
	painter.drawText(Example::font, 12.0f, counter, 24.0f, static_cast<float>(Example::height) - 28.0f, litefont::Color4f{0.6f, 0.6f, 0.6f, 1.0f});

	// A label that follows the triangle's top corner: moving it costs no layout.
	const litefont::Vector2f point = ExampleScene::getTagPoint(angle, Example::width, Example::height);
	painter.draw(Example::tag, point.x, point.y);

	// Frame 3: the triangle has been drawn after text three times, so a state
	// the device failed to restore would show in it.
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
		const litefont::Color4f color = ExampleScene::getCornerColor(corner);
		const litefont::Vector2f position = ExampleScene::getCorner(angle, corner);
		glColor3f(color.r, color.g, color.b);
		glVertex2f(position.x, position.y);
	}
	glEnd();
}

litefont::OpenGLDevice& Example::getDevice() {
	if (Example::core) {
		return Example::coreDevice;
	}
	return Example::legacyDevice;
}

litefont::TextPainter& Example::getPainter() {
	return Example::core ? Example::corePainter : Example::legacyPainter;
}

std::vector<litefont::Text> Example::getLabels() {
	const ExampleDevice device = Example::core
		? ExampleDevice{"OpenGL 3.3 core profile  ·  OpenGL33Device", "litefont::OpenGL33Device device;"}
		: ExampleDevice{"OpenGL 1.1  ·  OpenGL11Device", "litefont::OpenGL11Device device;"};
	return ExampleScene::getLabels(Example::family, Example::mono, Example::fontName, device);
}

int Example::checkAgainstCpu() {
	// The labels and a placed tag through OpenGL in every space the device
	// has, against the CPU, then the OpenGL state around a draw.
	std::vector<litefont::Text> labels = Example::getLabels();
	litefont::Text placedTag = Example::tag;
	placedTag.setPosition(litefont::Vector2f{410.0f, 300.0f});
	labels.push_back(placedTag);

	const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
	const GLenum contextProfileMask = 0x9126;
	GLint profile = 0;
	glGetIntegerv(contextProfileMask, &profile);
	while (glGetError() != GL_NO_ERROR) {
	}
	std::printf("OpenGL %s, %s profile, %s\n", version != nullptr ? version : "(unknown)", (profile & 1) != 0 ? "core" : "compatibility",
		Example::core ? "OpenGL33Device" : "OpenGL11Device");

	const litefont::Color4f background = ExampleScene::getBackground();
	auto clear = [&background]() {
		glClearColor(background.r, background.g, background.b, background.a);
		glClear(GL_COLOR_BUFFER_BIT);
	};
	auto useStack = []() {
		if (Example::core) {
			return false;
		}
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0.0, Example::width, 0.0, Example::height, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		Example::legacyDevice.useMatrixStack();
		return true;
	};
	const bool good = ExampleScene::checkSpaces(Example::getDevice(), Example::getPainter(), labels,
		litefont::Size2i{Example::width, Example::height}, clear, useStack);
	const bool restored = Example::checkStateRestored(labels);
	std::printf("%-20s %s\n", "OpenGL state", restored ? "restored after drawing" : "changed by drawing");
	if (Example::getDevice().getError() != litefont::OpenGLError::None) {
		std::printf("device error %d\n", static_cast<int>(Example::getDevice().getError()));
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
		Example::getPainter().draw(label);
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
