#pragma once

#include <litefont/Font.h>
#include <litefont/Bytes.h>
#include <litefont/ByteReader.h>
#include <litefont/ByteWriter.h>
#include <litefont/File.h>

#include <map>
#include <string>
#include <vector>

namespace litefont::tests {

/** TestPoint - One TrueType outline point in font units. */
struct TestPoint {
	std::int16_t x = 0;
	std::int16_t y = 0;
	bool onCurve = true;
};

/** TestComponent - One component of a composite test glyph. */
struct TestComponent {
	std::uint16_t glyph = 0;
	std::int16_t offsetX = 0;
	std::int16_t offsetY = 0;
	float scale = 1.0f;          // written as a uniform scale when it is not 1
	bool pointMatching = false;    // writes point indices instead of offsets
};

/** TestGlyph - A simple glyph (contours) or a composite glyph (components). */
struct TestGlyph {
	std::vector<std::vector<TestPoint>> contours;
	std::vector<TestComponent> components;
	std::uint16_t advance = 500;
	std::int16_t leftSideBearing = 0;
};

/** TestKerningPair - One pair for the kern table. */
struct TestKerningPair {
	std::uint16_t left = 0;
	std::uint16_t right = 0;
	std::int16_t value = 0;
};

/** TestName - One name table record, its text already encoded for its platform. */
struct TestName {
	std::uint16_t platform = 3;
	std::uint16_t encoding = 1;
	std::uint16_t language = 0x0409;
	std::uint16_t nameId = 1;
	std::vector<std::uint8_t> bytes;
};

/**
 * TestGposSubtable - One GPOS pair adjustment subtable. Format 1 lists pairs;
 * format 2 covers the listed first glyphs and kerns by class, glyphs missing
 * from a class map being class 0. The value formats decide which fields each
 * record has: x advance carries the value, other fields get filler.
 */
struct TestGposSubtable {
	std::uint16_t format = 1;
	std::uint16_t valueFormat1 = 0x0004;
	std::uint16_t valueFormat2 = 0x0000;
	std::vector<TestKerningPair> pairs;
	std::vector<std::uint16_t> coverage;
	std::map<std::uint16_t, std::uint16_t> firstClasses;
	std::map<std::uint16_t, std::uint16_t> secondClasses;
	std::vector<std::vector<std::int16_t>> classValues;
};

/**
 * TestGposLookup - One pair adjustment lookup, reached through its own
 * feature in the default language of its script, optionally wrapped in
 * extension subtables.
 */
struct TestGposLookup {
	std::string feature = "kern";
	std::string script = "DFLT";
	std::vector<TestGposSubtable> subtables;
	bool extension = false;
};

/** TestCff - The charstrings and subroutines of a CFF table, one byte program each. */
struct TestCff {
	std::vector<std::vector<std::uint8_t>> charStrings;
	std::vector<std::vector<std::uint8_t>> globalSubrs;
	std::vector<std::vector<std::uint8_t>> localSubrs;
};

/**
 * \class TestCharString TestFontBuilder.h
 *
 * TestCharString - Writes a Type 2 charstring: numbers in their shortest
 * encoding (or 16.16 fixed point), operators and escaped operators.
 */
class TestCharString {
public:
	TestCharString& number(std::int32_t value);
	TestCharString& fixed(float value);
	TestCharString& op(std::uint8_t code);
	TestCharString& escape(std::uint8_t code);
	TestCharString& raw(std::uint8_t byte);
	std::vector<std::uint8_t> take();

private:
	std::vector<std::uint8_t> bytes;
};

/**
 * \class TestFontBuilder TestFontBuilder.h
 *
 * TestFontBuilder - Writes small TrueType and CFF files with ByteWriter, so
 * font tests know every expected value exactly and can damage files on
 * purpose. Glyph 0 is an empty .notdef unless glyphs are added before
 * anything else is needed. With setCff() the outlines come from charstrings
 * instead of glyph contours, and the glyphs only give metrics.
 * buildCollection() packs built fonts into one collection file.
 */
class TestFontBuilder {
public:
	TestFontBuilder();

	TestFontBuilder& setLineMetrics(std::uint16_t unitsPerEm, std::int16_t ascender, std::int16_t descender, std::int16_t lineGap);
	TestFontBuilder& setOs2(std::int16_t typoAscender, std::int16_t typoDescender, std::int16_t typoLineGap, bool useTypoMetrics, std::int16_t xHeight, std::int16_t capHeight);
	TestFontBuilder& setStyle(std::uint16_t weight, bool bold, bool italic, std::int16_t italicAngle);
	TestFontBuilder& setDecorationMetrics(const FontMetrics& metrics);
	TestFontBuilder& setHorizontalMetricCount(std::uint16_t count);
	TestFontBuilder& setCharacterMapFormat(std::uint16_t format);
	TestFontBuilder& setGlyphIdArray(bool on);
	TestFontBuilder& setLongLocations(bool on);
	TestFontBuilder& omitTable(const std::string& tag);

	std::uint16_t addGlyph(const TestGlyph& glyph);
	TestFontBuilder& map(char32_t codePoint, std::uint16_t glyph);
	TestFontBuilder& addKerning(std::uint16_t left, std::uint16_t right, std::int16_t value);
	TestFontBuilder& addGposLookup(const TestGposLookup& lookup);
	TestFontBuilder& setNames(const std::string& family, const std::string& subfamily, const std::string& fullName);
	TestFontBuilder& addName(std::uint16_t platform, std::uint16_t encoding, std::uint16_t language, std::uint16_t nameId, const std::string& text);
	TestFontBuilder& addRawName(std::uint16_t platform, std::uint16_t encoding, std::uint16_t language, std::uint16_t nameId, const std::vector<std::uint8_t>& bytes);
	TestFontBuilder& setCff(const TestCff& cff);

	Bytes build() const;

	static Bytes buildCollection(const std::vector<Bytes>& fonts);

private:
	Bytes buildHead() const;
	Bytes buildMaxp() const;
	Bytes buildHhea() const;
	Bytes buildHmtx() const;
	Bytes buildOs2() const;
	Bytes buildCmap() const;
	Bytes buildKern() const;
	Bytes buildPost() const;
	Bytes buildName() const;
	Bytes buildGpos() const;
	Bytes buildCffTable() const;
	void buildGlyphs(Bytes& glyf, Bytes& loca) const;
	void writeSimpleGlyph(ByteWriter& writer, const TestGlyph& glyph) const;
	void writeCompositeGlyph(ByteWriter& writer, const TestGlyph& glyph) const;
	std::uint16_t getHorizontalMetricCount() const;

	static Bytes buildPairSubtable(const TestGposSubtable& subtable);
	static void writeValue(ByteWriter& writer, std::uint16_t valueFormat, std::int16_t xAdvance);
	static void writeIndex(ByteWriter& writer, const std::vector<std::vector<std::uint8_t>>& entries);
	static std::size_t getIndexSize(const std::vector<std::vector<std::uint8_t>>& entries);
	static std::uint32_t getTagValue(const std::string& tag);

	std::vector<TestGlyph> glyphs;
	std::map<char32_t, std::uint16_t> characterMap;
	std::vector<TestKerningPair> kerning;
	std::vector<TestGposLookup> gposLookups;
	std::vector<TestName> names;
	TestCff cff;
	bool hasCff = false;
	std::vector<std::string> omittedTables;
	std::uint16_t unitsPerEm = 1000;
	std::int16_t ascender = 800;
	std::int16_t descender = -200;
	std::int16_t lineGap = 0;
	bool hasOs2 = false;
	std::int16_t typoAscender = 0;
	std::int16_t typoDescender = 0;
	std::int16_t typoLineGap = 0;
	bool useTypoMetrics = false;
	std::int16_t xHeight = 0;
	std::int16_t capHeight = 0;
	bool hasStyle = false;
	FontMetrics decorationMetrics;
	bool hasDecorationMetrics = false;
	std::uint16_t weight = 400;
	bool bold = false;
	bool italic = false;
	std::int16_t italicAngle = 0;
	std::uint16_t horizontalMetricCount = 0;
	std::uint16_t characterMapFormat = 4;
	bool glyphIdArray = false;
	bool longLocations = false;
};

}
