#include <litefont/Font.h>
#include <litefont/Hash.h>
#include <litefont/Utf8.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace litefont {

bool Font::load(const std::filesystem::path& path, std::uint32_t face) {
	Bytes fileBytes;
	if (!File::read(path, fileBytes)) {
		*this = Font();
		return this->fail(FontError::File);
	}
	return this->load(std::move(fileBytes), face);
}

bool Font::load(Bytes bytes, std::uint32_t face) {
	*this = Font();
	this->bytes = std::move(bytes);

	const bool parsed = this->readTables(face)
		&& this->readHead()
		&& this->readMaxp()
		&& this->readHhea()
		&& this->readOs2()
		&& this->readCmap()
		&& this->readCff()
		&& this->readKern();
	if (!parsed) {
		const FontError reason = this->error;
		*this = Font();
		this->error = reason;
		return false;
	}

	// Traits, names and GPOS kerning are optional: whatever is missing or
	// malformed keeps its default.
	Font::readTraits(this->getTableReader(this->tables.head), this->getTableReader(this->tables.os2),
		this->getTableReader(this->tables.post), this->traits);
	Font::readNames(this->getTableReader(this->tables.name), this->names);
	this->readGpos();
	this->readDecorationMetrics();

	// The file's hash, continued with the face index for later faces of a collection.
	this->face = face;
	this->id = Hash::fnv1a(this->bytes.data);
	if (face > 0) {
		const std::array<std::uint8_t, 4> faceBytes{static_cast<std::uint8_t>(face >> 24), static_cast<std::uint8_t>(face >> 16),
			static_cast<std::uint8_t>(face >> 8), static_cast<std::uint8_t>(face)};
		this->id = Hash::fnv1a(faceBytes, this->id);
	}
	this->loaded = true;
	this->estimateMissingHeights();
	return true;
}

bool Font::isLoaded() const {
	return this->loaded;
}

FontError Font::getError() const {
	return this->error;
}

std::uint64_t Font::getId() const {
	return this->id;
}

std::uint32_t Font::getFace() const {
	return this->face;
}

std::uint16_t Font::getGlyphCount() const {
	return this->glyphCount;
}

bool Font::hasPostScriptOutlines() const {
	return this->postScriptOutlines;
}

const FontTraits& Font::getTraits() const {
	return this->traits;
}

const FontMetrics& Font::getMetrics() const {
	return this->metrics;
}

const FontNames& Font::getNames() const {
	return this->names;
}

GlyphId Font::getGlyph(char32_t codePoint) const {
	if (!this->loaded) {
		return 0;
	}
	if (this->characterMapFormat == 12) {
		return this->lookupFormat12(codePoint);
	}
	return this->lookupFormat4(codePoint);
}

GlyphMetrics Font::getGlyphMetrics(GlyphId glyph) const {
	GlyphMetrics glyphMetrics;
	if (!this->loaded || glyph >= this->glyphCount) {
		return glyphMetrics;
	}

	// Glyphs past the last full entry repeat its advance and have their own
	// left side bearing in the array that follows.
	const ByteReader table = this->getTableReader(this->tables.hmtx);
	const std::uint16_t lastFull = static_cast<std::uint16_t>(this->horizontalMetricCount - 1);
	ByteReader advance = table;
	advance.seek(static_cast<std::size_t>(glyph < this->horizontalMetricCount ? glyph : lastFull) * 4);
	advance.read(glyphMetrics.advance);

	ByteReader bearing = table;
	if (glyph < this->horizontalMetricCount) {
		bearing.seek(static_cast<std::size_t>(glyph) * 4 + 2);
	}
	else {
		bearing.seek(static_cast<std::size_t>(this->horizontalMetricCount) * 4 + static_cast<std::size_t>(glyph - this->horizontalMetricCount) * 2);
	}
	bearing.read(glyphMetrics.leftSideBearing);
	return glyphMetrics;
}

std::int16_t Font::getKerning(GlyphId left, GlyphId right) const {
	if (!this->loaded) {
		return 0;
	}
	if (this->gpos.hasKerning()) {
		return this->gpos.getKerning(this->getTableReader(this->tables.gpos), left, right);
	}
	return this->getTableKerning(left, right);
}

bool Font::getOutline(GlyphId glyph, Outline& outline) const {
	outline.clear();
	if (!this->loaded || glyph >= this->glyphCount) {
		return false;
	}
	if (this->postScriptOutlines) {
		return this->cff.getOutline(this->getTableReader(this->tables.cff), glyph, outline);
	}
	// Bounds the work a malicious composite glyph can cause across all levels.
	std::uint32_t componentBudget = 256;
	if (!this->appendGlyph(glyph, Affine2f::identity(), 0, componentBudget, outline)) {
		outline.clear();
		return false;
	}
	return true;
}

std::uint32_t Font::getFaceCount(const ByteReader& file) {
	ByteReader reader = file;
	std::uint32_t version = 0;
	if (!reader.read(version)) {
		return 0;
	}
	if (version == Font::getTag("ttcf")) {
		std::uint32_t collectionVersion = 0;
		std::uint32_t faceCount = 0;
		reader.read(collectionVersion);
		reader.read(faceCount);
		return reader.isGood() ? faceCount : 0;
	}
	return Font::isSfntVersion(version) ? 1 : 0;
}

FontError Font::readTableDirectory(const ByteReader& file, std::uint32_t face, FontTables& tables) {
	tables = FontTables();
	ByteReader reader = file;
	std::uint32_t version = 0;
	if (!reader.read(version)) {
		return FontError::Truncated;
	}

	// A collection starts with the offsets of its faces' table directories.
	if (version == Font::getTag("ttcf")) {
		std::uint32_t collectionVersion = 0;
		std::uint32_t faceCount = 0;
		std::uint32_t directory = 0;
		reader.read(collectionVersion);
		reader.read(faceCount);
		if (!reader.isGood()) {
			return FontError::Truncated;
		}
		if (face >= faceCount) {
			return FontError::MissingFace;
		}
		reader.skip(static_cast<std::size_t>(face) * 4);
		reader.read(directory);
		if (!reader.isGood()) {
			return FontError::Truncated;
		}
		reader = file;
		if (!reader.seek(directory) || !reader.read(version)) {
			return FontError::Truncated;
		}
	}
	else if (face != 0) {
		return FontError::MissingFace;
	}

	std::uint16_t tableCount = 0;
	reader.read(tableCount);
	reader.skip(6);
	if (!reader.isGood()) {
		return FontError::Truncated;
	}
	if (!Font::isSfntVersion(version)) {
		return FontError::UnsupportedFormat;
	}

	for (std::uint16_t i = 0; i < tableCount; i++) {
		std::uint32_t tableTag = 0;
		std::uint32_t checksum = 0;
		std::uint32_t offset = 0;
		std::uint32_t length = 0;
		reader.read(tableTag);
		reader.read(checksum);
		reader.read(offset);
		reader.read(length);
		if (!reader.isGood()) {
			return FontError::Truncated;
		}

		FontTable* table = nullptr;
		if (tableTag == Font::getTag("head")) {
			table = &tables.head;
		}
		else if (tableTag == Font::getTag("maxp")) {
			table = &tables.maxp;
		}
		else if (tableTag == Font::getTag("hhea")) {
			table = &tables.hhea;
		}
		else if (tableTag == Font::getTag("hmtx")) {
			table = &tables.hmtx;
		}
		else if (tableTag == Font::getTag("OS/2")) {
			table = &tables.os2;
		}
		else if (tableTag == Font::getTag("cmap")) {
			table = &tables.cmap;
		}
		else if (tableTag == Font::getTag("loca")) {
			table = &tables.loca;
		}
		else if (tableTag == Font::getTag("glyf")) {
			table = &tables.glyf;
		}
		else if (tableTag == Font::getTag("CFF ")) {
			table = &tables.cff;
		}
		else if (tableTag == Font::getTag("kern")) {
			table = &tables.kern;
		}
		else if (tableTag == Font::getTag("GPOS")) {
			table = &tables.gpos;
		}
		else if (tableTag == Font::getTag("post")) {
			table = &tables.post;
		}
		else if (tableTag == Font::getTag("name")) {
			table = &tables.name;
		}
		if (table != nullptr) {
			table->offset = offset;
			table->length = length;
			table->present = true;
		}
	}
	return FontError::None;
}

void Font::readTraits(const ByteReader& head, const ByteReader& os2, const ByteReader& post, FontTraits& traits) {
	traits = FontTraits();

	// macStyle bit 0 is bold and bit 1 italic; OS/2, when present, has the final say.
	ByteReader style = head;
	std::uint16_t macStyle = 0;
	style.seek(44);
	style.read(macStyle);
	if (style.isGood()) {
		traits.bold = (macStyle & 0x0001) != 0;
		traits.italic = (macStyle & 0x0002) != 0;
		traits.weight = traits.bold ? std::uint16_t{700} : std::uint16_t{400};
	}

	ByteReader selection = os2;
	std::uint16_t fsSelection = 0;
	selection.seek(62);
	selection.read(fsSelection);
	if (selection.isGood()) {
		traits.bold = (fsSelection & 0x0020) != 0;
		traits.italic = (fsSelection & 0x0001) != 0;
	}
	ByteReader weightClass = os2;
	std::uint16_t weight = 0;
	weightClass.seek(4);
	weightClass.read(weight);
	if (weightClass.isGood() && weight >= 1 && weight <= 1000) {
		traits.weight = weight;
	}

	// The italic angle is 16.16 fixed point.
	ByteReader angle = post;
	std::int32_t italicAngle = 0;
	angle.seek(4);
	angle.read(italicAngle);
	if (angle.isGood()) {
		traits.italicAngle = static_cast<float>(italicAngle) / 65536.0f;
	}
}

bool Font::readNames(const ByteReader& name, FontNames& names) {
	names = FontNames();
	constexpr std::uint16_t familyId = 1;
	constexpr std::uint16_t subfamilyId = 2;
	constexpr std::uint16_t fullNameId = 4;
	Font::readName(name, familyId, names.family);
	Font::readName(name, subfamilyId, names.subfamily);
	Font::readName(name, fullNameId, names.fullName);
	return !names.family.empty();
}

std::int16_t Font::getTableKerning(GlyphId left, GlyphId right) const {
	if (this->kerningPairCount == 0) {
		return 0;
	}

	// Format 0 pairs are sorted by (left << 16 | right): binary search.
	const std::uint32_t key = (static_cast<std::uint32_t>(left) << 16) | right;
	const ByteReader file(this->bytes.data);
	std::uint32_t low = 0;
	std::uint32_t high = this->kerningPairCount;
	while (low < high) {
		const std::uint32_t middle = low + (high - low) / 2;
		ByteReader pair = file;
		std::uint16_t pairLeft = 0;
		std::uint16_t pairRight = 0;
		std::int16_t value = 0;
		pair.seek(static_cast<std::size_t>(this->kerningPairsOffset) + static_cast<std::size_t>(middle) * 6);
		pair.read(pairLeft);
		pair.read(pairRight);
		pair.read(value);
		if (!pair.isGood()) {
			return 0;
		}
		const std::uint32_t pairKey = (static_cast<std::uint32_t>(pairLeft) << 16) | pairRight;
		if (pairKey == key) {
			return value;
		}
		if (pairKey < key) {
			low = middle + 1;
		}
		else {
			high = middle;
		}
	}
	return 0;
}

bool Font::fail(FontError error) {
	this->error = error;
	return false;
}

bool Font::readTables(std::uint32_t face) {
	const ByteReader file(this->bytes.data);
	const FontError directoryError = Font::readTableDirectory(file, face, this->tables);
	if (directoryError != FontError::None) {
		return this->fail(directoryError);
	}
	const std::array<const FontTable*, 13> all{&this->tables.head, &this->tables.maxp, &this->tables.hhea,
		&this->tables.hmtx, &this->tables.os2, &this->tables.cmap, &this->tables.loca, &this->tables.glyf,
		&this->tables.cff, &this->tables.kern, &this->tables.gpos, &this->tables.post, &this->tables.name};
	for (const FontTable* table : all) {
		if (table->present && !file.contains(table->offset, table->length)) {
			return this->fail(FontError::Truncated);
		}
	}

	// TrueType outlines when glyf and loca are there, else CFF.
	const bool trueType = this->tables.loca.present && this->tables.glyf.present;
	const bool complete = this->tables.head.present && this->tables.maxp.present
		&& this->tables.hhea.present && this->tables.hmtx.present
		&& this->tables.cmap.present && (trueType || this->tables.cff.present);
	if (!complete) {
		return this->fail(FontError::MissingTable);
	}
	this->postScriptOutlines = !trueType;
	return true;
}

bool Font::readHead() {
	ByteReader head = this->getTableReader(this->tables.head);
	std::uint16_t unitsPerEm = 0;
	std::int16_t locationFormat = 0;
	head.seek(18);
	head.read(unitsPerEm);
	head.seek(50);
	head.read(locationFormat);
	if (!head.isGood()) {
		return this->fail(FontError::Truncated);
	}
	// The location format only matters to TrueType outlines.
	const bool badLocations = !this->postScriptOutlines && locationFormat != 0 && locationFormat != 1;
	if (unitsPerEm < 16 || unitsPerEm > 16384 || badLocations) {
		return this->fail(FontError::UnsupportedFormat);
	}
	this->metrics.unitsPerEm = unitsPerEm;
	this->indexToLocFormat = locationFormat;
	return true;
}

bool Font::readMaxp() {
	ByteReader maxp = this->getTableReader(this->tables.maxp);
	maxp.seek(4);
	maxp.read(this->glyphCount);
	if (!maxp.isGood()) {
		return this->fail(FontError::Truncated);
	}
	if (this->glyphCount == 0) {
		return this->fail(FontError::UnsupportedFormat);
	}
	if (this->postScriptOutlines) {
		return true;
	}
	const std::size_t entrySize = this->indexToLocFormat == 0 ? 2 : 4;
	if ((static_cast<std::size_t>(this->glyphCount) + 1) * entrySize > this->tables.loca.length) {
		return this->fail(FontError::Truncated);
	}
	return true;
}

bool Font::readHhea() {
	ByteReader hhea = this->getTableReader(this->tables.hhea);
	hhea.seek(4);
	hhea.read(this->metrics.ascender);
	hhea.read(this->metrics.descender);
	hhea.read(this->metrics.lineGap);
	hhea.seek(34);
	hhea.read(this->horizontalMetricCount);
	if (!hhea.isGood()) {
		return this->fail(FontError::Truncated);
	}
	if (this->horizontalMetricCount == 0) {
		return this->fail(FontError::UnsupportedFormat);
	}
	if (this->horizontalMetricCount > this->glyphCount) {
		this->horizontalMetricCount = this->glyphCount;
	}
	if (static_cast<std::size_t>(this->horizontalMetricCount) * 4 > this->tables.hmtx.length) {
		return this->fail(FontError::Truncated);
	}
	return true;
}

bool Font::readOs2() {
	// OS/2 is optional; anything missing or short simply leaves the hhea values.
	if (this->tables.os2.length == 0) {
		return true;
	}
	const ByteReader os2 = this->getTableReader(this->tables.os2);

	ByteReader header = os2;
	std::uint16_t version = 0;
	header.read(version);

	constexpr std::uint16_t useTypoMetrics = 0x0080;
	ByteReader selection = os2;
	std::uint16_t fsSelection = 0;
	selection.seek(62);
	selection.read(fsSelection);

	ByteReader typo = os2;
	std::int16_t typoAscender = 0;
	std::int16_t typoDescender = 0;
	std::int16_t typoLineGap = 0;
	typo.seek(68);
	typo.read(typoAscender);
	typo.read(typoDescender);
	typo.read(typoLineGap);

	if (selection.isGood() && typo.isGood() && (fsSelection & useTypoMetrics) != 0) {
		this->metrics.ascender = typoAscender;
		this->metrics.descender = typoDescender;
		this->metrics.lineGap = typoLineGap;
	}

	if (header.isGood() && version >= 2) {
		ByteReader heights = os2;
		std::int16_t xHeight = 0;
		std::int16_t capHeight = 0;
		heights.seek(86);
		heights.read(xHeight);
		heights.read(capHeight);
		if (heights.isGood()) {
			this->metrics.xHeight = xHeight;
			this->metrics.capHeight = capHeight;
		}
	}
	return true;
}

bool Font::readCmap() {
	ByteReader cmap = this->getTableReader(this->tables.cmap);
	std::uint16_t version = 0;
	std::uint16_t subtableCount = 0;
	cmap.read(version);
	cmap.read(subtableCount);
	if (!cmap.isGood()) {
		return this->fail(FontError::Truncated);
	}

	// Lower is better: full Unicode first, then the Basic Multilingual Plane.
	auto preference = [](std::uint16_t platform, std::uint16_t encoding) -> std::int32_t {
		if (platform == 3 && encoding == 10) {
			return 0;
		}
		if (platform == 0 && encoding == 4) {
			return 1;
		}
		if (platform == 3 && encoding == 1) {
			return 2;
		}
		if (platform == 0 && encoding <= 3) {
			return 6 - encoding;
		}
		return -1;
	};

	std::int32_t bestPreference = -1;
	for (std::uint16_t i = 0; i < subtableCount; i++) {
		std::uint16_t platform = 0;
		std::uint16_t encoding = 0;
		std::uint32_t offset = 0;
		cmap.read(platform);
		cmap.read(encoding);
		cmap.read(offset);
		if (!cmap.isGood()) {
			return this->fail(FontError::Truncated);
		}

		const std::int32_t rank = preference(platform, encoding);
		if (rank < 0 || (bestPreference >= 0 && rank >= bestPreference)) {
			continue;
		}
		ByteReader subtable = this->getTableReader(this->tables.cmap);
		std::uint16_t format = 0;
		if (!subtable.seek(offset) || !subtable.read(format) || (format != 4 && format != 12)) {
			continue;
		}
		bestPreference = rank;
		this->characterMapOffset = this->tables.cmap.offset + offset;
		this->characterMapFormat = format;
	}

	return bestPreference >= 0 ? true : this->fail(FontError::UnsupportedFormat);
}

bool Font::readCff() {
	if (!this->postScriptOutlines) {
		return true;
	}
	if (!this->cff.read(this->getTableReader(this->tables.cff), this->glyphCount)) {
		return this->fail(FontError::UnsupportedFormat);
	}
	return true;
}

bool Font::readKern() {
	// Only the classic version 0 table, first horizontal format 0 subtable.
	// Anything else simply means no kerning from this table.
	if (this->tables.kern.length == 0) {
		return true;
	}
	ByteReader kern = this->getTableReader(this->tables.kern);
	std::uint16_t version = 0;
	std::uint16_t subtableCount = 0;
	kern.read(version);
	kern.read(subtableCount);
	if (!kern.isGood() || version != 0) {
		return true;
	}

	constexpr std::uint16_t horizontal = 0x0001;
	constexpr std::uint16_t minimumOrCrossStream = 0x0006;
	std::size_t position = 4;
	for (std::uint16_t i = 0; i < subtableCount; i++) {
		ByteReader subtable = kern;
		std::uint16_t subtableVersion = 0;
		std::uint16_t length = 0;
		std::uint16_t coverage = 0;
		std::uint16_t pairCount = 0;
		subtable.seek(position);
		subtable.read(subtableVersion);
		subtable.read(length);
		subtable.read(coverage);
		if (!subtable.isGood()) {
			return true;
		}

		const std::uint16_t format = static_cast<std::uint16_t>(coverage >> 8);
		if (format == 0 && (coverage & horizontal) != 0 && (coverage & minimumOrCrossStream) == 0) {
			subtable.read(pairCount);
			subtable.skip(6);
			if (!subtable.isGood()) {
				return true;
			}
			// The 16-bit length field overflows in some large tables, so the
			// pair count decides how many pairs there are, clamped to the table.
			const std::size_t pairsStart = position + 14;
			const std::size_t available = (kern.getSize() - std::min(pairsStart, kern.getSize())) / 6;
			this->kerningPairsOffset = this->tables.kern.offset + static_cast<std::uint32_t>(pairsStart);
			this->kerningPairCount = static_cast<std::uint16_t>(std::min<std::size_t>(pairCount, available));
			return true;
		}
		if (length < 6) {
			return true;
		}
		position += length;
	}
	return true;
}

void Font::readGpos() {
	// A GPOS table that cannot be read just leaves the kern table in charge.
	if (this->tables.gpos.present) {
		this->gpos.read(this->getTableReader(this->tables.gpos));
	}
}

void Font::readDecorationMetrics() {
	// Optional: OS/2 script and strikeout values, then the post underline.
	ByteReader os2 = this->getTableReader(this->tables.os2);
	std::int16_t subscriptSize = 0;
	std::int16_t subscriptOffset = 0;
	std::int16_t superscriptSize = 0;
	std::int16_t superscriptOffset = 0;
	std::int16_t strikeoutSize = 0;
	std::int16_t strikeoutPosition = 0;
	os2.seek(12);
	os2.read(subscriptSize);
	os2.skip(2);
	os2.read(subscriptOffset);
	os2.skip(2);
	os2.read(superscriptSize);
	os2.skip(2);
	os2.read(superscriptOffset);
	os2.read(strikeoutSize);
	os2.read(strikeoutPosition);
	if (os2.isGood()) {
		this->metrics.subscriptSize = subscriptSize;
		this->metrics.subscriptOffset = subscriptOffset;
		this->metrics.superscriptSize = superscriptSize;
		this->metrics.superscriptOffset = superscriptOffset;
		this->metrics.strikeoutSize = strikeoutSize;
		this->metrics.strikeoutPosition = strikeoutPosition;
	}

	ByteReader post = this->getTableReader(this->tables.post);
	std::int16_t underlinePosition = 0;
	std::int16_t underlineThickness = 0;
	post.seek(8);
	post.read(underlinePosition);
	post.read(underlineThickness);
	if (post.isGood()) {
		this->metrics.underlinePosition = underlinePosition;
		this->metrics.underlineThickness = underlineThickness;
	}
}

void Font::estimateMissingHeights() {
	// Fonts without OS/2 version 2 heights: measure the tops of x and H.
	Outline outline;
	if (this->metrics.xHeight == 0 && this->getOutline(this->getGlyph(U'x'), outline) && !outline.isEmpty()) {
		this->metrics.xHeight = static_cast<std::int16_t>(std::lround(outline.getBounds().getBottom()));
	}
	if (this->metrics.capHeight == 0 && this->getOutline(this->getGlyph(U'H'), outline) && !outline.isEmpty()) {
		this->metrics.capHeight = static_cast<std::int16_t>(std::lround(outline.getBounds().getBottom()));
	}
}

bool Font::readName(const ByteReader& name, std::uint16_t nameId, std::string& text) {
	text.clear();
	ByteReader header = name;
	std::uint16_t format = 0;
	std::uint16_t recordCount = 0;
	std::uint16_t storage = 0;
	header.read(format);
	header.read(recordCount);
	header.read(storage);
	if (!header.isGood()) {
		return false;
	}

	// Lower is better: Windows English, any Windows Unicode name, a Unicode
	// platform name, then Macintosh Roman English.
	auto preference = [](std::uint16_t platform, std::uint16_t encoding, std::uint16_t language) -> std::int32_t {
		if (platform == 3 && (encoding == 1 || encoding == 10)) {
			return language == 0x0409 ? 0 : 1;
		}
		if (platform == 0) {
			return 2;
		}
		if (platform == 1 && encoding == 0 && language == 0) {
			return 3;
		}
		return -1;
	};

	std::int32_t bestPreference = -1;
	ByteReader best;
	bool bestIsUtf16 = false;
	for (std::uint16_t i = 0; i < recordCount; i++) {
		std::uint16_t platform = 0;
		std::uint16_t encoding = 0;
		std::uint16_t language = 0;
		std::uint16_t recordId = 0;
		std::uint16_t length = 0;
		std::uint16_t offset = 0;
		header.read(platform);
		header.read(encoding);
		header.read(language);
		header.read(recordId);
		header.read(length);
		header.read(offset);
		if (!header.isGood()) {
			break;
		}
		const std::int32_t rank = preference(platform, encoding, language);
		if (recordId != nameId || rank < 0 || (bestPreference >= 0 && rank >= bestPreference)) {
			continue;
		}
		ByteReader data;
		if (!name.slice(static_cast<std::size_t>(storage) + offset, length, data)) {
			continue;
		}
		bestPreference = rank;
		best = data;
		bestIsUtf16 = platform != 1;
	}
	return bestPreference >= 0 && Font::decodeName(best, bestIsUtf16, text);
}

bool Font::decodeName(const ByteReader& data, bool utf16, std::string& text) {
	text.clear();
	auto append = [&text](char32_t codePoint) {
		std::array<char, 4> encoded{};
		const std::uint8_t length = Utf8::encode(codePoint, encoded);
		text.append(encoded.data(), length);
	};

	ByteReader reader = data;
	if (!utf16) {
		// Macintosh Roman: ASCII is kept and every other byte becomes U+FFFD.
		std::uint8_t byte = 0;
		while (reader.read(byte)) {
			append(byte < 0x80 ? static_cast<char32_t>(byte) : char32_t{0xFFFD});
		}
		return !text.empty();
	}

	// UTF-16 big-endian; an unpaired surrogate becomes U+FFFD.
	std::uint16_t unit = 0;
	while (reader.read(unit)) {
		if (unit >= 0xD800 && unit < 0xDC00) {
			ByteReader next = reader;
			std::uint16_t low = 0;
			if (next.read(low) && low >= 0xDC00 && low < 0xE000) {
				reader = next;
				append(0x10000 + ((static_cast<char32_t>(unit) - 0xD800) << 10) + (static_cast<char32_t>(low) - 0xDC00));
				continue;
			}
			append(char32_t{0xFFFD});
			continue;
		}
		append(unit >= 0xDC00 && unit < 0xE000 ? char32_t{0xFFFD} : static_cast<char32_t>(unit));
	}
	return !text.empty();
}

std::uint32_t Font::getTag(const char (&name)[5]) {
	return (static_cast<std::uint32_t>(static_cast<std::uint8_t>(name[0])) << 24)
		| (static_cast<std::uint32_t>(static_cast<std::uint8_t>(name[1])) << 16)
		| (static_cast<std::uint32_t>(static_cast<std::uint8_t>(name[2])) << 8)
		| static_cast<std::uint32_t>(static_cast<std::uint8_t>(name[3]));
}

bool Font::isSfntVersion(std::uint32_t version) {
	return version == 0x00010000u || version == Font::getTag("true") || version == Font::getTag("OTTO");
}

ByteReader Font::getTableReader(const FontTable& table) const {
	ByteReader reader;
	ByteReader(this->bytes.data).slice(table.offset, table.length, reader);
	return reader;
}

bool Font::getGlyphData(GlyphId glyph, ByteReader& data) const {
	ByteReader location = this->getTableReader(this->tables.loca);
	std::uint32_t start = 0;
	std::uint32_t end = 0;
	if (this->indexToLocFormat == 0) {
		std::uint16_t shortStart = 0;
		std::uint16_t shortEnd = 0;
		location.seek(static_cast<std::size_t>(glyph) * 2);
		location.read(shortStart);
		location.read(shortEnd);
		start = static_cast<std::uint32_t>(shortStart) * 2;
		end = static_cast<std::uint32_t>(shortEnd) * 2;
	}
	else {
		location.seek(static_cast<std::size_t>(glyph) * 4);
		location.read(start);
		location.read(end);
	}
	if (!location.isGood() || end < start) {
		return false;
	}
	return this->getTableReader(this->tables.glyf).slice(start, end - start, data);
}

bool Font::appendGlyph(GlyphId glyph, const Affine2f& transform, std::uint32_t depth, std::uint32_t& componentBudget, Outline& outline) const {
	constexpr std::uint32_t maximumDepth = 8;
	if (depth > maximumDepth) {
		return false;
	}
	ByteReader data;
	if (!this->getGlyphData(glyph, data)) {
		return false;
	}
	if (data.getSize() == 0) {
		return true;                        // an empty glyph, such as a space
	}

	std::int16_t contourCount = 0;
	data.read(contourCount);
	data.skip(8);                           // bounding box: recomputed from the outline when needed
	if (!data.isGood()) {
		return false;
	}
	if (contourCount >= 0) {
		return this->appendSimpleGlyph(data, contourCount, transform, outline);
	}
	return this->appendCompositeGlyph(data, transform, depth, componentBudget, outline);
}

bool Font::appendSimpleGlyph(ByteReader& data, std::int16_t contourCount, const Affine2f& transform, Outline& outline) const {
	constexpr std::uint8_t onCurveFlag = 0x01;
	constexpr std::uint8_t xShortFlag = 0x02;
	constexpr std::uint8_t yShortFlag = 0x04;
	constexpr std::uint8_t repeatFlag = 0x08;
	constexpr std::uint8_t xSameOrPositiveFlag = 0x10;
	constexpr std::uint8_t ySameOrPositiveFlag = 0x20;

	if (contourCount == 0) {
		return true;
	}

	std::vector<std::uint16_t> contourEnds(static_cast<std::size_t>(contourCount));
	for (std::uint16_t& contourEnd : contourEnds) {
		data.read(contourEnd);
	}
	if (!data.isGood()) {
		return false;
	}
	for (std::size_t i = 1; i < contourEnds.size(); i++) {
		if (contourEnds[i] <= contourEnds[i - 1]) {
			return false;
		}
	}
	const std::size_t pointCount = static_cast<std::size_t>(contourEnds.back()) + 1;

	std::uint16_t instructionLength = 0;
	data.read(instructionLength);
	data.skip(instructionLength);
	if (!data.isGood()) {
		return false;
	}

	std::vector<std::uint8_t> flags(pointCount);
	for (std::size_t i = 0; i < pointCount;) {
		std::uint8_t flag = 0;
		if (!data.read(flag)) {
			return false;
		}
		flags[i++] = flag;
		if ((flag & repeatFlag) != 0) {
			std::uint8_t repeatCount = 0;
			if (!data.read(repeatCount)) {
				return false;
			}
			for (std::uint32_t repeat = 0; repeat < repeatCount && i < pointCount; repeat++) {
				flags[i++] = flag;
			}
		}
	}

	// Coordinates are deltas: a short unsigned byte with a sign flag, a 16-bit
	// value, or nothing when the coordinate repeats the previous one.
	auto readCoordinates = [&data, &flags, pointCount](std::uint8_t shortFlag, std::uint8_t sameOrPositiveFlag, std::vector<std::int32_t>& values) {
		values.resize(pointCount);
		std::int32_t value = 0;
		for (std::size_t i = 0; i < pointCount; i++) {
			const std::uint8_t flag = flags[i];
			if ((flag & shortFlag) != 0) {
				std::uint8_t delta = 0;
				data.read(delta);
				value += (flag & sameOrPositiveFlag) != 0 ? delta : -static_cast<std::int32_t>(delta);
			}
			else if ((flag & sameOrPositiveFlag) == 0) {
				std::int16_t delta = 0;
				data.read(delta);
				value += delta;
			}
			values[i] = value;
		}
		return data.isGood();
	};
	std::vector<std::int32_t> xs;
	std::vector<std::int32_t> ys;
	if (!readCoordinates(xShortFlag, xSameOrPositiveFlag, xs) || !readCoordinates(yShortFlag, ySameOrPositiveFlag, ys)) {
		return false;
	}

	auto point = [&](std::size_t index) {
		return transform.apply(Vector2f{static_cast<float>(xs[index]), static_cast<float>(ys[index])});
	};
	auto isOnCurve = [&flags](std::size_t index) {
		return (flags[index] & onCurveFlag) != 0;
	};
	auto midpoint = [](Vector2f first, Vector2f second) {
		return (first + second) * 0.5f;
	};

	// Contours are emitted the way fontTools draws them, so outlines can be
	// compared exactly: start at the first on-curve point, split runs of
	// off-curve points at their implied midpoints, and let close() draw the
	// final straight edge back to the start.
	std::size_t start = 0;
	for (const std::uint16_t contourEnd : contourEnds) {
		const std::size_t end = contourEnd;
		const std::size_t count = end - start + 1;

		std::size_t firstOnCurve = count;
		for (std::size_t i = 0; i < count; i++) {
			if (isOnCurve(start + i)) {
				firstOnCurve = i;
				break;
			}
		}

		if (firstOnCurve == count) {
			// No on-curve points at all: the contour starts at the midpoint
			// between its last and first off-curve points.
			const Vector2f impliedStart = midpoint(point(end), point(start));
			outline.moveTo(impliedStart);
			for (std::size_t i = 0; i < count; i++) {
				const Vector2f control = point(start + i);
				const Vector2f next = i + 1 < count ? midpoint(control, point(start + i + 1)) : impliedStart;
				outline.quadTo(control, next);
			}
			outline.close();
			start = end + 1;
			continue;
		}

		outline.moveTo(point(start + firstOnCurve));
		std::vector<Vector2f> pending;
		for (std::size_t step = 1; step <= count; step++) {
			const std::size_t index = start + (firstOnCurve + step) % count;
			if (!isOnCurve(index)) {
				pending.push_back(point(index));
				continue;
			}
			const bool backAtStart = step == count;
			if (pending.empty()) {
				if (!backAtStart) {
					outline.lineTo(point(index));
				}
				continue;
			}
			for (std::size_t i = 0; i + 1 < pending.size(); i++) {
				outline.quadTo(pending[i], midpoint(pending[i], pending[i + 1]));
			}
			outline.quadTo(pending.back(), point(index));
			pending.clear();
		}
		outline.close();
		start = end + 1;
	}
	return true;
}

bool Font::appendCompositeGlyph(ByteReader& data, const Affine2f& transform, std::uint32_t depth, std::uint32_t& componentBudget, Outline& outline) const {
	constexpr std::uint16_t argumentsAreWords = 0x0001;
	constexpr std::uint16_t argumentsAreOffsets = 0x0002;
	constexpr std::uint16_t hasScale = 0x0008;
	constexpr std::uint16_t moreComponents = 0x0020;
	constexpr std::uint16_t hasXYScale = 0x0040;
	constexpr std::uint16_t hasTwoByTwo = 0x0080;

	auto fromF2Dot14 = [](std::int16_t value) {
		return static_cast<float>(value) / 16384.0f;
	};

	bool more = true;
	while (more) {
		if (componentBudget == 0) {
			return false;
		}
		componentBudget--;

		std::uint16_t flags = 0;
		std::uint16_t componentGlyph = 0;
		data.read(flags);
		data.read(componentGlyph);

		std::int32_t offsetX = 0;
		std::int32_t offsetY = 0;
		if ((flags & argumentsAreWords) != 0) {
			std::int16_t first = 0;
			std::int16_t second = 0;
			data.read(first);
			data.read(second);
			offsetX = first;
			offsetY = second;
		}
		else {
			std::int8_t first = 0;
			std::int8_t second = 0;
			data.read(first);
			data.read(second);
			offsetX = first;
			offsetY = second;
		}
		if ((flags & argumentsAreOffsets) == 0) {
			return false;                   // components placed by point matching are not supported
		}

		Affine2f component;
		if ((flags & hasScale) != 0) {
			std::int16_t scale = 0;
			data.read(scale);
			component.a = fromF2Dot14(scale);
			component.d = component.a;
		}
		else if ((flags & hasXYScale) != 0) {
			std::int16_t scaleX = 0;
			std::int16_t scaleY = 0;
			data.read(scaleX);
			data.read(scaleY);
			component.a = fromF2Dot14(scaleX);
			component.d = fromF2Dot14(scaleY);
		}
		else if ((flags & hasTwoByTwo) != 0) {
			std::int16_t xx = 0;
			std::int16_t xy = 0;
			std::int16_t yx = 0;
			std::int16_t yy = 0;
			data.read(xx);
			data.read(xy);
			data.read(yx);
			data.read(yy);
			component.a = fromF2Dot14(xx);
			component.b = fromF2Dot14(xy);
			component.c = fromF2Dot14(yx);
			component.d = fromF2Dot14(yy);
		}
		// The offset is applied after the scale (the Microsoft convention).
		component.tx = static_cast<float>(offsetX);
		component.ty = static_cast<float>(offsetY);

		if (!data.isGood() || componentGlyph >= this->glyphCount) {
			return false;
		}
		if (!this->appendGlyph(componentGlyph, component.then(transform), depth + 1, componentBudget, outline)) {
			return false;
		}
		more = (flags & moreComponents) != 0;
	}
	return true;                            // instructions after the last component are not needed
}

GlyphId Font::lookupFormat4(char32_t codePoint) const {
	if (codePoint > 0xFFFF) {
		return 0;
	}
	const std::uint32_t tableEnd = this->tables.cmap.offset + this->tables.cmap.length;
	ByteReader subtable;
	if (!ByteReader(this->bytes.data).slice(this->characterMapOffset, tableEnd - this->characterMapOffset, subtable)) {
		return 0;
	}
	auto readAt = [&subtable](std::size_t offset, std::uint16_t& value) {
		ByteReader reader = subtable;
		return reader.seek(offset) && reader.read(value);
	};

	std::uint16_t segmentCountTimesTwo = 0;
	if (!readAt(6, segmentCountTimesTwo) || segmentCountTimesTwo < 2) {
		return 0;
	}
	const std::size_t segmentCount = segmentCountTimesTwo / 2;
	const std::size_t endCodes = 14;
	const std::size_t startCodes = 16 + segmentCountTimesTwo;
	const std::size_t deltas = 16 + 2 * static_cast<std::size_t>(segmentCountTimesTwo);
	const std::size_t rangeOffsets = 16 + 3 * static_cast<std::size_t>(segmentCountTimesTwo);

	// Segments are sorted by end code: find the first that ends at or after the code point.
	std::size_t low = 0;
	std::size_t high = segmentCount;
	while (low < high) {
		const std::size_t middle = low + (high - low) / 2;
		std::uint16_t endCode = 0;
		if (!readAt(endCodes + middle * 2, endCode)) {
			return 0;
		}
		if (endCode < codePoint) {
			low = middle + 1;
		}
		else {
			high = middle;
		}
	}
	if (low == segmentCount) {
		return 0;
	}

	std::uint16_t startCode = 0;
	std::uint16_t delta = 0;
	std::uint16_t rangeOffset = 0;
	if (!readAt(startCodes + low * 2, startCode) || !readAt(deltas + low * 2, delta)
		|| !readAt(rangeOffsets + low * 2, rangeOffset) || codePoint < startCode) {
		return 0;
	}

	std::uint32_t glyph = 0;
	if (rangeOffset == 0) {
		glyph = (static_cast<std::uint32_t>(codePoint) + delta) & 0xFFFFu;
	}
	else {
		// The range offset counts bytes from its own position in the array.
		const std::size_t position = rangeOffsets + low * 2 + rangeOffset + (static_cast<std::size_t>(codePoint) - startCode) * 2;
		std::uint16_t arrayGlyph = 0;
		if (!readAt(position, arrayGlyph) || arrayGlyph == 0) {
			return 0;
		}
		glyph = (static_cast<std::uint32_t>(arrayGlyph) + delta) & 0xFFFFu;
	}
	return glyph < this->glyphCount ? static_cast<GlyphId>(glyph) : GlyphId(0);
}

GlyphId Font::lookupFormat12(char32_t codePoint) const {
	const std::uint32_t tableEnd = this->tables.cmap.offset + this->tables.cmap.length;
	ByteReader subtable;
	if (!ByteReader(this->bytes.data).slice(this->characterMapOffset, tableEnd - this->characterMapOffset, subtable)) {
		return 0;
	}
	auto readAt = [&subtable](std::size_t offset, std::uint32_t& value) {
		ByteReader reader = subtable;
		return reader.seek(offset) && reader.read(value);
	};

	std::uint32_t groupCount = 0;
	if (!readAt(12, groupCount)) {
		return 0;
	}
	const std::size_t groupsStart = 16;
	const std::size_t available = subtable.getSize() > groupsStart ? (subtable.getSize() - groupsStart) / 12 : 0;
	const std::size_t groups = std::min<std::size_t>(groupCount, available);

	// Groups are sorted by start: find the first that ends at or after the code point.
	std::size_t low = 0;
	std::size_t high = groups;
	while (low < high) {
		const std::size_t middle = low + (high - low) / 2;
		std::uint32_t endCode = 0;
		if (!readAt(groupsStart + middle * 12 + 4, endCode)) {
			return 0;
		}
		if (endCode < codePoint) {
			low = middle + 1;
		}
		else {
			high = middle;
		}
	}
	if (low == groups) {
		return 0;
	}

	std::uint32_t startCode = 0;
	std::uint32_t startGlyph = 0;
	if (!readAt(groupsStart + low * 12, startCode) || !readAt(groupsStart + low * 12 + 8, startGlyph) || codePoint < startCode) {
		return 0;
	}
	const std::uint64_t glyph = static_cast<std::uint64_t>(startGlyph) + (codePoint - startCode);
	return glyph < this->glyphCount ? static_cast<GlyphId>(glyph) : GlyphId(0);
}

}
