#include <litefont/Font.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <charconv>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace litefont::tests {

/**
 * Compares Font against reference dumps written by tools/oracle/dump_font.py
 * (fontTools) for the fonts in fonts/. The dumps live in out/reference/ and are
 * not part of the repository; without them the test only leaves a note.
 */
class FontOracleTests {
	static std::vector<std::string> splitTokens(const std::string& text) {
		std::vector<std::string> tokens;
		std::istringstream stream(text);
		std::string token;
		while (stream >> token) {
			tokens.push_back(token);
		}
		return tokens;
	}

	static bool tokensMatch(const std::vector<std::string>& expected, const std::vector<std::string>& actual, std::size_t firstToken) {
		if (expected.size() - firstToken != actual.size()) {
			return false;
		}
		for (std::size_t i = 0; i < actual.size(); i++) {
			const std::string& left = expected[firstToken + i];
			const std::string& right = actual[i];
			const bool command = !left.empty() && (left[0] >= 'A' && left[0] <= 'Z');
			if (command) {
				if (left != right) {
					return false;
				}
				continue;
			}
			const double a = std::stod(left);
			const double b = std::stod(right);
			if (std::fabs(a - b) > 0.01) {
				return false;
			}
		}
		return true;
	}

	static void compareFont(test::Context& context, const std::filesystem::path& referencePath) {
		std::ifstream stream(referencePath);
		std::string line;
		Font font;
		const std::string name = referencePath.stem().string();
		std::unordered_map<std::uint32_t, GlyphId> characterMap;
		std::unordered_map<std::uint32_t, std::int32_t> kerning;
		std::vector<GlyphId> kerningSample;
		std::int32_t glyphsCompared = 0;
		std::int32_t outlineMismatches = 0;
		std::int32_t metricMismatches = 0;
		std::int32_t kerningPairs = 0;
		std::int32_t kerningMismatches = 0;
		constexpr std::int32_t maximumReported = 5;

		while (std::getline(stream, line)) {
			const std::vector<std::string> tokens = splitTokens(line);
			if (tokens.empty()) {
				continue;
			}
			const std::string& kind = tokens[0];

			if (kind == "font" && tokens.size() >= 3) {
				// font <face> <path>: the path is the rest of the line.
				const std::uint32_t face = static_cast<std::uint32_t>(std::stoul(tokens[1]));
				// The dump's path, or the same file in the --fonts folder; neither: skipped.
				std::filesystem::path path = line.substr(line.find(' ', 5) + 1);
				if (!File::exists(path)) {
					path = test::Context::getFontFolder() / path.filename();
				}
				if (!File::exists(path)) {
					context.note(std::format("{}: {} is not available; skipped", name, File::fromPath(path.filename())));
					return;
				}
				const bool loaded = font.load(path, face);
				context.check(loaded, std::format("{}: the font loads", name));
				if (!loaded) {
					return;
				}
				context.checkEqual(font.getFace(), face);
			}
			else if (kind == "names") {
				std::vector<std::string> fields;
				std::istringstream fieldStream(line.substr(6));
				std::string field;
				while (std::getline(fieldStream, field, '\t')) {
					fields.push_back(field);
				}
				fields.resize(3);
				const FontNames& names = font.getNames();
				context.check(names.family == fields[0], std::format("{}: family \"{}\" expected \"{}\"", name, names.family, fields[0]));
				context.check(names.subfamily == fields[1], std::format("{}: subfamily \"{}\" expected \"{}\"", name, names.subfamily, fields[1]));
				context.check(names.fullName == fields[2], std::format("{}: full name \"{}\" expected \"{}\"", name, names.fullName, fields[2]));
			}
			else if (kind == "metrics") {
				const FontMetrics& metrics = font.getMetrics();
				context.checkEqual(static_cast<std::int32_t>(metrics.unitsPerEm), std::stoi(tokens[1]));
				context.checkEqual(static_cast<std::int32_t>(metrics.ascender), std::stoi(tokens[2]));
				context.checkEqual(static_cast<std::int32_t>(metrics.descender), std::stoi(tokens[3]));
				context.checkEqual(static_cast<std::int32_t>(metrics.lineGap), std::stoi(tokens[4]));
				if (std::stoi(tokens[5]) >= 0) {
					context.checkEqual(static_cast<std::int32_t>(metrics.xHeight), std::stoi(tokens[5]));
					context.checkEqual(static_cast<std::int32_t>(metrics.capHeight), std::stoi(tokens[6]));
				}
			}
			else if (kind == "traits") {
				const FontTraits& traits = font.getTraits();
				context.checkEqual(static_cast<std::int32_t>(traits.weight), std::stoi(tokens[1]));
				context.checkEqual(traits.bold, tokens[2] == "1");
				context.checkEqual(traits.italic, tokens[3] == "1");
				context.check(std::fabs(traits.italicAngle - std::stof(tokens[4])) < 1e-3f, std::format("{}: italic angle", name));
			}
			else if (kind == "decoration" && tokens.size() == 9) {
				const FontMetrics& metrics = font.getMetrics();
				const std::int32_t actual[] = {metrics.subscriptSize, metrics.subscriptOffset, metrics.superscriptSize, metrics.superscriptOffset,
					metrics.strikeoutSize, metrics.strikeoutPosition, metrics.underlinePosition, metrics.underlineThickness};
				for (std::size_t i = 0; i < 8; i++) {
					context.check(actual[i] == std::stoi(tokens[i + 1]), std::format("{}: decoration value {} is {} expected {}", name, i, actual[i], tokens[i + 1]));
				}
			}
			else if (kind == "glyphs") {
				context.checkEqual(static_cast<std::int32_t>(font.getGlyphCount()), std::stoi(tokens[1]));
			}
			else if (kind == "cmap") {
				characterMap[static_cast<std::uint32_t>(std::stoul(tokens[1]))] = static_cast<GlyphId>(std::stoul(tokens[2]));
			}
			else if (kind == "hmtx") {
				const GlyphId glyph = static_cast<GlyphId>(std::stoul(tokens[1]));
				const GlyphMetrics metrics = font.getGlyphMetrics(glyph);
				const bool same = metrics.advance == std::stoi(tokens[2]) && metrics.leftSideBearing == std::stoi(tokens[3]);
				if (!same && ++metricMismatches <= maximumReported) {
					context.check(false, std::format("{}: glyph {} metrics {} {} expected {} {}", name, glyph,
						metrics.advance, metrics.leftSideBearing, tokens[2], tokens[3]));
				}
			}
			else if (kind == "kernsample") {
				for (std::size_t i = 1; i < tokens.size(); i++) {
					kerningSample.push_back(static_cast<GlyphId>(std::stoul(tokens[i])));
				}
			}
			else if (kind == "kern") {
				const std::uint32_t key = (static_cast<std::uint32_t>(std::stoul(tokens[1])) << 16) | static_cast<std::uint32_t>(std::stoul(tokens[2]));
				kerning[key] = std::stoi(tokens[3]);
			}
			else if (kind == "outline") {
				glyphsCompared++;
				const GlyphId glyph = static_cast<GlyphId>(std::stoul(tokens[1]));
				Outline outline;
				const bool read = font.getOutline(glyph, outline);
				const bool unsupported = tokens.size() == 3 && tokens[2] == "unsupported";
				const bool same = unsupported ? !read : read && tokensMatch(tokens, splitTokens(outline.toSvgPath()), 2);
				if (!same && ++outlineMismatches <= maximumReported) {
					context.check(false, std::format("{}: glyph {} outline differs\n         expected: {}\n         actual:   {}",
						name, glyph, line.substr(0, 160), outline.toSvgPath().substr(0, 160)));
				}
			}
		}

		// GPOS fonts: every sampled pair, unlisted ones kerning by 0. Otherwise every listed pair.
		auto compareKerning = [&](GlyphId left, GlyphId right, std::int32_t expected) {
			kerningPairs++;
			const std::int16_t value = font.getKerning(left, right);
			if (value != expected && ++kerningMismatches <= maximumReported) {
				context.check(false, std::format("{}: kerning {} {} is {} expected {}", name, left, right, value, expected));
			}
		};
		if (!kerningSample.empty()) {
			for (const GlyphId left : kerningSample) {
				for (const GlyphId right : kerningSample) {
					const auto found = kerning.find((static_cast<std::uint32_t>(left) << 16) | right);
					compareKerning(left, right, found == kerning.end() ? 0 : found->second);
				}
			}
		}
		else {
			for (const auto& [key, value] : kerning) {
				compareKerning(static_cast<GlyphId>(key >> 16), static_cast<GlyphId>(key & 0xFFFFu), value);
			}
		}

		// Every Basic Multilingual Plane code point: mapped ones give their glyph, the rest 0.
		std::int32_t characterMismatches = 0;
		for (std::uint32_t codePoint = 0; codePoint <= 0xFFFF; codePoint++) {
			const auto found = characterMap.find(codePoint);
			const GlyphId expected = found == characterMap.end() ? GlyphId(0) : found->second;
			const GlyphId actual = font.getGlyph(static_cast<char32_t>(codePoint));
			if (actual != expected && ++characterMismatches <= maximumReported) {
				context.check(false, std::format("{}: U+{:04X} maps to {} expected {}", name, codePoint, actual, expected));
			}
		}
		for (const auto& [codePoint, glyph] : characterMap) {
			if (codePoint > 0xFFFF && font.getGlyph(static_cast<char32_t>(codePoint)) != glyph && ++characterMismatches <= maximumReported) {
				context.check(false, std::format("{}: U+{:X} maps wrongly", name, codePoint));
			}
		}

		context.note(std::format("{}: {} glyph outlines and metrics, {} mapped code points, {} kerning pairs compared; "
			"mismatches: {} outlines, {} metrics, {} code points, {} kerning",
			name, glyphsCompared, characterMap.size(), kerningPairs, outlineMismatches, metricMismatches,
			characterMismatches, kerningMismatches));
	}

	static inline test::Test matchesFontTools{"Font: matches fontTools on the local fonts (oracle)", [](test::Context& context) {
		const std::filesystem::path folder = "out/reference";
		std::error_code error;
		if (!std::filesystem::is_directory(folder, error)) {
			context.note("no reference dumps in out/reference; run tools/oracle/dump_font.py first");
			return;
		}
		for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
			if (entry.path().extension() == ".txt") {
				compareFont(context, entry.path());
			}
		}
	}};
};

}
