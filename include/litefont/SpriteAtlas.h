#pragma once

#include <litefont/Image.h>
#include <litefont/Sprite.h>
#include <litefont/Blur.h>
#include <litefont/Rect.h>

#include <litefont/AtlasPacker.h>

#include <functional>
#include <unordered_map>
#include <vector>

namespace litefont {

/**
 * \struct AtlasEntry SpriteAtlas.h <litefont/SpriteAtlas.h>
 * @addtogroup Atlas
 *
 * AtlasEntry - Where a sprite sits in the atlas, and the offset from its pen
 * or anchor to its top-left pixel.
 */
struct AtlasEntry {
	AtlasSlot slot;
	Vector2f origin;
};

/**
 * \struct AtlasRegion SpriteAtlas.h <litefont/SpriteAtlas.h>
 * @addtogroup Atlas
 *
 * AtlasRegion - A rectangle of one page whose pixels changed.
 */
struct AtlasRegion {
	std::uint16_t page = 0;
	Recti rect;
};

/** SpriteRenderFunction - Renders the sprite for a key on a cache miss; false if it cannot. */
using SpriteRenderFunction = std::function<bool(std::uint64_t key, Sprite& sprite)>;

/**
 * \class SpriteAtlas SpriteAtlas.h <litefont/SpriteAtlas.h>
 * @addtogroup Atlas
 *
 * SpriteAtlas - Keeps sprites in square page images it owns, keyed by a
 * 64-bit key. find() returns the entry for a key, rendering, packing and
 * copying the sprite on a miss and recording the pixels it wrote as a dirty
 * region. Empty sprites and keys that fail to render are remembered too, and
 * find() returns nullptr for them. It knows nothing about fonts, so icons
 * can share it.
 *
 * When every page is full, the least recently used page is cleared and
 * reused, but never a page someone holds a reference to (geometry still
 * drawing from it) and never one used since the last beginFrame(). If no
 * page can be cleared, find() returns nullptr and the sprite is skipped.
 */
class SpriteAtlas {
public:
	SpriteAtlas(std::uint16_t pageSize, PixelFormat format, std::uint16_t maxPages = 8, std::uint8_t padding = 1);

	const AtlasEntry* find(std::uint64_t key, const SpriteRenderFunction& render);
	void beginFrame();
	void addPageReference(std::uint16_t page);
	void removePageReference(std::uint16_t page);
	std::vector<AtlasRegion> takeDirtyRegions();

	const Image& getPage(std::uint16_t index) const;
	std::uint16_t getPageCount() const;
	std::uint16_t getPageSize() const;
	std::uint16_t getMaxPages() const;
	PixelFormat getFormat() const;
	std::uint64_t getLookupCount() const;
	std::uint32_t getEvictionCount() const;

	static std::uint16_t getPageLimit();

private:
	bool evictPage();

	AtlasPacker packer;
	PixelFormat format = PixelFormat::Gray8;
	std::vector<Image> pages;
	std::vector<std::uint32_t> pageLastUsed;
	std::vector<std::uint32_t> pageReferences;
	std::unordered_map<std::uint64_t, AtlasEntry> entries;
	std::vector<AtlasRegion> dirty;
	std::uint32_t frame = 1;
	std::uint64_t lookups = 0;
	std::uint32_t evictions = 0;
};

}
