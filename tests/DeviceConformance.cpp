#include "DeviceConformance.h"

#include <litefont/Vertex.h>
#include <litefont/Geometry.h>
#include <litefont/GraphicsDevice.h>

namespace litefont::tests {

Image DeviceConformance::draw(GraphicsDevice& device) {
	// An 8 x 8 coverage gradient and a 4 x 4 RGBA checker with alpha.
	Image gradient(8, 8, PixelFormat::Gray8);
	for (std::int32_t y = 0; y < 8; y++) {
		for (std::int32_t x = 0; x < 8; x++) {
			gradient.getRow(y)[x] = static_cast<std::uint8_t>(x * 32 + y * 3);
		}
	}
	Image checker(4, 4, PixelFormat::RGBA8);
	for (std::int32_t y = 0; y < 4; y++) {
		for (std::int32_t x = 0; x < 4; x++) {
			const bool odd = (x + y) % 2 == 1;
			checker.setPixel(x, y, odd ? Color4u8{255, 40, 40, 255} : Color4u8{40, 255, 120, 128});
		}
	}

	const TextureHandle gradientTexture = device.createTexture(TextureDesc{8, 8, PixelFormat::Gray8});
	device.updateTexture(gradientTexture, gradient, gradient.getBounds());
	const TextureHandle checkerTexture = device.createTexture(TextureDesc{4, 4, PixelFormat::RGBA8});
	device.updateTexture(checkerTexture, checker, checker.getBounds());

	auto quad = [](float x, float y, float size, Color4f color) {
		const std::uint32_t rgba = color.packRGBA8();
		return std::array<SpriteVertex, 4>{
			SpriteVertex{x, y, 0.0f, 0.0f, rgba},
			SpriteVertex{x + size, y, 1.0f, 0.0f, rgba},
			SpriteVertex{x + size, y + size, 1.0f, 1.0f, rgba},
			SpriteVertex{x, y + size, 0.0f, 1.0f, rgba},
		};
	};
	Geometry<SpriteVertex> geometry;
	geometry.addQuad(quad(2.0f, 2.0f, 8.0f, Color4f{1.0f, 1.0f, 1.0f, 1.0f}));
	geometry.addQuad(quad(12.5f, 2.5f, 8.0f, Color4f{1.0f, 0.5f, 0.0f, 1.0f}));
	geometry.addQuad(quad(24.0f, 2.0f, 16.0f, Color4f{0.0f, 1.0f, 1.0f, 0.75f}));
	const std::uint32_t white = Color4f{1.0f, 1.0f, 1.0f, 1.0f}.packRGBA8();
	const SpriteVertex triangle[] = {
		SpriteVertex{4.0f, 28.0f, 0.0f, 1.0f, white},
		SpriteVertex{20.0f, 14.0f, 0.5f, 0.0f, white},
		SpriteVertex{22.0f, 31.0f, 1.0f, 1.0f, white},
	};
	const std::uint32_t triangleIndices[] = {0, 1, 2};
	geometry.add(triangle, triangleIndices);
	geometry.addQuad(quad(-4.0f, -4.0f, 60.0f, Color4f{0.2f, 0.4f, 1.0f, 0.5f}));

	const GeometryHandle handle = device.createGeometry(SpriteVertex::layout());
	device.updateGeometry(handle, geometry.getView());
	const SpriteBatch batches[] = {
		SpriteBatch{SpriteEncoding::Coverage, gradientTexture, 0, 18, Recti{}},
		SpriteBatch{SpriteEncoding::Coverage, checkerTexture, 18, 3, Recti{}},
		SpriteBatch{SpriteEncoding::Coverage, gradientTexture, 21, 6, Recti{30, 20, 12, 8}},
	};
	device.beginFrame(Size2i{48, 32}, Color4f{0.1f, 0.1f, 0.1f, 1.0f});
	device.drawSprites(handle, batches);
	device.endFrame();
	return device.readPixels();
}

void DeviceConformance::run(test::Context& context, GraphicsDevice& device, const std::filesystem::path& golden, std::int32_t tolerance) {
	const Image frame = DeviceConformance::draw(device);
	context.check(frame.getFormat() == PixelFormat::RGBA8 && frame.getSize() == Size2i{48, 32}, "an RGBA8 frame of the target size");
	context.checkImage(frame, golden, tolerance);
}

}
