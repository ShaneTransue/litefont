#include "TestKit.h"
#include "RecordingDevice.h"

#include <filesystem>
#include <string>

namespace litefont::tests {

class TestKitTests {
	static inline test::Test recordsFailures{"TestKit: failed checks are recorded with their line", [](test::Context& context) {
		test::Context inner;
		inner.check(true, "passes");
		context.check(!inner.hasFailed(), "a passing check records nothing");

		const std::uint32_t failingLine = std::source_location::current().line() + 1;
		inner.check(false, "fails on purpose");
		context.check(inner.hasFailed(), "a failing check is recorded");
		context.checkEqual(inner.getFailures().size(), std::size_t{1});
		context.checkEqual(inner.getFailures().front().location.line(), failingLine);
		context.checkEqual(inner.getFailures().front().message, std::string("fails on purpose"));
	}};

	static inline test::Test reportsBothValues{"TestKit: checkEqual reports the expected and actual values", [](test::Context& context) {
		test::Context inner;
		inner.checkEqual(41, 42);
		context.checkEqual(inner.getFailures().size(), std::size_t{1});
		context.checkEqual(inner.getFailures().front().message, std::string("expected 42, got 41"));
	}};

	static inline test::Test imageTolerance{"TestKit: a one-level image difference passes at tolerance 1 and fails at 0", [](test::Context& context) {
		Image expected(3, 2, PixelFormat::RGB8);
		Image actual = expected;
		actual.getRow(1)[4] = 1;

		test::ImageDifference difference;
		context.check(test::ImageComparison::compare(actual, expected, 1, difference), "passes at tolerance 1");
		context.check(!test::ImageComparison::compare(actual, expected, 0, difference), "fails at tolerance 0");
		context.checkEqual(difference.largest, 1);
		context.checkEqual(difference.pixelCount, std::size_t{1});
		context.checkEqual(difference.map.getRow(1)[1], std::uint8_t{16});
		context.check(!test::ImageComparison::compare(Image(3, 3, PixelFormat::RGB8), expected, 255, difference), "sizes differ");
		context.check(!test::ImageComparison::compare(Image(3, 2, PixelFormat::Gray8), expected, 255, difference), "formats differ");
	}};

	static inline test::Test goldenImages{"TestKit: checkImage records a missing golden image, then compares with it", [](test::Context& context) {
		std::error_code error;
		const std::filesystem::path golden = std::filesystem::temp_directory_path(error) / "px_testkit_golden.pgm";
		std::filesystem::remove(golden, error);
		Image image(4, 4, PixelFormat::Gray8);
		image.fill(Color4f{0.5f, 0.5f, 0.5f, 1.0f});

		test::Context first;
		first.checkImage(image, golden);
		context.check(!first.hasFailed() && first.getNotes().size() == 1, "the first run records and notes it");
		test::Context second;
		second.checkImage(image, golden);
		context.check(!second.hasFailed() && second.getNotes().empty(), "the second run compares and passes");

		image.getRow(2)[2] = 0;
		test::Context third;
		third.checkImage(image, golden);
		context.check(third.hasFailed(), "a changed image fails");
		context.check(std::filesystem::exists(test::Context::getOutputFolder() / "px_testkit_golden.difference.pgm", error), "the difference map is written");
		std::filesystem::remove(golden, error);
	}};
};

}
