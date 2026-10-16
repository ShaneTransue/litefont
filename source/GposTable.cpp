#include <litefont/GposTable.h>

#include <algorithm>
#include <bit>

namespace litefont {

bool GposTable::read(const ByteReader& table) {
	this->subtables.clear();
	constexpr std::uint32_t kernTag = 0x6B65726Eu;      // 'kern'
	constexpr std::uint32_t defaultTag = 0x44464C54u;   // 'DFLT'
	constexpr std::uint32_t latinTag = 0x6C61746Eu;     // 'latn'

	ByteReader header = table;
	std::uint16_t major = 0;
	std::uint16_t minor = 0;
	std::uint16_t scriptList = 0;
	std::uint16_t featureList = 0;
	std::uint16_t lookupList = 0;
	header.read(major);
	header.read(minor);
	header.read(scriptList);
	header.read(featureList);
	header.read(lookupList);
	if (!header.isGood() || major != 1) {
		return false;
	}

	// Which features are 'kern' features.
	ByteReader features = table;
	std::uint16_t featureCount = 0;
	features.seek(featureList);
	features.read(featureCount);
	std::vector<bool> isKern(featureCount, false);
	std::vector<std::uint16_t> featureOffsets(featureCount, 0);
	for (std::uint16_t i = 0; i < featureCount; i++) {
		std::uint32_t tag = 0;
		features.read(tag);
		features.read(featureOffsets[i]);
		isKern[i] = tag == kernTag;
	}
	if (!features.isGood()) {
		return false;
	}

	// The kern features of the default script's default language, else
	// Latin's, else all of them.
	std::vector<std::uint16_t> chosen;
	ByteReader scripts = table;
	std::uint16_t scriptCount = 0;
	scripts.seek(scriptList);
	scripts.read(scriptCount);
	for (const std::uint32_t wanted : {defaultTag, latinTag}) {
		ByteReader records = scripts;
		for (std::uint16_t i = 0; i < scriptCount && chosen.empty(); i++) {
			std::uint32_t tag = 0;
			std::uint16_t scriptOffset = 0;
			records.read(tag);
			records.read(scriptOffset);
			if (!records.isGood() || tag != wanted) {
				continue;
			}
			ByteReader script = table;
			std::uint16_t defaultLanguage = 0;
			script.seek(static_cast<std::size_t>(scriptList) + scriptOffset);
			script.read(defaultLanguage);
			if (!script.isGood() || defaultLanguage == 0) {
				continue;
			}
			ByteReader language = table;
			std::uint16_t lookupOrder = 0;
			std::uint16_t required = 0;
			std::uint16_t count = 0;
			language.seek(static_cast<std::size_t>(scriptList) + scriptOffset + defaultLanguage);
			language.read(lookupOrder);
			language.read(required);
			language.read(count);
			std::vector<std::uint16_t> indices;
			if (required != 0xFFFF) {
				indices.push_back(required);
			}
			for (std::uint16_t k = 0; k < count; k++) {
				std::uint16_t index = 0;
				language.read(index);
				indices.push_back(index);
			}
			if (!language.isGood()) {
				continue;
			}
			for (const std::uint16_t index : indices) {
				if (index < featureCount && isKern[index]) {
					chosen.push_back(index);
				}
			}
		}
		if (!chosen.empty()) {
			break;
		}
	}
	if (chosen.empty()) {
		for (std::uint16_t i = 0; i < featureCount; i++) {
			if (isKern[i]) {
				chosen.push_back(i);
			}
		}
	}

	// The chosen features' lookups, in lookup list order, without repeats.
	std::vector<std::uint16_t> lookups;
	for (const std::uint16_t feature : chosen) {
		ByteReader reader = table;
		std::uint16_t parameters = 0;
		std::uint16_t count = 0;
		reader.seek(static_cast<std::size_t>(featureList) + featureOffsets[feature]);
		reader.read(parameters);
		reader.read(count);
		for (std::uint16_t k = 0; k < count; k++) {
			std::uint16_t lookup = 0;
			reader.read(lookup);
			if (reader.isGood()) {
				lookups.push_back(lookup);
			}
		}
	}
	std::sort(lookups.begin(), lookups.end());
	lookups.erase(std::unique(lookups.begin(), lookups.end()), lookups.end());

	ByteReader list = table;
	std::uint16_t lookupCount = 0;
	list.seek(lookupList);
	list.read(lookupCount);
	constexpr std::size_t mostSubtables = 4096;
	for (const std::uint16_t lookup : lookups) {
		if (lookup >= lookupCount || this->subtables.size() > mostSubtables) {
			continue;
		}
		ByteReader entry = table;
		std::uint16_t lookupOffset = 0;
		entry.seek(static_cast<std::size_t>(lookupList) + 2 + static_cast<std::size_t>(lookup) * 2);
		entry.read(lookupOffset);
		const std::uint32_t lookupStart = static_cast<std::uint32_t>(lookupList) + lookupOffset;
		ByteReader reader = table;
		std::uint16_t type = 0;
		std::uint16_t flags = 0;
		std::uint16_t count = 0;
		reader.seek(lookupStart);
		reader.read(type);
		reader.read(flags);
		reader.read(count);
		if (!entry.isGood() || !reader.isGood() || (type != 2 && type != 9)) {
			continue;
		}
		for (std::uint16_t k = 0; k < count; k++) {
			std::uint16_t subtableOffset = 0;
			reader.read(subtableOffset);
			std::uint32_t subtable = lookupStart + subtableOffset;
			if (type == 9) {
				// An extension: the real subtable sits behind a 32-bit offset.
				ByteReader extension = table;
				std::uint16_t format = 0;
				std::uint16_t extensionType = 0;
				std::uint32_t extensionOffset = 0;
				extension.seek(subtable);
				extension.read(format);
				extension.read(extensionType);
				extension.read(extensionOffset);
				if (!extension.isGood() || format != 1 || extensionType != 2 || std::uint64_t{subtable} + extensionOffset > table.getSize()) {
					continue;
				}
				subtable += extensionOffset;
			}
			if (reader.isGood() && subtable < table.getSize()) {
				this->subtables.push_back(GposPairSubtable{subtable, lookup});
			}
		}
	}
	return true;
}

bool GposTable::hasKerning() const {
	return !this->subtables.empty();
}

std::int16_t GposTable::getKerning(const ByteReader& table, std::uint16_t left, std::uint16_t right) const {
	// Subtables of one lookup are stored together, in lookup order.
	std::int32_t total = 0;
	std::size_t i = 0;
	while (i < this->subtables.size()) {
		const std::uint16_t lookup = this->subtables[i].lookup;
		bool applied = false;
		for (; i < this->subtables.size() && this->subtables[i].lookup == lookup; i++) {
			std::int16_t xAdvance = 0;
			if (!applied && GposTable::applyPair(table, this->subtables[i].offset, left, right, xAdvance)) {
				total += xAdvance;
				applied = true;
			}
		}
	}
	return static_cast<std::int16_t>(std::clamp<std::int32_t>(total, -32768, 32767));
}

bool GposTable::applyPair(const ByteReader& table, std::uint32_t subtable, std::uint16_t left, std::uint16_t right, std::int16_t& xAdvance) {
	xAdvance = 0;
	ByteReader reader = table;
	std::uint16_t format = 0;
	std::uint16_t coverage = 0;
	std::uint16_t valueFormat1 = 0;
	std::uint16_t valueFormat2 = 0;
	reader.seek(subtable);
	reader.read(format);
	reader.read(coverage);
	reader.read(valueFormat1);
	reader.read(valueFormat2);
	std::uint16_t coverageIndex = 0;
	if (!reader.isGood() || !GposTable::findCoverage(table, subtable + coverage, left, coverageIndex)) {
		return false;
	}
	const std::size_t recordSize = GposTable::getValueSize(valueFormat1) + GposTable::getValueSize(valueFormat2);

	if (format == 1) {
		// Explicit pairs: a pair set per first glyph, sorted by second glyph. A
		// set without the second glyph passes on to the next subtable.
		std::uint16_t pairSetCount = 0;
		reader.read(pairSetCount);
		if (!reader.isGood() || coverageIndex >= pairSetCount) {
			return false;
		}
		ByteReader offsetReader = table;
		std::uint16_t pairSet = 0;
		offsetReader.seek(static_cast<std::size_t>(subtable) + 10 + static_cast<std::size_t>(coverageIndex) * 2);
		offsetReader.read(pairSet);
		ByteReader set = table;
		std::uint16_t pairCount = 0;
		set.seek(static_cast<std::size_t>(subtable) + pairSet);
		set.read(pairCount);
		if (!offsetReader.isGood() || !set.isGood()) {
			return false;
		}
		const std::size_t records = set.getPosition();
		std::uint32_t low = 0;
		std::uint32_t high = pairCount;
		while (low < high) {
			const std::uint32_t middle = low + (high - low) / 2;
			ByteReader pair = table;
			std::uint16_t second = 0;
			pair.seek(records + static_cast<std::size_t>(middle) * (2 + recordSize));
			pair.read(second);
			if (!pair.isGood()) {
				return false;
			}
			if (second == right) {
				return GposTable::readValue(pair, valueFormat1, xAdvance);
			}
			if (second < right) {
				low = middle + 1;
			}
			else {
				high = middle;
			}
		}
		return false;
	}
	if (format != 2) {
		return false;
	}

	// Class pairs: the first glyph's class and the second's pick a record; a
	// covered first glyph always applies, even with a zero value.
	std::uint16_t classDefinition1 = 0;
	std::uint16_t classDefinition2 = 0;
	std::uint16_t class1Count = 0;
	std::uint16_t class2Count = 0;
	reader.read(classDefinition1);
	reader.read(classDefinition2);
	reader.read(class1Count);
	reader.read(class2Count);
	const std::uint16_t class1 = GposTable::getClass(table, subtable + classDefinition1, left);
	const std::uint16_t class2 = GposTable::getClass(table, subtable + classDefinition2, right);
	if (!reader.isGood() || class1 >= class1Count || class2 >= class2Count) {
		return false;
	}
	ByteReader record = table;
	record.seek(reader.getPosition() + (static_cast<std::size_t>(class1) * class2Count + class2) * recordSize);
	GposTable::readValue(record, valueFormat1, xAdvance);
	return true;
}

bool GposTable::findCoverage(const ByteReader& table, std::uint32_t coverage, std::uint16_t glyph, std::uint16_t& index) {
	ByteReader reader = table;
	std::uint16_t format = 0;
	std::uint16_t count = 0;
	reader.seek(coverage);
	reader.read(format);
	reader.read(count);
	if (!reader.isGood()) {
		return false;
	}
	const std::size_t start = reader.getPosition();
	std::uint32_t low = 0;
	std::uint32_t high = count;
	while (low < high) {
		const std::uint32_t middle = low + (high - low) / 2;
		ByteReader item = table;
		if (format == 1) {
			std::uint16_t value = 0;
			item.seek(start + static_cast<std::size_t>(middle) * 2);
			item.read(value);
			if (!item.isGood()) {
				return false;
			}
			if (value == glyph) {
				index = static_cast<std::uint16_t>(middle);
				return true;
			}
			if (value < glyph) {
				low = middle + 1;
			}
			else {
				high = middle;
			}
		}
		else if (format == 2) {
			std::uint16_t first = 0;
			std::uint16_t last = 0;
			std::uint16_t startIndex = 0;
			item.seek(start + static_cast<std::size_t>(middle) * 6);
			item.read(first);
			item.read(last);
			item.read(startIndex);
			if (!item.isGood()) {
				return false;
			}
			if (glyph < first) {
				high = middle;
			}
			else if (glyph > last) {
				low = middle + 1;
			}
			else {
				index = static_cast<std::uint16_t>(startIndex + (glyph - first));
				return true;
			}
		}
		else {
			return false;
		}
	}
	return false;
}

std::uint16_t GposTable::getClass(const ByteReader& table, std::uint32_t classDefinition, std::uint16_t glyph) {
	ByteReader reader = table;
	std::uint16_t format = 0;
	reader.seek(classDefinition);
	reader.read(format);
	if (format == 1) {
		std::uint16_t startGlyph = 0;
		std::uint16_t count = 0;
		reader.read(startGlyph);
		reader.read(count);
		if (!reader.isGood() || glyph < startGlyph || glyph - startGlyph >= count) {
			return 0;
		}
		std::uint16_t value = 0;
		reader.skip(static_cast<std::size_t>(glyph - startGlyph) * 2);
		reader.read(value);
		return reader.isGood() ? value : std::uint16_t{0};
	}
	if (format != 2) {
		return 0;
	}
	std::uint16_t count = 0;
	reader.read(count);
	const std::size_t start = reader.getPosition();
	std::uint32_t low = 0;
	std::uint32_t high = count;
	while (low < high) {
		const std::uint32_t middle = low + (high - low) / 2;
		ByteReader range = table;
		std::uint16_t first = 0;
		std::uint16_t last = 0;
		std::uint16_t value = 0;
		range.seek(start + static_cast<std::size_t>(middle) * 6);
		range.read(first);
		range.read(last);
		range.read(value);
		if (!range.isGood()) {
			return 0;
		}
		if (glyph < first) {
			high = middle;
		}
		else if (glyph > last) {
			low = middle + 1;
		}
		else {
			return value;
		}
	}
	return 0;
}

bool GposTable::readValue(ByteReader& record, std::uint16_t valueFormat, std::int16_t& xAdvance) {
	// A value record holds 16-bit fields for each set bit, in bit order; the
	// x advance follows x and y placement.
	xAdvance = 0;
	if ((valueFormat & 0x0004) == 0) {
		return record.isGood();
	}
	record.skip(static_cast<std::size_t>(std::popcount(static_cast<std::uint32_t>(valueFormat & 0x0003))) * 2);
	record.read(xAdvance);
	return record.isGood();
}

std::size_t GposTable::getValueSize(std::uint16_t valueFormat) {
	return static_cast<std::size_t>(std::popcount(static_cast<std::uint32_t>(valueFormat & 0x00FF))) * 2;
}

}
