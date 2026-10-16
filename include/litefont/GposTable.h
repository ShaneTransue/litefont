#pragma once

#include <litefont/ByteReader.h>

#include <vector>

namespace litefont {

/**
 * \struct GposPairSubtable GposTable.h <litefont/GposTable.h>
 * @addtogroup Text
 *
 * GposPairSubtable - One pair adjustment subtable of the kerning feature:
 * where it starts in the GPOS table (extension lookups already followed)
 * and which lookup it belongs to.
 */
struct GposPairSubtable {
	std::uint32_t offset = 0;
	std::uint16_t lookup = 0;
};

/**
 * \class GposTable GposTable.h <litefont/GposTable.h>
 * @addtogroup Text
 *
 * GposTable - OpenType pair kerning from the GPOS table, which modern fonts
 * use instead of (or as well as) the old kern table; every .otf kerns this
 * way. read() finds the lookups of the 'kern' feature, taken from the default
 * script, else Latin, else every kern feature, and their pair adjustment
 * subtables (lookup type 2, also inside type 9 extensions). getKerning()
 * applies them the way HarfBuzz does: within a lookup the first subtable that
 * covers the pair applies (an explicit pair list that lacks the second glyph
 * passes on to the next subtable, a class table always applies), and the
 * lookups add up. Only the first glyph's x advance is used, which is what
 * kerning pairs set.
 *
 * It keeps only offsets: every call takes the table's bytes.
 */
class GposTable {
public:
	bool read(const ByteReader& table);
	bool hasKerning() const;
	std::int16_t getKerning(const ByteReader& table, std::uint16_t left, std::uint16_t right) const;

private:
	static bool applyPair(const ByteReader& table, std::uint32_t subtable, std::uint16_t left, std::uint16_t right, std::int16_t& xAdvance);
	static bool findCoverage(const ByteReader& table, std::uint32_t coverage, std::uint16_t glyph, std::uint16_t& index);
	static std::uint16_t getClass(const ByteReader& table, std::uint32_t classDefinition, std::uint16_t glyph);
	static bool readValue(ByteReader& record, std::uint16_t valueFormat, std::int16_t& xAdvance);
	static std::size_t getValueSize(std::uint16_t valueFormat);

	std::vector<GposPairSubtable> subtables;
};

}
