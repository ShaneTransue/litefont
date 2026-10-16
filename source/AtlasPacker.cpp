#include <litefont/AtlasPacker.h>

#include <algorithm>

namespace litefont {

AtlasPacker::AtlasPacker(std::uint16_t pageSize, std::uint16_t maxPages, std::uint8_t padding) {
	this->pageSize = pageSize;
	this->maxPages = maxPages;
	this->padding = padding;
}

bool AtlasPacker::insert(std::uint16_t width, std::uint16_t height, AtlasSlot& slot) {
	slot = AtlasSlot{};
	const std::int32_t largest = std::int32_t{this->pageSize} - 2 * std::int32_t{this->padding};
	if (width == 0 || height == 0 || width > largest || height > largest) {
		return false;
	}
	for (std::uint16_t page = 0; page < this->shelves.size(); page++) {
		if (this->insertIntoPage(page, width, height, slot)) {
			return true;
		}
	}
	if (this->shelves.size() >= this->maxPages) {
		return false;
	}
	this->shelves.emplace_back();
	return this->insertIntoPage(static_cast<std::uint16_t>(this->shelves.size() - 1), width, height, slot);
}

void AtlasPacker::clearPage(std::uint16_t page) {
	if (page < this->shelves.size()) {
		this->shelves[page].clear();
	}
}

std::uint16_t AtlasPacker::getPageCount() const {
	return static_cast<std::uint16_t>(this->shelves.size());
}

std::uint16_t AtlasPacker::getPageSize() const {
	return this->pageSize;
}

std::uint16_t AtlasPacker::getMaxPages() const {
	return this->maxPages;
}

std::uint8_t AtlasPacker::getPadding() const {
	return this->padding;
}

bool AtlasPacker::insertIntoPage(std::uint16_t page, std::uint16_t width, std::uint16_t height, AtlasSlot& slot) {
	std::vector<AtlasShelf>& pageShelves = this->shelves[page];
	const std::int32_t size = this->pageSize;
	const std::int32_t gap = this->padding;

	// The shelf that fits with the least height to spare.
	AtlasShelf* best = nullptr;
	for (AtlasShelf& shelf : pageShelves) {
		const bool fits = shelf.height >= height && std::int32_t{shelf.nextX} + width + gap <= size;
		if (fits && (best == nullptr || shelf.height < best->height)) {
			best = &shelf;
		}
	}
	if (best == nullptr) {
		const std::int32_t top = pageShelves.empty() ? gap : std::int32_t{pageShelves.back().y} + pageShelves.back().height + gap;
		if (top + height + gap > size) {
			return false;
		}
		AtlasShelf shelf;
		shelf.y = static_cast<std::uint16_t>(top);
		shelf.height = height;
		shelf.nextX = static_cast<std::uint16_t>(gap);
		pageShelves.push_back(shelf);
		best = &pageShelves.back();
	}

	slot.page = page;
	slot.x = best->nextX;
	slot.y = best->y;
	slot.width = width;
	slot.height = height;
	best->nextX = static_cast<std::uint16_t>(std::int32_t{best->nextX} + width + gap);
	return true;
}

}
