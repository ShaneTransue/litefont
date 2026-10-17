#include <litefont/TextRenderer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace litefont {

TextRenderer::TextRenderer(GraphicsDevice& device, GlyphSource& glyphs, std::uint16_t atlasPageSize, std::uint16_t maxAtlasPages)
	: device(device), glyphs(glyphs), atlas(atlasPageSize, glyphs.getFormat(), maxAtlasPages) {
	this->deviceGeometry = this->device.createGeometry(SpriteVertex::layout());
	this->singleGeometry = this->device.createGeometry(SpriteVertex::layout());
}

TextHandle TextRenderer::add(const Text& text) {
	TextEntry entry;
	entry.text = &text;
	std::uint32_t index = 0;
	if (!this->freeEntries.empty()) {
		index = this->freeEntries.back();
		this->freeEntries.pop_back();
		this->entries[index] = std::move(entry);
	}
	else {
		index = static_cast<std::uint32_t>(this->entries.size());
		this->entries.push_back(std::move(entry));
	}
	return TextHandle{index + 1};
}

void TextRenderer::remove(TextHandle handle) {
	if (handle.value == 0 || handle.value > this->entries.size() || this->entries[handle.value - 1].text == nullptr) {
		return;
	}
	TextEntry& entry = this->entries[handle.value - 1];
	this->releasePages(entry);
	entry = TextEntry{};
	this->freeEntries.push_back(handle.value - 1);
	this->geometryChanged = true;
}

void TextRenderer::update(float deviceScale) {
	this->atlas.beginFrame();
	for (TextEntry& entry : this->entries) {
		if (entry.text != nullptr && this->refresh(entry, deviceScale)) {
			this->geometryChanged = true;
		}
	}
	this->uploadAtlas();
	if (this->geometryChanged) {
		this->rebuildGeometry();
		this->device.updateGeometry(this->deviceGeometry, this->geometry.getView());
		this->geometryChanged = false;
	}
}

void TextRenderer::draw() const {
	if (!this->batches.empty()) {
		this->device.drawSprites(this->deviceGeometry, this->batches);
	}
}

void TextRenderer::drawNow(TextHandle handle, float deviceScale) {
	if (handle.value == 0 || handle.value > this->entries.size() || this->entries[handle.value - 1].text == nullptr) {
		return;
	}
	// Every other Text holds references to its pages, so starting a frame per
	// call never lets the atlas reuse a page a cached Text still draws from.
	TextEntry& entry = this->entries[handle.value - 1];
	this->atlas.beginFrame();
	if (this->refresh(entry, deviceScale)) {
		this->geometryChanged = true;
	}
	this->uploadAtlas();

	// This Text's quads alone, layer by layer and page by page.
	this->single.clear();
	this->singleBatches.clear();
	if (!(entry.clipped && entry.clip.isEmpty())) {
		for (const TextLayer layer : {TextLayer::Background, TextLayer::Shadow, TextLayer::Outline, TextLayer::Fill}) {
			for (std::uint16_t page = 0; page < this->pageTextures.size(); page++) {
				const std::uint32_t firstIndex = static_cast<std::uint32_t>(this->single.getIndices().size());
				this->appendQuads(entry, layer, page, this->single);
				const std::uint32_t indexCount = static_cast<std::uint32_t>(this->single.getIndices().size()) - firstIndex;
				if (indexCount > 0) {
					this->singleBatches.push_back(SpriteBatch{this->glyphs.getEncoding(), this->pageTextures[page], firstIndex, indexCount, entry.clipped ? entry.clip : Recti{}});
				}
			}
		}
	}
	if (!this->singleBatches.empty()) {
		this->device.updateGeometry(this->singleGeometry, this->single.getView());
		this->device.drawSprites(this->singleGeometry, this->singleBatches);
	}
}

void TextRenderer::prepare(TextHandle handle, float deviceScale) {
	if (handle.value == 0 || handle.value > this->entries.size() || this->entries[handle.value - 1].text == nullptr) {
		return;
	}
	this->atlas.beginFrame();
	if (this->refresh(this->entries[handle.value - 1], deviceScale)) {
		this->geometryChanged = true;
	}
}

void TextRenderer::setSubpixelBins(std::uint8_t bins) {
	const std::uint8_t accepted = bins == 2 || bins == 4 ? bins : std::uint8_t{1};
	if (accepted == this->subpixelBins) {
		return;
	}
	this->subpixelBins = accepted;
	for (TextEntry& entry : this->entries) {
		entry.stale = true;
	}
}

std::uint8_t TextRenderer::getSubpixelBins() const {
	return this->subpixelBins;
}

const TextLayout* TextRenderer::getLayout(TextHandle handle) const {
	if (handle.value == 0 || handle.value > this->entries.size() || this->entries[handle.value - 1].text == nullptr) {
		return nullptr;
	}
	return &this->entries[handle.value - 1].layout;
}

const SpriteAtlas& TextRenderer::getAtlas() const {
	return this->atlas;
}

const Geometry<SpriteVertex>& TextRenderer::getGeometry() const {
	return this->geometry;
}

std::span<const SpriteBatch> TextRenderer::getBatches() const {
	return this->batches;
}

std::uint64_t TextRenderer::getRebuildCount() const {
	return this->rebuilds;
}

void TextRenderer::rebuild(TextEntry& entry) {
	const Text& text = *entry.text;
	entry.layout.build(text, this->deviceScale);
	entry.revision = text.getRevision();
	entry.stale = false;
	entry.vertices.clear();
	entry.quadPages.clear();
	entry.quadLayers.clear();
	this->rebuilds++;

	std::uint64_t pageMask = 0;
	const Font* font = text.getFont();
	const float largestSize64 = static_cast<float>(std::numeric_limits<std::uint32_t>::max() / 2);
	if (font != nullptr && font->isLoaded() && text.getSize() * this->deviceScale > 0.0f) {
		const std::vector<TextColorRun>& runs = text.getColorRuns();
		const std::uint32_t color = text.getColor().packRGBA8();
		const float bins = static_cast<float>(this->subpixelBins);
		const TextOutline& outline = text.getOutline();
		const TextShadow& shadow = text.getShadow();
		const TextBackground& background = text.getBackground();
		const float outlineWidth16 = std::clamp(std::round(outline.width * this->deviceScale * 16.0f), 0.0f, 65535.0f);
		const float blurRadius16 = std::clamp(std::round(shadow.blur * this->deviceScale * 16.0f), 0.0f, 65535.0f);
		const bool outlined = outlineWidth16 > 0.0f && outline.color.a > 0.0f;
		const bool shadowed = shadow.color.a > 0.0f;
		const Vector2f shadowOffset{std::round(shadow.offset.x * this->deviceScale), std::round(shadow.offset.y * this->deviceScale)};
		// A gradient spans the laid-out box, before any rotation, so it turns with the Text.
		const bool shaded = text.hasGradient();
		const Rectf shadedBox = entry.layout.getBounds();

		if (background.color.a > 0.0f) {
			const Rectf bounds = entry.layout.getBounds();
			const float padding = std::round(background.padding * this->deviceScale);
			const Rectf box{std::round(bounds.x) - padding, std::round(bounds.y) - padding,
				std::round(bounds.width) + 2.0f * padding, std::round(bounds.height) + 2.0f * padding};
			this->addSolid(entry, box, background.color.packRGBA8(), TextLayer::Background, pageMask);
		}

		std::size_t run = 0;
		for (const PlacedGlyph& placed : entry.layout.getGlyphs()) {
			// Each glyph renders at its own size: superscripts are smaller.
			const float size64 = std::round(placed.size * 64.0f);
			if (!(size64 >= 1.0f && size64 <= largestSize64)) {
				continue;
			}
			// The pen snaps to a whole pixel, or to the nearest bin of one, which
			// the glyph source renders as a shift in quarter pixels.
			const float steps = std::round(placed.pen.x * bins);
			const float whole = std::floor(steps / bins);
			const Font& face = placed.face.font != nullptr ? *placed.face.font : *font;
			GlyphKey key;
			key.font = face.getId();
			key.glyph = placed.glyph;
			key.sizePx64 = static_cast<std::uint32_t>(size64);
			key.subpixelBin = static_cast<std::uint8_t>((steps - whole * bins) * (4.0f / bins));
			key.simulatedBold = placed.face.simulatedBold;
			key.simulatedItalic = placed.face.simulatedItalic;
			auto find = [this, &face](const GlyphKey& glyphKey) {
				return this->atlas.find(glyphKey.getHash(), [this, &face, &glyphKey](std::uint64_t, Sprite& sprite) {
					return this->glyphs.render(face, glyphKey, sprite);
				});
			};
			const AtlasEntry* found = find(key);
			if (found == nullptr) {
				continue;
			}
			GlyphKey outlineKey = key;
			const AtlasEntry* border = nullptr;
			if (outlined) {
				outlineKey.outlineWidth16 = static_cast<std::uint16_t>(outlineWidth16);
				border = find(outlineKey);
			}
			if (shadowed) {
				// A soft shadow is its own sprite: the glyph or its outline, blurred.
				const AtlasEntry* behind = border != nullptr ? border : found;
				if (blurRadius16 > 0.0f) {
					GlyphKey shadowKey = border != nullptr ? outlineKey : key;
					shadowKey.blurRadius16 = static_cast<std::uint16_t>(blurRadius16);
					behind = find(shadowKey);
				}
				if (behind != nullptr) {
					this->addQuad(entry, *behind, whole + shadowOffset.x, placed.pen.y + shadowOffset.y, shadow.color.packRGBA8(), TextLayer::Shadow, pageMask);
				}
			}
			if (border != nullptr) {
				this->addQuad(entry, *border, whole, placed.pen.y, outline.color.packRGBA8(), TextLayer::Outline, pageMask);
			}

			while (run + 1 < runs.size() && runs[run + 1].firstByte <= placed.byteOffset) {
				run++;
			}
			std::uint32_t rgba = color;
			bool inRun = false;
			if (run < runs.size() && runs[run].firstByte <= placed.byteOffset && placed.byteOffset - runs[run].firstByte < runs[run].byteCount) {
				rgba = runs[run].color.packRGBA8();
				inRun = true;
			}

			const std::size_t firstVertex = entry.vertices.size();
			this->addQuad(entry, *found, whole, placed.pen.y, rgba, TextLayer::Fill, pageMask);
			if (shaded && !inRun) {
				TextRenderer::shade(std::span<SpriteVertex>(entry.vertices).subspan(firstVertex), text.getGradient(), shadedBox);
			}
		}

		for (const TextDecoration& decoration : entry.layout.getDecorations()) {
			const std::int32_t colorRun = TextLayout::findColorRun(runs, decoration.byteOffset);
			const std::uint32_t rgba = colorRun >= 0 ? runs[static_cast<std::size_t>(colorRun)].color.packRGBA8() : color;
			const std::size_t firstVertex = entry.vertices.size();
			this->addSolid(entry, decoration.rect, rgba, TextLayer::Fill, pageMask);
			if (shaded && colorRun < 0) {
				TextRenderer::shade(std::span<SpriteVertex>(entry.vertices).subspan(firstVertex), text.getGradient(), shadedBox);
			}
		}

		// A quarter turn about the position, which is a whole device pixel.
		if (text.getRotation() != TextRotation::None) {
			const Vector2f pivot{std::round(text.getPosition().x * this->deviceScale), std::round(text.getPosition().y * this->deviceScale)};
			TextRenderer::rotate(entry.vertices, pivot, text.getRotation());
		}
	}

	// Hold a reference to every page the quads sample, so the atlas never
	// evicts a page that retained geometry still draws from.
	for (std::uint16_t page = 0; page < SpriteAtlas::getPageLimit(); page++) {
		const std::uint64_t bit = std::uint64_t{1} << page;
		if ((pageMask & bit) != 0 && (entry.pageMask & bit) == 0) {
			this->atlas.addPageReference(page);
		}
		else if ((pageMask & bit) == 0 && (entry.pageMask & bit) != 0) {
			this->atlas.removePageReference(page);
		}
	}
	entry.pageMask = pageMask;
}

void TextRenderer::addQuad(TextEntry& entry, const AtlasEntry& sprite, float x, float y, std::uint32_t rgba, TextLayer layer, std::uint64_t& pageMask) const {
	const float pageSize = static_cast<float>(this->atlas.getPageSize());
	const AtlasSlot& slot = sprite.slot;
	const float left = x + sprite.origin.x;
	const float top = y + sprite.origin.y;
	const float right = left + static_cast<float>(slot.width);
	const float bottom = top + static_cast<float>(slot.height);
	const float u0 = static_cast<float>(slot.x) / pageSize;
	const float v0 = static_cast<float>(slot.y) / pageSize;
	const float u1 = static_cast<float>(slot.x + slot.width) / pageSize;
	const float v1 = static_cast<float>(slot.y + slot.height) / pageSize;
	entry.vertices.push_back(SpriteVertex{left, top, u0, v0, rgba});
	entry.vertices.push_back(SpriteVertex{right, top, u1, v0, rgba});
	entry.vertices.push_back(SpriteVertex{right, bottom, u1, v1, rgba});
	entry.vertices.push_back(SpriteVertex{left, bottom, u0, v1, rgba});
	entry.quadPages.push_back(slot.page);
	entry.quadLayers.push_back(layer);
	pageMask |= std::uint64_t{1} << slot.page;
}

void TextRenderer::addSolid(TextEntry& entry, const Rectf& rect, std::uint32_t rgba, TextLayer layer, std::uint64_t& pageMask) {
	// Every corner samples the middle of a 3 x 3 white sprite, so any size of
	// quad is one solid color, also with filtering.
	const AtlasEntry* solid = this->atlas.find(TextRenderer::getSolidKey(), [](std::uint64_t, Sprite& sprite) {
		sprite.image = Image(3, 3, PixelFormat::Gray8);
		sprite.image.fill(Color4f{1.0f, 1.0f, 1.0f, 1.0f});
		return true;
	});
	if (solid == nullptr || rect.width <= 0.0f || rect.height <= 0.0f) {
		return;
	}
	const float pageSize = static_cast<float>(this->atlas.getPageSize());
	const float u = (static_cast<float>(solid->slot.x) + 1.5f) / pageSize;
	const float v = (static_cast<float>(solid->slot.y) + 1.5f) / pageSize;
	const float right = rect.x + rect.width;
	const float bottom = rect.y + rect.height;
	entry.vertices.push_back(SpriteVertex{rect.x, rect.y, u, v, rgba});
	entry.vertices.push_back(SpriteVertex{right, rect.y, u, v, rgba});
	entry.vertices.push_back(SpriteVertex{right, bottom, u, v, rgba});
	entry.vertices.push_back(SpriteVertex{rect.x, bottom, u, v, rgba});
	entry.quadPages.push_back(solid->slot.page);
	entry.quadLayers.push_back(layer);
	pageMask |= std::uint64_t{1} << solid->slot.page;
}

void TextRenderer::getPlacement(const Text& text, Vector2f& offset, Recti& clip, bool& clipped) const {
	// The offset moves by whole device pixels; the clip covers every device
	// pixel its rectangle touches.
	offset = Vector2f{std::round(text.getOffset().x * this->deviceScale), std::round(text.getOffset().y * this->deviceScale)};
	clipped = text.isClipped();
	clip = Recti{};
	if (clipped) {
		const Rectf& area = text.getClip();
		const float left = std::floor(area.x * this->deviceScale);
		const float top = std::floor(area.y * this->deviceScale);
		const float right = std::ceil(area.getRight() * this->deviceScale);
		const float bottom = std::ceil(area.getBottom() * this->deviceScale);
		const float limit = 1e8f;
		const bool hasArea = area.width > 0.0f && area.height > 0.0f;
		if (hasArea && right > left && bottom > top && std::fabs(left) < limit && std::fabs(top) < limit && right < limit && bottom < limit) {
			clip = Recti{static_cast<std::int32_t>(left), static_cast<std::int32_t>(top), static_cast<std::int32_t>(right - left), static_cast<std::int32_t>(bottom - top)};
		}
	}
}

void TextRenderer::releasePages(TextEntry& entry) {
	for (std::uint16_t page = 0; page < SpriteAtlas::getPageLimit(); page++) {
		if ((entry.pageMask & (std::uint64_t{1} << page)) != 0) {
			this->atlas.removePageReference(page);
		}
	}
	entry.pageMask = 0;
}

void TextRenderer::uploadAtlas() {
	// One upload per changed page: the union of its dirty rectangles, or the
	// whole page the first time, so a GPU texture never shows stale memory.
	const std::uint16_t pageSize = this->atlas.getPageSize();
	std::vector<Recti> changed(this->atlas.getPageCount());
	while (this->pageTextures.size() < this->atlas.getPageCount()) {
		changed[this->pageTextures.size()] = Recti{0, 0, pageSize, pageSize};
		this->pageTextures.push_back(this->device.createTexture(TextureDesc{pageSize, pageSize, this->atlas.getFormat()}));
	}
	for (const AtlasRegion& region : this->atlas.takeDirtyRegions()) {
		changed[region.page] = changed[region.page].united(region.rect);
	}
	for (std::uint16_t page = 0; page < changed.size(); page++) {
		if (!changed[page].isEmpty()) {
			this->device.updateTexture(this->pageTextures[page], this->atlas.getPage(page), changed[page]);
		}
	}
}

void TextRenderer::rebuildGeometry() {
	// Layer by layer, then page by page within a layer: shadows and borders
	// are all drawn before any glyph, whatever page they come from. Within a
	// page, Texts that share a clip share a batch.
	this->geometry.clear();
	this->batches.clear();
	std::vector<const TextEntry*> pending;
	std::vector<const TextEntry*> later;
	for (const TextLayer layer : {TextLayer::Background, TextLayer::Shadow, TextLayer::Outline, TextLayer::Fill}) {
		for (std::uint16_t page = 0; page < this->pageTextures.size(); page++) {
			const std::uint64_t bit = std::uint64_t{1} << page;
			pending.clear();
			for (const TextEntry& entry : this->entries) {
				const bool hidden = entry.clipped && entry.clip.isEmpty();
				if (entry.text != nullptr && (entry.pageMask & bit) != 0 && !hidden) {
					pending.push_back(&entry);
				}
			}
			while (!pending.empty()) {
				const bool clipped = pending.front()->clipped;
				const Recti clip = pending.front()->clip;
				const std::uint32_t firstIndex = static_cast<std::uint32_t>(this->geometry.getIndices().size());
				later.clear();
				for (const TextEntry* entry : pending) {
					if (entry->clipped != clipped || (clipped && entry->clip != clip)) {
						later.push_back(entry);
						continue;
					}
					this->appendQuads(*entry, layer, page, this->geometry);
				}
				const std::uint32_t indexCount = static_cast<std::uint32_t>(this->geometry.getIndices().size()) - firstIndex;
				if (indexCount > 0) {
					this->batches.push_back(SpriteBatch{this->glyphs.getEncoding(), this->pageTextures[page], firstIndex, indexCount, clipped ? clip : Recti{}});
				}
				pending.swap(later);
			}
		}
	}
}

bool TextRenderer::refresh(TextEntry& entry, float deviceScale) {
	// A new layout only when the Text or the scale changed; placement changes
	// no layout, only where the quads go and what clips them.
	this->deviceScale = deviceScale;
	bool changed = false;
	if (entry.stale || entry.scale != deviceScale || entry.revision != entry.text->getRevision()) {
		this->rebuild(entry);
		entry.scale = deviceScale;
		changed = true;
	}
	Vector2f offset;
	Recti clip;
	bool clipped = false;
	this->getPlacement(*entry.text, offset, clip, clipped);
	if (offset.x != entry.offset.x || offset.y != entry.offset.y || clipped != entry.clipped || (clipped && clip != entry.clip)) {
		entry.offset = offset;
		entry.clip = clip;
		entry.clipped = clipped;
		changed = true;
	}
	return changed;
}

void TextRenderer::appendQuads(const TextEntry& entry, TextLayer layer, std::uint16_t page, Geometry<SpriteVertex>& target) const {
	for (std::size_t quad = 0; quad < entry.quadPages.size(); quad++) {
		if (entry.quadPages[quad] != page || entry.quadLayers[quad] != layer) {
			continue;
		}
		std::array<SpriteVertex, 4> corners{};
		for (std::size_t corner = 0; corner < 4; corner++) {
			corners[corner] = entry.vertices[quad * 4 + corner];
			corners[corner].x += entry.offset.x;
			corners[corner].y += entry.offset.y;
		}
		target.addQuad(corners);
	}
}

void TextRenderer::rotate(std::span<SpriteVertex> vertices, Vector2f pivot, TextRotation rotation) {
	// With y down, clockwise takes right to down and counterclockwise right to up.
	for (SpriteVertex& vertex : vertices) {
		const float dx = vertex.x - pivot.x;
		const float dy = vertex.y - pivot.y;
		if (rotation == TextRotation::Clockwise) {
			vertex.x = pivot.x - dy;
			vertex.y = pivot.y + dx;
		}
		else if (rotation == TextRotation::CounterClockwise) {
			vertex.x = pivot.x + dy;
			vertex.y = pivot.y - dx;
		}
	}
}

void TextRenderer::shade(std::span<SpriteVertex> vertices, const TextGradient& gradient, const Rectf& box) {
	// Each corner takes the gradient's color where it sits in the box; the
	// device blends the corner colors across the quad.
	for (SpriteVertex& vertex : vertices) {
		const float u = box.width > 0.0f ? (vertex.x - box.x) / box.width : 0.0f;
		const float v = box.height > 0.0f ? (vertex.y - box.y) / box.height : 0.0f;
		vertex.rgba = gradient.getColor(u, v).packRGBA8();
	}
}

std::uint64_t TextRenderer::getSolidKey() {
	// A fixed key no glyph hash is expected to meet.
	return 0x5058534F4C494431ull;
}

}
