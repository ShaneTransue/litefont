#include "TestFontBuilder.h"

#include <litefont/FontCatalog.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <chrono>
#include <filesystem>
#include <format>
#include <system_error>

namespace litefont::tests {

/**
 * The catalog over a folder of built fonts, where every answer is known, and
 * over the installed Windows fonts.
 */
class FontCatalogTests {
	static Bytes makeFace(const std::string& family, const std::string& subfamily, bool bold, bool italic) {
		TestFontBuilder builder;
		builder.setStyle(bold ? 700 : 400, bold, italic, italic ? -12 : 0);
		TestGlyph glyph;
		glyph.contours = {{{0, 0, true}, {0, 500, true}, {400, 500, true}, {400, 0, true}}};
		builder.map(U'a', builder.addGlyph(glyph));
		builder.setNames(family, subfamily, family + " " + subfamily);
		return builder.build();
	}

	static std::filesystem::path makeFolder() {
		std::error_code error;
		const std::filesystem::path folder = std::filesystem::temp_directory_path(error) / "px_font_catalog";
		std::filesystem::remove_all(folder, error);
		std::filesystem::create_directories(folder, error);
		File::write(folder / "alpha.ttf", makeFace("Alpha", "Regular", false, false));
		File::write(folder / "alpha-bold.TTF", makeFace("Alpha", "Bold", true, false));
		File::write(folder / "alpha-italic.ttf", makeFace("Alpha", "Italic", false, true));
		File::write(folder / "zeta-copy.ttf", makeFace("ALPHA", "Regular", false, false));
		File::write(folder / "pair.ttc", TestFontBuilder::buildCollection({
			makeFace("Beta", "Regular", false, false), makeFace("Gamma", "Bold Italic", true, true)}));
		File::write(folder / "broken.otf", Bytes{{'O', 'T', 'T', 'O', 0, 0}});
		File::write(folder / "notes.txt", makeFace("Delta", "Regular", false, false));
		TestFontBuilder nameless;
		nameless.addGlyph(TestGlyph{});
		File::write(folder / "nameless.ttf", nameless.build());
		return folder;
	}

	static inline test::Test scansFolder{"FontCatalog: scans a folder, reading each face's names and traits", [](test::Context& context) {
		const std::filesystem::path folder = makeFolder();
		FontCatalog catalog;
		context.checkEqual(catalog.scan(folder), std::size_t{6});
		context.checkEqual(catalog.add(folder / "alpha.ttf"), std::size_t{0});    // already known
		context.checkEqual(catalog.getEntries().size(), std::size_t{6});

		const std::vector<std::string> names = catalog.getFamilyNames();
		context.check(names == std::vector<std::string>{"Alpha", "Beta", "Gamma"}, "three families, sorted, case folded");
		bool sawGamma = false;
		for (const FontCatalogEntry& entry : catalog.getEntries()) {
			if (entry.names.family == "Gamma") {
				sawGamma = true;
				context.checkEqual(entry.face, std::uint32_t{1});
				context.check(entry.traits.bold && entry.traits.italic, "Gamma is bold italic");
				context.checkEqual(entry.names.fullName, std::string("Gamma Bold Italic"));
			}
		}
		context.check(sawGamma, "the second face of the collection is listed");

		std::error_code error;
		std::filesystem::remove_all(folder, error);
	}};

	static inline test::Test findsFamilies{"FontCatalog: finds a family by name and loads its faces once", [](test::Context& context) {
		const std::filesystem::path folder = makeFolder();
		FontCatalog catalog;
		catalog.scan(folder);

		FontFamily alpha;
		context.check(catalog.findFamily("alpha", alpha), "the name matches without regard to case");
		context.check(alpha.getFace(FontStyle::Regular) != nullptr, "regular");
		context.check(alpha.getFace(FontStyle::Bold) != nullptr, "bold");
		context.check(alpha.getFace(FontStyle::Italic) != nullptr, "italic");
		context.check(alpha.getFace(FontStyle::BoldItalic) == nullptr, "no bold italic face");
		context.checkEqual(alpha.getFace(FontStyle::Regular)->getNames().family, std::string("Alpha"));    // alpha.ttf comes before zeta-copy.ttf
		context.check(alpha.resolve(FontStyle::BoldItalic) == FontFace{alpha.getFace(FontStyle::Bold), false, true}, "bold italic slants the bold face");

		FontFamily again;
		catalog.findFamily("ALPHA", again);
		context.check(again.getFace(FontStyle::Bold) == alpha.getFace(FontStyle::Bold), "faces load once");

		FontFamily gamma;
		context.check(catalog.findFamily("Gamma", gamma), "a face inside a collection");
		context.check(gamma.getFace(FontStyle::BoldItalic) != nullptr && gamma.getFace(FontStyle::BoldItalic)->getFace() == 1, "face 1 of pair.ttc");

		FontFamily missing;
		context.check(!catalog.findFamily("Delta", missing), "a .txt file is not scanned");
		context.check(missing.getRegular() == nullptr, "and the family stays empty");
		context.check(catalog.load(99) == nullptr, "no entry 99");

		std::error_code error;
		std::filesystem::remove_all(folder, error);
	}};

	static inline test::Test localFonts{"FontCatalog: finds Segoe UI and Microsoft YaHei in the local fonts folder", [](test::Context& context) {
		const std::filesystem::path folder = test::Context::getFontFolder();
		std::error_code error;
		if (!std::filesystem::is_directory(folder, error)) {
			context.note("no fonts folder; skipped");
			return;
		}
		const auto start = std::chrono::steady_clock::now();
		FontCatalog catalog;
		const std::size_t faces = catalog.scan(folder);
		const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
		context.note(std::format("{} faces in {} families scanned in {} us", faces, catalog.getFamilyNames().size(), microseconds));

		FontFamily segoe;
		if (catalog.findFamily("Segoe UI", segoe)) {
			for (const FontStyle style : {FontStyle::Regular, FontStyle::Bold, FontStyle::Italic, FontStyle::BoldItalic}) {
				const Font* face = segoe.getFace(style);
				context.check(face != nullptr && face->getNames().family == "Segoe UI", "each of Segoe UI's four faces");
			}
			context.check(segoe.getFace(FontStyle::Bold)->getTraits().bold, "the bold face is bold");
		}
		FontFamily yahei;
		if (catalog.findFamily("Microsoft YaHei", yahei)) {
			context.check(yahei.getRegular()->getGlyph(0x4E2D) != 0, "Microsoft YaHei has U+4E2D");
			context.checkEqual(yahei.getRegular()->getFace(), std::uint32_t{0});
		}
	}};
};

}
