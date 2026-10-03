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

#ifndef GRUMPA_INVENTORY_H
#define GRUMPA_INVENTORY_H

#include "audio/mixer.h"
#include "common/array.h"
#include "common/rect.h"
#include "common/serializer.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

namespace Grumpa {

/**
 * The items (global actors 100..179), the cursor's held item and the inventory panel
 * (actor 90), docs/spec/inventory.md.
 */
class Inventory {
public:
	enum {
		kPanelId = 90,
		kFirstItem = 100,
		kLastItem = 179,
		kSlots = 9
	};
	enum ItemState {
		kGone = 1,
		kCarried = 3,
		kInScene = 4,
		kHeld = 6
	};

	struct Item {
		int id = 0;
		bool active = false, visible = false, latch = false;
		int32 state = kGone;
		int32 scene = -1;
		float pos[3] = {}, rot[3] = {};
		Common::String mesh, texture, icon, voice;  // IO_*.ANB, IT_*.tga, IC_*.tga, IS_*.wav
	};

	~Inventory();

	/** A new game: the items from Actors/Items.abi, the panel from 090_Inventory.atx. */
	bool load();
	/** Whether actor `id` is the panel or an item (the event VM routes its commands here). */
	static bool owns(int id) { return id == kPanelId || (id >= kFirstItem && id <= kLastItem); }
	/** Slot `slot` of actor `id` (slot 0 = an item's State), for conditions. */
	bool stateOf(int id, int slot, int32 &value) const;
	/** An item's or the panel's DoCommand (E-0503, E-0504). */
	void command(int id, int op, int arg1, int arg2);
	/** The scene the game shows (op 23 broadcast on entry). */
	void setScene(int num) { _scene = num; }

	int held() const { return _held; }
	bool shown() const { return _shown; }
	const Item *item(int id) const;
	/** A left click; true if the panel took it. */
	bool click(const Common::Point &p);
	/** Draw the panel when shown. */
	void draw(Graphics::ManagedSurface &screen);
	/** Set the system cursor to the held item's icon; false when nothing is held. */
	bool showHeldCursor();

	void syncState(Common::Serializer &s);

private:
	Item *find(int id);
	void setState(Item &it, int32 state);
	void hold(int id);
	bool add(int id);
	void say(const Common::String &wav);
	bool loadImage(const Common::String &path, Graphics::ManagedSurface &out) const;
	const Graphics::ManagedSurface *icon(const Item &it);

	Common::Array<Item> _items;
	int _held = -1;
	int _scene = -1;
	bool _shown = false, _locked = false;
	int _slot[kSlots];
	int _equip[2] = { -1, -1 };  // weapon (left), shield (right)
	Common::Point _pos;
	Common::String _fullVoice;
	Graphics::ManagedSurface _panel;
	Graphics::ManagedSurface _slotImg[2];
	Common::Array<Graphics::ManagedSurface *> _icons;  // by item index, loaded on demand
	Audio::SoundHandle _voice;
};

} // End of namespace Grumpa

#endif // GRUMPA_INVENTORY_H
