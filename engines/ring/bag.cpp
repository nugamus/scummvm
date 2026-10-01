/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "graphics/font.h"
#include "graphics/managed_surface.h"

#include "ring/bag.h"
#include "ring/resources.h"
#include "ring/world.h"

namespace Ring {

// Layout (aApplication::Init 0x408941..0x4089e8, spec/bag.md): six slots of 100 from x 18.
enum {
	kSlots = 6, kSlotX = 18, kSlotY = 42, kSlotWidth = 100, kNameY = 90, kScrollMs = 500
};

void Bag::add(int object) {
	if (has(object) || !_world.object(object))
		return;
	const Object &o = *_world.object(object);
	Item item;
	item.object = object;
	if (o.bagFrames > 0) {
		item.animation.reset(new Animation());
		item.animation->init(o.bagFrames, o.bagFps, o.bagFlags);
		item.frames.resize(item.animation->frames);
	} else {
		item.frames.resize(1);
	}
	_items.insert_at(0, item);
	if ((int)_items.size() > kSlots)
		_scroll = 0;
}

void Bag::remove(int object) {
	for (uint i = 0; i < _items.size(); i++)
		if (_items[i].object == object)
			_items.remove_at(i--);
	if ((int)_items.size() <= kSlots)
		_scroll = 0;
}

void Bag::removeAll() {
	_items.clear();
	_scroll = 0;
}

Common::Array<int> Bag::contents() const {
	Common::Array<int> ids;
	for (const Item &i : _items)
		ids.push_back(i.object);
	return ids;
}

bool Bag::has(int object) const {
	for (const Item &i : _items)
		if (i.object == object)
			return true;
	return false;
}

void Bag::show(uint32 now) {
	_shown = true;
	for (Item &i : _items)
		if (i.animation)
			i.animation->begin(now);
}

void Bag::hide() {
	_shown = false;
	for (Item &i : _items) {
		if (i.animation)
			i.animation->end();
		for (auto &f : i.frames)
			f.reset();
	}
}

Image *Bag::picture(Common::SharedPtr<Image> &img, const char *name) {
	// `\LIST\<name>` in SY's archive (aList::openImage 0x417440).
	if (!img)
		img.reset(_res.loadImage(kZoneSY, name, true, "LIST"));
	return img.get();
}

void Bag::draw(Graphics::ManagedSurface &dst, uint32 now) {
	Image *bgr = picture(_background, "bagbgr.tga"), *arr = picture(_arrow, "bagarr.tga"), *menu = picture(_menu, "menu_gur.tga");
	if (!bgr || !arr || !menu)
		return;
	bgr->draw(dst, 0, 0, 3);
	_leftOn = _scroll > 0;
	if (_leftOn)
		arr->draw(dst, 7, 48, 3);
	_rightOn = _scroll + kSlots < (int)_items.size();
	if (_rightOn)
		arr->draw(dst, 627, 48, 3);
	if (_menuLit)
		menu->draw(dst, 335, 8, 3);
	_menuLit = false;
	if (_erda) {
		Image *erda = _erdaLit ? picture(_erdaLitImage, "erda_gur.tga") : picture(_erdaNormal, "erda_gun.tga");
		if (erda)
			erda->draw(dst, 103, 0, 3);
	}
	_erdaLit = false;
	_slotsOn = 0;
	for (int k = 0; _scroll + k < (int)_items.size() && k < kSlots; k++, _slotsOn++) {
		Item &item = _items[_scroll + k];
		const Object *o = _world.object(item.object);
		if (!o)
			continue;
		// `\LSTICON\<icon>.tga`, or the bag animation's `\LSTICON\<icon>\<icon>.NNNN.tga`.
		int frame = 0;
		if (item.animation) {
			item.animation->advance(now);
			frame = CLIP<int>(item.animation->frame, 0, item.frames.size() - 1);
		}
		Common::SharedPtr<Image> &img = item.frames[frame];
		if (!img) {
			Common::String file = item.animation ? Common::String::format("%s\\%s.%04d.tga", o->icon.c_str(), o->icon.c_str(), frame + 1)
												 : o->icon + ".tga";
			img.reset(_res.loadImage(kZoneSY, file, true, "LSTICON"));
		}
		if (img)
			img->draw(dst, kSlotX + kSlotWidth * k + kSlotWidth / 2 - img->surface.w / 2, kSlotY, 3);
	}
}

int Bag::hit(int x, int y) const {
	// The hot spots in their order (0x4178c0); the first hit wins.
	if (_leftOn && x >= 0 && x < 30 && y >= 24 && y < 472)
		return kLeft;
	if (_rightOn && x >= 610 && x < 640 && y >= 24 && y < 472)
		return kRight;
	if (x >= 200 && x < 640 && y >= 0 && y < 30)
		return kMenu;
	if (_erda && x >= 90 && x < 150 && y >= 0 && y < 30)
		return kErdaButton;
	for (int k = 0; k < _slotsOn; k++)
		if (x >= 19 + kSlotWidth * k && x < 117 + kSlotWidth * k && y >= 42 && y < 87)
			return _items[_scroll + k].object;
	return kNone;
}

int Bag::track(int x, int y, uint32 now, Graphics::ManagedSurface &dst, const Graphics::Font *font) {
	int h = hit(x, y);
	if ((h == kLeft || h == kRight) && now - _scrollTime > kScrollMs) {
		if (h == kLeft && _scroll > 0)
			_scroll--;
		else if (h == kRight && _scroll < (int)_items.size())
			_scroll++;
		_scrollTime = now;
	} else if (h == kMenu) {
		_menuLit = true;
	} else if (h == kErdaButton) {
		_erdaLit = true;
	} else if (h > 0 && font) {
		// The object's name, centred on its slot and kept on screen: font 1, yellow on black.
		const Object *o = _world.object(h);
		int k = 0;
		while (_items[_scroll + k].object != h)
			k++;
		if (o && !o->name.empty()) {
			int w = font->getStringWidth(o->name);
			int tx = CLIP(kSlotX + kSlotWidth * k + kSlotWidth / 2 - w / 2, 0, MAX(0, 640 - w));
			dst.fillRect(Common::Rect(tx, kNameY, tx + w, kNameY + font->getFontHeight()), dst.format.RGBToColor(0, 0, 0));
			font->drawString(&dst, o->name, tx, kNameY, w, dst.format.RGBToColor(245, 235, 50));
		}
	}
	return h;
}

int Bag::click(int x, int y, uint32 now) {
	int h = hit(x, y);
	if (h == kLeft && _scroll > 0)
		_scroll--;
	else if (h == kRight && _scroll < (int)_items.size())
		_scroll++;
	if (h == kLeft || h == kRight)
		_scrollTime = now;
	return h;
}

} // End of namespace Ring
