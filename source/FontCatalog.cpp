#include <litefont/FontCatalog.h>

#include <algorithm>
#include <array>
#include <limits>
#include <system_error>

namespace litefont {

std::size_t FontCatalog::scan(const std::filesystem::path& folder) {
	std::error_code error;
	std::vector<std::filesystem::path> paths;
	for (std::filesystem::directory_iterator it(folder, error), end; !error && it != end; it.increment(error)) {
		if (it->is_regular_file(error) && FontCatalog::isFontFile(it->path())) {
			paths.push_back(it->path());
		}
	}
	// Sorted, so the first face found for a style does not depend on the file system's order.
	std::sort(paths.begin(), paths.end());
	std::size_t added = 0;
	for (const std::filesystem::path& path : paths) {
		added += this->add(path);
	}
	return added;
}

std::size_t FontCatalog::add(const std::filesystem::path& path) {
	// Native strings: comparing paths element by element is slow over hundreds of files.
	const bool known = std::any_of(this->entries.begin(), this->entries.end(), [&path](const FontCatalogEntry& entry) {
		return entry.path.native() == path.native();
	});
	std::vector<FontCatalogEntry> found;
	if (known || !FontCatalog::readEntries(path, found)) {
		return 0;
	}
	this->entries.insert(this->entries.end(), found.begin(), found.end());
	this->loaded.resize(this->entries.size(), nullptr);
	return found.size();
}

const std::vector<FontCatalogEntry>& FontCatalog::getEntries() const {
	return this->entries;
}

std::vector<std::string> FontCatalog::getFamilyNames() const {
	std::vector<std::string> names;
	for (const FontCatalogEntry& entry : this->entries) {
		const bool listed = std::any_of(names.begin(), names.end(), [&entry](const std::string& name) {
			return FontCatalog::isSameName(name, entry.names.family);
		});
		if (!listed) {
			names.push_back(entry.names.family);
		}
	}
	std::sort(names.begin(), names.end());
	return names;
}

bool FontCatalog::findFamily(std::string_view name, FontFamily& family) {
	family = FontFamily();
	constexpr std::size_t none = std::numeric_limits<std::size_t>::max();
	std::array<std::size_t, 4> chosen{none, none, none, none};
	for (std::size_t i = 0; i < this->entries.size(); i++) {
		const FontCatalogEntry& entry = this->entries[i];
		if (!FontCatalog::isSameName(entry.names.family, name)) {
			continue;
		}
		const FontStyle style = FontFamily::getStyle(entry.traits.bold, entry.traits.italic);
		if (chosen[static_cast<std::size_t>(style)] == none) {
			chosen[static_cast<std::size_t>(style)] = i;
		}
	}

	bool found = false;
	for (const FontStyle style : {FontStyle::Regular, FontStyle::Bold, FontStyle::Italic, FontStyle::BoldItalic}) {
		const std::size_t entry = chosen[static_cast<std::size_t>(style)];
		const Font* font = entry == none ? nullptr : this->load(entry);
		if (font != nullptr) {
			family.setFace(style, *font);
			found = true;
		}
	}
	return found;
}

const Font* FontCatalog::load(std::size_t entry) {
	if (entry >= this->entries.size()) {
		return nullptr;
	}
	if (this->loaded[entry] == nullptr) {
		Font font;
		if (!font.load(this->entries[entry].path, this->entries[entry].face)) {
			return nullptr;
		}
		this->fonts.push_back(std::move(font));
		this->loaded[entry] = &this->fonts.back();
	}
	return this->loaded[entry];
}

bool FontCatalog::readEntries(const std::filesystem::path& path, std::vector<FontCatalogEntry>& entries) {
	entries.clear();
	// The directories of every face sit near the start of the file; a file
	// that keeps one further in is read whole.
	constexpr std::size_t headerSize = 65536;
	Bytes header;
	if (!File::readRange(path, 0, headerSize, header)) {
		return false;
	}
	const std::uint32_t faceCount = Font::getFaceCount(ByteReader(header.data));
	bool wholeFile = header.data.size() < headerSize;
	for (std::uint32_t face = 0; face < faceCount; face++) {
		FontTables tables;
		FontError error = Font::readTableDirectory(ByteReader(header.data), face, tables);
		if (error == FontError::Truncated && !wholeFile) {
			wholeFile = File::read(path, header);
			error = Font::readTableDirectory(ByteReader(header.data), face, tables);
		}
		const bool outlines = (tables.glyf.present && tables.loca.present) || tables.cff.present;
		const bool loadable = tables.head.present && tables.maxp.present && tables.hhea.present
			&& tables.hmtx.present && tables.cmap.present && outlines;
		if (error != FontError::None || !loadable) {
			continue;
		}

		Bytes head;
		Bytes os2;
		Bytes post;
		Bytes name;
		if (!FontCatalog::readTable(path, tables.head, head) || !FontCatalog::readTable(path, tables.os2, os2)
			|| !FontCatalog::readTable(path, tables.post, post) || !FontCatalog::readTable(path, tables.name, name)) {
			continue;
		}
		FontCatalogEntry entry;
		entry.path = path;
		entry.face = face;
		Font::readTraits(ByteReader(head.data), ByteReader(os2.data), ByteReader(post.data), entry.traits);
		if (Font::readNames(ByteReader(name.data), entry.names)) {
			entries.push_back(std::move(entry));
		}
	}
	return !entries.empty();
}

bool FontCatalog::isSameName(std::string_view left, std::string_view right) {
	auto lower = [](char value) {
		return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
	};
	return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [&lower](char a, char b) {
		return lower(a) == lower(b);
	});
}

bool FontCatalog::readTable(const std::filesystem::path& path, const FontTable& table, Bytes& bytes) {
	bytes.data.clear();
	if (!table.present || table.length == 0) {
		return true;
	}
	return File::readRange(path, table.offset, table.length, bytes) && bytes.data.size() == table.length;
}

bool FontCatalog::isFontFile(const std::filesystem::path& path) {
	std::string extension = File::fromPath(path.extension());
	for (char& value : extension) {
		value = value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
	}
	return extension == ".ttf" || extension == ".otf" || extension == ".ttc" || extension == ".otc";
}

}
