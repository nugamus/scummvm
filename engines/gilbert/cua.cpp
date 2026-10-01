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

#include "gilbert/cua.h"
#include "gilbert/detection.h"
#include "gilbert/dialog.h"
#include "gilbert/gilbert.h"
#include "gilbert/logic.h"
#include "gilbert/menu.h"
#include "gilbert/room.h"
#include "gilbert/sound.h"

namespace Gilbert {

namespace {

enum {
	kBook = 0x08, kMenuButton = 0x26, kKort = 0x27, kUp = 0x2c, kDown = 0x2d, kBack = 0x73
};

} // End of anonymous namespace

void CloseUp::reset() {
	_objects.clear();
	_inventory.clear();
	_top = 0;
	_hover = _pressed = -1;
	_hoverObject = _hoverItem = -1;
	_carriedObject = _carriedItem = -1;
}

// cua::Load, call-back 3 (screens.md "Entering").
void CloseUp::load(uint32 id) {
	_vm->sound()->stopAll();
	if (id == 999)
		_pictures.load("maps/!global/kartmap.wxi");
	else
		_pictures.load(Common::Path(Common::String::format("maps/%d/cua%d.wxi", _vm->room()->current(), id)));
	_objects.clear();
	if (id != 999)
		_vm->room()->rereadRadar();
	_vm->setMode(GilbertEngine::kModeCua);
	_vm->room()->restartMusic();
	debugC(1, kDebugScript, "Close-up %d", id);
}

// Call-back 4 (screens.md "Objects").
void CloseUp::refreshObjects() {
	_objects.clear();
	Logic *logic = _vm->logic();
	for (uint i = 0; i < logic->cuaObjectCount(); i++) {
		ObjectData d;
		if (!logic->cuaObject(i, d))
			continue;
		Entry e;
		e.code = d.code;
		e.picture = d.picture;
		e.icon = d.icon;
		e.text = d.text;
		e.x = d.x;
		e.y = d.y;
		e.pickable = d.pickable;
		Picture *p = d.picture >= 0 ? _pictures[d.picture] : nullptr;
		if (p)
			e.rect = Common::Rect(64 + e.x, 50 + e.y, 64 + e.x + p->pattern(0).width(), 50 + e.y + p->pattern(0).height());
		_objects.push_back(e);
	}
}

// Call-back 5 (screens.md "Inventory").
void CloseUp::refreshInventory() {
	_inventory.clear();
	Logic *logic = _vm->logic();
	for (uint i = 0; i < logic->inventoryCount(); i++) {
		Entry e;
		if (logic->inventoryObject(i, e.code, e.icon, e.text))
			_inventory.push_back(e);
	}
	layout();
}

// Twelve cells from `top`, six to a row.
void CloseUp::layout() {
	for (uint k = 0; k < _inventory.size(); k++) {
		const int i = (int)k - _top;
		if (i < 0 || i >= 12) {
			_inventory[k].rect = Common::Rect();
			continue;
		}
		const int c = i % 6, r = i / 6;
		_inventory[k].rect = Common::Rect(377 + 27 * c, 377 + 26 * r, 404 + 27 * c, 403 + 26 * r);
	}
}

// The mode 2 tick (screens.md "Ticks by mode").
void CloseUp::tick() {
	draw();
	DialogBox *dialog = _vm->dialog();
	if (dialog->isOpen()) {
		dialog->handleMouse();
		dialog->draw();
	}
	handleMouse();
	_vm->drawCursor(0);
	_vm->present();
}

// cua::Draw (screens.md "Frame").
void CloseUp::draw() {
	_vm->clear();
	_vm->drawPicture(_pictures[0], 64, 50);
	for (int i = (int)_objects.size() - 1; i >= 0; i--) {
		const Entry &e = _objects[i];
		if ((int)e.code == _carriedObject || e.picture < 0 || !_pictures[e.picture])
			continue;
		_vm->drawPicture(_pictures[e.picture], 64 + e.x, 50 + e.y);
	}
	// The hotspot overlay: the objects something happens to, outlined.
	if (_vm->hotspotsShown() && !_vm->dialog()->isOpen())
		for (const Entry &e : _objects)
			if ((int)e.code != _carriedObject && !e.rect.isEmpty() && _vm->logic()->cuaObjectActive(e.code)) {
				_vm->fillAlpha(e.rect, 0xFFFF00, 40);
				_vm->frameRect(e.rect, 0xFFFF00);
			}
	drawFrame();
	if (_hoverObject >= 0 && _hoverObject < (int)_objects.size()) {
		const Common::Rect &r = _objects[_hoverObject].rect;
		drawTooltip(_objects[_hoverObject].text, r.left, r.top + r.height() / 2);
	}
	if (_hoverItem >= 0 && _hoverItem < (int)_inventory.size())
		drawTooltip(_inventory[_hoverItem].text, _inventory[_hoverItem].rect.left, _inventory[_hoverItem].rect.top - 10);

	// The carried object: an object as its icon, an inventory item where it was grabbed.
	const Common::Point m = _vm->mouse();
	Picture *strip = _vm->inventoryPictures()[0];
	for (const Entry &e : _objects)
		if ((int)e.code == _carriedObject)
			_vm->drawPattern(strip, e.icon, m.x - 16, m.y - 16);
	for (const Entry &e : _inventory)
		if ((int)e.code == _carriedItem)
			_vm->drawPattern(strip, e.icon, m.x - _carryOffset.x, m.y - _carryOffset.y);
}

// cua::DrawFrame (screens.md "Frame" step 3).
void CloseUp::drawFrame() {
	PictureCollection &i2 = _vm->interface2();
	Room *room = _vm->room();
	_vm->drawPicture(_vm->interface1()[0], 64, 50);
	room->drawEggs();
	_vm->drawPicture(i2[5], 371, 375);
	drawInventory();
	room->drawRadar(150);
	_vm->drawPicture(i2[kMenuButton], 70, 368);
	_vm->drawPicture(i2[0x98], 70, 396);
	_vm->drawPicture(i2[kUp], 537, 380);
	_vm->drawPicture(i2[kDown], 537, 400);
	room->drawBookButton();
	_vm->drawPicture(i2[0x77], 509, 336);
	_vm->drawPicture(i2[kBack], 513, 340);

	static const struct {
		int item, x, y, hover, pressed;
	} buttons[] = {
		{ kMenuButton, 70, 368, 0x28, 0x2a },
		{ kUp, 537, 380, 0x2e, 0x30 },
		{ kDown, 537, 400, 0x2f, 0x31 },
		{ kBack, 513, 340, 0x74, 0x75 },
	};
	for (const auto &b : buttons)
		if (_hover == b.item)
			_vm->drawPicture(i2[b.hover], b.x, b.y);
	for (const auto &b : buttons)
		if (_pressed == b.item) {
			_vm->drawPicture(i2[b.pressed], b.x, b.y);
			action(b.item);
			break;
		}
}

void CloseUp::drawInventory() {
	Picture *strip = _vm->inventoryPictures()[0];
	for (const Entry &e : _inventory)
		if (!e.rect.isEmpty() && (int)e.code != _carriedItem)
			_vm->drawPattern(strip, e.icon, e.rect.left + 1, e.rect.top);
}

// ui::DrawTooltip (screens.md "Descriptions").
void CloseUp::drawTooltip(const Common::String &text, int x, int y) {
	if (text.empty())
		return;
	const Common::U32String t = GilbertEngine::fromWindows1252(text);
	const int w = _vm->textWidth(t, 8);
	x = CLIP(x, 80, 560);
	y = MAX(y, 66);
	if (x + w > 560)
		x -= w;
	_vm->fillAlpha(Common::Rect(x - 4, y - 2, x + w + 4, y + 16), kColourBlack, 120);
	_vm->drawText(_vm->screen(), t, x, y, 8, kColourTan);
}

// cua::HandleMouse (screens.md "Mouse").
void CloseUp::handleMouse() {
	static const int items[] = { kBook, kMenuButton, kKort, kUp, kDown, kBack };
	PictureCollection &i2 = _vm->interface2();
	const Common::Point m = _vm->mouse();
	const Common::Rect mr(m.x - 3, m.y - 3, m.x + 3, m.y + 3);

	_hoverObject = -1;
	for (uint i = 0; i < _objects.size(); i++)
		if (_objects[i].rect.contains(m)) {
			_hoverObject = i;
			break;
		}
	_hoverItem = -1;
	for (uint i = 0; i < _inventory.size(); i++)
		if (_inventory[i].rect.contains(m))
			_hoverItem = i;
	auto firstHit = [&]() {
		for (int item : items) {
			Picture *p = i2[item];
			if (p && p->last.intersects(mr))
				return item;
		}
		return -1;
	};
	_pressed = -1;
	_hover = firstHit();
	bool changed = false;
	const bool press = _vm->press(GilbertEngine::kScreenCua, &changed);
	if (changed)
		_hover = -1;
	if (!press) {
		// The keyboard shortcuts and the mouse wheel press the buttons.
		const int s = _vm->shortcut();
		if (!_vm->dialog()->isOpen() && (s == kActionBack || s == kActionInventoryUp || s == kActionInventoryDown)) {
			_pressed = s == kActionBack ? kBack : (s == kActionInventoryUp ? kUp : kDown);
			_vm->takeShortcut();
		}
		return;
	}
	if (!Common::Rect(509, 336, 589, 396).contains(m)) {
		Common::Array<uint32> codes;
		for (const Entry &e : _objects)
			if (e.rect.contains(m))
				codes.push_back(e.code);
		for (uint32 code : codes)
			_vm->logic()->clickObjectInCua(code);
	}
	_pressed = firstHit();
}

void CloseUp::hotspots(Common::Array<Graphics::HotspotInfo> &list) {
	for (const Entry &e : _objects)
		if ((int)e.code != _carriedObject && !e.rect.isEmpty() && _vm->logic()->cuaObjectActive(e.code))
			list.push_back(Graphics::HotspotInfo(objectPoint(e), GilbertEngine::fromWindows1252(e.text),
			                                     e.pickable ? Graphics::kHotspotObject : Graphics::kHotspotDefault));
}

// The picture's opaque point nearest the middle of its rectangle, inside the view.
Common::Point CloseUp::objectPoint(const Entry &e) {
	Common::Rect r = e.rect;
	r.clip(Common::Rect(64, 50, 576, 370));
	const Common::Point mid((r.left + r.right) / 2, (r.top + r.bottom) / 2);
	Picture *p = e.picture >= 0 ? _pictures[e.picture] : nullptr;
	if (!p || !p->transparent || r.isEmpty())
		return mid;
	const Common::Rect pat = p->pattern(0);
	Common::Point best = mid;
	int bestD = -1;
	const int step = MAX(1, MIN(r.width(), r.height()) / 16);
	for (int y = r.top; y < r.bottom; y += step)
		for (int x = r.left; x < r.right; x += step) {
			const int sx = pat.left + x - e.rect.left, sy = pat.top + y - e.rect.top;
			if (sx < 0 || sy < 0 || sx >= p->surface.w || sy >= p->surface.h ||
			    *(const uint16 *)p->surface.getBasePtr(sx, sy) == p->key)
				continue;
			const int d = ABS(x - mid.x) + ABS(y - mid.y);
			if (bestD < 0 || d < bestD) {
				bestD = d;
				best = Common::Point(x, y);
			}
		}
	return best;
}

// cua::PickUp (screens.md "Taking and using").
void CloseUp::mouseDown() {
	const Common::Point m = _vm->mouse();
	for (const Entry &e : _objects)
		if (e.pickable && e.rect.contains(m))
			_carriedObject = e.code;
	for (const Entry &e : _inventory)
		if (e.rect.contains(m)) {
			_carriedItem = e.code;
			_carryOffset = Common::Point(m.x - e.rect.left, m.y - e.rect.top);
		}
}

// cua::Drop.
void CloseUp::mouseUp() {
	const int object = _carriedObject, item = _carriedItem;
	_carriedObject = _carriedItem = -1;
	if (_vm->mode() != GilbertEngine::kModeCua || _objects.empty() || (object < 0 && item < 0))
		return;
	const Common::Point m = _vm->mouse();
	Logic *logic = _vm->logic();
	Common::Array<uint32> under;
	for (const Entry &e : _objects)
		if (e.rect.contains(m))
			under.push_back(e.code);
	if (object >= 0) {
		if (Common::Rect(364, 367, 576, 429).contains(m))
			logic->objectToInventory(object);
		for (uint32 code : under)
			if ((int)code != object)
				logic->useObjectOnObject(object, code);
	}
	if (item >= 0)
		for (uint32 code : under)
			logic->useObjectOnObject(item, code);
}

// gmenu::Action in mode 2 (screens.md "Buttons").
void CloseUp::action(int item) {
	_vm->menu()->setLastAction(item);
	Sound *snd = _vm->sound();
	_hover = _pressed = -1;
	switch (item) {
	case kUp:
		snd->playWave(1, 1);
		if (_top > 5) {
			_top -= 6;
			layout();
		}
		break;
	case kDown:
		snd->playWave(1, 1);
		if (_top + 12 <= (int)_inventory.size()) {
			_top += 6;
			layout();
		}
		break;
	case kBack:
		if (_vm->dialog()->isOpen())
			break;
		snd->playWave(1, 4);
		snd->stopAll();
		_vm->setMode(GilbertEngine::kModeRoom);
		_vm->logic()->cuaEnd();
		_vm->room()->restartMusic();
		break;
	case kMenuButton:
		// The original leaves the menu silent here; the engine plays its music as from a
		// room (Q-0501).
		_vm->menu()->enterFromGame();
		break;
	default:
		break;
	}
}

} // End of namespace Gilbert
