#include <litefont/Vector2.h>
#include <litefont/Size2.h>
#include <litefont/Rect.h>
#include <litefont/Affine2.h>
#include <litefont/Matrix4.h>
#include "TestKit.h"
#include "RecordingDevice.h"

namespace litefont::tests {

class Vector2Tests {
	static inline test::Test arithmetic{"Vector2: arithmetic is component-wise, with scalar on either side", [](test::Context& context) {
		const Vector2f a{1.0f, 2.0f};
		const Vector2f b{3.0f, -4.0f};
		context.checkEqual(a + b, Vector2f{4.0f, -2.0f});
		context.checkEqual(a - b, Vector2f{-2.0f, 6.0f});
		context.checkEqual(-a, Vector2f{-1.0f, -2.0f});
		context.checkEqual(a * 2.0f, Vector2f{2.0f, 4.0f});
		context.checkEqual(2.0f * a, Vector2f{2.0f, 4.0f});
		context.checkEqual(b / 2.0f, Vector2f{1.5f, -2.0f});

		Vector2i accumulated{1, 1};
		accumulated += Vector2i{2, 3};
		accumulated -= Vector2i{1, 0};
		context.checkEqual(accumulated, Vector2i{2, 4});
	}};

	static inline test::Test products{"Vector2: dot, cross and length", [](test::Context& context) {
		const Vector2f a{3.0f, 4.0f};
		context.checkEqual(a.dot(Vector2f{1.0f, 0.0f}), 3.0f);
		context.checkEqual(Vector2f{1.0f, 0.0f}.cross(Vector2f{0.0f, 1.0f}), 1.0f);
		context.checkEqual(a.getLengthSquared(), 25.0f);
		context.checkEqual(a.getLength(), 5.0f);
		context.checkEqual(Vector2i{7, -3}.convert<float>(), Vector2f{7.0f, -3.0f});
	}};
};

class RectTests {
	static inline test::Test edges{"Rect: left and top edges are inside, right and bottom outside", [](test::Context& context) {
		const Recti rect{10, 20, 5, 5};
		context.check(rect.contains(Vector2i{10, 20}), "top-left corner");
		context.check(rect.contains(Vector2i{14, 24}), "last pixel");
		context.check(!rect.contains(Vector2i{15, 20}), "right edge");
		context.check(!rect.contains(Vector2i{10, 25}), "bottom edge");
		context.checkEqual(rect.getRight(), 15);
		context.checkEqual(rect.getBottom(), 25);
	}};

	static inline test::Test unionAndIntersection{"Rect: united and intersected, with empty rectangles", [](test::Context& context) {
		const Recti a{0, 0, 10, 10};
		const Recti b{5, 5, 10, 10};
		context.checkEqual(a.united(b), Recti{0, 0, 15, 15});
		context.checkEqual(a.intersected(b), Recti{5, 5, 5, 5});
		context.checkEqual(a.intersected(Recti{10, 0, 5, 5}), Recti{});
		context.checkEqual(Recti{}.united(b), b);
		context.check(Rectf{0.0f, 0.0f, 0.0f, 3.0f}.isEmpty(), "zero width is empty");
		context.checkEqual(Rectf::fromCorners(Vector2f{4.0f, 1.0f}, Vector2f{1.0f, 3.0f}), Rectf{1.0f, 1.0f, 3.0f, 2.0f});
	}};
};

class Affine2Tests {
	static inline test::Test applies{"Affine2: applies in the SVG matrix order", [](test::Context& context) {
		Affine2f transform;
		transform.a = 2.0f;
		transform.b = 1.0f;
		transform.c = -1.0f;
		transform.d = 3.0f;
		transform.tx = 10.0f;
		transform.ty = 20.0f;
		// x' = 2x - y + 10, y' = x + 3y + 20
		context.checkEqual(transform.apply(Vector2f{1.0f, 1.0f}), Vector2f{11.0f, 24.0f});
		context.check(Affine2f::identity().isIdentity(), "identity");
	}};

	static inline test::Test composes{"Affine2: then applies this transform first and the next one second", [](test::Context& context) {
		const Affine2f scale = Affine2f::scale(2.0f, 3.0f);
		const Affine2f move = Affine2f::translate(5.0f, -1.0f);
		const Vector2f point{1.0f, 1.0f};

		context.checkEqual(scale.then(move).apply(point), move.apply(scale.apply(point)));
		context.checkEqual(move.then(scale).apply(point), scale.apply(move.apply(point)));
		context.checkEqual(scale.then(move).apply(point), Vector2f{7.0f, 2.0f});
	}};
};

class Matrix4Tests {
	// A point (x, y, 0, 1) through the matrix, divided by w.
	static Vector2f apply(const Matrix4f& matrix, Vector2f point) {
		const std::array<float, 16>& m = matrix.values;
		const float w = m[3] * point.x + m[7] * point.y + m[15];
		return Vector2f{(m[0] * point.x + m[4] * point.y + m[12]) / w, (m[1] * point.x + m[5] * point.y + m[13]) / w};
	}

	static inline test::Test order{"Matrix4: column-major, and a * b applies b first", [](test::Context& context) {
		const Matrix4f move = Matrix4f::translate(5.0f, -1.0f, 0.0f);
		const Matrix4f scale = Matrix4f::scale(2.0f, 3.0f, 1.0f);
		context.checkEqual(move.get(0, 3), 5.0f);
		context.checkEqual(move.values[12], 5.0f);
		context.checkEqual(Matrix4Tests::apply(move * scale, Vector2f{1.0f, 1.0f}), Vector2f{7.0f, 2.0f});
		context.checkEqual(Matrix4Tests::apply(scale * move, Vector2f{1.0f, 1.0f}), Vector2f{12.0f, 0.0f});
		context.check(Matrix4f::identity() * move == move, "identity on the left");
		context.check(Matrix4f::fromColumnMajor(move.getData()) == move, "from column-major values");
	}};

	static inline test::Test ortho{"Matrix4: ortho maps its box to -1 to 1 the way glOrtho does", [](test::Context& context) {
		// y down: the top-left pixel corner is (-1, 1), the bottom-right (1, -1).
		const Matrix4f pixels = Matrix4f::ortho(0.0f, 1024.0f, 512.0f, 0.0f, -1.0f, 1.0f);
		context.checkEqual(Matrix4Tests::apply(pixels, Vector2f{0.0f, 0.0f}), Vector2f{-1.0f, 1.0f});
		context.checkEqual(Matrix4Tests::apply(pixels, Vector2f{1024.0f, 512.0f}), Vector2f{1.0f, -1.0f});
		context.checkEqual(Matrix4Tests::apply(pixels, Vector2f{512.0f, 256.0f}), Vector2f{0.0f, 0.0f});
		context.checkEqual(pixels.get(2, 2), -1.0f);
	}};

	static inline test::Test affineAndTurn{"Matrix4: from an Affine2, and a quarter turn about z", [](test::Context& context) {
		const Affine2f affine = Affine2f::scale(2.0f, -0.5f).then(Affine2f::translate(10.0f, 20.0f));
		const Vector2f point{3.0f, 4.0f};
		context.checkEqual(Matrix4Tests::apply(Matrix4f::fromAffine(affine), point), affine.apply(point));
		// Counterclockwise with y up: x goes to y.
		const Vector2f turned = Matrix4Tests::apply(Matrix4f::rotateZ(1.57079633f), Vector2f{1.0f, 0.0f});
		context.check(std::fabs(turned.x) < 1e-6f && std::fabs(turned.y - 1.0f) < 1e-6f, "x turns to y");
	}};
};

}
