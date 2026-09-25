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

#ifndef X3D_INVENTORY_H
#define X3D_INVENTORY_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str-array.h"

namespace Graphics {
struct Surface;
}

namespace Common {
class Serializer;
}

namespace X3D {

class Interaction;
class Renderer;

// The Space inventory bar, frame PorteF (docs/engine-spec/ui.md, Inventory bar)
// ponytail: the layout of PorteF.fra is built in; load it with the frame system once the
// menus need one
class Inventory {
public:
	Inventory();
	~Inventory();

	// The bar lives for the whole game; each scene's hotspots and cursor attach to it
	void attach(Interaction *interaction);
	void clear() { _items.clear(); _offset = 0; } // a new game
	void syncState(Common::Serializer &s); // save.md PORTEF

	void toggle();              // Space
	void show() { slide(true); }
	void hide() { slide(false); }
	void tick(uint32 ms);       // logic time; the slide steps every 50 ms
	void add(const Common::String &item); // "U02_01P"
	bool has(const Common::String &item) const;

	// Mouse over the 640x480 2D layer: true when the bar takes it (hover sets cursor)
	bool contains(const Common::Point &p) const;
	int cursorAt(const Common::Point &p) const; // cursor kind over the bar
	void click(const Common::Point &p);

	void draw(Renderer &r, int xOffset);

private:
	void slide(bool up);
	Graphics::Surface *image(const Common::String &name);

	Interaction *_interaction = nullptr;
	Common::StringArray _items;
	int _offset = 0;             // scroll, in slots (<= 0)
	int _y = 480, _step = 0, _ticks = 0;
	uint32 _nextTick = 0, _now = 0;
	Graphics::Surface *_background = nullptr;
	Common::StringArray _imageNames;
	Common::Array<Graphics::Surface *> _images;
};

} // End of namespace X3D

#endif // X3D_INVENTORY_H
