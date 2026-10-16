#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <concepts>
#include <initializer_list>
#include <span>
#include <type_traits>
#include <utility>

namespace litefont {

/**
 * @addtogroup Graphics
 *
 * VertexSemantic - What a vertex attribute means to whoever draws it.
 */
enum class VertexSemantic : std::uint8_t {
	Position,
	TexCoord,
	Color,
	Normal,
	Custom
};

/**
 * @addtogroup Graphics
 *
 * AttributeFormat - How a vertex attribute is stored: two to four 32-bit
 * floats, or four bytes read as values from 0 to 1.
 */
enum class AttributeFormat : std::uint8_t {
	Float32x2,
	Float32x3,
	Float32x4,
	UNorm8x4
};

/**
 * \struct VertexAttribute Vertex.h <litefont/Vertex.h>
 * @addtogroup Graphics
 *
 * VertexAttribute - One attribute of a vertex layout: its meaning, its
 * format and its byte offset inside the vertex.
 */
struct VertexAttribute {
	VertexSemantic semantic = VertexSemantic::Position;
	AttributeFormat format = AttributeFormat::Float32x2;
	std::uint16_t offset = 0;

	constexpr bool operator==(const VertexAttribute&) const = default;
};

/**
 * \class VertexLayout Vertex.h <litefont/Vertex.h>
 * @addtogroup Graphics
 *
 * VertexLayout - The attributes of a vertex type, with offsets and stride
 * computed when the layout is built attribute by attribute in member order.
 * Devices read vertices through layouts, so they never need the vertex type.
 */
class VertexLayout {
public:
	static constexpr VertexLayout sequential(std::initializer_list<std::pair<VertexSemantic, AttributeFormat>> attributes);

	constexpr std::span<const VertexAttribute> getAttributes() const;
	constexpr const VertexAttribute* find(VertexSemantic semantic) const;
	constexpr std::uint16_t getStride() const;

	constexpr bool operator==(const VertexLayout&) const = default;

	static constexpr std::uint16_t getSize(AttributeFormat format);

private:
	std::array<VertexAttribute, 8> attributes{};
	std::uint8_t count = 0;
	std::uint16_t stride = 0;
};

/** VertexType - A plain vertex struct whose layout describes exactly its bytes. */
template <typename V>
concept VertexType = std::is_standard_layout_v<V> && std::is_trivially_copyable_v<V> &&
	requires {
		{ V::layout() } -> std::same_as<VertexLayout>;
	};

/** Positioned2D - Any vertex with an x and a y, whatever else it holds. */
template <typename V>
concept Positioned2D = requires(const V& vertex) {
	{ vertex.x } -> std::convertible_to<float>;
	{ vertex.y } -> std::convertible_to<float>;
};

/**
 * \struct Vertex2D Vertex.h <litefont/Vertex.h>
 * @addtogroup Graphics
 *
 * Vertex2D - The smallest vertex: a 2D position.
 */
struct Vertex2D {
	float x = 0.0f;
	float y = 0.0f;

	static constexpr VertexLayout layout();
};

/**
 * \struct SpriteVertex Vertex.h <litefont/Vertex.h>
 * @addtogroup Graphics
 *
 * SpriteVertex - A corner of a textured quad (glyphs now, icons later): a
 * position in device pixels, a texture coordinate from 0 to 1, and a color
 * packed with Color4::packRGBA8 (straight alpha).
 */
struct SpriteVertex {
	float x = 0.0f;
	float y = 0.0f;
	float u = 0.0f;
	float v = 0.0f;
	std::uint32_t rgba = 0xFFFFFFFFu;

	static constexpr VertexLayout layout();
};

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------
constexpr VertexLayout VertexLayout::sequential(std::initializer_list<std::pair<VertexSemantic, AttributeFormat>> attributes) {
	VertexLayout layout;
	for (const std::pair<VertexSemantic, AttributeFormat>& attribute : attributes) {
		if (layout.count == layout.attributes.size()) {
			break;
		}
		VertexAttribute& slot = layout.attributes[layout.count];
		slot.semantic = attribute.first;
		slot.format = attribute.second;
		slot.offset = layout.stride;
		layout.stride = static_cast<std::uint16_t>(layout.stride + VertexLayout::getSize(attribute.second));
		layout.count++;
	}
	return layout;
}

constexpr std::span<const VertexAttribute> VertexLayout::getAttributes() const {
	return std::span<const VertexAttribute>(this->attributes.data(), this->count);
}

constexpr const VertexAttribute* VertexLayout::find(VertexSemantic semantic) const {
	for (std::uint8_t i = 0; i < this->count; i++) {
		if (this->attributes[i].semantic == semantic) {
			return &this->attributes[i];
		}
	}
	return nullptr;
}

constexpr std::uint16_t VertexLayout::getStride() const {
	return this->stride;
}

constexpr std::uint16_t VertexLayout::getSize(AttributeFormat format) {
	switch (format) {
	case AttributeFormat::Float32x2:
		return 8;
	case AttributeFormat::Float32x3:
		return 12;
	case AttributeFormat::Float32x4:
		return 16;
	case AttributeFormat::UNorm8x4:
		return 4;
	}
	return 0;
}

constexpr VertexLayout Vertex2D::layout() {
	return VertexLayout::sequential({{VertexSemantic::Position, AttributeFormat::Float32x2}});
}

constexpr VertexLayout SpriteVertex::layout() {
	return VertexLayout::sequential({
		{VertexSemantic::Position, AttributeFormat::Float32x2},
		{VertexSemantic::TexCoord, AttributeFormat::Float32x2},
		{VertexSemantic::Color, AttributeFormat::UNorm8x4},
	});
}

}
