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

#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/system.h"
#include "graphics/cursorman.h"
#include "image/jpeg.h"
#include "image/tga.h"

#include "grumpa/inventory.h"

namespace Grumpa {

// The panel's layout around its top-left (x, y) and size (E-0504).
enum {
	kSlotGrid = 258,   // three 86-pixel columns
	kSlotSize = 86,
	kEquipSize = 96,
	kShieldX = 210,
	kIconOffset = 32
};

Inventory::~Inventory() {
	for (uint i = 0; i < _icons.size(); i++)
		delete _icons[i];
}

static Common::String readPstr(Common::SeekableReadStream &s) {
	int32 n = s.readSint32LE();
	if (n <= 0 || n > 1024)
		return Common::String();
	Common::String r;
	for (int32 i = 0; i < n; i++) {
		char c = s.readByte();
		if (c)
			r += c;
	}
	return r;
}

static float readFloat(Common::SeekableReadStream &s) {
	uint32 v = s.readUint32LE();
	float f;
	memcpy(&f, &v, 4);
	return f;
}

bool Inventory::load() {
	for (uint i = 0; i < _icons.size(); i++)
		delete _icons[i];
	_icons.clear();
	_items.clear();
	_held = -1;
	_shown = _locked = false;
	for (int i = 0; i < kSlots; i++)
		_slot[i] = -1;
	_equip[0] = _equip[1] = -1;

	// Items.abi: type-5 records (E-0503): id, active, visible, +0x4d8, State, four names,
	// scene, position, rotation, three u32, a pair list.
	Common::File f;
	if (!f.open("Actors/Items.abi"))
		return false;
	while (f.size() - f.pos() >= 8) {
		uint32 type = f.readUint32LE();
		Item it;
		it.id = f.readUint32LE();
		if (type != 5) {
			warning("Grumpa: Items.abi: type %u record", type);
			return false;
		}
		it.active = f.readUint32LE() != 0;
		it.visible = f.readUint32LE() != 0;
		f.readUint32LE();
		it.state = f.readSint32LE();
		it.mesh = readPstr(f);
		it.texture = readPstr(f);
		it.icon = readPstr(f);
		it.voice = readPstr(f);
		it.scene = f.readSint32LE();
		for (int k = 0; k < 3; k++)
			it.pos[k] = readFloat(f);
		for (int k = 0; k < 3; k++)
			it.rot[k] = readFloat(f);
		f.skip(12);
		int32 pairs = f.readSint32LE();
		if (pairs < 0 || pairs > 1024 || f.err())
			return false;
		f.skip(8 * pairs);
		_items.push_back(it);
	}
	_icons.resize(_items.size());
	for (uint i = 0; i < _icons.size(); i++)
		_icons[i] = nullptr;

	// The panel: the first block of 090_Inventory.atx (the reader skips header-less blocks,
	// E-0504): id, two flags, x, y, panel image, slot image, the "inventory full" voice.
	Common::File atx;
	if (!atx.open("UI/090_Inventory/090_Inventory.atx"))
		return false;
	Common::Array<Common::String> fields;
	bool inBlock = false;
	while (!atx.eos() && fields.size() < 8) {
		Common::String line = atx.readLine();
		int slash = line.findFirstOf('/');
		if (slash >= 0)
			line = Common::String(line.c_str(), slash);
		line.trim();
		if (line.empty())
			continue;
		if (line == "{")
			inBlock = true;
		else if (line == "}")
			break;
		else if (inBlock)
			fields.push_back(line);
	}
	if (fields.size() < 8)
		return false;
	_pos = Common::Point(atoi(fields[3].c_str()), atoi(fields[4].c_str()));
	_fullVoice = fields[7];
	Common::String slot = fields[6];  // InventorySlot_0000.jpg: frame 0 empty, 1 filled
	return loadImage("UI/090_Inventory/" + fields[5], _panel)
		&& loadImage("UI/090_Inventory/" + slot, _slotImg[0])
		&& loadImage("UI/090_Inventory/" + Common::String(slot.c_str(), slot.size() - 5) + "1.jpg", _slotImg[1]);
}

// Load a JPG or TGA into the screen format with the blue key mapped to pure blue (E-0107).
bool Inventory::loadImage(const Common::String &path, Graphics::ManagedSurface &out) const {
	Common::File f;
	if (!f.open(Common::Path(path)))
		return false;
	Graphics::PixelFormat fmt = g_system->getScreenFormat();
	const Graphics::Surface *src = nullptr;
	Image::JPEGDecoder jpeg;
	Image::TGADecoder tga;
	if (path.hasSuffixIgnoreCase(".jpg")) {
		jpeg.setOutputPixelFormat(Graphics::PixelFormat(4, 8, 8, 8, 8, 0, 8, 16, 24));
		if (jpeg.loadStream(f))
			src = jpeg.getSurface();
	} else if (tga.loadStream(f)) {
		src = tga.getSurface();
	}
	if (!src)
		return false;
	out.create(src->w, src->h, fmt);
	for (int y = 0; y < src->h; y++) {
		for (int x = 0; x < src->w; x++) {
			byte a, r, g, b;
			src->format.colorToARGB(src->getPixel(x, y), a, r, g, b);
			if (b > 200 && r < 96 && g < 96)
				r = g = 0, b = 255;
			out.setPixel(x, y, fmt.RGBToColor(r, g, b));
		}
	}
	return true;
}

Inventory::Item *Inventory::find(int id) {
	for (uint i = 0; i < _items.size(); i++)
		if (_items[i].id == id)
			return &_items[i];
	return nullptr;
}

const Inventory::Item *Inventory::item(int id) const {
	return const_cast<Inventory *>(this)->find(id);
}

bool Inventory::stateOf(int id, int slot, int32 &value) const {
	const Item *it = item(id);
	if (!it || slot != 0)
		return false;
	value = it->state;
	return true;
}

void Inventory::setState(Item &it, int32 state) {
	it.state = state;
	if (state == kHeld)
		_held = it.id;
	else if (_held == it.id)
		_held = -1;
}

// Put item `id` on the cursor (-1: clear it); holding sets its State to 6 (E-0503).
void Inventory::hold(int id) {
	_held = -1;
	Item *it = find(id);
	if (it)
		setState(*it, kHeld);
}

// FUN_004387d0: counters, no duplicates, the first free slot, else "inventory full".
bool Inventory::add(int id) {
	Item *it = find(id);
	if (!it)
		return false;
	if (id == 174 || (id >= 177 && id <= 179)) {
		// Water Drop and the coins count on actor 8 ((8, 50, 100) / (8, 9, 1)) and are not
		// carried (E-0503).
		// ponytail: actor 8's score/coin counters are not modelled yet; the item just goes.
		setState(*it, kGone);
		return true;
	}
	for (int i = 0; i < kSlots; i++)
		if (_slot[i] == id)
			return true;
	for (int i = 0; i < kSlots; i++) {
		if (_slot[i] == -1) {
			_slot[i] = id;
			setState(*it, kCarried);
			return true;
		}
	}
	say(_fullVoice);
	// Full: it drops beside Grumpa (State 4 at actor 3's position). Without a player
	// character it stays in the current scene where it was (Q-0202).
	setState(*it, kInScene);
	it->scene = _scene;
	return false;
}

void Inventory::command(int id, int op, int arg1, int arg2) {
	if (id == kPanelId) {
		switch (op) {
		case 2: _shown = true; break;
		case 3: _shown = false; break;
		case 11: _locked = false; break;
		case 12: _locked = true; _shown = false; break;
		case 19:
			if (!_locked)
				_shown = !_shown;
			break;
		default: break;
		}
		return;
	}
	Item *it = find(id);
	if (!it)
		return;
	if (op == 52) {
		it->latch = false;
		return;
	}
	if (it->latch)
		return;
	switch (op) {
	case 0: say(it->voice); break;
	case 2: it->visible = true; break;
	case 3: it->visible = false; break;
	case 11: it->active = true; break;
	case 12: it->active = false; break;
	case 13:
		setState(*it, kGone);
		it->active = it->visible = false;
		it->latch = true;
		it->scene = -1;
		break;
	case 16: setState(*it, arg1); break;
	case 23: _scene = arg1; break;
	case 42: add(id); break;
	case 54:
		it->scene = arg1;
		setState(*it, kInScene);
		break;
	case 71:
		// ponytail: the target actor's position needs the 3D actor model; keep the item's own.
		it->scene = _scene;
		it->active = it->visible = true;
		setState(*it, kInScene);
		break;
	default: break;
	}
}

bool Inventory::click(const Common::Point &p) {
	if (!_shown)
		return false;
	Common::Rect panel(_pos.x, _pos.y, _pos.x + _panel.w, _pos.y + _panel.h + 3 * kSlotSize);
	if (!panel.contains(p))
		return false;
	int left = _pos.x + (_panel.w - kSlotGrid) / 2, top = _pos.y + _panel.h;
	for (int i = 0; i < kSlots; i++) {
		Common::Rect r = Common::Rect::center(0, 0, kSlotSize, kSlotSize);
		r.moveTo(left + (i % 3) * kSlotSize, top + (i / 3) * kSlotSize);
		if (!r.contains(p))
			continue;
		int heldId = _held;
		if (heldId == -1) {
			if (_slot[i] != -1) {
				Item *it = find(_slot[i]);
				_slot[i] = -1;
				if (it) {
					hold(it->id);
					say(it->voice);
				}
			}
		} else if (_slot[i] == -1) {
			_slot[i] = heldId;
			setState(*find(heldId), kCarried);
		} else {
			_held = -1;
			add(heldId);
		}
		return true;
	}
	// The shield slot (right of Grumpa) takes 134 and 138 only (E-0504); the weapon slot and
	// the two buttons are Q-0502.
	Common::Rect shield(_pos.x + kShieldX, _pos.y, _pos.x + kShieldX + kEquipSize, _pos.y + kEquipSize);
	if (shield.contains(p)) {
		if (_held == -1 && _equip[1] != -1) {
			hold(_equip[1]);
			_equip[1] = -1;
		} else if (_held == 134 || _held == 138) {
			if (_equip[1] != -1)
				add(_equip[1]);
			_equip[1] = _held;
			setState(*find(_held), kCarried);
		}
	}
	return true;
}

const Graphics::ManagedSurface *Inventory::icon(const Item &it) {
	uint idx = &it - &_items[0];
	if (!_icons[idx]) {
		_icons[idx] = new Graphics::ManagedSurface();
		if (!loadImage("Bitmaps/" + it.icon, *_icons[idx]))
			warning("Grumpa: icon %s not found", it.icon.c_str());
	}
	return _icons[idx]->w ? _icons[idx] : nullptr;
}

void Inventory::draw(Graphics::ManagedSurface &screen) {
	if (!_shown)
		return;
	uint32 key = screen.format.RGBToColor(0, 0, 255);
	screen.transBlitFrom(_panel, _pos, key);
	int left = _pos.x + (_panel.w - kSlotGrid) / 2, top = _pos.y + _panel.h;
	for (int i = 0; i < kSlots; i++) {
		Common::Point at(left + (i % 3) * kSlotSize, top + (i / 3) * kSlotSize);
		screen.transBlitFrom(_slotImg[_slot[i] != -1], at, key);
		const Item *it = item(_slot[i]);
		const Graphics::ManagedSurface *ic = it ? icon(*it) : nullptr;
		if (ic)
			screen.transBlitFrom(*ic, at + Common::Point(kIconOffset, kIconOffset), key);
	}
	for (int k = 0; k < 2; k++) {
		const Item *it = item(_equip[k]);
		const Graphics::ManagedSurface *ic = it ? icon(*it) : nullptr;
		if (ic)
			screen.transBlitFrom(*ic, _pos + Common::Point(k * kShieldX + kIconOffset, kIconOffset), key);
	}
}

bool Inventory::showHeldCursor() {
	const Item *it = item(_held);
	const Graphics::ManagedSurface *ic = it ? icon(*it) : nullptr;
	if (!ic)
		return false;
	CursorMan.replaceCursor(ic->rawSurface(), ic->w / 2, ic->h / 2, ic->format.RGBToColor(0, 0, 255));
	CursorMan.showMouse(true);
	return true;
}

void Inventory::say(const Common::String &wav) {
	Common::File *f = new Common::File();
	if (!f->open(Common::Path("Sounds/" + wav)) && !f->open(Common::Path("Sounds_Swedish/" + wav))
		&& !f->open(Common::Path("Sounds_/" + wav))) {
		delete f;
		return;
	}
	Audio::SeekableAudioStream *s = Audio::makeWAVStream(f, DisposeAfterUse::YES);
	if (!s)
		return;
	g_system->getMixer()->stopHandle(_voice);
	g_system->getMixer()->playStream(Audio::Mixer::kSpeechSoundType, &_voice, s);
}

void Inventory::syncState(Common::Serializer &s) {
	for (uint i = 0; i < _items.size(); i++) {
		Item &it = _items[i];
		s.syncAsByte(it.active);
		s.syncAsByte(it.visible);
		s.syncAsByte(it.latch);
		s.syncAsSint32LE(it.state);
		s.syncAsSint32LE(it.scene);
		for (int k = 0; k < 3; k++)
			s.syncAsFloatLE(it.pos[k]);
		for (int k = 0; k < 3; k++)
			s.syncAsFloatLE(it.rot[k]);
	}
	for (int i = 0; i < kSlots; i++)
		s.syncAsSint32LE(_slot[i]);
	s.syncAsSint32LE(_equip[0]);
	s.syncAsSint32LE(_equip[1]);
	s.syncAsSint32LE(_held);
	s.syncAsByte(_shown);
	s.syncAsByte(_locked);
}

} // End of namespace Grumpa
