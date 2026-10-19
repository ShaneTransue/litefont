<h1 align="center"><img src="doc/figures/banner.png" alt="litefont" width="760"></h1>

A small text rendering library in C++20: OpenType fonts in, crisp text out, drawn through whichever graphics API your program uses (OpenGL 1.1, an OpenGL 3.3 core profile, Direct3D 11) or on the CPU.

No third-party libraries and no extension loader. Pick the device for your graphics API once, load a font once, and call `drawText` wherever your program already draws, next to your own drawing.

```cpp
#include <litefont/litefont.h>

litefont::Font font;                          // 1. declare
litefont::OpenGL11Device device;              //    or OpenGL33Device, D3D11Device, SoftwareDevice
litefont::TextPainter painter(device);

void init() {
	font.load("NotoSans-Regular.ttf");        // 2. load once
}

void display() {
	// ... your own drawing ...
	painter.drawText(font, 16.0f, "Hello world", 20.0f, 40.0f);   // 3. draw
}
```

## Contents

- [Features](#features)
- [Output](#output)
- [Dependencies](#dependencies)
- [Building](#building)
- [Using litefont in your project](#using-litefont-in-your-project)
- [Usage](#usage)
- [Examples](#examples)
- [Project layout](#project-layout)
- [License](#license)

## Features

| | |
|---|---|
| **Fonts** | TrueType and CFF (`.otf`) outlines, collections (`.ttc`, `.otc`), cmap formats 4 and 12, kerning from GPOS or the `kern` table, names and traits. Every read is bounds-checked, so damaged files fail cleanly. |
| **Families** | Regular, bold, italic and bold italic faces, with bold and italic simulated when a face is missing; fallback families for characters a font lacks; a catalog that finds families by name in a folder you choose. |
| **Glyphs** | Exact-area antialiasing with vertical hinting, so small text has solid baselines and x-heights; outlines, sharp and blurred shadows; all cached in a texture atlas. Gradients with a color in each corner of the text, blended by the device at no atlas cost. |
| **Layout** | Wrapping, alignment, tabs, line and letter spacing, line limits with an ellipsis, superscripts and subscripts, underline and strikethrough, colors and styles per range of text, anchors and quarter-turn rotation for labels. |
| **Editing** | Caret boxes, selection rectangles, hit tests and caret movement for building text fields. |
| **Devices** | One small interface (`GraphicsDevice`) with a device per graphics API: `OpenGL11Device`, `OpenGL33Device`, `D3D11Device` and `SoftwareDevice` for the CPU. Each puts your graphics state back after drawing, and all of them give the same pixels to within one level. |
| **Drawing** | Immediate `TextPainter` (`drawText`, cached per string) or retained `TextRenderer`, over any device; text on the pixel grid, or in your own transform (a matrix, or the OpenGL matrix stack), re-rendered at the size it shows. |

## Output

The freeglut example. The program draws a triangle with `glBegin`/`glEnd`, and litefont draws the text on top through OpenGL 1.1:
- a title shaded pink at the top to light blue at the bottom
- a wrapped paragraph about the library
- a feature list with a color per range, an underline and a strikethrough
- equations with superscripts and subscripts, each in its own color
- bold text with an outline, a blurred shadow and a different color in each corner
- the code that draws it, in a monospaced font with syntax colors, on a background box
- a label held to two lines and cut with an ellipsis
- a name tag following the triangle's corner, and a rotated axis label

The Direct3D 11 example draws the same scene in a plain Win32 program, and `--core` draws it in an OpenGL 3.3 core profile context.

<p align="center"><img src="doc/figures/freeglut.png" alt="The freeglut example" width="820"></p>

The same engine on the CPU (`SoftwareDevice`), at 150 percent scale: hinted small text, kerning, an outlined and shadowed label, a CFF (`.otf`) font and monospaced code.

<p align="center"><img src="doc/figures/samples.png" alt="Text rendered on the CPU" width="611"></p>

## Dependencies

| What | Needed for | Notes |
|---|---|---|
| A C++20 compiler with `<format>` | everything | Visual Studio 2022 17.2 or newer, GCC 13 or newer, Clang 17 or newer |
| [CMake](https://cmake.org) 3.20 or newer | building | |
| OpenGL (comes with the system) | the OpenGL devices | turn off with `-DLITEFONT_OPENGL=OFF` |
| Direct3D 11 (comes with the Windows SDK) | `D3D11Device`, Windows only | turn off with `-DLITEFONT_D3D11=OFF` |
| [freeglut](https://freeglut.sourceforge.net) | the freeglut example only | through vcpkg, your package manager or Homebrew (on macOS it needs XQuartz) |

The core of litefont (fonts, layout, glyphs, `TextPainter`, `TextRenderer` and the CPU device) uses only the C++ standard library; with both GPU devices turned off it depends on nothing else. The devices stay out of each other's way:
- **OpenGL:** no OpenGL header and no GLEW, GLAD or other loader. `source/OpenGLFunctions.cpp`, the only file that knows the operating system, finds each function at run time (`wglGetProcAddress` on Windows, the OpenGL framework on macOS, EGL or GLX on Linux).
- **Direct3D 11:** your program hands over its `ID3D11Device` and context, and litefont calls through them, so it links no Direct3D library. Its shaders are compiled ahead of time with `fxc` and built in (`source/D3D11Shaders.h`, which also shows their HLSL source). Its header only declares the Direct3D interfaces, so including litefont never pulls in Windows headers.

So far litefont has been built and tested with Visual Studio 2026 on Windows: OpenGL 1.1 (compatibility), OpenGL 3.3 core, Direct3D 11 and the CPU. The code is portable C++20, and the CMake setup covers GCC and Clang on Linux and macOS, but those builds, and the Linux and macOS OpenGL function lookup, have not been tried yet.

## Building

```sh
git clone https://github.com/ShaneTransue/litefont.git
cd litefont
cmake --preset default
cmake --build --preset release
```

That builds the library and the examples in Release (CMake 3.21 or newer for presets), and assembles a ready-to-use folder:

```
build/bin_release/
    examples/    the example programs, their DLLs, and fonts/ (open-source Noto fonts)
    sdk/         include/, lib/ (the library and its CMake package) and the license
```

The examples in `bin_release/examples` start with a double-click: given no fonts, they read the `fonts` folder next to them. Point another project at the SDK with `-DCMAKE_PREFIX_PATH=<...>/bin_release/sdk` and `find_package(litefont)`.

| Preset | What |
|---|---|
| `default` | configures `build/` for Release and Debug side by side; programs go to `build/bin/<config>`, libraries to `build/lib/<config>` |
| `vcpkg` | the same, with packages (freeglut) from the vcpkg in `VCPKG_ROOT` |
| `asan` | an AddressSanitizer build in `build-asan/` |

Build presets: `release` (everything, then `bin_release`), `debug`, `vcpkg-release` and `asan`. For settings of your own, such as a vcpkg path, put presets in a `CMakeUserPresets.json`, which git ignores.

Without presets, the usual commands work too; the `dist` target assembles `bin_<config>`:

```sh
cmake -S . -B build -DLITEFONT_BUILD_EXAMPLES=ON
cmake --build build --config Release --target dist
```

The examples need freeglut for the OpenGL one; the Direct3D 11 one needs nothing beyond Visual Studio.

**Windows (vcpkg)**

```sh
vcpkg install freeglut
set VCPKG_ROOT=<vcpkg>
cmake --preset vcpkg
cmake --build --preset vcpkg-release
```

**Linux (Debian, Ubuntu)**

```sh
sudo apt install build-essential cmake libgl1-mesa-dev freeglut3-dev
cmake --preset default
cmake --build --preset release
```

**macOS (Homebrew)**

```sh
brew install cmake freeglut
cmake --preset default
cmake --build --preset release
```

### Options

| Option | Default | Effect |
|---|---|---|
| `LITEFONT_OPENGL` | `ON` | build the OpenGL devices and link OpenGL |
| `LITEFONT_D3D11` | `ON` on Windows | build the Direct3D 11 device |
| `LITEFONT_BUILD_EXAMPLES` | `OFF` | build the examples (the freeglut one only when GLUT is found, the Direct3D 11 one only on Windows) |
| `LITEFONT_EXAMPLE_FONTS` | `examples/fonts` | the fonts copied next to the windowed examples (and into `bin_release`) as `fonts/`, so they run without arguments |
| `LITEFONT_INSTALL` | `ON` when built on its own | install rules and a CMake package |
| `LITEFONT_WARNINGS_AS_ERRORS` | `OFF` | treat compiler warnings as errors |
| `LITEFONT_SANITIZE` | `OFF` | build with AddressSanitizer |
| `BUILD_SHARED_LIBS` | `OFF` | build a shared library instead of a static one (the DLL goes next to the programs in `build/bin`) |

### Installing

```sh
cmake --install build --config Release --prefix <install folder>
```

This installs the SDK: the headers, the library and a CMake package (`litefontConfig.cmake`), the same files as `bin_release/sdk`.

## Using litefont in your project

With **FetchContent**, straight from GitHub:

```cmake
include(FetchContent)
FetchContent_Declare(litefont GIT_REPOSITORY https://github.com/ShaneTransue/litefont.git GIT_TAG main)
FetchContent_MakeAvailable(litefont)

target_link_libraries(app PRIVATE litefont::litefont)
```

As a **subdirectory** (a copy or a git submodule):

```cmake
add_subdirectory(litefont)
target_link_libraries(app PRIVATE litefont::litefont)
```

After **installing**:

```cmake
find_package(litefont REQUIRED)
target_link_libraries(app PRIVATE litefont::litefont)
```

When litefont is added to another project, only the library is built; its examples stay off unless you turn them on.

## Usage

Everything is in the `litefont` namespace, and `<litefont/litefont.h>` includes what a program usually needs. Each class is documented in its header.

### Devices

Name the device for your graphics API once, where you declare it; everything after works on the `GraphicsDevice` interface and draws the same:

| Device | For | Setup |
|---|---|---|
| `OpenGL11Device` | any OpenGL compatibility context (the default in freeglut, GLFW, SDL and Qt), next to `glBegin`/`glEnd` | none; it finds its functions at the first draw |
| `OpenGL33Device` | an OpenGL 3.3 or newer context, core or compatibility | none; it finds its functions at the first draw |
| `D3D11Device` | Direct3D 11, feature level 10.0 or newer | `D3D11Device device(d3dDevice, context);` or `device.attach(...)` once they exist |
| `SoftwareDevice` | the CPU: an image in memory | `device.beginFrame(size, clearColor);` before drawing, `device.readPixels()` after |

- **State:** each GPU device saves the state it changes and restores it after drawing (`glPushAttrib` and the matrix stacks in OpenGL 1.1; reading and resetting each value in OpenGL 3.3 and Direct3D 11). `setRestoreState(false)` skips that if you manage state yourself.
- **Objects:** each device makes its textures, buffers and shaders at the first draw, so it can be declared before the context or device exists. `painter.release()` deletes them (with the context current) and starts over. `device.getError()` says why nothing was drawn.

### Drawing

```cpp
litefont::TextPainter painter(device);

painter.drawText(font, 24.0f, "Score: 120", 10.0f, 10.0f, litefont::Color4f{1.0f, 0.8f, 0.2f, 1.0f});
const litefont::Rectf box = painter.measureText(font, 24.0f, "Centered");
painter.drawText(font, 24.0f, "Centered", (width - box.width) / 2.0f, 100.0f);
```

- **Immediate:** every call draws at once, in order with your own drawing.
- **Cached:** each distinct string (font, size, style, color) keeps its layout and glyphs, so redrawing it costs no layout or glyph rendering, even when it moves every frame. The least recently drawn strings go once more than 1024 are kept (`setCacheCapacity`).
- **Coordinates:** pixels from the top-left corner of the current viewport, by default. Call `setScale(ratio)` on high-density screens.

### Drawing in your own transform

By default text sits on the pixel grid. To draw it in your own coordinates instead (a zoomable 2D view, a rotating label, a point in a 3D scene), tell the device:

```cpp
// any device: a matrix (column-major, projection times model-view)
device.setMatrix(projection * view * model);
painter.drawText(font, 0.5f, "A", x, y);    // a size of half a unit
device.useViewport();

// OpenGL 1.1: whatever you set on the matrix stack
glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(-w / 2, w / 2, -h / 2, h / 2, -1, 1);
glMatrixMode(GL_MODELVIEW);  glLoadIdentity(); glRotatef(degrees, 0, 0, 1);
device.useMatrixStack();
painter.draw(label);
device.useViewport();
```

- **Units:** positions and sizes are in your units, and text grows downward from its position. Y is up by default, as with `glOrtho(0, w, 0, h)` or world coordinates; call `device.setYUp(false)` for a y-down space.
- **Sharp at any zoom:** each string is rendered at about the size it covers on screen (in steps of a sixth of an octave), and your matrix carries it the rest of the way with linear filtering. Moving, turning or zooming it costs no new layout until the size crosses a step.
- **Clips** and `setScale` apply to the pixel grid only.

### Styled text and labels

A `Text` holds every setting of a piece of text; `painter.draw(text, x, y)` draws it.

```cpp
litefont::FontFamily family(regular);
family.addFace(bold);                                   // real faces are used when present

litefont::Text label("E = mc2, in bold", family, 18.0f);
label.setStyleRuns({
	litefont::TextStyleRun{6, 1, litefont::FontStyle::Regular, litefont::TextScript::Superscript},
	litefont::TextStyleRun{12, 4, litefont::FontStyle::Bold}});
label.setOutline(litefont::TextOutline{1.0f, litefont::Color4f{0.0f, 0.0f, 0.0f, 1.0f}});
label.setShadow(litefont::TextShadow{litefont::Vector2f{2.0f, 2.0f}, litefont::Color4f{0.0f, 0.0f, 0.0f, 0.6f}, 3.0f});
label.setGradient(litefont::TextGradient::vertical(pink, lightBlue));   // or a color for each corner

litefont::Text axis("Stress (MPa)", family, 14.0f);
axis.setRotation(litefont::TextRotation::CounterClockwise);
axis.setAnchor(litefont::Vector2f{0.5f, 0.5f});         // centered on its position

painter.draw(label, 20.0f, 40.0f);
painter.draw(axis, 30.0f, 300.0f);
```

Other settings include wrap width, alignment, line and letter spacing, line limits with an ellipsis, colors per range, underline, strikethrough and a background box.

A gradient gives each corner of the text's box a color (`TextGradient{topLeft, topRight, bottomLeft, bottomRight}`, or `vertical` and `horizontal` for two). Glyphs, underlines and strikethroughs take the blend where they sit; color runs keep their own colors, and outlines, shadows and backgrounds keep theirs.

### Rendering on the CPU

The CPU is a device like the others, so the same painter renders into an image:

```cpp
litefont::SoftwareDevice device;
litefont::TextPainter painter(device);

device.beginFrame(litefont::Size2i{400, 60}, litefont::Color4f{1.0f, 1.0f, 1.0f, 1.0f});
painter.drawText(font, 20.0f, "Rendered without a GPU", 10.0f, 16.0f, litefont::Color4f{0.1f, 0.1f, 0.1f, 1.0f});
device.endFrame();
device.readPixels().save("text.pam");                   // PGM, PPM or PAM
```

### Fonts

- `Font::load(path, face)` reads a `.ttf`, an `.otf`, or one face of a `.ttc`; `Font::load(bytes)` reads a font already in memory.
- `FontFamily::addFallback(other)` takes characters the family lacks from another family, for example CJK text inside a Latin UI font.
- `FontCatalog::scan(folder)` reads only the names and traits of the fonts in a folder. `findFamily("Name", family)` then loads that family's faces.

litefont never searches the system for fonts. It reads the files and folders you give it.

## Examples

The presets build the examples (`LITEFONT_BUILD_EXAMPLES`). Pass any `.ttf`, `.otf` or `.ttc` file, and a monospaced one second for the code sample if you like. Given no fonts, the windowed examples read the `fonts` folder next to the program and pick a common sans-serif family (with its bold face) and a monospaced family from it. That folder comes from `examples/fonts`: Noto Sans, Noto Sans Bold and Noto Sans Mono, under the SIL Open Font License (`examples/fonts/OFL.txt`), not litefont's MIT license.

| Example | Run |
|---|---|
| `examples/hello_glut`: the smallest OpenGL program, "Hello world" in a freeglut window in about 50 lines, with no classes: `init`, `display`, `resize` and `main` | `litefont_hello_glut [font.ttf]` |
| `examples/hello_d3d11`: the smallest Direct3D 11 program, "Hello world" in a plain Win32 window | `litefont_hello_d3d11 [font.ttf]` |
| `examples/freeglut`: a triangle drawn with `glBegin`, text on top, and a label that follows a moving point | `litefont_freeglut [font.ttf] [mono.ttf]` |
| the same scene in an OpenGL 3.3 core profile context, through `OpenGL33Device` | `litefont_freeglut <font.ttf> --core` |
| `examples/d3d11`: the same scene in a plain Win32 program, through `D3D11Device`, with the triangle drawn by the program's own shaders | `litefont_d3d11 [font.ttf] [mono.ttf]` |
| comparing the GPU against the CPU device in every space the device has (pixels, matrices with y down and up, the OpenGL matrix stack), and checking that drawing leaves the graphics state as it was | `--self-check` on either (add `--core` for the OpenGL 3.3 device) |
| `examples/rasterize`: text to an image file on the CPU | `litefont_rasterize <font.ttf> "Hello" 16 hello.ppm --theme light` |

`litefont_rasterize` also takes `--scale`, `--width`, `--align`, `--style`, `--outline`, `--shadow` and `--pixel-advances`.

## Project layout

```
include/litefont/    public headers (litefont.h includes the usual ones)
source/              implementation
examples/            freeglut, Direct3D 11 and command-line examples (shared/ holds their common scene,
                     fonts/ the open-source fonts they ship with)
doc/                 a short PDF guide (litefont.pdf) and its LaTeX source
cmake/               package configuration template
CMakePresets.json    the build presets
```

## License

litefont is released under the [MIT License](LICENSE). Copyright (c) 2026 Shane Transue.
