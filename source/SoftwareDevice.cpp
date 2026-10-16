#include <litefont/SoftwareDevice.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace litefont {

TextureHandle SoftwareDevice::createTexture(const TextureDesc& desc) {
	this->textures.emplace_back(desc.width, desc.height, desc.format);
	return TextureHandle{static_cast<std::uint32_t>(this->textures.size())};
}

void SoftwareDevice::updateTexture(TextureHandle texture, const Image& source, const Recti& region) {
	if (texture.value == 0 || texture.value > this->textures.size()) {
		return;
	}
	this->textures[texture.value - 1].copyFrom(source, region, region.x, region.y);
}

GeometryHandle SoftwareDevice::createGeometry(const VertexLayout& layout) {
	SoftwareGeometry geometry;
	geometry.layout = layout;
	this->geometries.push_back(std::move(geometry));
	return GeometryHandle{static_cast<std::uint32_t>(this->geometries.size())};
}

void SoftwareDevice::updateGeometry(GeometryHandle geometry, const GeometryView& view) {
	if (geometry.value == 0 || geometry.value > this->geometries.size()) {
		return;
	}
	SoftwareGeometry& target = this->geometries[geometry.value - 1];
	target.layout = view.layout;
	target.vertexBytes.assign(view.vertexBytes.begin(), view.vertexBytes.end());
	target.indices.assign(view.indices.begin(), view.indices.end());
}

void SoftwareDevice::beginFrame(Size2i target, Color4f clear) {
	this->frame = Image(target.width, target.height, PixelFormat::RGBA8);
	this->frame.fill(clear);
}

void SoftwareDevice::drawSprites(GeometryHandle geometry, std::span<const SpriteBatch> batches) {
	if (geometry.value == 0 || geometry.value > this->geometries.size() || this->frame.isEmpty()) {
		return;
	}
	const SoftwareGeometry& source = this->geometries[geometry.value - 1];
	for (const SpriteBatch& batch : batches) {
		if (batch.encoding != SpriteEncoding::Coverage || batch.texture.value == 0 || batch.texture.value > this->textures.size()) {
			continue;
		}
		const Image& texture = this->textures[batch.texture.value - 1];
		const Recti clip = batch.clip.isEmpty() ? this->frame.getBounds() : batch.clip.intersected(this->frame.getBounds());
		const std::uint64_t end = std::min<std::uint64_t>(std::uint64_t{batch.firstIndex} + batch.indexCount, source.indices.size());
		for (std::uint64_t i = batch.firstIndex; i + 3 <= end; i += 3) {
			std::array<SoftwareVertex, 3> corners;
			bool good = true;
			for (std::size_t corner = 0; corner < 3; corner++) {
				good = good && SoftwareDevice::readVertex(source, source.indices[static_cast<std::size_t>(i) + corner], corners[corner]);
			}
			if (good) {
				this->drawTriangle(corners, texture, clip);
			}
		}
	}
}

void SoftwareDevice::endFrame() {
}

Image SoftwareDevice::readPixels() const {
	return this->frame;
}

void SoftwareDevice::drawTriangle(const std::array<SoftwareVertex, 3>& corners, const Image& texture, const Recti& clip) {
	constexpr float farthest = 1e7f;
	for (const SoftwareVertex& corner : corners) {
		if (!std::isfinite(corner.position.x) || !std::isfinite(corner.position.y)
			|| std::fabs(corner.position.x) > farthest || std::fabs(corner.position.y) > farthest) {
			return;
		}
	}
	auto edge = [](Vector2f a, Vector2f b, Vector2f point) {
		return (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);
	};

	// Order the corners so the area is positive (clockwise with y down).
	const SoftwareVertex* v0 = &corners[0];
	const SoftwareVertex* v1 = &corners[1];
	const SoftwareVertex* v2 = &corners[2];
	float area = edge(v0->position, v1->position, v2->position);
	if (area == 0.0f) {
		return;
	}
	if (area < 0.0f) {
		std::swap(v1, v2);
		area = -area;
	}
	const Vector2f p0 = v0->position;
	const Vector2f p1 = v1->position;
	const Vector2f p2 = v2->position;

	// Top-left rule: a pixel center exactly on an edge belongs to the triangle
	// only for top edges (flat, pointing right) and left edges (pointing up).
	auto isTopLeft = [](Vector2f a, Vector2f b) {
		const float dx = b.x - a.x;
		const float dy = b.y - a.y;
		return (dy == 0.0f && dx > 0.0f) || dy < 0.0f;
	};
	const bool topLeft0 = isTopLeft(p1, p2);
	const bool topLeft1 = isTopLeft(p2, p0);
	const bool topLeft2 = isTopLeft(p0, p1);

	const std::int64_t left = std::max<std::int64_t>(clip.x, static_cast<std::int64_t>(std::floor(std::min({p0.x, p1.x, p2.x}))));
	const std::int64_t top = std::max<std::int64_t>(clip.y, static_cast<std::int64_t>(std::floor(std::min({p0.y, p1.y, p2.y}))));
	const std::int64_t right = std::min<std::int64_t>(clip.getRight(), static_cast<std::int64_t>(std::ceil(std::max({p0.x, p1.x, p2.x}))));
	const std::int64_t bottom = std::min<std::int64_t>(clip.getBottom(), static_cast<std::int64_t>(std::ceil(std::max({p0.y, p1.y, p2.y}))));

	for (std::int64_t y = top; y < bottom; y++) {
		std::uint8_t* row = this->frame.getRow(static_cast<std::int32_t>(y));
		const float centerY = static_cast<float>(y) + 0.5f;
		for (std::int64_t x = left; x < right; x++) {
			const Vector2f center{static_cast<float>(x) + 0.5f, centerY};
			const float w0 = edge(p1, p2, center);
			const float w1 = edge(p2, p0, center);
			const float w2 = edge(p0, p1, center);
			const bool inside = (w0 > 0.0f || (w0 == 0.0f && topLeft0))
				&& (w1 > 0.0f || (w1 == 0.0f && topLeft1))
				&& (w2 > 0.0f || (w2 == 0.0f && topLeft2));
			if (!inside) {
				continue;
			}
			const float l0 = w0 / area;
			const float l1 = w1 / area;
			const float l2 = w2 / area;
			const Vector2f texCoord = v0->texCoord * l0 + v1->texCoord * l1 + v2->texCoord * l2;
			const Color4f texel = SoftwareDevice::sample(texture, texCoord);
			Color4f source;
			source.r = texel.r * (v0->color.r * l0 + v1->color.r * l1 + v2->color.r * l2);
			source.g = texel.g * (v0->color.g * l0 + v1->color.g * l1 + v2->color.g * l2);
			source.b = texel.b * (v0->color.b * l0 + v1->color.b * l1 + v2->color.b * l2);
			source.a = texel.a * (v0->color.a * l0 + v1->color.a * l1 + v2->color.a * l2);
			SoftwareDevice::blend(row + static_cast<std::size_t>(x) * 4, source);
		}
	}
}

bool SoftwareDevice::readVertex(const SoftwareGeometry& geometry, std::uint32_t index, SoftwareVertex& vertex) {
	const VertexLayout& layout = geometry.layout;
	const std::size_t stride = layout.getStride();
	const std::size_t start = static_cast<std::size_t>(index) * stride;
	if (stride == 0 || start + stride > geometry.vertexBytes.size()) {
		return false;
	}
	const std::uint8_t* bytes = geometry.vertexBytes.data() + start;
	auto readable = [stride](const VertexAttribute* attribute, std::size_t size) {
		return attribute != nullptr && attribute->offset + size <= stride
			&& VertexLayout::getSize(attribute->format) >= size;
	};

	const VertexAttribute* position = layout.find(VertexSemantic::Position);
	if (!readable(position, 8) || position->format == AttributeFormat::UNorm8x4) {
		return false;
	}
	std::memcpy(&vertex.position.x, bytes + position->offset, 4);
	std::memcpy(&vertex.position.y, bytes + position->offset + 4, 4);

	vertex.texCoord = Vector2f{};
	const VertexAttribute* texCoord = layout.find(VertexSemantic::TexCoord);
	if (readable(texCoord, 8) && texCoord->format != AttributeFormat::UNorm8x4) {
		std::memcpy(&vertex.texCoord.x, bytes + texCoord->offset, 4);
		std::memcpy(&vertex.texCoord.y, bytes + texCoord->offset + 4, 4);
	}

	vertex.color = Color4f{1.0f, 1.0f, 1.0f, 1.0f};
	const VertexAttribute* color = layout.find(VertexSemantic::Color);
	if (readable(color, 4) && color->format == AttributeFormat::UNorm8x4) {
		vertex.color = Color4u8{bytes[color->offset], bytes[color->offset + 1], bytes[color->offset + 2], bytes[color->offset + 3]}.convert<float>();
	}
	else if (readable(color, 16) && color->format == AttributeFormat::Float32x4) {
		std::memcpy(&vertex.color.r, bytes + color->offset, 4);
		std::memcpy(&vertex.color.g, bytes + color->offset + 4, 4);
		std::memcpy(&vertex.color.b, bytes + color->offset + 8, 4);
		std::memcpy(&vertex.color.a, bytes + color->offset + 12, 4);
	}
	return true;
}

Color4f SoftwareDevice::sample(const Image& texture, Vector2f texCoord) {
	if (texture.isEmpty() || !std::isfinite(texCoord.x) || !std::isfinite(texCoord.y)) {
		return Color4f{0.0f, 0.0f, 0.0f, 0.0f};
	}
	// Texel centers sit at half coordinates. A position within 1e-4 of a
	// center counts as on it, so whole-pixel sprites copy texels exactly.
	constexpr float snap = 1e-4f;
	const float width = static_cast<float>(texture.getWidth());
	const float height = static_cast<float>(texture.getHeight());
	const float x = std::clamp(texCoord.x, -1.0f, 2.0f) * width - 0.5f;
	const float y = std::clamp(texCoord.y, -1.0f, 2.0f) * height - 0.5f;
	float x0 = std::floor(x);
	float y0 = std::floor(y);
	float fx = x - x0;
	float fy = y - y0;
	if (fx < snap) {
		fx = 0.0f;
	}
	else if (fx > 1.0f - snap) {
		fx = 0.0f;
		x0 += 1.0f;
	}
	if (fy < snap) {
		fy = 0.0f;
	}
	else if (fy > 1.0f - snap) {
		fy = 0.0f;
		y0 += 1.0f;
	}

	auto fetch = [&texture](float column, float row) {
		const std::int32_t cx = std::clamp(static_cast<std::int32_t>(column), 0, texture.getWidth() - 1);
		const std::int32_t cy = std::clamp(static_cast<std::int32_t>(row), 0, texture.getHeight() - 1);
		const Color4f color = texture.getPixel(cx, cy).convert<float>();
		if (texture.getFormat() == PixelFormat::Gray8) {
			return Color4f{1.0f, 1.0f, 1.0f, color.r};
		}
		return color;
	};
	auto mix = [](const Color4f& a, const Color4f& b, float t) {
		return Color4f{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
	};
	const Color4f topRow = fx == 0.0f ? fetch(x0, y0) : mix(fetch(x0, y0), fetch(x0 + 1.0f, y0), fx);
	if (fy == 0.0f) {
		return topRow;
	}
	const Color4f bottomRow = fx == 0.0f ? fetch(x0, y0 + 1.0f) : mix(fetch(x0, y0 + 1.0f), fetch(x0 + 1.0f, y0 + 1.0f), fx);
	return mix(topRow, bottomRow, fy);
}

void SoftwareDevice::blend(std::uint8_t* pixel, const Color4f& source) {
	const float alpha = std::clamp(source.a, 0.0f, 1.0f);
	if (alpha <= 0.0f) {
		return;
	}
	const float keep = 1.0f - alpha;
	auto mix = [alpha, keep](float color, std::uint8_t under) {
		return ChannelConversion::convert<std::uint8_t>(color * alpha + static_cast<float>(under) / 255.0f * keep);
	};
	pixel[0] = mix(source.r, pixel[0]);
	pixel[1] = mix(source.g, pixel[1]);
	pixel[2] = mix(source.b, pixel[2]);
	pixel[3] = ChannelConversion::convert<std::uint8_t>(alpha + static_cast<float>(pixel[3]) / 255.0f * keep);
}

}
