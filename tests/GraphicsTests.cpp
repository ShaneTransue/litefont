#include <litefont/Vertex.h>
#include <litefont/Geometry.h>
#include <litefont/GraphicsDevice.h>
#include <litefont/SoftwareDevice.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include "DeviceConformance.h"

#include <cstddef>
#include <limits>

namespace litefont::tests {

struct NoLayout {
	float x = 0.0f;
	float y = 0.0f;
};

class GraphicsTests {
	static_assert(sizeof(Vertex2D) == Vertex2D::layout().getStride());
	static_assert(sizeof(SpriteVertex) == SpriteVertex::layout().getStride());
	static_assert(offsetof(SpriteVertex, u) == 8 && offsetof(SpriteVertex, rgba) == 16);
	static_assert(SpriteVertex::layout().getAttributes()[1].offset == offsetof(SpriteVertex, u));
	static_assert(SpriteVertex::layout().getAttributes()[2].offset == offsetof(SpriteVertex, rgba));
	static_assert(VertexType<SpriteVertex> && VertexType<Vertex2D>);
	static_assert(!VertexType<NoLayout> && Positioned2D<NoLayout>);

	// A quad covering whole pixels, sampling the full texture.
	static std::array<SpriteVertex, 4> makeQuad(float x, float y, float width, float height, Color4f color) {
		const std::uint32_t rgba = color.packRGBA8();
		return std::array<SpriteVertex, 4>{
			SpriteVertex{x, y, 0.0f, 0.0f, rgba},
			SpriteVertex{x + width, y, 1.0f, 0.0f, rgba},
			SpriteVertex{x + width, y + height, 1.0f, 1.0f, rgba},
			SpriteVertex{x, y + height, 0.0f, 1.0f, rgba},
		};
	}

	static Image drawQuad(const Image& texture, Size2i target, Color4f clear, const std::array<SpriteVertex, 4>& corners, Recti clip = Recti{}) {
		SoftwareDevice device;
		const TextureHandle handle = device.createTexture(TextureDesc{static_cast<std::uint16_t>(texture.getWidth()), static_cast<std::uint16_t>(texture.getHeight()), texture.getFormat()});
		device.updateTexture(handle, texture, texture.getBounds());
		Geometry<SpriteVertex> geometry;
		geometry.addQuad(corners);
		const GeometryHandle buffer = device.createGeometry(SpriteVertex::layout());
		device.updateGeometry(buffer, geometry.getView());
		const SpriteBatch batch{SpriteEncoding::Coverage, handle, 0, 6, clip};
		device.beginFrame(target, clear);
		device.drawSprites(buffer, std::span<const SpriteBatch>(&batch, 1));
		device.endFrame();
		return device.readPixels();
	}

	static inline test::Test layouts{"VertexLayout: offsets and stride follow the attributes in order", [](test::Context& context) {
		const VertexLayout layout = VertexLayout::sequential({
			{VertexSemantic::Position, AttributeFormat::Float32x3},
			{VertexSemantic::Color, AttributeFormat::UNorm8x4},
			{VertexSemantic::Normal, AttributeFormat::Float32x3},
		});
		context.checkEqual(layout.getStride(), std::uint16_t{28});
		context.checkEqual(layout.getAttributes().size(), std::size_t{3});
		context.checkEqual(layout.getAttributes()[2].offset, std::uint16_t{16});
		context.check(layout.find(VertexSemantic::Color) != nullptr && layout.find(VertexSemantic::TexCoord) == nullptr, "find by semantic");
		context.check(SpriteVertex::layout() == SpriteVertex::layout() && !(Vertex2D::layout() == SpriteVertex::layout()), "layouts compare");
	}};

	static inline test::Test geometry{"Geometry: quads make two triangles, views expose the bytes, and changes bump the revision", [](test::Context& context) {
		Geometry<SpriteVertex> sprites;
		sprites.addQuad(makeQuad(0.0f, 0.0f, 2.0f, 2.0f, Color4f{}));
		sprites.addQuad(makeQuad(5.0f, 1.0f, 2.0f, 3.0f, Color4f{}));
		const std::vector<std::uint32_t> expected = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
		context.check(std::vector<std::uint32_t>(sprites.getIndices().begin(), sprites.getIndices().end()) == expected, "indices");
		const GeometryView view = sprites.getView();
		context.checkEqual(view.vertexBytes.size(), std::size_t{8 * sizeof(SpriteVertex)});
		context.check(view.layout == SpriteVertex::layout(), "the vertex type's layout");
		context.check(view.vertexBytes.data() == reinterpret_cast<const std::uint8_t*>(sprites.getVertices().data()), "the same bytes, not a copy");
		context.checkEqual(sprites.getRevision(), std::uint32_t{2});
		context.checkEqual(sprites.getBounds(), Rectf{0.0f, 0.0f, 7.0f, 4.0f});

		Geometry<Vertex2D> points;
		const Vertex2D triangle[] = {Vertex2D{1.0f, 1.0f}, Vertex2D{2.0f, 1.0f}, Vertex2D{1.0f, 2.0f}};
		const std::uint32_t indices[] = {0, 1, 2};
		points.add(triangle, indices);
		points.add(triangle, indices);
		context.checkEqual(points.getIndices()[5], std::uint32_t{5});
		points.clear();
		context.check(points.getVertices().empty() && points.getRevision() == 3, "clear empties and counts");
	}};

	static inline test::Test blending{"SoftwareDevice: coverage blends straight alpha over the frame as hand-computed", [](test::Context& context) {
		Image half(1, 1, PixelFormat::Gray8);
		half.getRow(0)[0] = 128;
		const Image frame = drawQuad(half, Size2i{2, 2}, Color4f{0.0f, 0.0f, 1.0f, 1.0f}, makeQuad(0.0f, 0.0f, 2.0f, 2.0f, Color4f{1.0f, 0.0f, 0.0f, 1.0f}));
		// alpha = 128 / 255: red 0.502 -> 128, blue 0.498 -> 127.
		context.check(frame.getPixel(1, 1) == Color4u8{128, 0, 127, 255}, "red over blue at half coverage");

		Image full(1, 1, PixelFormat::Gray8);
		full.getRow(0)[0] = 255;
		const Image faded = drawQuad(full, Size2i{2, 2}, Color4f{0.0f, 0.0f, 0.0f, 0.0f}, makeQuad(0.0f, 0.0f, 2.0f, 2.0f, Color4f{1.0f, 1.0f, 1.0f, 0.5f}));
		context.check(faded.getPixel(0, 0) == Color4u8{128, 128, 128, 128}, "vertex alpha over transparent black");
	}};

	static inline test::Test exactCopy{"SoftwareDevice: a whole-pixel sprite copies its texels exactly, each pixel drawn once", [](test::Context& context) {
		Image texture(5, 3, PixelFormat::Gray8);
		for (std::int32_t y = 0; y < 3; y++) {
			for (std::int32_t x = 0; x < 5; x++) {
				texture.getRow(y)[x] = static_cast<std::uint8_t>(17 + x * 40 + y * 9);
			}
		}
		const Image frame = drawQuad(texture, Size2i{9, 7}, Color4f{0.0f, 0.0f, 0.0f, 1.0f}, makeQuad(3.0f, 2.0f, 5.0f, 3.0f, Color4f{1.0f, 1.0f, 1.0f, 1.0f}));
		std::int32_t wrong = 0;
		for (std::int32_t y = 0; y < 7; y++) {
			for (std::int32_t x = 0; x < 9; x++) {
				const bool inside = x >= 3 && x < 8 && y >= 2 && y < 5;
				const std::uint8_t expected = inside ? texture.getRow(y - 2)[x - 3] : 0;
				wrong += frame.getPixel(x, y).r == expected ? 0 : 1;
			}
		}
		context.checkEqual(wrong, 0);

		// At half alpha a pixel drawn twice (once per triangle) would reach 191.
		Image full(1, 1, PixelFormat::Gray8);
		full.getRow(0)[0] = 255;
		const Image once = drawQuad(full, Size2i{6, 6}, Color4f{0.0f, 0.0f, 0.0f, 1.0f}, makeQuad(0.0f, 0.0f, 6.0f, 6.0f, Color4f{1.0f, 1.0f, 1.0f, 0.5f}));
		std::int32_t doubled = 0;
		for (std::int32_t y = 0; y < 6; y++) {
			for (std::int32_t x = 0; x < 6; x++) {
				doubled += once.getPixel(x, y).r == 128 ? 0 : 1;
			}
		}
		context.checkEqual(doubled, 0);
	}};

	static inline test::Test clips{"SoftwareDevice: a batch's clip rectangle bounds what it draws", [](test::Context& context) {
		Image full(1, 1, PixelFormat::Gray8);
		full.getRow(0)[0] = 255;
		const Image frame = drawQuad(full, Size2i{8, 8}, Color4f{0.0f, 0.0f, 0.0f, 1.0f}, makeQuad(-2.0f, -2.0f, 20.0f, 20.0f, Color4f{1.0f, 1.0f, 1.0f, 1.0f}), Recti{2, 3, 4, 2});
		std::int32_t lit = 0;
		std::int32_t wrong = 0;
		for (std::int32_t y = 0; y < 8; y++) {
			for (std::int32_t x = 0; x < 8; x++) {
				const bool inside = x >= 2 && x < 6 && y >= 3 && y < 5;
				lit += frame.getPixel(x, y).r == 255 ? 1 : 0;
				wrong += (frame.getPixel(x, y).r == 255) == inside ? 0 : 1;
			}
		}
		context.checkEqual(lit, 8);
		context.checkEqual(wrong, 0);
	}};

	static inline test::Test robust{"SoftwareDevice: bad handles, indices and coordinates draw nothing and never crash", [](test::Context& context) {
		SoftwareDevice device;
		const TextureHandle texture = device.createTexture(TextureDesc{2, 2, PixelFormat::Gray8});
		const GeometryHandle buffer = device.createGeometry(SpriteVertex::layout());
		const float nan = std::numeric_limits<float>::quiet_NaN();
		const SpriteVertex vertices[] = {
			SpriteVertex{nan, 0.0f, 0.0f, 0.0f, 0xFFFFFFFFu},
			SpriteVertex{1e30f, 0.0f, nan, 0.0f, 0xFFFFFFFFu},
			SpriteVertex{0.0f, 5.0f, 0.0f, 0.0f, 0xFFFFFFFFu},
		};
		const std::uint32_t indices[] = {0, 1, 2, 2, 9, 1, 0};
		GeometryView view;
		view.vertexBytes = std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(vertices), sizeof(vertices));
		view.layout = SpriteVertex::layout();
		view.indices = indices;
		device.updateGeometry(buffer, view);
		device.updateGeometry(GeometryHandle{99}, view);
		device.updateTexture(TextureHandle{42}, Image(2, 2, PixelFormat::Gray8), Recti{0, 0, 2, 2});
		const SpriteBatch batches[] = {
			SpriteBatch{SpriteEncoding::Coverage, texture, 0, 1000, Recti{}},
			SpriteBatch{SpriteEncoding::Coverage, TextureHandle{7}, 0, 3, Recti{}},
			SpriteBatch{SpriteEncoding::DistanceField, texture, 0, 3, Recti{}},
		};
		device.drawSprites(buffer, batches);
		device.beginFrame(Size2i{4, 4}, Color4f{0.0f, 0.0f, 0.0f, 1.0f});
		device.drawSprites(buffer, batches);
		device.drawSprites(GeometryHandle{0}, batches);
		device.endFrame();
		context.check(device.readPixels().getPixel(0, 0) == Color4u8{0, 0, 0, 255}, "nothing was drawn");
	}};

	static inline test::Test conformance{"SoftwareDevice: passes the device conformance scene", [](test::Context& context) {
		SoftwareDevice device;
		DeviceConformance::run(context, device, "tests/golden/device-conformance.pam");
	}};

	static inline test::Test recording{"RecordingDevice: counts every call and forwards it unchanged", [](test::Context& context) {
		SoftwareDevice software;
		test::RecordingDevice device(software);
		DeviceConformance::run(context, device, "tests/golden/device-conformance.pam");
		const test::DeviceCallCounts& counts = device.getCounts();
		context.checkEqual(counts.createTexture, std::uint32_t{2});
		context.checkEqual(counts.updateTexture, std::uint32_t{2});
		context.checkEqual(counts.uploadedTexels, std::uint64_t{80});
		context.checkEqual(counts.updateGeometry, std::uint32_t{1});
		context.checkEqual(counts.drawSprites, std::uint32_t{1});
		context.checkEqual(counts.beginFrame, std::uint32_t{1});

		test::RecordingDevice alone;
		context.check(alone.createTexture(TextureDesc{}).value != alone.createGeometry(SpriteVertex::layout()).value, "distinct handles");
		context.check(alone.readPixels().isEmpty(), "and no pixels");
	}};
};

}
