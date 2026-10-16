#pragma once

#include <litefont/CffTable.h>
#include <litefont/GposTable.h>
#include <litefont/Bytes.h>
#include <litefont/ByteReader.h>
#include <litefont/ByteWriter.h>
#include <litefont/File.h>
#include <litefont/Outline.h>
#include <string>
#include <string_view>

#include <filesystem>

namespace litefont {

/** GlyphId - A glyph's index inside a font; 0 is the missing glyph. */
using GlyphId = std::uint16_t;

/**
 * @addtogroup Text
 *
 * FontError - Why the last Font::load failed.
 */
enum class FontError : std::uint8_t {
	None,
	File,
	Truncated,
	MissingTable,
	UnsupportedFormat,
	MissingFace,
	BadGlyph
};

/**
 * \struct FontMetrics Font.h <litefont/Font.h>
 * @addtogroup Text
 *
 * FontMetrics - Vertical metrics in font units. Line metrics come from hhea,
 * or from the OS/2 typographic values when the font sets USE_TYPO_METRICS.
 * The script, strikeout and underline values are what the font recommends,
 * as stored (OS/2 and post), and 0 when it has none: subscripts sit
 * subscriptOffset below the baseline and superscripts superscriptOffset
 * above it, at their sizes in font units of the full-size font; the
 * strikeout's top is strikeoutPosition above the baseline and the
 * underline's top underlinePosition above it (negative: below).
 */
struct FontMetrics {
	std::uint16_t unitsPerEm = 0;
	std::int16_t ascender = 0;
	std::int16_t descender = 0;
	std::int16_t lineGap = 0;
	std::int16_t xHeight = 0;
	std::int16_t capHeight = 0;
	std::int16_t subscriptSize = 0;
	std::int16_t subscriptOffset = 0;
	std::int16_t superscriptSize = 0;
	std::int16_t superscriptOffset = 0;
	std::int16_t strikeoutSize = 0;
	std::int16_t strikeoutPosition = 0;
	std::int16_t underlinePosition = 0;
	std::int16_t underlineThickness = 0;
};

/**
 * \struct FontTraits Font.h <litefont/Font.h>
 * @addtogroup Text
 *
 * FontTraits - Which face of its family a font is: its weight class (400
 * regular, 700 bold), whether it is the bold and the italic face, and the
 * italic angle in degrees (negative leans right, as fonts store it). Taken
 * from OS/2 (weight and fsSelection), head (macStyle) and post.
 */
struct FontTraits {
	std::uint16_t weight = 400;
	bool bold = false;
	bool italic = false;
	float italicAngle = 0.0f;
};

/**
 * \struct FontNames Font.h <litefont/Font.h>
 * @addtogroup Text
 *
 * FontNames - What the name table calls a font, in UTF-8: its family
 * ("Segoe UI"), its style within the family ("Bold Italic") and its full
 * name ("Segoe UI Bold Italic"). The family is the one that groups at most a
 * regular, bold, italic and bold italic face, so a semibold face usually
 * names a family of its own ("Segoe UI Semibold"). Windows English names are
 * preferred, then any Windows name, then Macintosh names.
 */
struct FontNames {
	std::string family;
	std::string subfamily;
	std::string fullName;
};

/**
 * \struct GlyphMetrics Font.h <litefont/Font.h>
 * @addtogroup Text
 *
 * GlyphMetrics - Horizontal metrics of one glyph, in font units.
 */
struct GlyphMetrics {
	std::uint16_t advance = 0;
	std::int16_t leftSideBearing = 0;
};

/**
 * \struct FontTable Font.h <litefont/Font.h>
 * @addtogroup Text
 *
 * FontTable - Where one table sits inside the font file, counted from the
 * start of the file (also inside a collection). A table can be present with
 * a length of 0 (a glyf table when every glyph is empty).
 */
struct FontTable {
	std::uint32_t offset = 0;
	std::uint32_t length = 0;
	bool present = false;
};

/**
 * \struct FontTables Font.h <litefont/Font.h>
 * @addtogroup Text
 *
 * FontTables - The tables the Font reads, located once when it loads.
 */
struct FontTables {
	FontTable head;
	FontTable maxp;
	FontTable hhea;
	FontTable hmtx;
	FontTable os2;
	FontTable cmap;
	FontTable loca;
	FontTable glyf;
	FontTable cff;
	FontTable kern;
	FontTable gpos;
	FontTable post;
	FontTable name;
};

/**
 * \class Font Font.h <litefont/Font.h>
 * @addtogroup Text
 *
 * Font - An OpenType font with TrueType (glyf) or PostScript (CFF) outlines,
 * from a single font file (.ttf, .otf) or one face of a collection (.ttc,
 * .otc). It owns its file's bytes and answers questions about them:
 * metrics, names, the glyph for a code point, advances, kerning and outlines.
 * Kerning comes from the GPOS 'kern' feature when the font has one, else from
 * the kern table. Every read is bounds-checked, so a malformed file makes
 * load() or getOutline() return false instead of reading outside the data.
 *
 * Each face of a collection loaded as its own Font keeps its own copy of
 * the whole file. getFaceCount() tells how many faces a file holds.
 *
 * The static read functions parse the parts of a font that describe it
 * (table directory, traits, names) from table bytes alone, so a catalog can
 * describe a font from a few small reads without loading it.
 */
class Font {
public:
	bool load(const std::filesystem::path& path, std::uint32_t face = 0);
	bool load(Bytes bytes, std::uint32_t face = 0);

	bool isLoaded() const;
	FontError getError() const;
	std::uint64_t getId() const;
	std::uint32_t getFace() const;
	std::uint16_t getGlyphCount() const;
	bool hasPostScriptOutlines() const;
	const FontMetrics& getMetrics() const;
	const FontTraits& getTraits() const;
	const FontNames& getNames() const;

	GlyphId getGlyph(char32_t codePoint) const;
	GlyphMetrics getGlyphMetrics(GlyphId glyph) const;
	std::int16_t getKerning(GlyphId left, GlyphId right) const;
	bool getOutline(GlyphId glyph, Outline& outline) const;

	static std::uint32_t getFaceCount(const ByteReader& file);
	static FontError readTableDirectory(const ByteReader& file, std::uint32_t face, FontTables& tables);
	static void readTraits(const ByteReader& head, const ByteReader& os2, const ByteReader& post, FontTraits& traits);
	static bool readNames(const ByteReader& name, FontNames& names);

private:
	bool fail(FontError error);
	bool readTables(std::uint32_t face);
	bool readHead();
	bool readMaxp();
	bool readHhea();
	bool readOs2();
	bool readCmap();
	bool readCff();
	bool readKern();
	void readGpos();
	void readDecorationMetrics();
	void estimateMissingHeights();
	std::int16_t getTableKerning(GlyphId left, GlyphId right) const;

	static bool readName(const ByteReader& name, std::uint16_t nameId, std::string& text);
	static bool decodeName(const ByteReader& data, bool utf16, std::string& text);
	static std::uint32_t getTag(const char (&name)[5]);
	static bool isSfntVersion(std::uint32_t version);

	ByteReader getTableReader(const FontTable& table) const;
	bool getGlyphData(GlyphId glyph, ByteReader& data) const;
	bool appendGlyph(GlyphId glyph, const Affine2f& transform, std::uint32_t depth, std::uint32_t& componentBudget, Outline& outline) const;
	bool appendSimpleGlyph(ByteReader& data, std::int16_t contourCount, const Affine2f& transform, Outline& outline) const;
	bool appendCompositeGlyph(ByteReader& data, const Affine2f& transform, std::uint32_t depth, std::uint32_t& componentBudget, Outline& outline) const;
	GlyphId lookupFormat4(char32_t codePoint) const;
	GlyphId lookupFormat12(char32_t codePoint) const;

	Bytes bytes;
	FontTables tables;
	FontMetrics metrics;
	FontTraits traits;
	FontNames names;
	CffTable cff;
	GposTable gpos;
	std::uint32_t face = 0;
	std::uint16_t glyphCount = 0;
	std::uint16_t horizontalMetricCount = 0;
	std::int16_t indexToLocFormat = 0;
	std::uint32_t characterMapOffset = 0;
	std::uint16_t characterMapFormat = 0;
	std::uint32_t kerningPairsOffset = 0;
	std::uint16_t kerningPairCount = 0;
	std::uint64_t id = 0;
	bool postScriptOutlines = false;
	bool loaded = false;
	FontError error = FontError::None;
};

}
