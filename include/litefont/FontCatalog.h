#pragma once

#include <litefont/FontFamily.h>

#include <deque>
#include <filesystem>
#include <vector>

namespace litefont {

/**
 * \struct FontCatalogEntry FontCatalog.h <litefont/FontCatalog.h>
 * @addtogroup Text
 *
 * FontCatalogEntry - One face a catalog knows: its file, its index inside
 * the file (0 unless the file is a collection), its names and its traits.
 */
struct FontCatalogEntry {
	std::filesystem::path path;
	std::uint32_t face = 0;
	FontNames names;
	FontTraits traits;
};

/**
 * \class FontCatalog FontCatalog.h <litefont/FontCatalog.h>
 * @addtogroup Text
 *
 * FontCatalog - Knows the fonts in folders without loading them, and loads
 * a family by name. scan() looks at every .ttf, .otf, .ttc and .otc file in a
 * folder (not its subfolders) and reads only what describes each face: the
 * table directory and the head, OS/2, post and name tables, a few small
 * reads per face. Faces that could not load (no outlines, no character map)
 * are left out. A file already in the catalog is not added again.
 *
 * findFamily() matches the family name (name ID 1) without regard to ASCII
 * case and fills a FontFamily with the family's regular, bold, italic and
 * bold italic faces, each placed by its own traits; when two faces claim the
 * same style, the first one found wins. There is no matching by weight: a
 * semibold face usually names a family of its own ("Segoe UI Semibold").
 * Faces load on first use and stay loaded, so the families the catalog
 * fills point into it and must not outlive it.
 */
class FontCatalog {
public:
	FontCatalog() = default;
	FontCatalog(const FontCatalog&) = delete;
	FontCatalog& operator=(const FontCatalog&) = delete;
	FontCatalog(FontCatalog&&) = default;
	FontCatalog& operator=(FontCatalog&&) = default;

	std::size_t scan(const std::filesystem::path& folder);
	std::size_t add(const std::filesystem::path& path);
	const std::vector<FontCatalogEntry>& getEntries() const;
	std::vector<std::string> getFamilyNames() const;
	bool findFamily(std::string_view name, FontFamily& family);
	const Font* load(std::size_t entry);

	static bool readEntries(const std::filesystem::path& path, std::vector<FontCatalogEntry>& entries);
	static bool isSameName(std::string_view left, std::string_view right);

private:
	static bool readTable(const std::filesystem::path& path, const FontTable& table, Bytes& bytes);
	static bool isFontFile(const std::filesystem::path& path);

	std::vector<FontCatalogEntry> entries;
	std::vector<const Font*> loaded;
	std::deque<Font> fonts;
};

}
