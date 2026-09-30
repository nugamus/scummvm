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

#ifndef GILBERT_CUA_H
#define GILBERT_CUA_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

#include "gilbert/collection.h"

namespace Gilbert {

class GilbertEngine;

/** Close-ups (mode 2) with the inventory, the travel map among them (screens.md). */
class CloseUp {
public:
	explicit CloseUp(GilbertEngine *vm) : _vm(vm) {}

	void reset();

	// Call-backs 3, 4, 5.
	void load(uint32 id);
	void refreshObjects();
	void refreshInventory();

	/** One tick of mode 2. */
	void tick();
	/** Mouse down and up with something carried (screens.md "Taking and using"). */
	void mouseDown();
	void mouseUp();

private:
	struct Entry {
		uint32 code = 0;
		int picture = -1;
		uint32 icon = 0;
		Common::String text;
		int x = 0, y = 0;
		bool pickable = false;
		Common::Rect rect;
	};

	void draw();
	void drawFrame();
	void drawInventory();
	void handleMouse();
	void action(int item);
	void layout();
	void drawTooltip(const Common::String &text, int x, int y);

	GilbertEngine *_vm;
	PictureCollection _pictures;
	Common::Array<Entry> _objects, _inventory;
	int _top = 0;
	int _hoverObject = -1, _hoverItem = -1;
	int _hover = -1, _pressed = -1;
	int _carriedObject = -1, _carriedItem = -1;
	Common::Point _carryOffset;
};

} // End of namespace Gilbert

#endif // GILBERT_CUA_H
