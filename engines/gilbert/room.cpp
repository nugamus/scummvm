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
#include "common/system.h"

#include "gilbert/book.h"
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
	kBook = 0x08, kMenuButton = 0x26, kKort = 0x27, kArrowUp = 0x2c, kArrowDown = 0x2d
};

// Standing cycles by facing (rooms.md "Gilbert").
int standingBase(int facing) {
	switch (facing) {
	case 0: return 168;
	case 4: return 144;
	case 8: return 156;
	case 12: return 204;
	case 16: return 120;
	case 20: return 192;
	case 24: return 180;
	case 28: return 132;
	default: return -1;
	}
}

// The shadow's pattern while standing: the first walking frame of the facing.
int shadowBase(int facing) {
	switch (facing) {
	case 0: return 15;
	case 4: return 60;
	case 8: return 30;
	case 12: return 90;
	case 16: return 105;
	case 20: return 75;
	case 24: return 0;
	case 28: return 45;
	default: return 0;
	}
}

} // End of anonymous namespace

void Room::reset() {
	_current = (uint32)-1;
	_ox = _oy = 0;
	_shown = false;
	_frame = 0;
	_list.clear();
	_radar = Common::Rect();
	_cursor = 0;
	_hover = _pressed = -1;
	_radarAlpha = 0;
	_radarStep = 1;
	_newTopic = false;
	_blink = 50;
	_blinkStep = 8;
	_lastStep = -1;
}

// ---------------------------------------------------------------------------
// Entering a room (rooms.md "Entering a room")

void Room::load(uint32 id, int x, int y, int facing) {
	_vm->sound()->stopAll();
	if (_current != (uint32)-1)
		fade(false);
	_shown = false;
	_vm->setMode(GilbertEngine::kModeLoading);

	if (id != _current) {
		const Common::String dir = Common::String::format("maps/%d/", id);
		_objects.load(Common::Path(dir + Common::String::format("w%do.wxi", id)));
		_picture.load(Common::Path(dir + Common::String::format("w%d.wxi", id)));
		_mask.load(Common::Path(dir + Common::String::format("w%dm.wxi", id)));
		// ctrl<n>.map (formats README): "ML01", s32 high, a 32-byte record, the cells.
		_cells.clear();
		_mapW = _mapH = 0;
		Common::File f;
		if (f.open(Common::Path(dir + Common::String::format("ctrl%d.map", id))) && f.readUint32BE() == MKTAG('M', 'L', '0', '1')) {
			f.skip(4 + 16);
			_mapW = f.readUint32LE();
			_mapH = f.readUint32LE();
			const uint32 size = f.readUint32LE();
			f.skip(4);
			_cells.resize(size / 4);
			for (uint32 i = 0; i < size / 4; i++)
				_cells[i] = f.readUint32LE();
		} else {
			warning("Gilbert: no control map for room %d", id);
		}
		_current = id;
	}
	_w = _mapW * 16;
	_h = _mapH * 16;

	// The start scroll: the start point centred when the room allows it.
	const int sx = x >= 512 ? MAX(256 - x, 512 - _w) : 0;
	const int sy = y >= 320 ? MAX(160 - y, 320 - _h) : 0;
	_x = x + sx;
	_y = y + sy;
	_ox = sx + 64;
	_oy = sy + 50;

	_up = _down = _left = _right = false;
	_facing = facing;
	_vm->setMode(GilbertEngine::kModeRoom);
	refreshObjects();
	_list.clear();
	_radar = _vm->logic()->radarRect();
	restartMusic();
	debugC(1, kDebugScript, "Room %d: %dx%d, Gilbert at (%d, %d) facing %d", id, _w, _h, x, y, facing);
}

void Room::refreshObjects() {
	_list.clear();
	Logic *logic = _vm->logic();
	const uint n = MIN<uint>(logic->walkmapObjectCount(), 100);
	for (uint i = 0; i < n; i++) {
		ObjectData d;
		if (!logic->walkmapObject(i, d))
			continue;
		Object o;
		o.picture = d.picture;
		o.x = d.x;
		o.y = d.y;
		_list.push_back(o);
	}
}

void Room::roomMusic(const Common::String &name) {
	if (name != _music)
		_vm->sound()->openStream(Sound::kMusic, name, true);
	_music = name;
}

void Room::restartMusic() {
	if (!_music.empty())
		_vm->sound()->openStream(Sound::kMusic, _music, true);
}

// Gilbert's position (call-backs 13, 14): his frame's centre in room pixels.
Common::Point Room::gilbertPosition() const {
	return Common::Point((int)_x - _ox + 48, (int)_y - _oy + 48);
}

// Call-back 19 and the area check read the cells like this (rooms.md "Walking and area hits").
int Room::mapCell(int x, int y) const {
	if (x < 0 || y < 0 || x > _mapW || y > _mapH)
		return 0;
	const uint i = y * _mapW + x;
	return i < _cells.size() ? (int16)_cells[i] : 0;
}

void Room::walk(int d) {
	_right = d == 4 || d == 8 || d == 12;
	_left = d == 20 || d == 24 || d == 28;
	_up = d == 0 || d == 4 || d == 28;
	_down = d == 12 || d == 16 || d == 20;
}

void Room::newTopic() {
	_newTopic = true;
	_vm->sound()->playWave(1, 10);
}

// ---------------------------------------------------------------------------
// The tick

void Room::tick(uint32 elapsed) {
	draw();
	DialogBox *dialog = _vm->dialog();
	if (dialog->isOpen()) {
		// The dialogue takes the press; the room neither walks nor presses buttons.
		dialog->handleMouse();
		dialog->draw();
		_cursor = 0;
	} else {
		handleMouse();
	}
	const int divisor = _vm->ctrlHeld() ? 20 : 30;
	const int lag = MAX<int>(elapsed / 16, 1);
	moveGilbert(1000 / (lag + divisor));
	_vm->drawCursor(_cursor);
	_vm->present();
}

void Room::fade(bool in) {
	// 256 gamma ramp updates without a clock in the original (Q-0300): 300 ms here.
	const int from = in ? _vm->brightness() : 256, to = in ? 256 : 64;
	const uint32 start = g_system->getMillis();
	for (;;) {
		const uint32 t = g_system->getMillis() - start;
		const int level = t >= 300 ? to : from + (to - from) * (int)t / 300;
		_vm->present(level);
		if (level == to || _vm->shouldQuit())
			break;
		_vm->pollEvents();
		g_system->delayMillis(10);
	}
}

// ---------------------------------------------------------------------------
// Drawing (rooms.md "Drawing")

void Room::draw() {
	_vm->clear();
	drawLayer(_picture[0], _ox, _oy, Common::Rect(64, 50, 576, 370));
	drawObjectsAndGilbert();
	drawLayer(_mask[0], _ox, _oy, Common::Rect((int)_x, (int)_y, (int)_x + 96, (int)_y + 96));
	drawPanel();
	if (!_shown) {
		fade(true);
		_shown = true;
		_vm->menu()->roomShown();
	}
}

// TMaplibHolder::DrawLayer: 64x64 tiles from the origin, inside a rectangle.
void Room::drawLayer(Picture *pic, int ox, int oy, const Common::Rect &r) {
	if (!pic || pic->patternW <= 0 || pic->patternH <= 0)
		return;
	if (ox > 576 || oy > 430 || ox > r.right || oy > r.bottom)
		return;
	const int cols = pic->surface.w / pic->patternW, rows = pic->surface.h / pic->patternH;
	const int c0 = ox < 64 && ox < r.left ? (r.left - ox) / 64 : 0;
	const int r0 = oy < 50 && oy < r.top ? (r.top - oy) / 64 : 0;
	for (int row = r0; row < rows; row++) {
		const int y = oy + 64 * row;
		if (y >= 430 || y >= r.bottom)
			break;
		for (int col = c0; col < cols; col++) {
			const int x = ox + 64 * col;
			if (x >= 576 || x >= r.right)
				break;
			_vm->drawPattern(pic, row * cols + col, x, y);
		}
	}
}

void Room::drawObjectsAndGilbert() {
	const int feet = (int)_y + 96;
	for (int pass = 0; pass < 2; pass++) {
		if (pass == 1) {
			// The shadow, then Gilbert.
			Picture *all = _vm->gilbert()[1];
			const int pattern = _frame <= 119 ? _frame : shadowBase(_facing);
			if (all && pattern < all->patternCount())
				_vm->blendPattern(all, pattern, Common::Rect((int)_x, (int)_y, (int)_x + 96, (int)_y + 96), 70);
			_vm->drawPattern(_vm->gilbert()[0], _frame, (int)_x, (int)_y);
		}
		for (int i = (int)_list.size() - 1; i >= 0; i--) {
			const Object &o = _list[i];
			Picture *p = o.picture >= 0 ? _objects[o.picture] : nullptr;
			if (!p)
				continue;
			const bool behind = _oy + o.y + p->surface.h <= feet;
			if (behind == (pass == 0))
				_vm->drawPattern(p, 0, _ox + o.x, _oy + o.y);
		}
	}
}

// ui::DrawRoomPanel (rooms.md "Panel").
void Room::drawPanel() {
	PictureCollection &i2 = _vm->interface2();
	_vm->drawPicture(_vm->interface1()[0], 64, 50);
	drawEggs();
	drawRadar(50);
	_vm->drawPicture(i2[kMenuButton], 70, 368);
	_vm->drawPicture(i2[kKort], 70, 396);
	_vm->drawPicture(i2[0xa8], 537, 380);
	_vm->drawPicture(i2[0xa9], 537, 400);
	drawBookButton();

	if (_hover == kBook)
		_vm->drawPicture(i2[0x09], 298, 333);
	else if (_hover == kMenuButton)
		_vm->drawPicture(i2[0x28], 70, 368);
	else if (_hover == kKort)
		_vm->drawPicture(i2[0x29], 70, 396);

	const int pressed = _pressed;
	if (pressed == kBook)
		_vm->drawPicture(i2[0x0a], 298, 333);
	else if (pressed == kMenuButton)
		_vm->drawPicture(i2[0x2a], 70, 368);
	else if (pressed == kKort)
		_vm->drawPicture(i2[0x2b], 70, 396);
	if (pressed == kBook || pressed == kMenuButton || pressed == kKort)
		action(pressed);
}

void Room::drawEggs() {
	PictureCollection &i2 = _vm->interface2();
	const int32 eggs = _vm->logic()->variable(199);
	if (eggs == 0)
		_vm->drawPicture(i2[4], 296, 374);
	else if (eggs >= 1 && eggs <= 6)
		_vm->drawPicture(i2[0x99 + eggs - 1], 296, 374);
}

void Room::drawRadar(int maxAlpha) {
	_vm->drawPicture(_vm->interface2()[6], 151, 347);
	const Common::Rect r(152 + _radar.left, 348 + _radar.top, 152 + _radar.right, 348 + _radar.bottom);
	_vm->fillAlpha(r, kColourTan, _radarAlpha);
	_vm->frameRect(r, 0xF9B528);
	_radarAlpha += _radarStep;
	if (_radarAlpha <= 0) {
		_radarAlpha = 0;
		_radarStep = 1;
	} else if (_radarAlpha >= maxAlpha) {
		_radarAlpha = maxAlpha;
		_radarStep = -1;
	}
}

void Room::drawBookButton() {
	PictureCollection &i2 = _vm->interface2();
	_vm->drawPicture(i2[kBook], 298, 333);
	if (_newTopic) {
		_vm->blendPattern(i2[0x0a], 0, Common::Rect(298, 333, 346, 373), _blink);
		_blink += _blinkStep;
		if (_blink <= 50) {
			_blink = 50;
			_blinkStep = 8;
		} else if (_blink >= 200) {
			_blink = 200;
			_blinkStep = -2;
		}
	}
}

void Room::rereadRadar() {
	_radar = _vm->logic()->radarRect();
}

// gmenu::Action in mode 1 (rooms.md "Actions in the room").
void Room::action(int item) {
	Sound *snd = _vm->sound();
	switch (item) {
	case kBook:
		snd->playWave(1, 4);
		_hover = _pressed = -1;
		_vm->book()->open();
		break;
	case kMenuButton:
		_hover = _pressed = -1;
		_vm->menu()->enterFromGame();
		break;
	case kKort:
		// Event walkmap * 100 + 99 opens close-up 999, the travel map.
		snd->playWave(1, 4);
		_vm->logic()->walkmapAreaHit(99999);
		_hover = _pressed = -1;
		_vm->setMode(GilbertEngine::kModeCua);
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Mouse (rooms.md "Mouse")

void Room::handleMouse() {
	static const int items[] = { kBook, kMenuButton, kKort, kArrowUp, kArrowDown };
	PictureCollection &i2 = _vm->interface2();
	const Common::Point m = _vm->mouse();
	const Common::Rect mr(m.x - 3, m.y - 3, m.x + 3, m.y + 3);
	const bool press = _vm->takeLeftPress();
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
	const bool walkArea = Common::Rect(74, 60, 566, 420).contains(m) && !Common::Rect(289, 328, 351, 380).contains(m) &&
	                      !Common::Rect(509, 336, 539, 366).contains(m) && !Common::Rect(74, 360, 566, 420).contains(m);
	if (walkArea) {
		if (_vm->leftHeld())
			_vm->logic()->pathNewPath(m.x - _ox, m.y - _oy);
		const int v = mapCell((m.x - _ox) / 16, (m.y - _oy) / 16);
		_cursor = v == 1 ? 6 : (v >= 2 ? 7 : 0);
	} else {
		_cursor = 0;
		if (m.x >= 64 && m.x <= 74 && _ox < 64) {
			_ox += 6;
			_x += 6;
			_cursor = 2;
		} else if (m.x >= 566 && m.x <= 576 && _ox >= 586 - _w) {
			_ox -= 6;
			_x -= 6;
			_cursor = 3;
		}
		if (m.y <= 60 && _oy < 50) {
			_oy += 6;
			_y += 6;
			_cursor = 4;
		} else if (m.y >= 420 && m.y <= 430 && _oy >= 394 - _h) {
			_oy -= 6;
			_y -= 6;
			_cursor = 5;
		}
	}
	if (press) {
		_hover = -1;
		_pressed = firstHit();
		debugC(1, kDebugScript, "Room: press at (%d, %d): panel item %d", m.x, m.y, _pressed);
	}
}

// ---------------------------------------------------------------------------
// Gilbert (rooms.md "Gilbert")

void Room::moveGilbert(int m) {
	int f = (int)_counter;
	static const struct {
		bool up, down, left, right;
		double dx, dy;
		int base, facing;
	} cases[] = {
		{ true, false, false, true, 0.053, -0.053, 60, 4 },
		{ true, false, true, false, -0.053, -0.053, 45, 28 },
		{ false, true, false, true, 0.053, 0.053, 90, 12 },
		{ false, true, true, false, -0.053, 0.053, 75, 20 },
		{ true, false, false, false, 0, -0.075, 15, 0 },
		{ false, true, false, false, 0, 0.075, 105, 16 },
		{ false, false, true, false, -0.075, 0, 0, 24 },
		{ false, false, false, true, 0.075, 0, 30, 8 },
	};
	bool moved = false;
	for (const auto &c : cases) {
		if ((c.up && !_up) || (c.down && !_down) || (c.left && !_left) || (c.right && !_right))
			continue;
		stepAreaCheck();
		_x += c.dx * m;
		_y += c.dy * m;
		if (f > 15)
			f = 0;
		_frame = c.base + f;
		_facing = c.facing;
		moved = true;
		break;
	}
	_walking = moved;
	if (moved) {
		_counter += 0.02 * m;
		if (_counter > 14)
			_counter = 0;
	} else {
		if (f > 11) {
			f = 0;
			_counter = 0;
		}
		const int base = standingBase(_facing);
		if (base >= 0)
			_frame = base + f;
		_counter += 0.002 * m;
		if (_counter > 11)
			_counter = 0;
	}
	if (_walking && f != _lastStep) {
		if (f == 0)
			_vm->sound()->playWave(1, 6);
		else if (f == 6)
			_vm->sound()->playWave(1, 7);
	}
	_lastStep = f;

	// Gilbert stays inside the room.
	if ((int)_x - _ox < -48)
		_x = _ox - 48;
	else if ((int)_x - _ox >= _w - 48)
		_x = _ox + _w - 49;
	if ((int)_y - _oy < -48)
		_y = _oy - 48;
	else if ((int)_y - _oy >= _h - 48)
		_y = _oy + _h - 49;
}

// room::StepAreaCheck (rooms.md "Walking and area hits").
void Room::stepAreaCheck() {
	Common::Point p = gilbertPosition();
	const Common::Point own(p.x / 16, p.y / 16);
	if (_left)
		p.x -= 16;
	if (_right)
		p.x += 16;
	if (_up)
		p.y -= 16;
	if (_down)
		p.y += 16;
	const int v = mapCell(p.x / 16, p.y / 16);
	if (v < 2 || v > 31)
		return;
	Logic *logic = _vm->logic();
	for (uint i = 0; i < logic->pathItemCount(); i++)
		if (logic->pathItem(i) == own) {
			logic->walkmapAreaHit(v - 1);
			return;
		}
}

} // End of namespace Gilbert
