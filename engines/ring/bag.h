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

#ifndef RING_BAG_H
#define RING_BAG_H

#include "common/array.h"
#include "common/ptr.h"

namespace Graphics {
class Font;
class ManagedSurface;
}

namespace Ring {

struct Animation;
struct Image;
class Resources;
class World;

/**
 * The inventory (`aList`, app+0x8d, spec/bag.md): the objects carried, the strip drawn
 * at the top of the window while shown, and the object in hand.
 */
class Bag {
public:
	Bag(World &world, Resources &res) : _world(world), _res(res) {}

	/** `BagAdd`: at the front, once. */
	void add(int object);
	/** `BagRem`. */
	void remove(int object);
	/** `BagRemAll`. */
	void removeAll();
	/** `BagIsIn`. */
	bool has(int object) const;
	/** The objects, front first (`aList::LoadSave`). */
	Common::Array<int> contents() const;

	bool shown() const { return _shown; }
	/** 0x4192e0: shown, the bag animations started. */
	void show(uint32 now);
	/** 0x419350: hidden, the icons freed. */
	void hide();

	/** The object in hand (+0x95), 0 when none. */
	int held() const { return _held; }
	void setHeld(int object) { _held = object; }

	/** Erda's button (+0xd2): in every zone but SY and AS (0x402210). */
	void setErda(bool on) { _erda = on; }

	/** 0x418ca0: the strip, its arrows, highlights and objects; enables the hot spots. */
	void draw(Graphics::ManagedSurface &dst, uint32 now);

	enum Hit { kNone = -1, kLeft = -2, kRight = -3, kMenu = -4, kErdaButton = -5 };
	/**
	 * 0x418a70: the arrows scroll every 500 ms, a slot shows the object's name, the menu
	 * band and Erda light up. Returns the hot spot hit: an object id for a slot.
	 */
	int track(int x, int y, uint32 now, Graphics::ManagedSurface &dst, const Graphics::Font *font);
	/** 0x418520: the arrows scroll by one; returns the hot spot hit, an object id for a slot. */
	int click(int x, int y, uint32 now);

private:
	struct Item {
		int object = 0;
		Common::SharedPtr<Animation> animation; ///< `ObjAddBagAni`, else the icon picture
		Common::Array<Common::SharedPtr<Image> > frames;
	};

	int hit(int x, int y) const;
	Image *picture(Common::SharedPtr<Image> &img, const char *name);

	World &_world;
	Resources &_res;
	Common::Array<Item> _items; ///< front first
	int _scroll = 0;
	bool _shown = false;
	int _held = 0;
	bool _erda = false, _erdaLit = false, _menuLit = false;
	bool _leftOn = false, _rightOn = false;
	int _slotsOn = 0;
	uint32 _scrollTime = 0;
	Common::SharedPtr<Image> _background, _arrow, _menu, _erdaNormal, _erdaLitImage;
};

} // End of namespace Ring

#endif // RING_BAG_H
