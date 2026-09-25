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

#include "common/debug.h"
#include "common/file.h"
#include "common/textconsole.h"

#include "graphics/surface.h"

#include "image/bmp.h"

#include "x3d/interaction.h"
#include "x3d/inventory.h"
#include "x3d/renderer.h"

namespace X3D {

// PorteF.fra: the bar, its arrows and the item strip (ui.md table), at the bar's y
static const int kBarTop = 420, kParked = 480;
static const Common::Rect kLeftArrow(20, 10, 53, 50), kRightArrow(580, 10, 613, 50);
static const int kStripLeft = 57, kStripRight = 592, kSlots = 7;

Inventory::Inventory(Interaction &interaction) : _interaction(interaction) {
	_background = image("PorteF");
}

Inventory::~Inventory() {
	for (Graphics::Surface *s : _images) {
		if (s) {
			s->free();
			delete s;
		}
	}
}

Graphics::Surface *Inventory::image(const Common::String &name) {
	for (uint i = 0; i < _imageNames.size(); i++)
		if (_imageNames[i].equalsIgnoreCase(name))
			return _images[i];
	Graphics::Surface *s = loadBitmap(Common::Path("2dbit/" + name + ".bmp"));
	if (!s)
		warning("Missing bitmap 2dbit/%s.bmp", name.c_str());
	_imageNames.push_back(name);
	_images.push_back(s);
	return s;
}

void Inventory::slide(bool up) {
	debug(1, "inventory %s from y %d", up ? "up" : "down", _y);
	// Up to y = 420 or down to 480, 4 px per 50 ms tick
	_step = up ? -4 : 4;
	_ticks = up ? (_y - kBarTop) / 4 : (kParked - _y) / 4;
}

void Inventory::toggle() {
	if (_ticks > 0)
		slide(_step > 0);
	else
		slide(_y == kParked);
}

void Inventory::tick(uint32 ms) {
	_now = ms;
	while (_nextTick <= _now) {
		_nextTick += 50;
		if (_ticks > 0) {
			_y = CLIP(_y + _step, kBarTop, kParked);
			_ticks--;
		}
	}
}

void Inventory::add(const Common::String &item) {
	if (!has(item))
		_items.push_back(item);
}

bool Inventory::has(const Common::String &item) const {
	for (const Common::String &i : _items)
		if (i.equalsIgnoreCase(item))
			return true;
	return false;
}

bool Inventory::contains(const Common::Point &p) const {
	return _y < kParked && p.y >= _y && p.x >= 0 && p.x < 640;
}

int Inventory::cursorAt(const Common::Point &p) const {
	// Items take (4), the rest of the bar clicks (2)
	for (uint i = 0; i < _items.size(); i++) {
		const int x = 86 + 70 * ((int)i + _offset);
		if (Common::Rect(x, _y + 5, x + 51, _y + 56).contains(p))
			return 4;
	}
	return 2;
}

void Inventory::click(const Common::Point &p) {
	const Common::Point local(p.x, p.y - _y);
	if (kLeftArrow.contains(local)) {
		if ((int)_items.size() + _offset > kSlots)
			_offset--;
		return;
	}
	if (kRightArrow.contains(local)) {
		if (_offset < 0)
			_offset++;
		return;
	}
	if (p.x < kStripLeft || p.x >= kStripRight)
		return;

	// An item goes onto the cursor (a held one goes back first); the strip stores a held item
	const Common::String held = _interaction.heldItem();
	for (uint i = 0; i < _items.size(); i++) {
		const int x = 86 + 70 * ((int)i + _offset);
		if (!Common::Rect(x, _y + 5, x + 51, _y + 56).contains(p))
			continue;
		const Common::String item = _items.remove_at(i);
		if (!held.empty())
			add(held + "P");
		_interaction.holdItem(item.substr(0, 6));
		hide();
		return;
	}
	if (!held.empty()) {
		add(held + "P");
		_interaction.holdItem("");
		hide();
	}
}

void Inventory::draw(Renderer &r, int xOffset) {
	if (_y >= kParked)
		return;
	if (_background)
		r.drawImage(*_background, xOffset, _y, false);
	for (uint i = 0; i < _items.size(); i++) {
		// ponytail: items scrolled out of the strip are not drawn (clipping is Q-0062)
		const int x = 86 + 70 * ((int)i + _offset);
		if (x < kStripLeft || x + 51 > kStripRight)
			continue;
		if (Graphics::Surface *s = image(_items[i]))
			r.drawImage(*s, xOffset + x, _y + 5, false);
	}
}

} // End of namespace X3D
