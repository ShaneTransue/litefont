#pragma once

#include <cstddef>
#include <cstdint>

#include <vector>

namespace litefont {

/**
 * \struct AtlasSlot AtlasPacker.h <litefont/AtlasPacker.h>
 * @addtogroup Atlas
 *
 * AtlasSlot - Where a packed rectangle sits: its page and its top-left
 * corner and size in pixels.
 */
struct AtlasSlot {
	std::uint16_t page = 0;
	std::uint16_t x = 0;
	std::uint16_t y = 0;
	std::uint16_t width = 0;
	std::uint16_t height = 0;
};

/**
 * \struct AtlasShelf AtlasPacker.h <litefont/AtlasPacker.h>
 * @addtogroup Atlas
 *
 * AtlasShelf - One row of a page: rectangles no taller than height sit side
 * by side from the left, and nextX is where the next one goes.
 */
struct AtlasShelf {
	std::uint16_t y = 0;
	std::uint16_t height = 0;
	std::uint16_t nextX = 0;
};

/**
 * \class AtlasPacker AtlasPacker.h <litefont/AtlasPacker.h>
 * @addtogroup Atlas
 *
 * AtlasPacker - Packs rectangles into square pages with a shelf packer. Each
 * rectangle goes on the shelf whose height wastes the least, or on a new
 * shelf at the bottom of the page, or on the next page; pages are added up
 * to maxPages. padding pixels separate rectangles from each other and from
 * the page edges, so filtering never bleeds a neighbor in. clearPage empties
 * a page so a cache can evict it.
 */
class AtlasPacker {
public:
	AtlasPacker(std::uint16_t pageSize, std::uint16_t maxPages, std::uint8_t padding = 1);

	bool insert(std::uint16_t width, std::uint16_t height, AtlasSlot& slot);
	void clearPage(std::uint16_t page);

	std::uint16_t getPageCount() const;
	std::uint16_t getPageSize() const;
	std::uint16_t getMaxPages() const;
	std::uint8_t getPadding() const;

private:
	bool insertIntoPage(std::uint16_t page, std::uint16_t width, std::uint16_t height, AtlasSlot& slot);

	std::vector<std::vector<AtlasShelf>> shelves;
	std::uint16_t pageSize = 0;
	std::uint16_t maxPages = 0;
	std::uint8_t padding = 0;
};

}
