#include "TestFontBuilder.h"

#include <litefont/Utf8.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace litefont::tests {

TestFontBuilder::TestFontBuilder() {
	this->glyphs.push_back(TestGlyph{});    // glyph 0: an empty .notdef
}

TestFontBuilder& TestFontBuilder::setLineMetrics(std::uint16_t unitsPerEm, std::int16_t ascender, std::int16_t descender, std::int16_t lineGap) {
	this->unitsPerEm = unitsPerEm;
	this->ascender = ascender;
	this->descender = descender;
	this->lineGap = lineGap;
	return *this;
}

TestFontBuilder& TestFontBuilder::setOs2(std::int16_t typoAscender, std::int16_t typoDescender, std::int16_t typoLineGap, bool useTypoMetrics, std::int16_t xHeight, std::int16_t capHeight) {
	this->hasOs2 = true;
	this->typoAscender = typoAscender;
	this->typoDescender = typoDescender;
	this->typoLineGap = typoLineGap;
	this->useTypoMetrics = useTypoMetrics;
	this->xHeight = xHeight;
	this->capHeight = capHeight;
	return *this;
}

TestFontBuilder& TestFontBuilder::setHorizontalMetricCount(std::uint16_t count) {
	this->horizontalMetricCount = count;
	return *this;
}

TestFontBuilder& TestFontBuilder::setCharacterMapFormat(std::uint16_t format) {
	this->characterMapFormat = format;
	return *this;
}

TestFontBuilder& TestFontBuilder::setGlyphIdArray(bool on) {
	this->glyphIdArray = on;
	return *this;
}

TestFontBuilder& TestFontBuilder::setLongLocations(bool on) {
	this->longLocations = on;
	return *this;
}

TestFontBuilder& TestFontBuilder::omitTable(const std::string& tag) {
	this->omittedTables.push_back(tag);
	return *this;
}

std::uint16_t TestFontBuilder::addGlyph(const TestGlyph& glyph) {
	this->glyphs.push_back(glyph);
	return static_cast<std::uint16_t>(this->glyphs.size() - 1);
}

TestFontBuilder& TestFontBuilder::map(char32_t codePoint, std::uint16_t glyph) {
	this->characterMap[codePoint] = glyph;
	return *this;
}

TestFontBuilder& TestFontBuilder::addKerning(std::uint16_t left, std::uint16_t right, std::int16_t value) {
	this->kerning.push_back(TestKerningPair{left, right, value});
	return *this;
}

TestFontBuilder& TestFontBuilder::setStyle(std::uint16_t weight, bool bold, bool italic, std::int16_t italicAngle) {
	this->hasOs2 = true;
	this->hasStyle = true;
	this->weight = weight;
	this->bold = bold;
	this->italic = italic;
	this->italicAngle = italicAngle;
	return *this;
}

TestFontBuilder& TestFontBuilder::setDecorationMetrics(const FontMetrics& metrics) {
	// Only the script, strikeout and underline values are written.
	this->hasOs2 = true;
	this->hasDecorationMetrics = true;
	this->decorationMetrics = metrics;
	return *this;
}

Bytes TestFontBuilder::build() const {
	std::vector<std::pair<std::string, Bytes>> tables;
	auto add = [&](const std::string& tag, Bytes bytes) {
		if (std::find(this->omittedTables.begin(), this->omittedTables.end(), tag) == this->omittedTables.end()) {
			tables.emplace_back(tag, std::move(bytes));
		}
	};

	Bytes glyf;
	Bytes loca;
	this->buildGlyphs(glyf, loca);
	add("head", this->buildHead());
	add("maxp", this->buildMaxp());
	add("hhea", this->buildHhea());
	add("hmtx", this->buildHmtx());
	if (this->hasOs2) {
		add("OS/2", this->buildOs2());
	}
	add("cmap", this->buildCmap());
	if (this->hasCff) {
		add("CFF ", this->buildCffTable());
	}
	else {
		add("loca", std::move(loca));
		add("glyf", std::move(glyf));
	}
	if (!this->kerning.empty()) {
		add("kern", this->buildKern());
	}
	if (!this->gposLookups.empty()) {
		add("GPOS", this->buildGpos());
	}
	if (!this->names.empty()) {
		add("name", this->buildName());
	}
	if (this->hasStyle || this->hasDecorationMetrics) {
		add("post", this->buildPost());
	}
	std::sort(tables.begin(), tables.end(), [](const auto& left, const auto& right) {
		return left.first < right.first;
	});

	const std::uint16_t tableCount = static_cast<std::uint16_t>(tables.size());
	std::uint16_t searchRange = 16;
	std::uint16_t entrySelector = 0;
	while (static_cast<std::uint32_t>(searchRange) * 2 <= static_cast<std::uint32_t>(tableCount) * 16) {
		searchRange = static_cast<std::uint16_t>(searchRange * 2);
		entrySelector++;
	}

	ByteWriter writer;
	writer.write(this->hasCff ? TestFontBuilder::getTagValue("OTTO") : std::uint32_t{0x00010000u});
	writer.write(tableCount);
	writer.write(searchRange);
	writer.write(entrySelector);
	writer.write(static_cast<std::uint16_t>(tableCount * 16 - searchRange));

	std::uint32_t offset = 12 + 16 * static_cast<std::uint32_t>(tableCount);
	for (const auto& [tag, bytes] : tables) {
		writer.write(TestFontBuilder::getTagValue(tag));
		writer.write(std::uint32_t{0});            // checksum, not checked by the reader
		writer.write(offset);
		writer.write(static_cast<std::uint32_t>(bytes.data.size()));
		offset += static_cast<std::uint32_t>((bytes.data.size() + 3) / 4 * 4);
	}
	for (const auto& [tag, bytes] : tables) {
		writer.writeBuffer(bytes.data);
		writer.align(4);
	}
	return writer.takeBytes();
}

TestCharString& TestCharString::number(std::int32_t value) {
	if (value >= -107 && value <= 107) {
		this->bytes.push_back(static_cast<std::uint8_t>(value + 139));
	}
	else if (value >= 108 && value <= 1131) {
		const std::int32_t rest = value - 108;
		this->bytes.push_back(static_cast<std::uint8_t>(247 + rest / 256));
		this->bytes.push_back(static_cast<std::uint8_t>(rest % 256));
	}
	else if (value >= -1131 && value <= -108) {
		const std::int32_t rest = -value - 108;
		this->bytes.push_back(static_cast<std::uint8_t>(251 + rest / 256));
		this->bytes.push_back(static_cast<std::uint8_t>(rest % 256));
	}
	else {
		const std::uint16_t bits = static_cast<std::uint16_t>(static_cast<std::int16_t>(value));
		this->bytes.push_back(28);
		this->bytes.push_back(static_cast<std::uint8_t>(bits >> 8));
		this->bytes.push_back(static_cast<std::uint8_t>(bits));
	}
	return *this;
}

TestCharString& TestCharString::fixed(float value) {
	const std::uint32_t bits = static_cast<std::uint32_t>(static_cast<std::int32_t>(std::lround(value * 65536.0f)));
	this->bytes.push_back(255);
	for (std::int32_t shift = 24; shift >= 0; shift -= 8) {
		this->bytes.push_back(static_cast<std::uint8_t>(bits >> shift));
	}
	return *this;
}

TestCharString& TestCharString::op(std::uint8_t code) {
	this->bytes.push_back(code);
	return *this;
}

TestCharString& TestCharString::escape(std::uint8_t code) {
	this->bytes.push_back(12);
	this->bytes.push_back(code);
	return *this;
}

TestCharString& TestCharString::raw(std::uint8_t byte) {
	this->bytes.push_back(byte);
	return *this;
}

std::vector<std::uint8_t> TestCharString::take() {
	return std::move(this->bytes);
}

TestFontBuilder& TestFontBuilder::addGposLookup(const TestGposLookup& lookup) {
	this->gposLookups.push_back(lookup);
	return *this;
}

TestFontBuilder& TestFontBuilder::setNames(const std::string& family, const std::string& subfamily, const std::string& fullName) {
	this->addName(3, 1, 0x0409, 1, family);
	this->addName(3, 1, 0x0409, 2, subfamily);
	this->addName(3, 1, 0x0409, 4, fullName);
	return *this;
}

TestFontBuilder& TestFontBuilder::addName(std::uint16_t platform, std::uint16_t encoding, std::uint16_t language, std::uint16_t nameId, const std::string& text) {
	// Macintosh names keep their bytes; the others are UTF-16 big-endian.
	std::vector<std::uint8_t> bytes;
	if (platform == 1) {
		bytes.assign(text.begin(), text.end());
	}
	else {
		Utf8 walker(text);
		CodePoint codePoint;
		auto unit = [&bytes](std::uint32_t value) {
			bytes.push_back(static_cast<std::uint8_t>(value >> 8));
			bytes.push_back(static_cast<std::uint8_t>(value));
		};
		while (walker.next(codePoint)) {
			if (codePoint.value >= 0x10000) {
				const std::uint32_t rest = static_cast<std::uint32_t>(codePoint.value) - 0x10000;
				unit(0xD800 + (rest >> 10));
				unit(0xDC00 + (rest & 0x3FF));
			}
			else {
				unit(static_cast<std::uint32_t>(codePoint.value));
			}
		}
	}
	return this->addRawName(platform, encoding, language, nameId, bytes);
}

TestFontBuilder& TestFontBuilder::addRawName(std::uint16_t platform, std::uint16_t encoding, std::uint16_t language, std::uint16_t nameId, const std::vector<std::uint8_t>& bytes) {
	this->names.push_back(TestName{platform, encoding, language, nameId, bytes});
	return *this;
}

TestFontBuilder& TestFontBuilder::setCff(const TestCff& cff) {
	this->cff = cff;
	this->hasCff = true;
	if (this->glyphs.size() < cff.charStrings.size()) {
		this->glyphs.resize(cff.charStrings.size());
	}
	return *this;
}

Bytes TestFontBuilder::buildCollection(const std::vector<Bytes>& fonts) {
	// Each font moves to its place in the file, so its table offsets move with it.
	ByteWriter writer;
	writer.write(TestFontBuilder::getTagValue("ttcf"));
	writer.write(std::uint32_t{0x00010000u});
	writer.write(static_cast<std::uint32_t>(fonts.size()));
	std::uint32_t offset = 12 + 4 * static_cast<std::uint32_t>(fonts.size());
	std::vector<std::uint32_t> starts;
	for (const Bytes& font : fonts) {
		starts.push_back(offset);
		writer.write(offset);
		offset += static_cast<std::uint32_t>((font.data.size() + 3) / 4 * 4);
	}
	for (std::size_t i = 0; i < fonts.size(); i++) {
		const std::size_t start = writer.getSize();
		writer.writeBuffer(fonts[i].data);
		writer.align(4);
		ByteReader directory(fonts[i].data);
		std::uint16_t tableCount = 0;
		directory.seek(4);
		directory.read(tableCount);
		for (std::uint16_t table = 0; table < tableCount; table++) {
			const std::size_t position = 12 + static_cast<std::size_t>(table) * 16 + 8;
			ByteReader entry(fonts[i].data);
			std::uint32_t tableOffset = 0;
			entry.seek(position);
			entry.read(tableOffset);
			writer.patch(start + position, tableOffset + starts[i]);
		}
	}
	return writer.takeBytes();
}

Bytes TestFontBuilder::buildName() const {
	ByteWriter writer;
	writer.write(std::uint16_t{0});
	writer.write(static_cast<std::uint16_t>(this->names.size()));
	writer.write(static_cast<std::uint16_t>(6 + 12 * this->names.size()));
	std::uint16_t offset = 0;
	for (const TestName& name : this->names) {
		writer.write(name.platform);
		writer.write(name.encoding);
		writer.write(name.language);
		writer.write(name.nameId);
		writer.write(static_cast<std::uint16_t>(name.bytes.size()));
		writer.write(offset);
		offset = static_cast<std::uint16_t>(offset + name.bytes.size());
	}
	for (const TestName& name : this->names) {
		writer.writeBuffer(name.bytes);
	}
	return writer.takeBytes();
}

Bytes TestFontBuilder::buildGpos() const {
	// Scripts in the order lookups first name them; each lookup has its own feature.
	std::vector<std::string> scripts;
	for (const TestGposLookup& lookup : this->gposLookups) {
		if (std::find(scripts.begin(), scripts.end(), lookup.script) == scripts.end()) {
			scripts.push_back(lookup.script);
		}
	}
	const std::uint16_t lookupCount = static_cast<std::uint16_t>(this->gposLookups.size());

	ByteWriter scriptList;
	scriptList.write(static_cast<std::uint16_t>(scripts.size()));
	std::vector<std::size_t> scriptOffsets;
	for (const std::string& script : scripts) {
		scriptList.write(TestFontBuilder::getTagValue(script));
		scriptOffsets.push_back(scriptList.getSize());
		scriptList.write(std::uint16_t{0});
	}
	for (std::size_t i = 0; i < scripts.size(); i++) {
		scriptList.patch(scriptOffsets[i], static_cast<std::uint16_t>(scriptList.getSize()));
		std::vector<std::uint16_t> features;
		for (std::uint16_t k = 0; k < lookupCount; k++) {
			if (this->gposLookups[k].script == scripts[i]) {
				features.push_back(k);
			}
		}
		scriptList.write(std::uint16_t{4});        // default language right after this header
		scriptList.write(std::uint16_t{0});        // no other languages
		scriptList.write(std::uint16_t{0});        // lookup order
		scriptList.write(std::uint16_t{0xFFFF});   // no required feature
		scriptList.write(static_cast<std::uint16_t>(features.size()));
		for (const std::uint16_t feature : features) {
			scriptList.write(feature);
		}
	}

	ByteWriter featureList;
	featureList.write(lookupCount);
	for (std::uint16_t k = 0; k < lookupCount; k++) {
		featureList.write(TestFontBuilder::getTagValue(this->gposLookups[k].feature));
		featureList.write(static_cast<std::uint16_t>(2 + 6 * lookupCount + 6 * k));
	}
	for (std::uint16_t k = 0; k < lookupCount; k++) {
		featureList.write(std::uint16_t{0});       // no parameters
		featureList.write(std::uint16_t{1});
		featureList.write(k);
	}

	ByteWriter lookupList;
	lookupList.write(lookupCount);
	for (std::uint16_t k = 0; k < lookupCount; k++) {
		lookupList.write(std::uint16_t{0});
	}
	for (std::uint16_t k = 0; k < lookupCount; k++) {
		const TestGposLookup& lookup = this->gposLookups[k];
		const std::size_t lookupStart = lookupList.getSize();
		lookupList.patch(2 + static_cast<std::size_t>(k) * 2, static_cast<std::uint16_t>(lookupStart));
		const std::uint16_t subtableCount = static_cast<std::uint16_t>(lookup.subtables.size());
		lookupList.write(static_cast<std::uint16_t>(lookup.extension ? 9 : 2));
		lookupList.write(std::uint16_t{0});
		lookupList.write(subtableCount);
		const std::size_t offsetsStart = lookupList.getSize();
		for (std::uint16_t i = 0; i < subtableCount; i++) {
			lookupList.write(std::uint16_t{0});
		}
		// Extensions: one 8-byte record per subtable, each pointing past them all.
		std::vector<std::size_t> extensionStarts;
		if (lookup.extension) {
			for (std::uint16_t i = 0; i < subtableCount; i++) {
				lookupList.patch(offsetsStart + static_cast<std::size_t>(i) * 2, static_cast<std::uint16_t>(lookupList.getSize() - lookupStart));
				extensionStarts.push_back(lookupList.getSize());
				lookupList.write(std::uint16_t{1});
				lookupList.write(std::uint16_t{2});
				lookupList.write(std::uint32_t{0});
			}
		}
		for (std::uint16_t i = 0; i < subtableCount; i++) {
			if (lookup.extension) {
				lookupList.patch(extensionStarts[i] + 4, static_cast<std::uint32_t>(lookupList.getSize() - extensionStarts[i]));
			}
			else {
				lookupList.patch(offsetsStart + static_cast<std::size_t>(i) * 2, static_cast<std::uint16_t>(lookupList.getSize() - lookupStart));
			}
			lookupList.writeBuffer(TestFontBuilder::buildPairSubtable(lookup.subtables[i]).data);
			lookupList.align(2);
		}
	}

	const std::uint16_t scriptStart = 10;
	const std::uint16_t featureStart = static_cast<std::uint16_t>(scriptStart + scriptList.getSize());
	const std::uint16_t lookupStart = static_cast<std::uint16_t>(featureStart + featureList.getSize());
	ByteWriter writer;
	writer.write(std::uint16_t{1});
	writer.write(std::uint16_t{0});
	writer.write(scriptStart);
	writer.write(featureStart);
	writer.write(lookupStart);
	writer.writeBuffer(scriptList.getBytes().data);
	writer.writeBuffer(featureList.getBytes().data);
	writer.writeBuffer(lookupList.getBytes().data);
	return writer.takeBytes();
}

Bytes TestFontBuilder::buildPairSubtable(const TestGposSubtable& subtable) {
	ByteWriter writer;
	writer.write(subtable.format);
	writer.write(std::uint16_t{0});                // coverage, patched below
	writer.write(subtable.valueFormat1);
	writer.write(subtable.valueFormat2);

	if (subtable.format == 1) {
		// A pair set per first glyph, both sorted, and a format 1 coverage.
		std::map<std::uint16_t, std::map<std::uint16_t, std::int16_t>> sets;
		for (const TestKerningPair& pair : subtable.pairs) {
			sets[pair.left][pair.right] = pair.value;
		}
		writer.write(static_cast<std::uint16_t>(sets.size()));
		const std::size_t setOffsets = writer.getSize();
		for (std::size_t i = 0; i < sets.size(); i++) {
			writer.write(std::uint16_t{0});
		}
		std::size_t i = 0;
		for (const auto& [left, seconds] : sets) {
			writer.patch(setOffsets + i * 2, static_cast<std::uint16_t>(writer.getSize()));
			writer.write(static_cast<std::uint16_t>(seconds.size()));
			for (const auto& [right, value] : seconds) {
				writer.write(right);
				TestFontBuilder::writeValue(writer, subtable.valueFormat1, value);
				TestFontBuilder::writeValue(writer, subtable.valueFormat2, 0);
			}
			i++;
		}
		writer.patch(2, static_cast<std::uint16_t>(writer.getSize()));
		writer.write(std::uint16_t{1});
		writer.write(static_cast<std::uint16_t>(sets.size()));
		for (const auto& [left, seconds] : sets) {
			writer.write(left);
		}
		return writer.takeBytes();
	}

	// Format 2: class records, a range coverage, ranges for the first
	// classes and a class array for the second ones.
	const std::uint16_t class1Count = static_cast<std::uint16_t>(subtable.classValues.size());
	const std::uint16_t class2Count = static_cast<std::uint16_t>(subtable.classValues.empty() ? 0 : subtable.classValues[0].size());
	const std::size_t definitions = writer.getSize();
	writer.write(std::uint16_t{0});
	writer.write(std::uint16_t{0});
	writer.write(class1Count);
	writer.write(class2Count);
	for (const std::vector<std::int16_t>& row : subtable.classValues) {
		for (const std::int16_t value : row) {
			TestFontBuilder::writeValue(writer, subtable.valueFormat1, value);
			TestFontBuilder::writeValue(writer, subtable.valueFormat2, 0);
		}
	}

	std::vector<std::uint16_t> covered = subtable.coverage;
	std::sort(covered.begin(), covered.end());
	writer.patch(2, static_cast<std::uint16_t>(writer.getSize()));
	std::vector<std::array<std::uint16_t, 3>> ranges;
	for (const std::uint16_t glyph : covered) {
		if (!ranges.empty() && glyph == ranges.back()[1] + 1) {
			ranges.back()[1] = glyph;
		}
		else {
			ranges.push_back({glyph, glyph, 0});
		}
	}
	writer.write(std::uint16_t{2});
	writer.write(static_cast<std::uint16_t>(ranges.size()));
	std::uint16_t coverageIndex = 0;
	for (const std::array<std::uint16_t, 3>& range : ranges) {
		writer.write(range[0]);
		writer.write(range[1]);
		writer.write(coverageIndex);
		coverageIndex = static_cast<std::uint16_t>(coverageIndex + range[1] - range[0] + 1);
	}

	writer.patch(definitions, static_cast<std::uint16_t>(writer.getSize()));
	std::vector<std::array<std::uint16_t, 3>> classRanges;
	for (const auto& [glyph, glyphClass] : subtable.firstClasses) {
		if (!classRanges.empty() && glyph == classRanges.back()[1] + 1 && glyphClass == classRanges.back()[2]) {
			classRanges.back()[1] = glyph;
		}
		else {
			classRanges.push_back({glyph, glyph, glyphClass});
		}
	}
	writer.write(std::uint16_t{2});
	writer.write(static_cast<std::uint16_t>(classRanges.size()));
	for (const std::array<std::uint16_t, 3>& range : classRanges) {
		writer.write(range[0]);
		writer.write(range[1]);
		writer.write(range[2]);
	}

	writer.patch(definitions + 2, static_cast<std::uint16_t>(writer.getSize()));
	const std::uint16_t first = subtable.secondClasses.empty() ? std::uint16_t{0} : subtable.secondClasses.begin()->first;
	const std::uint16_t last = subtable.secondClasses.empty() ? std::uint16_t{0} : subtable.secondClasses.rbegin()->first;
	writer.write(std::uint16_t{1});
	writer.write(first);
	writer.write(static_cast<std::uint16_t>(subtable.secondClasses.empty() ? 0 : last - first + 1));
	for (std::uint32_t glyph = first; !subtable.secondClasses.empty() && glyph <= last; glyph++) {
		const auto found = subtable.secondClasses.find(static_cast<std::uint16_t>(glyph));
		writer.write(found == subtable.secondClasses.end() ? std::uint16_t{0} : found->second);
	}
	return writer.takeBytes();
}

void TestFontBuilder::writeValue(ByteWriter& writer, std::uint16_t valueFormat, std::int16_t xAdvance) {
	// Fields in bit order: placements and advances get filler except the x
	// advance; device table offsets are 0.
	for (std::uint16_t bit = 0; bit < 8; bit++) {
		if ((valueFormat & (1u << bit)) == 0) {
			continue;
		}
		if (bit == 2) {
			writer.write(xAdvance);
		}
		else if (bit < 4) {
			writer.write(std::int16_t{77});
		}
		else {
			writer.write(std::uint16_t{0});
		}
	}
}

Bytes TestFontBuilder::buildCffTable() const {
	// Header, name, top DICT, strings, global subroutines, charstrings, the
	// private DICT and the local subroutines. Offsets in the top DICT are
	// 5-byte integers, so its size is known before the layout is.
	const std::vector<std::vector<std::uint8_t>> nameEntries{{'T', 'e', 's', 't'}};
	const bool hasLocalSubrs = !this->cff.localSubrs.empty();
	const std::size_t topDictSize = 17;
	const std::size_t privateSize = hasLocalSubrs ? 6 : 0;
	const std::size_t charStringsOffset = 4 + TestFontBuilder::getIndexSize(nameEntries)
		+ TestFontBuilder::getIndexSize({std::vector<std::uint8_t>(topDictSize, 0)}) + 2
		+ TestFontBuilder::getIndexSize(this->cff.globalSubrs);
	const std::size_t privateOffset = charStringsOffset + TestFontBuilder::getIndexSize(this->cff.charStrings);

	auto integer = [](std::vector<std::uint8_t>& dict, std::int32_t value) {
		dict.push_back(29);
		for (std::int32_t shift = 24; shift >= 0; shift -= 8) {
			dict.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> shift));
		}
	};
	std::vector<std::uint8_t> topDict;
	integer(topDict, static_cast<std::int32_t>(charStringsOffset));
	topDict.push_back(17);
	integer(topDict, static_cast<std::int32_t>(privateSize));
	integer(topDict, static_cast<std::int32_t>(privateOffset));
	topDict.push_back(18);

	ByteWriter writer;
	writer.write(std::uint8_t{1});
	writer.write(std::uint8_t{0});
	writer.write(std::uint8_t{4});
	writer.write(std::uint8_t{4});
	TestFontBuilder::writeIndex(writer, nameEntries);
	TestFontBuilder::writeIndex(writer, {topDict});
	writer.write(std::uint16_t{0});                // no strings
	TestFontBuilder::writeIndex(writer, this->cff.globalSubrs);
	TestFontBuilder::writeIndex(writer, this->cff.charStrings);
	if (hasLocalSubrs) {
		std::vector<std::uint8_t> privateDict;
		integer(privateDict, static_cast<std::int32_t>(privateSize));    // subroutines right after the private DICT
		privateDict.push_back(19);
		writer.writeBuffer(privateDict);
		TestFontBuilder::writeIndex(writer, this->cff.localSubrs);
	}
	return writer.takeBytes();
}

void TestFontBuilder::writeIndex(ByteWriter& writer, const std::vector<std::vector<std::uint8_t>>& entries) {
	writer.write(static_cast<std::uint16_t>(entries.size()));
	if (entries.empty()) {
		return;
	}
	std::uint32_t end = 1;
	for (const std::vector<std::uint8_t>& entry : entries) {
		end += static_cast<std::uint32_t>(entry.size());
	}
	const std::uint8_t offsetSize = end < 0x100 ? 1 : (end < 0x10000 ? 2 : (end < 0x1000000 ? 3 : 4));
	writer.write(offsetSize);
	std::uint32_t offset = 1;
	auto writeOffset = [&writer, offsetSize](std::uint32_t value) {
		for (std::int32_t shift = (offsetSize - 1) * 8; shift >= 0; shift -= 8) {
			writer.write(static_cast<std::uint8_t>(value >> shift));
		}
	};
	writeOffset(offset);
	for (const std::vector<std::uint8_t>& entry : entries) {
		offset += static_cast<std::uint32_t>(entry.size());
		writeOffset(offset);
	}
	for (const std::vector<std::uint8_t>& entry : entries) {
		writer.writeBuffer(entry);
	}
}

std::size_t TestFontBuilder::getIndexSize(const std::vector<std::vector<std::uint8_t>>& entries) {
	ByteWriter writer;
	TestFontBuilder::writeIndex(writer, entries);
	return writer.getSize();
}

std::uint32_t TestFontBuilder::getTagValue(const std::string& tag) {
	std::uint32_t value = 0;
	for (std::size_t i = 0; i < 4; i++) {
		value = (value << 8) | static_cast<std::uint8_t>(i < tag.size() ? tag[i] : ' ');
	}
	return value;
}

Bytes TestFontBuilder::buildHead() const {
	ByteWriter writer;
	writer.write(std::uint32_t{0x00010000u});      // version
	writer.write(std::uint32_t{0x00010000u});      // font revision
	writer.write(std::uint32_t{0});                // checksum adjustment
	writer.write(std::uint32_t{0x5F0F3CF5u});      // magic number
	writer.write(std::uint16_t{0x000B});           // flags
	writer.write(this->unitsPerEm);
	writer.write(std::int64_t{0});                 // created
	writer.write(std::int64_t{0});                 // modified
	writer.write(std::int16_t{0});                 // xMin, yMin, xMax, yMax
	writer.write(std::int16_t{0});
	writer.write(std::int16_t{0});
	writer.write(std::int16_t{0});
	writer.write(static_cast<std::uint16_t>((this->bold ? 1 : 0) | (this->italic ? 2 : 0)));   // mac style
	writer.write(std::uint16_t{8});                // lowest readable size
	writer.write(std::int16_t{2});                 // font direction hint
	writer.write(static_cast<std::int16_t>(this->longLocations ? 1 : 0));
	writer.write(std::int16_t{0});                 // glyph data format
	return writer.takeBytes();
}

Bytes TestFontBuilder::buildMaxp() const {
	ByteWriter writer;
	writer.write(std::uint32_t{0x00005000u});
	writer.write(static_cast<std::uint16_t>(this->glyphs.size()));
	return writer.takeBytes();
}

Bytes TestFontBuilder::buildHhea() const {
	std::uint16_t widest = 0;
	for (const TestGlyph& glyph : this->glyphs) {
		widest = std::max(widest, glyph.advance);
	}
	ByteWriter writer;
	writer.write(std::uint32_t{0x00010000u});
	writer.write(this->ascender);
	writer.write(this->descender);
	writer.write(this->lineGap);
	writer.write(widest);
	for (std::size_t i = 0; i < 3; i++) {
		writer.write(std::int16_t{0});             // min left and right bearings, max extent
	}
	writer.write(std::int16_t{1});                 // caret slope rise
	writer.write(std::int16_t{0});                 // caret slope run
	writer.write(std::int16_t{0});                 // caret offset
	for (std::size_t i = 0; i < 4; i++) {
		writer.write(std::int16_t{0});             // reserved
	}
	writer.write(std::int16_t{0});                 // metric data format
	writer.write(this->getHorizontalMetricCount());
	return writer.takeBytes();
}

Bytes TestFontBuilder::buildHmtx() const {
	const std::uint16_t fullCount = this->getHorizontalMetricCount();
	ByteWriter writer;
	for (std::size_t i = 0; i < this->glyphs.size(); i++) {
		if (i < fullCount) {
			writer.write(this->glyphs[i].advance);
		}
		writer.write(this->glyphs[i].leftSideBearing);
	}
	return writer.takeBytes();
}

Bytes TestFontBuilder::buildOs2() const {
	ByteWriter writer;
	writer.write(std::uint16_t{4});                // version
	writer.write(std::uint16_t{0});                // average width
	writer.write(this->weight);
	// Width class and fsType, then the script and strikeout values, then
	// zeros up to fsSelection.
	writer.write(std::uint16_t{0});
	writer.write(std::uint16_t{0});
	const FontMetrics& lines = this->decorationMetrics;
	for (const std::int16_t value : {lines.subscriptSize, lines.subscriptSize, std::int16_t{0}, lines.subscriptOffset,
			lines.superscriptSize, lines.superscriptSize, std::int16_t{0}, lines.superscriptOffset, lines.strikeoutSize, lines.strikeoutPosition}) {
		writer.write(value);
	}
	writer.writeBuffer(std::vector<std::uint8_t>(32, 0));
	std::uint16_t selection = this->useTypoMetrics ? 0x0080 : 0x0000;
	selection |= this->bold ? 0x0020 : 0x0000;
	selection |= this->italic ? 0x0001 : 0x0000;
	selection |= !this->bold && !this->italic ? 0x0040 : 0x0000;
	writer.write(selection);                // fsSelection
	writer.writeBuffer(std::vector<std::uint8_t>(4, 0));
	writer.write(this->typoAscender);
	writer.write(this->typoDescender);
	writer.write(this->typoLineGap);
	writer.writeBuffer(std::vector<std::uint8_t>(12, 0));
	writer.write(this->xHeight);
	writer.write(this->capHeight);
	writer.writeBuffer(std::vector<std::uint8_t>(6, 0));
	return writer.takeBytes();
}

Bytes TestFontBuilder::buildPost() const {
	ByteWriter writer;
	writer.write(std::uint32_t{0x00030000u});      // version 3: no glyph names
	writer.write(static_cast<std::int32_t>(std::int32_t{this->italicAngle} * 65536));
	writer.write(this->decorationMetrics.underlinePosition);
	writer.write(this->decorationMetrics.underlineThickness);
	writer.writeBuffer(std::vector<std::uint8_t>(20, 0));
	return writer.takeBytes();
}

Bytes TestFontBuilder::buildCmap() const {
	ByteWriter writer;
	writer.write(std::uint16_t{0});                // version
	writer.write(std::uint16_t{1});                // one subtable
	writer.write(std::uint16_t{3});                // Windows platform
	writer.write(static_cast<std::uint16_t>(this->characterMapFormat == 12 ? 10 : 1));
	writer.write(std::uint32_t{12});

	if (this->characterMapFormat == 12) {
		std::vector<std::uint32_t> starts;
		std::vector<std::uint32_t> ends;
		std::vector<std::uint32_t> firstGlyphs;
		for (const auto& [codePoint, glyph] : this->characterMap) {
			const bool extends = !starts.empty() && codePoint == ends.back() + 1
				&& glyph == firstGlyphs.back() + (ends.back() - starts.back()) + 1;
			if (extends) {
				ends.back() = codePoint;
			}
			else {
				starts.push_back(codePoint);
				ends.push_back(codePoint);
				firstGlyphs.push_back(glyph);
			}
		}
		writer.write(std::uint16_t{12});
		writer.write(std::uint16_t{0});
		writer.write(static_cast<std::uint32_t>(16 + 12 * starts.size()));
		writer.write(std::uint32_t{0});            // language
		writer.write(static_cast<std::uint32_t>(starts.size()));
		for (std::size_t i = 0; i < starts.size(); i++) {
			writer.write(starts[i]);
			writer.write(ends[i]);
			writer.write(firstGlyphs[i]);
		}
		return writer.takeBytes();
	}

	// Format 4: segments of consecutive code points. With deltas, a segment
	// also needs consecutive glyphs; with a glyph id array it does not.
	std::vector<std::uint16_t> starts;
	std::vector<std::uint16_t> ends;
	std::vector<std::vector<std::uint16_t>> segmentGlyphs;
	for (const auto& [codePoint, glyph] : this->characterMap) {
		if (codePoint >= 0xFFFF) {
			continue;
		}
		const bool consecutive = !starts.empty() && codePoint == static_cast<char32_t>(ends.back()) + 1;
		const bool extends = consecutive && (this->glyphIdArray || glyph == segmentGlyphs.back().back() + 1);
		if (extends) {
			ends.back() = static_cast<std::uint16_t>(codePoint);
			segmentGlyphs.back().push_back(glyph);
		}
		else {
			starts.push_back(static_cast<std::uint16_t>(codePoint));
			ends.push_back(static_cast<std::uint16_t>(codePoint));
			segmentGlyphs.push_back(std::vector<std::uint16_t>{glyph});
		}
	}

	const std::uint16_t segmentCount = static_cast<std::uint16_t>(starts.size() + 1);
	std::vector<std::uint16_t> deltas;
	std::vector<std::uint16_t> rangeOffsets;
	std::vector<std::uint16_t> glyphArray;
	for (std::size_t i = 0; i < starts.size(); i++) {
		if (this->glyphIdArray) {
			deltas.push_back(0);
			rangeOffsets.push_back(static_cast<std::uint16_t>((segmentCount - i) * 2 + glyphArray.size() * 2));
			glyphArray.insert(glyphArray.end(), segmentGlyphs[i].begin(), segmentGlyphs[i].end());
		}
		else {
			deltas.push_back(static_cast<std::uint16_t>(segmentGlyphs[i].front() - starts[i]));
			rangeOffsets.push_back(0);
		}
	}
	starts.push_back(0xFFFF);
	ends.push_back(0xFFFF);
	deltas.push_back(1);
	rangeOffsets.push_back(0);

	std::uint16_t searchRange = 2;
	std::uint16_t entrySelector = 0;
	while (searchRange * 2 <= segmentCount * 2) {
		searchRange = static_cast<std::uint16_t>(searchRange * 2);
		entrySelector++;
	}

	writer.write(std::uint16_t{4});
	writer.write(static_cast<std::uint16_t>(16 + 8 * segmentCount + 2 * glyphArray.size()));
	writer.write(std::uint16_t{0});                // language
	writer.write(static_cast<std::uint16_t>(segmentCount * 2));
	writer.write(searchRange);
	writer.write(entrySelector);
	writer.write(static_cast<std::uint16_t>(segmentCount * 2 - searchRange));
	for (const std::uint16_t end : ends) {
		writer.write(end);
	}
	writer.write(std::uint16_t{0});                // reserved padding
	for (const std::uint16_t start : starts) {
		writer.write(start);
	}
	for (const std::uint16_t delta : deltas) {
		writer.write(delta);
	}
	for (const std::uint16_t rangeOffset : rangeOffsets) {
		writer.write(rangeOffset);
	}
	for (const std::uint16_t glyph : glyphArray) {
		writer.write(glyph);
	}
	return writer.takeBytes();
}

Bytes TestFontBuilder::buildKern() const {
	std::vector<TestKerningPair> pairs = this->kerning;
	std::sort(pairs.begin(), pairs.end(), [](const TestKerningPair& left, const TestKerningPair& right) {
		return (static_cast<std::uint32_t>(left.left) << 16 | left.right) < (static_cast<std::uint32_t>(right.left) << 16 | right.right);
	});

	ByteWriter writer;
	writer.write(std::uint16_t{0});                // version
	writer.write(std::uint16_t{1});                // one subtable
	writer.write(std::uint16_t{0});                // subtable version
	writer.write(static_cast<std::uint16_t>(14 + 6 * pairs.size()));
	writer.write(std::uint16_t{0x0001});           // format 0, horizontal
	writer.write(static_cast<std::uint16_t>(pairs.size()));
	writer.write(std::uint16_t{6});                // search range and friends: not used by the reader
	writer.write(std::uint16_t{0});
	writer.write(std::uint16_t{0});
	for (const TestKerningPair& pair : pairs) {
		writer.write(pair.left);
		writer.write(pair.right);
		writer.write(pair.value);
	}
	return writer.takeBytes();
}

void TestFontBuilder::buildGlyphs(Bytes& glyf, Bytes& loca) const {
	ByteWriter glyphWriter;
	std::vector<std::uint32_t> offsets;
	for (const TestGlyph& glyph : this->glyphs) {
		offsets.push_back(static_cast<std::uint32_t>(glyphWriter.getSize()));
		if (!glyph.components.empty()) {
			this->writeCompositeGlyph(glyphWriter, glyph);
		}
		else if (!glyph.contours.empty()) {
			this->writeSimpleGlyph(glyphWriter, glyph);
		}
		glyphWriter.align(2);
	}
	offsets.push_back(static_cast<std::uint32_t>(glyphWriter.getSize()));
	glyf = glyphWriter.takeBytes();

	ByteWriter locationWriter;
	for (const std::uint32_t offset : offsets) {
		if (this->longLocations) {
			locationWriter.write(offset);
		}
		else {
			locationWriter.write(static_cast<std::uint16_t>(offset / 2));
		}
	}
	loca = locationWriter.takeBytes();
}

void TestFontBuilder::writeSimpleGlyph(ByteWriter& writer, const TestGlyph& glyph) const {
	std::vector<TestPoint> points;
	std::vector<std::uint16_t> contourEnds;
	for (const std::vector<TestPoint>& contour : glyph.contours) {
		points.insert(points.end(), contour.begin(), contour.end());
		contourEnds.push_back(static_cast<std::uint16_t>(points.size() - 1));
	}

	std::int16_t xMin = points.front().x;
	std::int16_t yMin = points.front().y;
	std::int16_t xMax = xMin;
	std::int16_t yMax = yMin;
	for (const TestPoint& point : points) {
		xMin = std::min(xMin, point.x);
		yMin = std::min(yMin, point.y);
		xMax = std::max(xMax, point.x);
		yMax = std::max(yMax, point.y);
	}
	writer.write(static_cast<std::int16_t>(glyph.contours.size()));
	writer.write(xMin);
	writer.write(yMin);
	writer.write(xMax);
	writer.write(yMax);
	for (const std::uint16_t contourEnd : contourEnds) {
		writer.write(contourEnd);
	}
	writer.write(std::uint16_t{0});                // no instructions

	// Use every encoding the format allows: repeated coordinates, short
	// deltas with a sign flag, 16-bit deltas, and repeated flags.
	std::vector<std::uint8_t> flags;
	std::vector<std::int32_t> dxs;
	std::vector<std::int32_t> dys;
	std::int32_t previousX = 0;
	std::int32_t previousY = 0;
	for (const TestPoint& point : points) {
		const std::int32_t dx = point.x - previousX;
		const std::int32_t dy = point.y - previousY;
		previousX = point.x;
		previousY = point.y;

		std::uint32_t flag = point.onCurve ? 0x01u : 0x00u;
		if (dx == 0) {
			flag |= 0x10u;
		}
		else if (std::abs(dx) < 256) {
			flag |= dx > 0 ? 0x12u : 0x02u;
		}
		if (dy == 0) {
			flag |= 0x20u;
		}
		else if (std::abs(dy) < 256) {
			flag |= dy > 0 ? 0x24u : 0x04u;
		}
		flags.push_back(static_cast<std::uint8_t>(flag));
		dxs.push_back(dx);
		dys.push_back(dy);
	}

	for (std::size_t i = 0; i < flags.size();) {
		std::size_t run = 1;
		while (i + run < flags.size() && flags[i + run] == flags[i] && run < 256) {
			run++;
		}
		if (run >= 2) {
			writer.write(static_cast<std::uint8_t>(flags[i] | 0x08));
			writer.write(static_cast<std::uint8_t>(run - 1));
		}
		else {
			writer.write(flags[i]);
		}
		i += run;
	}

	auto writeDeltas = [&writer, &flags](const std::vector<std::int32_t>& deltas, std::uint8_t shortFlag) {
		for (std::size_t i = 0; i < deltas.size(); i++) {
			if (deltas[i] == 0) {
				continue;
			}
			if ((flags[i] & shortFlag) != 0) {
				writer.write(static_cast<std::uint8_t>(std::abs(deltas[i])));
			}
			else {
				writer.write(static_cast<std::int16_t>(deltas[i]));
			}
		}
	};
	writeDeltas(dxs, 0x02);
	writeDeltas(dys, 0x04);
}

void TestFontBuilder::writeCompositeGlyph(ByteWriter& writer, const TestGlyph& glyph) const {
	writer.write(std::int16_t{-1});
	for (std::size_t i = 0; i < 4; i++) {
		writer.write(std::int16_t{0});             // bounding box: not read
	}
	for (std::size_t i = 0; i < glyph.components.size(); i++) {
		const TestComponent& component = glyph.components[i];
		std::uint16_t flags = 0x0001;              // 16-bit arguments
		if (!component.pointMatching) {
			flags |= 0x0002;                // arguments are offsets
		}
		if (component.scale != 1.0f) {
			flags |= 0x0008;                // uniform scale
		}
		if (i + 1 < glyph.components.size()) {
			flags |= 0x0020;                // more components follow
		}
		writer.write(flags);
		writer.write(component.glyph);
		writer.write(component.offsetX);
		writer.write(component.offsetY);
		if (component.scale != 1.0f) {
			writer.write(static_cast<std::int16_t>(std::lround(component.scale * 16384.0f)));
		}
	}
}

std::uint16_t TestFontBuilder::getHorizontalMetricCount() const {
	const std::uint16_t total = static_cast<std::uint16_t>(this->glyphs.size());
	return this->horizontalMetricCount == 0 ? total : std::min(this->horizontalMetricCount, total);
}

}
