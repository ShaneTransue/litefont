#pragma once

#include <litefont/GraphicsDevice.h>

namespace litefont::test {

/**
 * \struct DeviceCallCounts RecordingDevice.h <litefont/TestKit.h>
 *
 * DeviceCallCounts - How often each GraphicsDevice call was made, and how
 * many texels updateTexture uploaded.
 */
struct DeviceCallCounts {
	std::uint32_t createTexture = 0;
	std::uint32_t updateTexture = 0;
	std::uint64_t uploadedTexels = 0;
	std::uint32_t createGeometry = 0;
	std::uint32_t updateGeometry = 0;
	std::uint32_t beginFrame = 0;
	std::uint32_t drawSprites = 0;
	std::uint32_t endFrame = 0;
};

/**
 * \class RecordingDevice RecordingDevice.h <litefont/TestKit.h>
 *
 * RecordingDevice - A GraphicsDevice that counts every call, so tests can
 * check that unchanged frames upload nothing. Given another device it
 * forwards every call to it; alone it hands out handles and draws nothing.
 */
class RecordingDevice : public GraphicsDevice {
public:
	RecordingDevice() = default;
	explicit RecordingDevice(GraphicsDevice& inner);

	TextureHandle createTexture(const TextureDesc& desc) override;
	void updateTexture(TextureHandle texture, const Image& source, const Recti& region) override;
	GeometryHandle createGeometry(const VertexLayout& layout) override;
	void updateGeometry(GeometryHandle geometry, const GeometryView& view) override;

	void beginFrame(Size2i target, Color4f clear) override;
	void drawSprites(GeometryHandle geometry, std::span<const SpriteBatch> batches) override;
	void endFrame() override;
	Image readPixels() const override;
	Recti getViewport() const override;

	const DeviceCallCounts& getCounts() const;
	void resetCounts();

private:
	GraphicsDevice* inner = nullptr;
	DeviceCallCounts counts;
	std::uint32_t handles = 0;
};

}
