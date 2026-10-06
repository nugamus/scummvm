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

#include "grumpa/character.h"
#include "grumpa/dialogue.h"
#include "grumpa/events.h"
#include "grumpa/inventory.h"

namespace Grumpa {

// The panel's layout around its top-left (x, y) and size (E-0504).
enum {
	kSlotGrid = 258,   // three 86-pixel columns
	kSlotSize = 86,
	kEquipSize = 96,
	kShieldX = 210,
	kIconOffset = 32,
	kButtonY = 96, kButtonH = 36,
	kDiskX = 70, kDiskW = 30, kDoorX = 200, kDoorW = 32
};

// What the equipment slots take and the attachment Grumpa wears for each (E-0901).
static const int kWeapons[][2] = { { 100, 4 }, { 101, 1 }, { 110, 3 }, { 113, 2 } };
static const int kShields[][2] = { { 138, 0 }, { 134, 5 } };

static int attachmentOf(int slot, int id) {
	const int (*t)[2] = slot == 0 ? kWeapons : kShields;
	int n = slot == 0 ? ARRAYSIZE(kWeapons) : ARRAYSIZE(kShields);
	for (int i = 0; i < n; i++)
		if (t[i][0] == id)
			return t[i][1];
	return -1;
}

Inventory::~Inventory() {
	for (uint i = 0; i < _icons.size(); i++)
		delete _icons[i];
	for (uint i = 0; i < _items.size(); i++)
		_items[i].skin.free();
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
	for (uint i = 0; i < _items.size(); i++)
		_items[i].skin.free();
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

// 0x4387d0: counters, no duplicates, the first free slot, else "inventory full".
bool Inventory::add(int id) {
	Item *it = find(id);
	if (!it)
		return false;
	if (id == 174 || (id >= 177 && id <= 179)) {
		// Water Drop and the coins count on actor 8 and are not carried (E-0503).
		if (_vm)
			_vm->deliver(8, id == 174 ? 50 : 9, id == 174 ? 100 : 1, 0);
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
	dropBesidePlayer(*it);
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
	case 71: {
		// At the actor, 30 units higher, turned as it (E-0900). ponytail: only characters
		// carry a position in the engine; another actor leaves the item where it was.
		Character *c = _chars ? _chars->find(arg1) : nullptr;
		Vec3 at(it->pos[0], it->pos[1], it->pos[2]);
		float yaw = it->rot[1];
		if (c) {
			at = Vec3(c->pos.x, c->pos.y + 30.0f, c->pos.z);
			yaw = c->yaw;
		}
		place(*it, at, yaw);
		break;
	}
	case 18: {  // a click: pick up a hovered item lying here (E-0900)
		Common::Point p((int16)(arg1 & 0xffff), (int16)((uint32)arg1 >> 16));
		if (it->state != kInScene || it->scene != _scene || !it->hovered || _held != -1 ||
			!clickRect(it->rect).contains(p))
			break;
		add(id);
		sayFile("effect_item.wav", _pickSound);
		debug(1, "Grumpa: picked up item %d", id);
		break;
	}
	default: break;
	}
}

bool Inventory::click(const Common::Point &p) {
	if (!_shown)
		return false;
	Common::Rect panel(_pos.x, _pos.y, _pos.x + _panel.w, _pos.y + _panel.h + 3 * kSlotSize);
	if (!panel.contains(p))
		return false;
	// The door and diskette buttons answer the plain cursor only (E-0901).
	Common::Rect door(_pos.x + kDoorX, _pos.y + kButtonY, _pos.x + kDoorX + kDoorW, _pos.y + kButtonY + kButtonH);
	Common::Rect disk(_pos.x + kDiskX, _pos.y + kButtonY, _pos.x + kDiskX + kDiskW, _pos.y + kButtonY + kButtonH);
	if (_held == -1 && (door.contains(p) || disk.contains(p))) {
		_request = door.contains(p) ? 60 : 61;
		return true;
	}
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
	// Then the shield slot (right of Grumpa), then the weapon slot (left).
	for (int k = 1; k >= 0; k--) {
		Common::Rect r(_pos.x + k * kShieldX, _pos.y, _pos.x + k * kShieldX + kEquipSize, _pos.y + kEquipSize);
		if (r.contains(p)) {
			equipClick(k);
			break;
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

// An equipment slot (0 weapon, 1 shield; E-0901): take its item onto an empty cursor, or put
// a fitting held item in (the old one back to the inventory); anything else goes back to the
// inventory. ponytail: Grumpa wearing the attachment (0x421780) is not drawn yet.
void Inventory::equipClick(int k) {
	if (_held == -1) {
		if (_equip[k] == -1)
			return;
		Item *it = find(_equip[k]);
		debug(1, "Grumpa: Grumpa takes off attachment %d", attachmentOf(k, _equip[k]));
		_equip[k] = -1;
		if (it) {
			hold(it->id);
			say(it->voice);
		}
		return;
	}
	int heldId = _held;
	if (attachmentOf(k, heldId) < 0) {
		_held = -1;
		add(heldId);
		return;
	}
	if (_equip[k] != -1)
		add(_equip[k]);
	_equip[k] = heldId;
	setState(*find(heldId), kCarried);
	debug(1, "Grumpa: Grumpa wears attachment %d", attachmentOf(k, heldId));
}

Common::Rect Inventory::clickRect(const Common::Rect &r) {
	Common::Rect w = r;
	if (w.width() < 40) {
		w.left = r.left - 30;
		w.right = r.left + 30;
	}
	if (w.height() < 40) {
		w.top = r.top - 30;
		w.bottom = r.top + 30;
	}
	return w;
}

bool Inventory::itemAt(const Common::Point &p) const {
	for (uint i = 0; i < _items.size(); i++) {
		const Item &it = _items[i];
		if (it.hovered && it.state == kInScene && it.scene == _scene && clickRect(it.rect).contains(p))
			return true;
	}
	return false;
}

// Every 2 updates an item lying here turns 0.05 rad, steps its glow and checks the mouse:
// over its rectangle, the panel hidden and the player within 160 units, it is hovered and
// says its name once each time the mouse comes onto it (E-0900).
void Inventory::update(const Character *player, const Common::Point &mouse) {
	_player = player;
	for (uint i = 0; i < _items.size(); i++) {
		Item &it = _items[i];
		if (!it.active || it.scene != _scene || it.state != kInScene)
			continue;
		if (++it.tick < 2)
			continue;
		it.tick = 0;
		if (it.glow >= 0 && ++it.glow > 10)
			it.glow = -1;
		it.rot[1] += 0.05f;
		if (it.rot[1] > 6.2831855f)
			it.rot[1] -= 6.2831855f;
		bool near = true;
		if (player) {
			float dx = it.pos[0] - player->pos.x, dy = it.pos[1] - player->pos.y, dz = it.pos[2] - player->pos.z;
			near = sqrtf(dx * dx + dy * dy + dz * dz) < 160.0f;
		}
		if (!_shown && near && !it.rect.isEmpty() && clickRect(it.rect).contains(mouse)) {  // drawn once
			if (it.sayArmed)
				say(it.voice);
			it.sayArmed = false;
			it.hovered = true;
		} else {
			it.sayArmed = true;
			it.hovered = false;
		}
	}
}

// Lying in the current scene at `pos` turned by `yaw`, shown, with a glow and the pick-up
// sound (op 71 and the drop beside Grumpa, E-0900).
void Inventory::place(Item &it, const Vec3 &pos, float yaw) {
	it.pos[0] = pos.x;
	it.pos[1] = pos.y;
	it.pos[2] = pos.z;
	it.rot[0] = it.rot[2] = 0.0f;
	it.rot[1] = yaw;
	it.active = it.visible = true;
	it.scene = _scene;
	setState(it, kInScene);
	it.glow = 0;
	sayFile("effect_item.wav", _pickSound);
}

// The panel is full: at the player's character, 20 units up and 30 ahead (E-0900).
// ponytail: no walk-mesh test yet (the 30-back and on-the-spot fallbacks); without a player
// character the item stays where it was.
void Inventory::dropBesidePlayer(Item &it) {
	if (!_player) {
		setState(it, kInScene);
		it.scene = _scene;
		return;
	}
	Vec3 at(_player->pos.x + sinf(_player->yaw) * 30.0f, _player->pos.y + 20.0f,
			_player->pos.z + cosf(_player->yaw) * 30.0f);
	place(it, at, _player->yaw);
}

void Inventory::sayFile(const Common::String &wav, Audio::SoundHandle &h) {
	Common::SeekableReadStream *f = openSound(wav);
	Audio::SeekableAudioStream *s = f ? Audio::makeWAVStream(f, DisposeAfterUse::YES) : nullptr;
	if (!s)
		return;
	g_system->getMixer()->stopHandle(h);
	g_system->getMixer()->playStream(Audio::Mixer::kSFXSoundType, &h, s);
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
