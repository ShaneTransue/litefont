#include "TestKit.h"
#include "RecordingDevice.h"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>

namespace litefont::test {

void Context::check(bool condition, std::string_view description, std::source_location location) {
	if (!condition) {
		this->fail(std::string(description), location);
	}
}

void Context::checkImage(const Image& actual, const std::filesystem::path& golden, std::int32_t tolerance, std::source_location location) {
	std::error_code error;
	if (!std::filesystem::exists(golden, error)) {
		std::filesystem::create_directories(golden.parent_path(), error);
		if (!actual.save(golden)) {
			this->fail(std::format("cannot record golden image {}", golden.string()), location);
			return;
		}
		this->note(std::format("recorded golden image {}", golden.string()));
		return;
	}

	Image expected;
	if (!expected.load(golden)) {
		this->fail(std::format("cannot read golden image {}", golden.string()), location);
		return;
	}
	ImageDifference difference;
	if (ImageComparison::compare(actual, expected, tolerance, difference)) {
		return;
	}

	const std::filesystem::path folder = Context::getOutputFolder();
	std::filesystem::create_directories(folder, error);
	const std::string stem = golden.stem().string();
	actual.save(folder / (stem + ".actual" + golden.extension().string()));
	if (!difference.map.isEmpty()) {
		difference.map.save(folder / (stem + ".difference.pgm"));
	}
	this->fail(std::format("{} differs from its golden image: {} pixels beyond tolerance {}, largest difference {}; see {}",
		stem, difference.pixelCount, tolerance, difference.largest, folder.string()), location);
}

std::filesystem::path Context::getOutputFolder() {
	return std::filesystem::path("out") / "test-output";
}

std::filesystem::path Context::getFontFolder() {
	return Context::fontFolder();
}

void Context::setFontFolder(const std::filesystem::path& folder) {
	Context::fontFolder() = folder;
}

std::filesystem::path& Context::fontFolder() {
	// The folder of real fonts given with --fonts; tests that need one skip without it.
	static std::filesystem::path folder = "fonts";
	return folder;
}

void Context::note(std::string message) {
	this->notes.push_back(std::move(message));
}

const std::vector<TestFailure>& Context::getFailures() const {
	return this->failures;
}

const std::vector<std::string>& Context::getNotes() const {
	return this->notes;
}

bool Context::hasFailed() const {
	return !this->failures.empty();
}

void Context::fail(std::string message, std::source_location location) {
	TestFailure failure;
	failure.message = std::move(message);
	failure.location = location;
	this->failures.push_back(std::move(failure));
}

bool ImageComparison::compare(const Image& actual, const Image& expected, std::int32_t tolerance, ImageDifference& difference) {
	difference = ImageDifference();
	if (actual.getSize() != expected.getSize() || actual.getFormat() != expected.getFormat()) {
		difference.largest = 255;
		difference.pixelCount = static_cast<std::size_t>(std::max(actual.getWidth(), expected.getWidth()))
			* static_cast<std::size_t>(std::max(actual.getHeight(), expected.getHeight()));
		return false;
	}

	difference.map = Image(actual.getWidth(), actual.getHeight(), PixelFormat::Gray8);
	const std::size_t channels = static_cast<std::size_t>(actual.getChannelCount());
	for (std::int32_t y = 0; y < actual.getHeight(); y++) {
		const std::uint8_t* actualRow = actual.getRow(y);
		const std::uint8_t* expectedRow = expected.getRow(y);
		std::uint8_t* mapRow = difference.map.getRow(y);
		for (std::int32_t x = 0; x < actual.getWidth(); x++) {
			std::int32_t largest = 0;
			for (std::size_t channel = 0; channel < channels; channel++) {
				const std::size_t i = static_cast<std::size_t>(x) * channels + channel;
				largest = std::max(largest, std::abs(std::int32_t{actualRow[i]} - std::int32_t{expectedRow[i]}));
			}
			mapRow[x] = static_cast<std::uint8_t>(std::min(largest * 16, 255));
			difference.largest = std::max(difference.largest, largest);
			difference.pixelCount += largest > tolerance ? 1 : 0;
		}
	}
	return difference.pixelCount == 0;
}

Test::Test(std::string_view name, TestFunction function) {
	this->name = std::string(name);
	this->function = std::move(function);
	Test::getAll().push_back(this);
}

const std::string& Test::getName() const {
	return this->name;
}

void Test::run(Context& context) const {
	this->function(context);
}

std::vector<const Test*>& Test::getAll() {
	// A function-local static, so registration works whatever order the
	// test files' static members are initialized in.
	static std::vector<const Test*> tests;
	return tests;
}

int TestRunner::run(int argumentCount, char** arguments) {
	// [--fonts <folder>] [part of a test name]
	std::string filter;
	for (int i = 1; i < argumentCount; i++) {
		const std::string argument = arguments[i];
		if (argument == "--fonts" && i + 1 < argumentCount) {
			Context::setFontFolder(arguments[++i]);
		}
		else {
			filter = argument;
		}
	}

	std::vector<const Test*> tests = Test::getAll();
	std::sort(tests.begin(), tests.end(), [](const Test* left, const Test* right) {
		return left->getName() < right->getName();
	});

	std::int32_t passed = 0;
	std::int32_t failed = 0;
	constexpr std::size_t maxFailuresShown = 10;

	for (const Test* test : tests) {
		if (!filter.empty() && test->getName().find(filter) == std::string::npos) {
			continue;
		}

		Context context;
		try {
			test->run(context);
		}
		catch (const std::exception& exception) {
			context.check(false, std::string("threw: ") + exception.what());
		}

		if (!context.hasFailed()) {
			passed++;
			std::cout << "[pass] " << test->getName() << "\n";
			for (const std::string& note : context.getNotes()) {
				std::cout << "       note: " << note << "\n";
			}
			continue;
		}

		failed++;
		std::cout << "[FAIL] " << test->getName() << "\n";
		for (const std::string& note : context.getNotes()) {
			std::cout << "       note: " << note << "\n";
		}
		const std::vector<TestFailure>& failures = context.getFailures();
		for (std::size_t i = 0; i < failures.size() && i < maxFailuresShown; i++) {
			const std::filesystem::path file = failures[i].location.file_name();
			std::cout << "       " << file.filename().string() << ":" << failures[i].location.line()
				<< ": " << failures[i].message << "\n";
		}
		if (failures.size() > maxFailuresShown) {
			std::cout << "       ... and " << (failures.size() - maxFailuresShown) << " more\n";
		}
	}

	std::cout << "\n" << passed << " passed, " << failed << " failed\n";
	return failed == 0 ? 0 : 1;
}

}
