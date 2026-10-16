#include <litefont/CffTable.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string>

namespace litefont {

bool CffTable::read(const ByteReader& table, std::uint16_t glyphCount) {
	*this = CffTable();
	this->glyphCount = glyphCount;

	ByteReader header = table;
	std::uint8_t major = 0;
	std::uint8_t minor = 0;
	std::uint8_t headerSize = 0;
	header.read(major);
	header.read(minor);
	header.read(headerSize);
	if (!header.isGood() || major != 1) {
		return false;
	}

	// Header, then the name, top DICT, string and global subroutine indexes.
	CffIndex names;
	CffIndex topDicts;
	CffIndex strings;
	if (!CffTable::readIndex(table, headerSize, names) || !CffTable::readIndex(table, names.end, topDicts)
		|| !CffTable::readIndex(table, topDicts.end, strings) || !CffTable::readIndex(table, strings.end, this->globalSubrs)) {
		return false;
	}
	ByteReader top;
	if (!CffTable::getEntry(table, topDicts, 0, top)) {
		return false;
	}

	constexpr std::uint16_t charStringsKey = 17;
	constexpr std::uint16_t charStringTypeKey = 0x0C06;
	constexpr std::uint16_t registryKey = 0x0C1E;
	constexpr std::uint16_t fontDictArrayKey = 0x0C24;
	constexpr std::uint16_t fontDictSelectKey = 0x0C25;
	std::array<double, 4> values{};
	std::uint32_t count = 0;
	if (CffTable::findDictValues(top, charStringTypeKey, values, count) && count > 0 && values[0] != 2.0) {
		return false;
	}
	if (!CffTable::findDictValues(top, charStringsKey, values, count) || count < 1
		|| !CffTable::readIndex(table, static_cast<std::uint32_t>(values[0]), this->charStrings)) {
		return false;
	}

	this->cidKeyed = CffTable::findDictValues(top, registryKey, values, count);
	if (this->cidKeyed) {
		// CID-keyed: one font DICT per group of glyphs, each with its own subroutines.
		CffIndex fontDicts;
		if (!CffTable::findDictValues(top, fontDictArrayKey, values, count) || count < 1
			|| !CffTable::readIndex(table, static_cast<std::uint32_t>(values[0]), fontDicts)) {
			return false;
		}
		if (!CffTable::findDictValues(top, fontDictSelectKey, values, count) || count < 1) {
			return false;
		}
		this->fontDictSelect = static_cast<std::uint32_t>(values[0]);
		for (std::uint32_t i = 0; i < fontDicts.count && i < 256; i++) {
			ByteReader fontDict;
			CffIndex subrs;
			if (!CffTable::getEntry(table, fontDicts, i, fontDict) || !CffTable::readLocalSubrs(table, fontDict, subrs)) {
				return false;
			}
			this->localSubrs.push_back(subrs);
		}
	}
	else {
		CffIndex subrs;
		if (!CffTable::readLocalSubrs(table, top, subrs)) {
			return false;
		}
		this->localSubrs.push_back(subrs);
	}
	this->loaded = true;
	return true;
}

bool CffTable::getOutline(const ByteReader& table, std::uint16_t glyph, Outline& outline) const {
	outline.clear();
	if (!this->loaded || glyph >= this->glyphCount || glyph >= this->charStrings.count) {
		return false;
	}
	ByteReader code;
	if (!CffTable::getEntry(table, this->charStrings, glyph, code)) {
		return false;
	}
	const std::uint8_t fontDict = this->getFontDict(table, glyph);
	const CffIndex empty;
	const CffIndex& subrs = fontDict < this->localSubrs.size() ? this->localSubrs[fontDict] : empty;

	CharStringState state;
	if (!this->execute(table, code, subrs, 0, state, outline)) {
		outline.clear();
		return false;
	}
	if (state.open) {
		outline.close();
	}
	return true;
}

bool CffTable::isLoaded() const {
	return this->loaded;
}

bool CffTable::isCidKeyed() const {
	return this->cidKeyed;
}

bool CffTable::execute(const ByteReader& table, ByteReader code, const CffIndex& localSubrs, std::uint32_t depth, CharStringState& state, Outline& outline) const {
	constexpr std::uint32_t deepestSubr = 10;
	if (depth > deepestSubr) {
		return false;
	}

	auto moveTo = [&state, &outline](float dx, float dy) {
		if (state.open) {
			outline.close();
		}
		state.point = state.point + Vector2f{dx, dy};
		outline.moveTo(state.point);
		state.open = true;
	};
	auto lineTo = [&state, &outline, &moveTo](float dx, float dy) {
		if (!state.open) {
			moveTo(0.0f, 0.0f);
		}
		state.point = state.point + Vector2f{dx, dy};
		outline.lineTo(state.point);
	};
	auto curveTo = [&state, &outline, &moveTo](float dx1, float dy1, float dx2, float dy2, float dx3, float dy3) {
		if (!state.open) {
			moveTo(0.0f, 0.0f);
		}
		const Vector2f first = state.point + Vector2f{dx1, dy1};
		const Vector2f second = first + Vector2f{dx2, dy2};
		state.point = second + Vector2f{dx3, dy3};
		outline.cubicTo(first, second, state.point);
	};
	// The first stack-clearing operator may carry the advance width as an
	// extra first argument; returns where the real arguments start.
	auto skipWidth = [&state](std::uint32_t evenOdd) {
		std::uint32_t first = 0;
		if (!state.widthSeen) {
			if (((state.count % 2) ^ evenOdd) != 0 && state.count > 0) {
				first = 1;
			}
			state.widthSeen = true;
		}
		return first;
	};
	auto countHints = [&state, &skipWidth]() {
		const std::uint32_t first = skipWidth(0);
		state.hints += (state.count - first) / 2;
		state.count = 0;
	};
	auto closeContour = [&state, &outline]() {
		if (state.open) {
			outline.close();
			state.open = false;
		}
	};
	const float* args = state.stack.data();

	std::uint8_t b0 = 0;
	while (code.read(b0)) {
		// Operands.
		if (b0 == 28 || b0 >= 32) {
			float value = 0.0f;
			if (b0 == 28) {
				std::int16_t number = 0;
				code.read(number);
				value = static_cast<float>(number);
			}
			else if (b0 <= 246) {
				value = static_cast<float>(std::int32_t{b0} - 139);
			}
			else if (b0 <= 250) {
				std::uint8_t b1 = 0;
				code.read(b1);
				value = static_cast<float>((std::int32_t{b0} - 247) * 256 + std::int32_t{b1} + 108);
			}
			else if (b0 <= 254) {
				std::uint8_t b1 = 0;
				code.read(b1);
				value = static_cast<float>(-(std::int32_t{b0} - 251) * 256 - std::int32_t{b1} - 108);
			}
			else {
				std::int32_t fixed = 0;
				code.read(fixed);
				value = static_cast<float>(static_cast<double>(fixed) / 65536.0);
			}
			if (!code.isGood() || state.count >= state.stack.size()) {
				return false;
			}
			state.stack[state.count++] = value;
			continue;
		}

		const std::uint32_t n = state.count;
		switch (b0) {
		case 1:     // hstem
		case 3:     // vstem
		case 18:    // hstemhm
		case 23:    // vstemhm
			countHints();
			break;
		case 19:    // hintmask
		case 20:    // cntrmask
			if (state.hintMaskBytes == 0) {
				countHints();
				state.hintMaskBytes = (state.hints + 7) / 8;
			}
			if (!code.skip(state.hintMaskBytes)) {
				return false;
			}
			break;
		case 21: {  // rmoveto
			closeContour();
			const std::uint32_t first = skipWidth(0);
			if (n - first < 2) {
				return false;
			}
			moveTo(args[first], args[first + 1]);
			state.count = 0;
			break;
		}
		case 22:    // hmoveto
		case 4: {   // vmoveto
			closeContour();
			const std::uint32_t first = skipWidth(1);
			if (n - first < 1) {
				return false;
			}
			moveTo(b0 == 22 ? args[first] : 0.0f, b0 == 22 ? 0.0f : args[first]);
			state.count = 0;
			break;
		}
		case 5:     // rlineto
			for (std::uint32_t i = 0; i + 1 < n; i += 2) {
				lineTo(args[i], args[i + 1]);
			}
			state.count = 0;
			break;
		case 6:     // hlineto
		case 7: {   // vlineto
			bool horizontal = b0 == 6;
			for (std::uint32_t i = 0; i < n; i++) {
				lineTo(horizontal ? args[i] : 0.0f, horizontal ? 0.0f : args[i]);
				horizontal = !horizontal;
			}
			state.count = 0;
			break;
		}
		case 8:     // rrcurveto
			for (std::uint32_t i = 0; i + 5 < n; i += 6) {
				curveTo(args[i], args[i + 1], args[i + 2], args[i + 3], args[i + 4], args[i + 5]);
			}
			state.count = 0;
			break;
		case 24: {  // rcurveline
			if (n < 2) {
				return false;
			}
			for (std::uint32_t i = 0; i + 6 <= n - 2; i += 6) {
				curveTo(args[i], args[i + 1], args[i + 2], args[i + 3], args[i + 4], args[i + 5]);
			}
			lineTo(args[n - 2], args[n - 1]);
			state.count = 0;
			break;
		}
		case 25: {  // rlinecurve
			if (n < 6) {
				return false;
			}
			for (std::uint32_t i = 0; i + 1 < n - 6; i += 2) {
				lineTo(args[i], args[i + 1]);
			}
			curveTo(args[n - 6], args[n - 5], args[n - 4], args[n - 3], args[n - 2], args[n - 1]);
			state.count = 0;
			break;
		}
		case 26:    // vvcurveto: dx1? {dya dxb dyb dyc}+
		case 27: {  // hhcurveto: dy1? {dxa dxb dyb dxc}+
			std::uint32_t i = n % 2;
			float first = i == 1 ? args[0] : 0.0f;
			for (; i + 3 < n; i += 4) {
				if (b0 == 26) {
					curveTo(first, args[i], args[i + 1], args[i + 2], 0.0f, args[i + 3]);
				}
				else {
					curveTo(args[i], first, args[i + 1], args[i + 2], args[i + 3], 0.0f);
				}
				first = 0.0f;
			}
			state.count = 0;
			break;
		}
		case 30:    // vhcurveto
		case 31: {  // hvcurveto
			// Curves alternate between starting vertical and horizontal; the
			// last one may take one extra final argument.
			bool vertical = b0 == 30;
			std::uint32_t i = 0;
			while (n - i >= 4) {
				const float last = n - i == 5 ? args[i + 4] : 0.0f;
				if (vertical) {
					curveTo(0.0f, args[i], args[i + 1], args[i + 2], args[i + 3], last);
				}
				else {
					curveTo(args[i], 0.0f, args[i + 1], args[i + 2], last, args[i + 3]);
				}
				i += n - i == 5 ? 5 : 4;
				vertical = !vertical;
			}
			state.count = 0;
			break;
		}
		case 10:    // callsubr
		case 29: {  // callgsubr
			if (n < 1) {
				return false;
			}
			const CffIndex& subrs = b0 == 10 ? localSubrs : this->globalSubrs;
			const std::int64_t number = static_cast<std::int64_t>(args[n - 1]) + CffTable::getBias(subrs);
			state.count--;
			ByteReader subr;
			if (number < 0 || !CffTable::getEntry(table, subrs, static_cast<std::uint32_t>(number), subr)) {
				return false;
			}
			if (!this->execute(table, subr, localSubrs, depth + 1, state, outline)) {
				return false;
			}
			if (state.ended) {
				return true;
			}
			break;
		}
		case 11:    // return
			return true;
		case 14: {  // endchar
			closeContour();
			const std::uint32_t first = skipWidth(0);
			if (n - first >= 4) {
				return false;   // seac: accented glyphs built from standard encoding
			}
			state.count = 0;
			state.ended = true;
			return true;
		}
		case 12: {
			std::uint8_t b1 = 0;
			if (!code.read(b1)) {
				return false;
			}
			if (b1 == 35 && n >= 13) {          // flex
				curveTo(args[0], args[1], args[2], args[3], args[4], args[5]);
				curveTo(args[6], args[7], args[8], args[9], args[10], args[11]);
			}
			else if (b1 == 34 && n >= 7) {      // hflex
				curveTo(args[0], 0.0f, args[1], args[2], args[3], 0.0f);
				curveTo(args[4], 0.0f, args[5], -args[2], args[6], 0.0f);
			}
			else if (b1 == 36 && n >= 9) {      // hflex1
				curveTo(args[0], args[1], args[2], args[3], args[4], 0.0f);
				curveTo(args[5], 0.0f, args[6], args[7], args[8], -(args[1] + args[3] + args[7]));
			}
			else if (b1 == 37 && n >= 11) {     // flex1
				const float dx = args[0] + args[2] + args[4] + args[6] + args[8];
				const float dy = args[1] + args[3] + args[5] + args[7] + args[9];
				curveTo(args[0], args[1], args[2], args[3], args[4], args[5]);
				if (std::fabs(dx) > std::fabs(dy)) {
					curveTo(args[6], args[7], args[8], args[9], args[10], -dy);
				}
				else {
					curveTo(args[6], args[7], args[8], args[9], -dx, args[10]);
				}
			}
			else if (b1 == 0 || b1 == 34 || b1 == 35 || b1 == 36 || b1 == 37) {
				// dotsection is ignored; a flex short of arguments draws nothing.
				if (b1 != 0) {
					return false;
				}
			}
			else {
				return false;   // arithmetic and storage operators
			}
			state.count = 0;
			break;
		}
		default:
			// Reserved operators clear the stack, as fontTools does.
			state.count = 0;
			break;
		}
	}
	return code.getRemaining() == 0;
}

std::uint8_t CffTable::getFontDict(const ByteReader& table, std::uint16_t glyph) const {
	if (!this->cidKeyed) {
		return 0;
	}
	ByteReader select = table;
	std::uint8_t format = 0;
	select.seek(this->fontDictSelect);
	select.read(format);
	if (format == 0) {
		std::uint8_t fontDict = 0;
		select.skip(glyph);
		select.read(fontDict);
		return select.isGood() ? fontDict : std::uint8_t{0};
	}
	if (format != 3) {
		return 0;
	}
	// Ranges sorted by first glyph, then a sentinel glyph.
	std::uint16_t rangeCount = 0;
	select.read(rangeCount);
	const std::size_t ranges = select.getPosition();
	std::uint32_t low = 0;
	std::uint32_t high = rangeCount;
	std::uint8_t found = 0;
	while (low < high) {
		const std::uint32_t middle = low + (high - low) / 2;
		ByteReader range = table;
		std::uint16_t first = 0;
		std::uint8_t fontDict = 0;
		std::uint16_t next = 0;
		range.seek(ranges + static_cast<std::size_t>(middle) * 3);
		range.read(first);
		range.read(fontDict);
		range.read(next);
		if (!range.isGood()) {
			return 0;
		}
		if (glyph < first) {
			high = middle;
		}
		else if (glyph >= next) {
			low = middle + 1;
		}
		else {
			found = fontDict;
			break;
		}
	}
	return found;
}

bool CffTable::readIndex(const ByteReader& table, std::uint32_t offset, CffIndex& index) {
	index = CffIndex{};
	ByteReader reader = table;
	std::uint16_t count = 0;
	reader.seek(offset);
	reader.read(count);
	if (!reader.isGood()) {
		return false;
	}
	index.count = count;
	if (count == 0) {
		index.end = offset + 2;
		index.dataStart = index.end;
		return true;
	}
	std::uint8_t offsetSize = 0;
	reader.read(offsetSize);
	if (!reader.isGood() || offsetSize < 1 || offsetSize > 4) {
		return false;
	}
	index.offsetSize = offsetSize;
	index.offsetsStart = offset + 3;
	const std::uint64_t dataStart = std::uint64_t{index.offsetsStart} + (std::uint64_t{count} + 1) * offsetSize - 1;

	std::uint32_t last = 0;
	reader.seek(static_cast<std::size_t>(index.offsetsStart) + static_cast<std::size_t>(count) * offsetSize);
	for (std::uint8_t i = 0; i < offsetSize; i++) {
		std::uint8_t byte = 0;
		reader.read(byte);
		last = (last << 8) | byte;
	}
	const std::uint64_t end = dataStart + last;
	if (!reader.isGood() || end > table.getSize()) {
		return false;
	}
	index.dataStart = static_cast<std::uint32_t>(dataStart);
	index.end = static_cast<std::uint32_t>(end);
	return true;
}

bool CffTable::getEntry(const ByteReader& table, const CffIndex& index, std::uint32_t item, ByteReader& entry) {
	if (item >= index.count) {
		return false;
	}
	auto readOffset = [&table, &index](std::uint32_t position, std::uint32_t& value) {
		ByteReader reader = table;
		reader.seek(static_cast<std::size_t>(index.offsetsStart) + static_cast<std::size_t>(position) * index.offsetSize);
		value = 0;
		for (std::uint8_t i = 0; i < index.offsetSize; i++) {
			std::uint8_t byte = 0;
			reader.read(byte);
			value = (value << 8) | byte;
		}
		return reader.isGood();
	};
	std::uint32_t start = 0;
	std::uint32_t end = 0;
	if (!readOffset(item, start) || !readOffset(item + 1, end) || start < 1 || end < start) {
		return false;
	}
	return table.slice(static_cast<std::size_t>(index.dataStart) + start, end - start, entry);
}

bool CffTable::findDictValues(ByteReader dict, std::uint16_t key, std::array<double, 4>& values, std::uint32_t& count) {
	std::array<double, 48> operands{};
	std::uint32_t operandCount = 0;
	std::uint8_t b0 = 0;
	while (dict.read(b0)) {
		if (b0 <= 21) {
			std::uint16_t op = b0;
			if (b0 == 12) {
				std::uint8_t b1 = 0;
				if (!dict.read(b1)) {
					return false;
				}
				op = static_cast<std::uint16_t>(0x0C00 | b1);
			}
			if (op == key) {
				count = std::min<std::uint32_t>(operandCount, 4);
				for (std::uint32_t i = 0; i < count; i++) {
					values[i] = operands[i];
				}
				return true;
			}
			operandCount = 0;
			continue;
		}

		double value = 0.0;
		if (b0 == 28) {
			std::int16_t number = 0;
			dict.read(number);
			value = number;
		}
		else if (b0 == 29) {
			std::int32_t number = 0;
			dict.read(number);
			value = number;
		}
		else if (b0 == 30) {
			// A real number in packed decimal digits, ended by the nibble 15.
			std::string text;
			bool done = false;
			while (!done) {
				std::uint8_t packed = 0;
				if (!dict.read(packed)) {
					return false;
				}
				for (const std::uint8_t nibble : {static_cast<std::uint8_t>(packed >> 4), static_cast<std::uint8_t>(packed & 15)}) {
					if (done) {
						break;
					}
					if (nibble <= 9) {
						text.push_back(static_cast<char>('0' + nibble));
					}
					else if (nibble == 10) {
						text.push_back('.');
					}
					else if (nibble == 11) {
						text.push_back('e');
					}
					else if (nibble == 12) {
						text += "e-";
					}
					else if (nibble == 14) {
						text.push_back('-');
					}
					else if (nibble == 15) {
						done = true;
					}
				}
			}
			std::from_chars(text.data(), text.data() + text.size(), value);
		}
		else if (b0 >= 32 && b0 <= 246) {
			value = std::int32_t{b0} - 139;
		}
		else if (b0 >= 247 && b0 <= 250) {
			std::uint8_t b1 = 0;
			dict.read(b1);
			value = (std::int32_t{b0} - 247) * 256 + std::int32_t{b1} + 108;
		}
		else if (b0 >= 251 && b0 <= 254) {
			std::uint8_t b1 = 0;
			dict.read(b1);
			value = -(std::int32_t{b0} - 251) * 256 - std::int32_t{b1} - 108;
		}
		else {
			return false;
		}
		if (!dict.isGood()) {
			return false;
		}
		if (operandCount < operands.size()) {
			operands[operandCount++] = value;
		}
	}
	return false;
}

bool CffTable::readLocalSubrs(const ByteReader& table, ByteReader fontDict, CffIndex& subrs) {
	constexpr std::uint16_t privateKey = 18;
	constexpr std::uint16_t subrsKey = 19;
	subrs = CffIndex{};
	std::array<double, 4> values{};
	std::uint32_t count = 0;
	if (!CffTable::findDictValues(fontDict, privateKey, values, count) || count < 2) {
		return true;
	}
	const std::uint32_t size = static_cast<std::uint32_t>(values[0]);
	const std::uint32_t offset = static_cast<std::uint32_t>(values[1]);
	ByteReader privateDict;
	if (!table.slice(offset, size, privateDict)) {
		return false;
	}
	if (!CffTable::findDictValues(privateDict, subrsKey, values, count) || count < 1) {
		return true;
	}
	return CffTable::readIndex(table, offset + static_cast<std::uint32_t>(values[0]), subrs);
}

std::int32_t CffTable::getBias(const CffIndex& subrs) {
	if (subrs.count < 1240) {
		return 107;
	}
	return subrs.count < 33900 ? 1131 : 32768;
}

}
