#include <litefont/File.h>
#include <litefont/Utf8.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <filesystem>
#include <system_error>

namespace litefont::tests {

class FileTests {
	static std::filesystem::path makeTemporaryPath(const char* name) {
		std::error_code error;
		return std::filesystem::temp_directory_path(error) / name;
	}

	static inline test::Test roundTrips{"File: write then read returns the same bytes", [](test::Context& context) {
		const std::filesystem::path path = makeTemporaryPath("px_file_round_trip.bin");
		const Bytes written{{0, 1, 2, 250, 251, 252}};

		context.check(File::write(path, written), "write succeeds");
		context.check(File::exists(path), "the file exists afterwards");

		Bytes read;
		context.check(File::read(path, read), "read succeeds");
		context.check(read.data == written.data, "same bytes");

		std::error_code error;
		std::filesystem::remove(path, error);
	}};

	static inline test::Test ranges{"File: readRange reads part of a file and stops at its end", [](test::Context& context) {
		const std::filesystem::path path = makeTemporaryPath("px_file_range.bin");
		context.check(File::write(path, Bytes{{10, 11, 12, 13, 14, 15}}), "write succeeds");

		Bytes part;
		context.check(File::readRange(path, 2, 3, part), "a range inside the file reads");
		context.check(part.data == std::vector<std::uint8_t>{12, 13, 14}, "with its bytes");
		context.check(File::readRange(path, 4, 100, part), "a range past the end reads what exists");
		context.check(part.data == std::vector<std::uint8_t>{14, 15}, "the last two bytes");
		context.check(File::readRange(path, 6, 4, part) && part.data.empty(), "a range at the end is empty");
		context.check(!File::readRange(path, 7, 1, part), "a range starting past the end fails");
		context.check(!File::readRange(makeTemporaryPath("px_file_range_missing.bin"), 0, 4, part), "a missing file fails");

		std::error_code error;
		std::filesystem::remove(path, error);
	}};

	static inline test::Test emptyFiles{"File: empty files read as empty bytes", [](test::Context& context) {
		const std::filesystem::path path = makeTemporaryPath("px_file_empty.bin");
		context.check(File::write(path, Bytes{}), "write an empty file");
		Bytes read{{9}};
		context.check(File::read(path, read), "read it");
		context.check(read.data.empty(), "and get nothing");

		std::error_code error;
		std::filesystem::remove(path, error);
	}};

	static inline test::Test missingFiles{"File: missing files and folders fail without throwing", [](test::Context& context) {
		const std::filesystem::path missing = makeTemporaryPath("px_file_does_not_exist.bin");
		Bytes read{{1, 2, 3}};
		context.check(!File::exists(missing), "missing file does not exist");
		context.check(!File::read(missing, read), "reading it fails");
		context.check(read.data.empty(), "and clears the bytes");

		std::error_code error;
		const std::filesystem::path folder = std::filesystem::temp_directory_path(error);
		context.check(!File::exists(folder), "a folder is not a file");
		context.check(!File::read(folder, read), "reading a folder fails");
		context.check(!File::write(folder, Bytes{{1}}), "writing to a folder fails");
	}};

	static inline test::Test utf8Paths{"File: toPath keeps names outside ASCII intact", [](test::Context& context) {
		// Latin, Cyrillic and Greek letters, written as UTF-8 bytes.
		const std::string name = Utf8::fromU8(u8"px_ünïcødé_файл_Ω.bin");
		std::error_code error;
		const std::filesystem::path path = std::filesystem::temp_directory_path(error) / File::toPath(name);
		context.check(path.filename().u8string() == Utf8::toU8(name), "the path holds the same name");
		context.check(File::write(path, Bytes{{7, 8}}), "write");
		Bytes read;
		context.check(File::read(path, read) && read.data.size() == 2, "read back");
		context.checkEqual(File::fromPath(path.filename()), name);
		std::filesystem::remove(path, error);
	}};
};

}
