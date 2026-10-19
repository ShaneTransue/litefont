#include "RecordingDevice.h"

namespace litefont::test {

RecordingDevice::RecordingDevice(GraphicsDevice& inner) {
	this->inner = &inner;
}

TextureHandle RecordingDevice::createTexture(const TextureDesc& desc) {
	this->counts.createTexture++;
	if (this->inner != nullptr) {
		return this->inner->createTexture(desc);
	}
	return TextureHandle{++this->handles};
}

void RecordingDevice::updateTexture(TextureHandle texture, const Image& source, const Recti& region) {
	this->counts.updateTexture++;
	this->counts.uploadedTexels += static_cast<std::uint64_t>(region.width) * static_cast<std::uint64_t>(region.height);
	if (this->inner != nullptr) {
		this->inner->updateTexture(texture, source, region);
	}
}

GeometryHandle RecordingDevice::createGeometry(const VertexLayout& layout) {
	this->counts.createGeometry++;
	if (this->inner != nullptr) {
		return this->inner->createGeometry(layout);
	}
	return GeometryHandle{++this->handles};
}

void RecordingDevice::updateGeometry(GeometryHandle geometry, const GeometryView& view) {
	this->counts.updateGeometry++;
	if (this->inner != nullptr) {
		this->inner->updateGeometry(geometry, view);
	}
}

void RecordingDevice::beginFrame(Size2i target, Color4f clear) {
	this->counts.beginFrame++;
	if (this->inner != nullptr) {
		this->inner->beginFrame(target, clear);
	}
}

void RecordingDevice::drawSprites(GeometryHandle geometry, std::span<const SpriteBatch> batches) {
	this->counts.drawSprites++;
	if (this->inner != nullptr) {
		this->inner->drawSprites(geometry, batches);
	}
}

void RecordingDevice::endFrame() {
	this->counts.endFrame++;
	if (this->inner != nullptr) {
		this->inner->endFrame();
	}
}

Image RecordingDevice::readPixels() const {
	return this->inner != nullptr ? this->inner->readPixels() : Image();
}

Recti RecordingDevice::getViewport() const {
	return this->inner != nullptr ? this->inner->getViewport() : Recti{};
}

const DeviceCallCounts& RecordingDevice::getCounts() const {
	return this->counts;
}

void RecordingDevice::resetCounts() {
	this->counts = DeviceCallCounts{};
}

}
