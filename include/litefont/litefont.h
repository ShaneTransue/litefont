#pragma once

/**
 * litefont - A small font engine: OpenType fonts (TrueType and CFF outlines,
 * collections, GPOS kerning) rendered to crisp, hinted glyph images, laid
 * out as paragraphs and labels, and drawn through OpenGL 1.1 or on the CPU.
 *
 * This header includes everything a program usually needs. The quickest
 * start is OpenGLRenderer: load a Font, then call drawText() while drawing.
 */

#include <litefont/Font.h>
#include <litefont/FontCatalog.h>
#include <litefont/FontFamily.h>
#include <litefont/GlyphRasterizer.h>
#include <litefont/OpenGLRenderer.h>
#include <litefont/SoftwareDevice.h>
#include <litefont/Text.h>
#include <litefont/TextCursor.h>
#include <litefont/TextLayout.h>
#include <litefont/TextRenderer.h>
