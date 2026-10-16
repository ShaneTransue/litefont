#pragma once

#include <litefont/Bytes.h>
#include <string>
#include <string_view>

#include <filesystem>

namespace litefont {

/**
 * \class File File.h <litefont/File.h>
 * @addtogroup IO
 *
 * File - Reads a whole file, or a range of it, into Bytes and writes Bytes to
 * a file, using std::filesystem and file streams. A range that runs past the
 * end of the file is cut short there, so reading a header of fixed size from
 * a smaller file still succeeds with the bytes that exist. Structure inside the bytes is the job of
 * ByteReader and ByteWriter. Nothing here throws; every call returns false
 * when the file cannot be read or written.
 *
 * toPath and fromPath convert between UTF-8 Strings and paths. Use them
 * instead of constructing a path from a std::string: on Windows that reads the
 * std::string in the ANSI code page and garbles names outside ASCII.
 */
class File {
public:
	static bool read(const std::filesystem::path& path, Bytes& bytes);
	static bool readRange(const std::filesystem::path& path, std::uint64_t offset, std::size_t size, Bytes& bytes);
	static bool write(const std::filesystem::path& path, const Bytes& bytes);
	static bool exists(const std::filesystem::path& path);
	static std::filesystem::path toPath(std::string_view utf8);
	static std::string fromPath(const std::filesystem::path& path);
};

}
