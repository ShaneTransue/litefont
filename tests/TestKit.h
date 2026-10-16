#pragma once

#include <litefont/Image.h>
#include <litefont/Sprite.h>
#include <litefont/Blur.h>

#include <filesystem>
#include <format>
#include <functional>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace litefont::test {

class Context;

using TestFunction = std::function<void(Context&)>;

/**
 * \struct TestFailure TestKit.h <litefont/TestKit.h>
 *
 * TestFailure - One failed check: what failed and where.
 */
struct TestFailure {
	std::string message;
	std::source_location location;
};

/**
 * \struct ImageDifference TestKit.h <litefont/TestKit.h>
 *
 * ImageDifference - How two images differ: the largest difference of any
 * channel, how many pixels differ by more than the tolerance, and a gray map
 * of every difference (16 times brighter, so one level shows).
 */
struct ImageDifference {
	std::int32_t largest = 0;
	std::size_t pixelCount = 0;
	Image map;
};

/**
 * \class ImageComparison TestKit.h <litefont/TestKit.h>
 *
 * ImageComparison - Compares two images channel by channel. Images of
 * different sizes or formats never match.
 */
class ImageComparison {
public:
	static bool compare(const Image& actual, const Image& expected, std::int32_t tolerance, ImageDifference& difference);
};

/**
 * \class Context TestKit.h <litefont/TestKit.h>
 *
 * Context - What a running test reports through. Checks record failures with
 * the call site and let the test keep running, so one run shows every problem.
 *
 * checkImage() compares an image with a golden file. A missing golden file is
 * written from the image and noted (delete it to record again); on a mismatch
 * the image and a difference map go to getOutputFolder().
 */
class Context {
public:
	void check(bool condition, std::string_view description = "check failed",
		std::source_location location = std::source_location::current());

	template <typename Actual, typename Expected>
	void checkEqual(const Actual& actual, const Expected& expected,
		std::source_location location = std::source_location::current());

	void checkImage(const Image& actual, const std::filesystem::path& golden, std::int32_t tolerance = 0,
		std::source_location location = std::source_location::current());

	void note(std::string message);

	static std::filesystem::path getOutputFolder();
	static std::filesystem::path getFontFolder();
	static void setFontFolder(const std::filesystem::path& folder);

	const std::vector<TestFailure>& getFailures() const;
	const std::vector<std::string>& getNotes() const;
	bool hasFailed() const;

private:
	void fail(std::string message, std::source_location location);

	static std::filesystem::path& fontFolder();

	std::vector<TestFailure> failures;
	std::vector<std::string> notes;
};

/**
 * \class Test TestKit.h <litefont/TestKit.h>
 *
 * Test - A named test that registers itself when it is constructed. Declare one
 * as an inline static member of a test class, holding a lambda:
 *
 *     class ByteReaderTests {
 *         static inline litefont::test::Test readsBigEndian{"ByteReader: reads big-endian", [](litefont::test::Context& context) {
 *             ...
 *         }};
 *     };
 */
class Test {
public:
	Test(std::string_view name, TestFunction function);

	const std::string& getName() const;
	void run(Context& context) const;

	static std::vector<const Test*>& getAll();

private:
	std::string name;
	TestFunction function;
};

/**
 * \class TestRunner TestKit.h <litefont/TestKit.h>
 *
 * TestRunner - Runs every registered test in name order, or only those whose
 * name contains the first command-line argument, prints a report and returns
 * the process exit code (0 when everything passed).
 */
class TestRunner {
public:
	static int run(int argumentCount, char** arguments);
};

template <typename Actual, typename Expected>
void Context::checkEqual(const Actual& actual, const Expected& expected, std::source_location location) {
	if (actual == expected) {
		return;
	}
	// C++20: a type without a formatter has a disabled, not default-constructible one.
	if constexpr (std::is_default_constructible_v<std::formatter<Actual, char>> && std::is_default_constructible_v<std::formatter<Expected, char>>) {
		this->fail(std::format("expected {}, got {}", expected, actual), location);
	}
	else {
		this->fail("values differ", location);
	}
}

}
