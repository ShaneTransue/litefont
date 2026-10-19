// The smallest litefont program with OpenGL: "Hello world" in a freeglut window.
//
//     litefont_hello_glut [font.ttf]
//
// Without a font it loads fonts/NotoSans-Regular.ttf next to the program.

#include <litefont/litefont.h>

#include <GL/freeglut.h>

#include <cstdio>
#include <filesystem>

// (1) Declared once: a font, the device for OpenGL, and a painter.
static litefont::Font font;
static litefont::OpenGL11Device device;
static litefont::TextPainter painter(device);

// (2) Loaded once.
static bool init(int argumentCount, char** arguments) {
	const std::filesystem::path path = argumentCount > 1 ? litefont::File::toPath(arguments[1])
		: litefont::File::toPath(arguments[0]).parent_path() / "fonts" / "NotoSans-Regular.ttf";
	if (!font.load(path)) {
		std::printf("cannot load %s\n", path.string().c_str());
		return false;
	}
	return true;
}

// (3) Drawn wherever the program draws.
static void display() {
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	painter.drawText(font, 48.0f, "Hello world", 40.0f, 40.0f);
	painter.drawText(font, 18.0f, "drawn with litefont through OpenGL", 42.0f, 110.0f, litefont::Color4f{0.6f, 0.8f, 1.0f, 1.0f});
	glutSwapBuffers();
}

static void resize(int width, int height) {
	glViewport(0, 0, width, height);
}

int main(int argumentCount, char** arguments) {
	glutInit(&argumentCount, arguments);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(640, 360);
	glutCreateWindow("Hello litefont");
	if (!init(argumentCount, arguments)) {
		return 1;
	}
	glutDisplayFunc(display);
	glutReshapeFunc(resize);
	glutMainLoop();
	return 0;
}
