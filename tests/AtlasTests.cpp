#include <litefont/AtlasPacker.h>
#include <litefont/SpriteAtlas.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <random>
#include <vector>

namespace litefont::tests {

class AtlasTests {
	// Two slots overlap when their rectangles, each grown by the padding on
	// the right and bottom, intersect.
	static bool overlap(const AtlasSlot& first, const AtlasSlot& second, std::int32_t padding) {
		if (first.page != second.page) {
			return false;
		}
		const Recti a{first.x, first.y, first.width + padding, first.height + padding};
		const Recti b{second.x, second.y, second.width + padding, second.height + padding};
		return !a.intersected(b).isEmpty();
	}

	static Sprite makeSprite(std::int32_t width, std::int32_t height, std::uint8_t value) {
		Sprite sprite;
		sprite.image = Image(width, height, PixelFormat::Gray8);
		sprite.image.fill(Color4f{value / 255.0f, value / 255.0f, value / 255.0f, 1.0f});
		sprite.origin = Vector2f{-1.0f, -static_cast<float>(height)};
		return sprite;
	}

	static inline test::Test packs{"AtlasPacker: packed rectangles never overlap and stay inside their page with padding", [](test::Context& context) {
		std::mt19937 random(21u);
		std::uniform_int_distribution<std::int32_t> size(1, 40);
		for (const std::uint8_t padding : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2}}) {
			AtlasPacker packer(256, 6, padding);
			std::vector<AtlasSlot> slots;
			AtlasSlot slot;
			while (packer.insert(static_cast<std::uint16_t>(size(random)), static_cast<std::uint16_t>(size(random)), slot)) {
				slots.push_back(slot);
			}
			std::int32_t wrong = 0;
			for (std::size_t i = 0; i < slots.size(); i++) {
				const AtlasSlot& s = slots[i];
				wrong += s.x >= padding && s.y >= padding && s.x + s.width + padding <= 256 && s.y + s.height + padding <= 256 ? 0 : 1;
				for (std::size_t j = i + 1; j < slots.size(); j++) {
					wrong += overlap(s, slots[j], padding) ? 1 : 0;
				}
			}
			context.checkEqual(wrong, 0);
			context.checkEqual(packer.getPageCount(), std::uint16_t{6});
			context.note(std::format("padding {}: {} rectangles in 6 pages", padding, slots.size()));
		}
	}};

	static inline test::Test efficiency{"AtlasPacker: glyph-sized rectangles fill pages efficiently", [](test::Context& context) {
		std::mt19937 random(5u);
		std::uniform_int_distribution<std::int32_t> width(4, 14);
		std::uniform_int_distribution<std::int32_t> height(9, 18);
		AtlasPacker packer(512, 1, 1);
		std::int64_t area = 0;
		AtlasSlot slot;
		std::uint16_t w = static_cast<std::uint16_t>(width(random));
		std::uint16_t h = static_cast<std::uint16_t>(height(random));
		while (packer.insert(w, h, slot)) {
			area += std::int64_t{w} * h;
			w = static_cast<std::uint16_t>(width(random));
			h = static_cast<std::uint16_t>(height(random));
		}
		const double used = static_cast<double>(area) / (512.0 * 512.0);
		context.check(used > 0.6, "more than 60 percent of the page holds glyph pixels");
		context.note(std::format("{:.0f} percent of a 512 px page used", used * 100.0));
	}};

	static inline test::Test limits{"AtlasPacker: rejects empty and oversized rectangles and reuses a cleared page", [](test::Context& context) {
		AtlasPacker packer(64, 1, 1);
		AtlasSlot slot;
		context.check(!packer.insert(0, 5, slot), "zero width");
		context.check(!packer.insert(63, 5, slot), "wider than the page minus padding");
		context.check(packer.insert(62, 62, slot), "exactly the page minus padding");
		context.check(!packer.insert(1, 1, slot), "the only page is full");
		packer.clearPage(0);
		context.check(packer.insert(10, 10, slot) && slot.x == 1 && slot.y == 1, "a cleared page starts over");
	}};

	static inline test::Test caches{"SpriteAtlas: a repeated key renders once, and empty sprites are remembered", [](test::Context& context) {
		SpriteAtlas atlas(64, PixelFormat::Gray8, 2);
		std::int32_t renders = 0;
		const SpriteRenderFunction render = [&renders](std::uint64_t key, Sprite& sprite) {
			renders++;
			if (key == 0) {
				return true;
			}
			sprite = makeSprite(static_cast<std::int32_t>(key), 3, static_cast<std::uint8_t>(key * 10));
			return true;
		};
		const AtlasEntry* first = atlas.find(5, render);
		const AtlasEntry* again = atlas.find(5, render);
		context.check(first != nullptr && first == again, "the same entry");
		context.checkEqual(renders, 1);
		context.check(atlas.find(0, render) == nullptr && atlas.find(0, render) == nullptr, "an empty sprite has no entry");
		context.checkEqual(renders, 2);
		context.check(first->origin == Vector2f{-1.0f, -3.0f}, "the origin is kept");
		context.checkEqual(atlas.getLookupCount(), std::uint64_t{4});
	}};

	static inline test::Test dirtyRegions{"SpriteAtlas: dirty regions cover exactly the pixels written, and pages hold the sprites", [](test::Context& context) {
		SpriteAtlas atlas(64, PixelFormat::Gray8, 1);
		const SpriteRenderFunction render = [](std::uint64_t key, Sprite& sprite) {
			sprite = makeSprite(static_cast<std::int32_t>(key % 7) + 2, static_cast<std::int32_t>(key % 5) + 2, static_cast<std::uint8_t>(40 + key));
			return true;
		};
		std::vector<const AtlasEntry*> entries;
		for (std::uint64_t key = 1; key <= 20; key++) {
			entries.push_back(atlas.find(key, render));
		}
		const std::vector<AtlasRegion> regions = atlas.takeDirtyRegions();
		context.checkEqual(regions.size(), std::size_t{20});
		std::int32_t wrong = 0;
		for (std::uint64_t key = 1; key <= 20; key++) {
			const AtlasEntry* entry = entries[key - 1];
			const Recti expected{entry->slot.x, entry->slot.y, entry->slot.width, entry->slot.height};
			wrong += regions[key - 1].rect == expected ? 0 : 1;
			for (std::int32_t y = expected.y; y < expected.getBottom(); y++) {
				for (std::int32_t x = expected.x; x < expected.getRight(); x++) {
					wrong += atlas.getPage(0).getRow(y)[x] == 40 + key ? 0 : 1;
				}
			}
		}
		context.checkEqual(wrong, 0);
		context.check(atlas.takeDirtyRegions().empty(), "taking clears the list");
	}};

	static inline test::Test evicts{"SpriteAtlas: eviction reuses the oldest free page and keeps this frame's sprites", [](test::Context& context) {
		// Each 30 x 30 sprite fills a 32 px page by itself, so two pages hold two sprites.
		SpriteAtlas atlas(32, PixelFormat::Gray8, 2);
		std::int32_t renders = 0;
		const SpriteRenderFunction render = [&renders](std::uint64_t key, Sprite& sprite) {
			renders++;
			sprite = makeSprite(30, 30, static_cast<std::uint8_t>(key));
			return true;
		};
		context.check(atlas.find(1, render) != nullptr && atlas.find(2, render) != nullptr, "two pages fill");
		context.check(atlas.find(3, render) == nullptr, "both pages were used this frame, so nothing is evicted");
		context.checkEqual(atlas.getEvictionCount(), std::uint32_t{0});

		atlas.beginFrame();
		context.check(atlas.find(2, render) != nullptr, "sprite 2 is used this frame");
		const AtlasEntry* third = atlas.find(3, render);
		context.check(third != nullptr && third->slot.page == 0, "page 0, unused this frame, is reused");
		context.checkEqual(atlas.getEvictionCount(), std::uint32_t{1});
		context.check(atlas.find(2, render) != nullptr, "sprite 2 survives");

		atlas.beginFrame();
		atlas.addPageReference(0);
		atlas.addPageReference(1);
		const std::int32_t before = renders;
		context.check(atlas.find(1, render) == nullptr, "referenced pages are never evicted");
		context.checkEqual(renders, before + 1);
		atlas.removePageReference(1);
		context.check(atlas.find(1, render) != nullptr, "a released page can be");
		const std::vector<AtlasRegion> regions = atlas.takeDirtyRegions();
		context.check(!regions.empty() && regions.back().page == 1, "the rewritten page is dirty");
	}};
};

}
