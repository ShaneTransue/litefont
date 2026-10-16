#include <litefont/SpriteAtlas.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace litefont {

SpriteAtlas::SpriteAtlas(std::uint16_t pageSize, PixelFormat format, std::uint16_t maxPages, std::uint8_t padding)
	: packer(pageSize, std::min(maxPages, SpriteAtlas::getPageLimit()), padding) {
	this->format = format;
}

const AtlasEntry* SpriteAtlas::find(std::uint64_t key, const SpriteRenderFunction& render) {
	this->lookups++;
	const auto found = this->entries.find(key);
	if (found != this->entries.end()) {
		if (found->second.slot.width == 0) {
			return nullptr;
		}
		this->pageLastUsed[found->second.slot.page] = this->frame;
		return &found->second;
	}

	// A miss. Empty sprites and failures are remembered as zero-size slots,
	// so they are not rendered again.
	Sprite sprite;
	AtlasEntry entry;
	if (!render(key, sprite) || sprite.image.isEmpty()) {
		entry.origin = sprite.origin;
		this->entries.emplace(key, entry);
		return nullptr;
	}
	const std::int32_t width = sprite.image.getWidth();
	const std::int32_t height = sprite.image.getHeight();
	if (width > std::numeric_limits<std::uint16_t>::max() || height > std::numeric_limits<std::uint16_t>::max()) {
		this->entries.emplace(key, entry);
		return nullptr;
	}
	AtlasSlot slot;
	const std::uint16_t slotWidth = static_cast<std::uint16_t>(width);
	const std::uint16_t slotHeight = static_cast<std::uint16_t>(height);
	if (!this->packer.insert(slotWidth, slotHeight, slot)) {
		// Not remembered: once a page frees up, the sprite can still be added.
		if (!this->evictPage() || !this->packer.insert(slotWidth, slotHeight, slot)) {
			return nullptr;
		}
	}

	while (this->pages.size() <= slot.page) {
		this->pages.emplace_back(this->packer.getPageSize(), this->packer.getPageSize(), this->format);
		this->pageLastUsed.push_back(this->frame);
		this->pageReferences.push_back(0);
	}
	this->pages[slot.page].copyFrom(sprite.image, slot.x, slot.y);
	this->pageLastUsed[slot.page] = this->frame;
	AtlasRegion region;
	region.page = slot.page;
	region.rect = Recti{slot.x, slot.y, width, height};
	this->dirty.push_back(region);

	entry.slot = slot;
	entry.origin = sprite.origin;
	return &this->entries.insert_or_assign(key, entry).first->second;
}

void SpriteAtlas::beginFrame() {
	this->frame++;
}

void SpriteAtlas::addPageReference(std::uint16_t page) {
	if (page < this->pageReferences.size()) {
		this->pageReferences[page]++;
	}
}

void SpriteAtlas::removePageReference(std::uint16_t page) {
	if (page < this->pageReferences.size() && this->pageReferences[page] > 0) {
		this->pageReferences[page]--;
	}
}

std::vector<AtlasRegion> SpriteAtlas::takeDirtyRegions() {
	return std::exchange(this->dirty, {});
}

const Image& SpriteAtlas::getPage(std::uint16_t index) const {
	return this->pages[index];
}

std::uint16_t SpriteAtlas::getPageCount() const {
	return static_cast<std::uint16_t>(this->pages.size());
}

std::uint16_t SpriteAtlas::getPageSize() const {
	return this->packer.getPageSize();
}

std::uint16_t SpriteAtlas::getMaxPages() const {
	return this->packer.getMaxPages();
}

PixelFormat SpriteAtlas::getFormat() const {
	return this->format;
}

std::uint64_t SpriteAtlas::getLookupCount() const {
	return this->lookups;
}

std::uint32_t SpriteAtlas::getEvictionCount() const {
	return this->evictions;
}

std::uint16_t SpriteAtlas::getPageLimit() {
	// Users track the pages they draw from in a 64-bit mask.
	return 64;
}

bool SpriteAtlas::evictPage() {
	bool found = false;
	std::uint16_t oldest = 0;
	for (std::uint16_t page = 0; page < this->pages.size(); page++) {
		const bool free = this->pageReferences[page] == 0 && this->pageLastUsed[page] < this->frame;
		if (free && (!found || this->pageLastUsed[page] < this->pageLastUsed[oldest])) {
			found = true;
			oldest = page;
		}
	}
	if (!found) {
		return false;
	}

	this->packer.clearPage(oldest);
	std::erase_if(this->entries, [oldest](const std::pair<const std::uint64_t, AtlasEntry>& item) {
		return item.second.slot.width != 0 && item.second.slot.page == oldest;
	});
	this->pages[oldest].fill(Color4f{0.0f, 0.0f, 0.0f, 0.0f});
	AtlasRegion region;
	region.page = oldest;
	region.rect = this->pages[oldest].getBounds();
	this->dirty.push_back(region);
	this->evictions++;
	return true;
}

}
