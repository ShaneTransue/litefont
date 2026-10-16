#include <litefont/File.h>

#include <fstream>
#include <system_error>

namespace litefont {

bool File::read(const std::filesystem::path& path, Bytes& bytes) {
	bytes.data.clear();

	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error)) {
		return false;
	}
	const std::uintmax_t size = std::filesystem::file_size(path, error);
	if (error) {
		return false;
	}

	std::ifstream stream(path, std::ios::binary);
	if (!stream) {
		return false;
	}
	bytes.data.resize(static_cast<std::size_t>(size));
	if (size > 0) {
		stream.read(reinterpret_cast<char*>(bytes.data.data()), static_cast<std::streamsize>(size));
		if (static_cast<std::uintmax_t>(stream.gcount()) != size) {
			bytes.data.clear();
			return false;
		}
	}
	return true;
}

bool File::readRange(const std::filesystem::path& path, std::uint64_t offset, std::size_t size, Bytes& bytes) {
	bytes.data.clear();

	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error)) {
		return false;
	}
	const std::uintmax_t fileSize = std::filesystem::file_size(path, error);
	if (error || offset > fileSize) {
		return false;
	}
	const std::uintmax_t available = fileSize - offset;
	const std::size_t count = available < size ? static_cast<std::size_t>(available) : size;

	std::ifstream stream(path, std::ios::binary);
	if (!stream) {
		return false;
	}
	bytes.data.resize(count);
	if (count > 0) {
		stream.seekg(static_cast<std::streamoff>(offset));
		stream.read(reinterpret_cast<char*>(bytes.data.data()), static_cast<std::streamsize>(count));
		if (static_cast<std::size_t>(stream.gcount()) != count) {
			bytes.data.clear();
			return false;
		}
	}
	return true;
}

bool File::write(const std::filesystem::path& path, const Bytes& bytes) {
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream) {
		return false;
	}
	if (!bytes.data.empty()) {
		stream.write(reinterpret_cast<const char*>(bytes.data.data()), static_cast<std::streamsize>(bytes.data.size()));
	}
	stream.flush();
	return static_cast<bool>(stream);
}

bool File::exists(const std::filesystem::path& path) {
	std::error_code error;
	return std::filesystem::is_regular_file(path, error);
}

std::filesystem::path File::toPath(std::string_view utf8) {
	// Through char8_t, so the bytes are read as UTF-8 on every platform.
	std::u8string text(utf8.size(), u8'\0');
	for (std::size_t i = 0; i < utf8.size(); i++) {
		text[i] = static_cast<char8_t>(utf8[i]);
	}
	return std::filesystem::path(text);
}

std::string File::fromPath(const std::filesystem::path& path) {
	const std::u8string text = path.u8string();
	std::string result(text.size(), '\0');
	for (std::size_t i = 0; i < text.size(); i++) {
		result[i] = static_cast<char>(text[i]);
	}
	return result;
}

}
