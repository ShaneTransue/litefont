#pragma once

#include <litefont/ByteReader.h>
#include <litefont/Outline.h>

#include <array>
#include <vector>

namespace litefont {

/**
 * \struct CffIndex CffTable.h <litefont/CffTable.h>
 * @addtogroup Text
 *
 * CffIndex - Where one CFF INDEX sits in the table: how many entries it
 * has, the size of its offsets, where the offsets start and where byte 0 of
 * its data is (offsets count from 1).
 */
struct CffIndex {
	std::uint32_t count = 0;
	std::uint8_t offsetSize = 0;
	std::uint32_t offsetsStart = 0;
	std::uint32_t dataStart = 0;
	std::uint32_t end = 0;
};

/**
 * \struct CharStringState CffTable.h <litefont/CffTable.h>
 * @addtogroup Text
 *
 * CharStringState - A Type 2 charstring interpreter's state while it draws
 * one glyph: the argument stack, the current point, whether a contour is
 * open, the hints counted so far and the hint mask size they fix, and
 * whether the advance width argument and endchar have been seen.
 */
struct CharStringState {
	std::array<float, 48> stack{};
	std::uint32_t count = 0;
	Vector2f point;
	std::uint32_t hints = 0;
	std::uint32_t hintMaskBytes = 0;
	bool open = false;
	bool widthSeen = false;
	bool ended = false;
};

/**
 * \class CffTable CffTable.h <litefont/CffTable.h>
 * @addtogroup Text
 *
 * CffTable - The PostScript outlines of an OpenType (.otf) font: the CFF
 * table's indexes and dictionaries, located once, and a Type 2 charstring
 * interpreter that draws a glyph into an Outline of lines and cubic curves
 * in font units. Name-keyed and CID-keyed fonts are read (CID fonts pick each
 * glyph's local subroutines through FDSelect). The interpreter follows
 * fontTools: the first stack-clearing operator may carry the advance width,
 * hints are counted until the first hint mask fixes its size, every moveto
 * and endchar closes the open contour, and operators it does not draw with
 * are ignored. The deprecated seac form of endchar, the arithmetic and
 * storage operators and CFF2 fail the glyph.
 *
 * It keeps only offsets: every call takes the table's bytes, so the Font
 * that owns them can move freely.
 */
class CffTable {
public:
	bool read(const ByteReader& table, std::uint16_t glyphCount);
	bool getOutline(const ByteReader& table, std::uint16_t glyph, Outline& outline) const;
	bool isLoaded() const;
	bool isCidKeyed() const;

private:
	bool execute(const ByteReader& table, ByteReader code, const CffIndex& localSubrs, std::uint32_t depth, CharStringState& state, Outline& outline) const;
	std::uint8_t getFontDict(const ByteReader& table, std::uint16_t glyph) const;

	static bool readIndex(const ByteReader& table, std::uint32_t offset, CffIndex& index);
	static bool getEntry(const ByteReader& table, const CffIndex& index, std::uint32_t item, ByteReader& entry);
	static bool findDictValues(ByteReader dict, std::uint16_t key, std::array<double, 4>& values, std::uint32_t& count);
	static bool readLocalSubrs(const ByteReader& table, ByteReader fontDict, CffIndex& subrs);
	static std::int32_t getBias(const CffIndex& subrs);

	CffIndex charStrings;
	CffIndex globalSubrs;
	std::vector<CffIndex> localSubrs;
	std::uint32_t fontDictSelect = 0;
	std::uint16_t glyphCount = 0;
	bool cidKeyed = false;
	bool loaded = false;
};

}
