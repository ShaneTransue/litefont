#pragma once

/**
 * litefont - A small text rendering library: OpenType fonts (TrueType and CFF
 * outlines, collections, GPOS kerning) rendered to crisp, hinted glyph
 * images, laid out as paragraphs and labels, and drawn through any
 * GraphicsDevice: OpenGL 1.1 or 3.3, Direct3D 11, or the CPU.
 *
 * This header includes everything a program usually needs. The quickest
 * start is the device for the program's graphics API with a TextPainter:
 * load a Font, then call drawText() while drawing.
 */

#include <litefont/Font.h>
#include <litefont/FontCatalog.h>
#include <litefont/FontFamily.h>
#include <litefont/GlyphRasterizer.h>
#include <litefont/Matrix4.h>
#include <litefont/D3D11Device.h>
#include <litefont/OpenGL11Device.h>
#include <litefont/OpenGL33Device.h>
#include <litefont/SoftwareDevice.h>
#include <litefont/Text.h>
#include <litefont/TextCursor.h>
#include <litefont/TextLayout.h>
#include <litefont/TextPainter.h>
#include <litefont/TextRenderer.h>
