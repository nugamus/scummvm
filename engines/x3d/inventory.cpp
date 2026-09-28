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

#include "common/serializer.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/textconsole.h"

#include "graphics/surface.h"

#include "x3d/detection.h"
#include "x3d/interaction.h"
#include "x3d/inventory.h"
#include "x3d/renderer.h"

namespace X3D {

// PorteF.fra: the bar, its arrows and the item strip, at the bar's y
static const int kBarTop = 420;
static const int kParked = 480;
static const int kStripLeft = 57;
static const int kStripRight = 592;
static const int kSlots = 7;

void Inventory::clear() {
	_items.clear();
	_offset = 0;
}

void Inventory::syncState(Common::Serializer &s) {
	uint32 n = _items.size();
	s.syncAsUint32LE(n);
	if (s.isLoading())
		clear();
	for (uint32 i = 0; i < n; i++) {
		Common::String item = s.isSaving() ? _items[i] : "";
		s.syncString(item);
		if (s.isLoading())
			_items.push_back(item);
	}
}

void Inventory::attach(Interaction *interaction) {
	_interaction = interaction;
	_y = 480;
	_step = 0;
	_ticks = 0;
	// Logic time restarts with each scene
	_nextTick = 0;
	_now = 0;
}

Inventory::Inventory() {
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
	debugC(1, kDebugInput, "inventory %s from y %d", up ? "up" : "down", _y);
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

Common::Rect Inventory::slotRect(uint i) const {
	// An item's slot clipped to the strip (57..592): only the visible part shows and takes clicks
	const int x = 86 + 70 * ((int)i + _offset);
	const int left = MAX(x, kStripLeft);
	const int right = MIN(x + 51, kStripRight);
	return left < right ? Common::Rect(left, _y + 5, right, _y + 56) : Common::Rect();
}

int Inventory::cursorAt(const Common::Point &p) const {
	// Items take (4), the rest of the bar clicks (2)
	for (uint i = 0; i < _items.size(); i++)
		if (slotRect(i).contains(p))
			return 4;
	return 2;
}

void Inventory::click(const Common::Point &p) {
	const Common::Point local(p.x, p.y - _y);
	if (Common::Rect(20, 10, 53, 50).contains(local)) { // left arrow
		if ((int)_items.size() + _offset > kSlots)
			_offset--;
		return;
	}
	if (Common::Rect(580, 10, 613, 50).contains(local)) { // right arrow
		if (_offset < 0)
			_offset++;
		return;
	}
	if (p.x < kStripLeft || p.x >= kStripRight)
		return;

	// An item goes onto the cursor (a held one goes back first); the strip stores a held item
	const Common::String held = _interaction->heldItem();
	for (uint i = 0; i < _items.size(); i++) {
		if (!slotRect(i).contains(p))
			continue;
		const Common::String item = _items.remove_at(i);
		if (!held.empty())
			add(held + "P");
		_interaction->holdItem(item.substr(0, 6));
		hide();
		return;
	}
	if (!held.empty()) {
		add(held + "P");
		_interaction->holdItem("");
		hide();
	}
}

void Inventory::draw(Renderer &r, int xOffset) {
	if (_y >= kParked)
		return;
	if (_background)
		r.drawImage(*_background, xOffset, _y, false);
	for (uint i = 0; i < _items.size(); i++) {
		const Common::Rect slot = slotRect(i);
		if (slot.isEmpty())
			continue;
		if (Graphics::Surface *s = image(_items[i])) {
			const int x = 86 + 70 * ((int)i + _offset);
			const int left = slot.left - x;
			const int right = MIN(slot.right - x, (int)s->w);
			if (left >= right)
				continue;
			const Graphics::Surface part = s->getSubArea(Common::Rect(left, 0, right, s->h));
			r.drawImage(part, xOffset + slot.left, _y + 5, false);
		}
	}
}

} // End of namespace X3D
