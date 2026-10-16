#pragma once

#include <litefont/Rect.h>

#include <litefont/Vertex.h>

#include <algorithm>
#include <span>
#include <vector>

namespace litefont {

/**
 * @addtogroup Graphics
 *
 * SpriteEncoding - How a sprite's pixels encode its shape: coverage (the
 * share of each pixel inside the shape) or a distance field.
 */
enum class SpriteEncoding : std::uint8_t {
	Coverage,
	DistanceField
};

/**
 * \struct TextureHandle Geometry.h <litefont/Geometry.h>
 * @addtogroup Graphics
 *
 * TextureHandle - A texture owned by a GraphicsDevice; 0 is no texture.
 */
struct TextureHandle {
	std::uint32_t value = 0;

	bool operator==(const TextureHandle&) const = default;
};

/**
 * \struct GeometryHandle Geometry.h <litefont/Geometry.h>
 * @addtogroup Graphics
 *
 * GeometryHandle - A geometry buffer owned by a GraphicsDevice; 0 is none.
 */
struct GeometryHandle {
	std::uint32_t value = 0;

	bool operator==(const GeometryHandle&) const = default;
};

/**
 * \struct SpriteBatch Geometry.h <litefont/Geometry.h>
 * @addtogroup Graphics
 *
 * SpriteBatch - A range of indices (whole triangles) drawn with one sprite
 * encoding and one texture, inside a clip rectangle in device pixels. An
 * empty clip rectangle means no clipping.
 */
struct SpriteBatch {
	SpriteEncoding encoding = SpriteEncoding::Coverage;
	TextureHandle texture;
	std::uint32_t firstIndex = 0;
	std::uint32_t indexCount = 0;
	Recti clip;
};

/**
 * \struct GeometryView Geometry.h <litefont/Geometry.h>
 * @addtogroup Graphics
 *
 * GeometryView - Geometry with its vertex type erased: the vertex bytes,
 * the layout that reads them, and the indices. What devices receive.
 */
struct GeometryView {
	std::span<const std::uint8_t> vertexBytes;
	VertexLayout layout;
	std::span<const std::uint32_t> indices;
};

/**
 * \class Geometry Geometry.h <litefont/Geometry.h>
 * @addtogroup Graphics
 *
 * Geometry - Vertices of one vertex type and the indices that make them into
 * triangles. Every change increments the revision, so whoever uploads it
 * knows when to upload again.
 */
template <VertexType V>
class Geometry {
public:
	void clear();
	void addQuad(const std::array<V, 4>& corners);
	void add(std::span<const V> vertices, std::span<const std::uint32_t> indices);

	std::span<const V> getVertices() const;
	std::span<const std::uint32_t> getIndices() const;
	GeometryView getView() const;
	std::uint32_t getRevision() const;
	Rectf getBounds() const requires Positioned2D<V>;

private:
	std::vector<V> vertices;
	std::vector<std::uint32_t> indices;
	std::uint32_t revision = 0;
};

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------
template <VertexType V>
void Geometry<V>::clear() {
	this->vertices.clear();
	this->indices.clear();
	this->revision++;
}

template <VertexType V>
void Geometry<V>::addQuad(const std::array<V, 4>& corners) {
	// Corners go around the quad; two triangles share the first and third.
	const std::uint32_t base = static_cast<std::uint32_t>(this->vertices.size());
	this->vertices.insert(this->vertices.end(), corners.begin(), corners.end());
	const std::uint32_t quad[] = {base, base + 1, base + 2, base, base + 2, base + 3};
	this->indices.insert(this->indices.end(), std::begin(quad), std::end(quad));
	this->revision++;
}

template <VertexType V>
void Geometry<V>::add(std::span<const V> vertices, std::span<const std::uint32_t> indices) {
	const std::uint32_t base = static_cast<std::uint32_t>(this->vertices.size());
	this->vertices.insert(this->vertices.end(), vertices.begin(), vertices.end());
	for (const std::uint32_t index : indices) {
		this->indices.push_back(base + index);
	}
	this->revision++;
}

template <VertexType V>
std::span<const V> Geometry<V>::getVertices() const {
	return this->vertices;
}

template <VertexType V>
std::span<const std::uint32_t> Geometry<V>::getIndices() const {
	return this->indices;
}

template <VertexType V>
GeometryView Geometry<V>::getView() const {
	GeometryView view;
	view.vertexBytes = std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(this->vertices.data()), this->vertices.size() * sizeof(V));
	view.layout = V::layout();
	view.indices = this->indices;
	return view;
}

template <VertexType V>
std::uint32_t Geometry<V>::getRevision() const {
	return this->revision;
}

template <VertexType V>
Rectf Geometry<V>::getBounds() const requires Positioned2D<V> {
	if (this->vertices.empty()) {
		return Rectf{};
	}
	float left = this->vertices.front().x;
	float top = this->vertices.front().y;
	float right = left;
	float bottom = top;
	for (const V& vertex : this->vertices) {
		left = std::min(left, static_cast<float>(vertex.x));
		top = std::min(top, static_cast<float>(vertex.y));
		right = std::max(right, static_cast<float>(vertex.x));
		bottom = std::max(bottom, static_cast<float>(vertex.y));
	}
	return Rectf{left, top, right - left, bottom - top};
}

}
