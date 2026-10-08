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

// China's interface bar: the inventory row, the held object, documents and the notebook
// (spec/china-interface.md, E-1100..E-1104). The map and the documentation base are not
// implemented (Q-1100, Q-1102).

#include "common/debug.h"
#include "common/system.h"

#include "graphics/cursorman.h"

#include "cryomni3d/china/engine.h"

namespace CryOmni3D {
namespace China {

static const int kBandTop = 415; // E-1100
static const int kBandRows = 65;
static const int kCursorDefaultId = 11;

// Objects' inventory names (LABELS.TXT keys, +0x2c) and document keys (+0x20, objects 0..18),
// from the original's object table (E-1107).
static const char *const kObjectLabelKeys[CryOmni3DEngine_China::kObjectCount] = {
	"LISTE_BOITES", "ORIGINAUX", "POSTHUME", "CONFES1", "CONFES2", "CONFES3", "CONFES4", "INDIC1", "INDIC2",
	"INDIC3", "INDIC4", "LISTE_VICTIMES", "PROCLA", "EDI", "LETTRE_VIERGE", "REBU", "PLBOMB",
	"INDICE_CACHETS", "INDICE_CACHETS2", "SCEAUX", "TOURNEVIS", "CIRE", "RUYI", "PINCEAU", "BURIN", "MARTEAU",
	"CLE_WANG", "CURE_DENTS", "PINCEAU_ESP", "PIECES", "MANDAT1", "MANDAT2", "MANDAT3", "MANDAT4", "CLE_JARRE",
	"" // object 35 has no label (E-1107)
};
static const uint kDocumentCount = 19;
static const char *const kObjectDocKeys[kDocumentCount] = {
	"lboites", "origine", "posthum", "confess1", "confess2", "confess3", "confess4", "indice1", "indice2",
	"indice3", "indice4", "victime", "proclam", "edit", "lvierge", "rebus", "plbombe", "cachet1", "cachet2"
};
// The text box of each document (x0, y0, x1, y1), objects 0..13 (E-1103; 14..18 have none, Q-1101)
static const int kDocumentBoxes[14][4] = {
	{ 10, 10, 275, 350 }, { 10, 10, 200, 50 }, { 10, 270, 500, 470 }, { 10, 10, 160, 300 },
	{ 10, 10, 330, 300 }, { 10, 10, 275, 300 }, { 10, 10, 275, 300 }, { 200, 10, 400, 50 },
	{ 10, 300, 200, 400 }, { 10, 400, 275, 500 }, { 100, 10, 300, 50 }, { 10, 10, 275, 250 },
	{ 10, 10, 400, 300 }, { 10, 10, 400, 300 }
};

Common::String CryOmni3DEngine_China::objectLabelKey(uint id) const {
	return !_objLabelKey[id].empty() ? _objLabelKey[id] : Common::String(kObjectLabelKeys[id]);
}

Common::String CryOmni3DEngine_China::objectDocKey(uint id) const {
	return !_objDocKey[id].empty() ? _objDocKey[id] : Common::String(id < kDocumentCount ? kObjectDocKeys[id] : "");
}

void CryOmni3DEngine_China::barLoad() {
	if (_barLoaded) {
		return;
	}
	// Each sprite carries its screen position (E-1101)
	static const char *const names[kBarSpriteCount] = { "SPIRSORT", "OEIL", "CADRES", "BNOTE", "BOUSS" };
	for (uint i = 0; i < kBarSpriteCount; i++) {
		loadSprite(Common::Path(Common::String::format("INVENT/%s.SPR", names[i])), _barSprites[i]);
	}
	_barLoaded = true;
}

// The slot picture (c_) and, for documents, the eye cursor (i_) of an object (E-1102)
void CryOmni3DEngine_China::barObjectSprites(uint id) {
	if (id >= kObjectCount) {
		return;
	}
	if (_objSlot[id].surface.empty()) {
		loadSprite(Common::Path(Common::String::format("SPRITES/OBJETS/C_%s.SPR", objectStem(id))), _objSlot[id]);
	}
	if (id < kDocumentCount && _objEye[id].surface.empty()) {
		loadSprite(Common::Path(Common::String::format("SPRITES/OBJETS/I_%s.SPR", objectStem(id))), _objEye[id]);
	}
}

// Slot rebuild on opening (E-1102): objects in state 2 keep their slot, the others without one
// take the first free; any other object loses its slot.
void CryOmni3DEngine_China::barRebuildSlots() {
	for (uint i = 0; i < kSlotCount; i++) {
		_slots[i] = kNoObject;
	}
	for (uint id = 0; id < kObjectCount; id++) {
		Object &o = _objects[id];
		if (o.state == 2 && o.slot >= 0 && o.slot < (int)kSlotCount && _slots[o.slot] == kNoObject) {
			_slots[o.slot] = id;
		} else {
			o.slot = -1;
		}
	}
	for (uint id = 0; id < kObjectCount; id++) {
		Object &o = _objects[id];
		if (o.state == 2 && o.slot < 0) {
			const int free = barFirstFreeSlot();
			if (free < 0) {
				warning("China: no free inventory slot for object %u", id);
				continue;
			}
			o.slot = free;
			_slots[free] = id;
		}
	}
	for (uint id = 0; id < kObjectCount; id++) {
		if (_objects[id].state == 2 || id == _heldObject) {
			barObjectSprites(id);
		}
	}
}

int CryOmni3DEngine_China::barFirstFreeSlot() const {
	for (uint i = 0; i < kSlotCount; i++) {
		if (_slots[i] == kNoObject) {
			return i;
		}
	}
	return -1;
}

// The object goes to the hand: state 1, its r_ sprite is the cursor (E-1102).
void CryOmni3DEngine_China::barHold(uint id) {
	_objects[id].state = 1;
	_objects[id].slot = -1;
	_heldObject = id;
	barObjectSprites(id);
	loadSprite(Common::Path(Common::String::format("SPRITES/OBJETS/R_%s.SPR", objectStem(id))), _heldCursor);
	_cursorId = -1;
}

Common::Rect CryOmni3DEngine_China::barRect(int sprite) const {
	const Sprite &s = _barSprites[sprite];
	return Common::Rect(s.pos.x, s.pos.y, s.pos.x + s.surface.w, s.pos.y + s.surface.h);
}

// Which sprites show (E-1101)
bool CryOmni3DEngine_China::barVisible(int sprite) const {
	switch (sprite) {
	case kBarSpiral:
		return !_puzzleMode || (_place && Common::String(_place->name).equalsIgnoreCase("jixw210"));
	case kBarNote:
		return !visitMode();
	case kBarCompass:
		return !_puzzleMode && _barWarp;
	default:
		return true;
	}
}

// Slot hit box: strictly between 111+49i and 149+49i, 437 and 475 (E-1102)
int CryOmni3DEngine_China::barSlotAt(const Common::Point &p) const {
	if (p.y <= 437 || p.y >= 475) {
		return -1;
	}
	for (uint i = 0; i < kSlotCount; i++) {
		if (p.x > 111 + 49 * (int)i && p.x < 149 + 49 * (int)i) {
			return i;
		}
	}
	return -1;
}

void CryOmni3DEngine_China::barPresent() {
	g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, 640, 480);
	g_system->updateScreen();
	waitFrame();
	pollEvents();
}

// One frame of the bar (E-1100): the saved frame, the visible rows of the band, the sprites
// `lower` rows below their place, the hover label.
void CryOmni3DEngine_China::barDraw(int visibleRows, int lower, int hoveredSlot) {
	_screen.blitFrom(_barSaved);
	const int rows = CLIP(visibleRows, 0, kBandRows);
	if (rows > 0) {
		_screen.blitFrom(_barBand, Common::Rect(0, kBandRows - rows, 640, kBandRows),
		                 Common::Point(0, kBandTop + kBandRows - rows));
	}
	// Draw order (E-1101); puzzle-supplied sprites are not drawn (Q-1103)
	const Sprite &frames = _barSprites[kBarFrames];
	_screen.transBlitFrom(frames.surface, Common::Point(frames.pos.x, frames.pos.y + lower), frames.keyColor);
	for (uint i = 0; i < kSlotCount; i++) {
		if (_slots[i] != kNoObject) {
			const Sprite &o = _objSlot[_slots[i]];
			_screen.transBlitFrom(o.surface, Common::Point(111 + 49 * i, 437 + lower), o.keyColor);
		}
	}
	static const int order[] = { kBarNote, kBarCompass, kBarSpiral, kBarEye };
	for (uint i = 0; i < ARRAYSIZE(order); i++) {
		if (barVisible(order[i])) {
			const Sprite &s = _barSprites[order[i]];
			_screen.transBlitFrom(s.surface, Common::Point(s.pos.x, s.pos.y + lower), s.keyColor);
		}
	}
	if (hoveredSlot >= 0 && _slots[hoveredSlot] != kNoObject) {
		// The name at (3, 415) in white, joined to the slot by a line in 0xF520 (E-1102)
		const int cx = 130 + 49 * hoveredSlot;
		const uint32 line = textColor(0xF520);
		_screen.hLine(3, 428, cx, line);
		_screen.vLine(cx, 428, 437, line);
		_fontManager.setCurrentFont(0);
		_fontManager.setForeColor(textColor(0xffff));
		_fontManager.displayStr(3, kBandTop, label(objectLabelKey(_slots[hoveredSlot]).c_str()));
	}
}

// Slide in over 16 frames, out over 17 (E-1100)
void CryOmni3DEngine_China::barSlide(bool in) {
	if (in) {
		for (int k = 0; k < 16 && !shouldAbort(); k++) {
			barDraw(4 * k + 1, 64 - 4 * k, -1);
			barPresent();
		}
	} else {
		for (int j = 0; j < 17 && !shouldAbort(); j++) {
			barDraw(64 - 4 * j, 4 * j, -1);
			barPresent();
		}
	}
}

// Greedy word wrap in the current font; a newline always breaks.
void CryOmni3DEngine_China::wrapText(const Common::String &text, int width, Common::Array<Common::String> &lines) {
	Common::String current, word;
	for (uint i = 0; i <= text.size(); i++) {
		const char c = i < text.size() ? text[i] : '\n';
		if (c != ' ' && c != '\n' && c != '\r') {
			word += c;
			continue;
		}
		if (!word.empty()) {
			const Common::String candidate = current.empty() ? word : current + " " + word;
			if (!current.empty() && (int)_fontManager.getStrWidth(candidate) > width) {
				lines.push_back(current);
				current = word;
			} else {
				current = candidate;
			}
			word.clear();
		}
		if (c == '\n' && !current.empty()) {
			lines.push_back(current);
			current.clear();
		}
	}
}

// Reading the held document with the eye (E-1103).
void CryOmni3DEngine_China::barDocument() {
	const uint id = _heldObject;
	Common::String key = objectDocKey(id);
	if (!image(key.c_str())) {
		return;
	}
	key.toLowercase();
	if (id != 1) {
		Common::Array<int16> samples;
		playVoice(key, samples);
	}
	Common::Array<Common::String> lines;
	_fontManager.setCurrentFont(1);
	int box[4] = { 0, 0, 0, 0 };
	if (id < ARRAYSIZE(kDocumentBoxes)) {
		memcpy(box, kDocumentBoxes[id], sizeof(box));
		wrapText(label(key.c_str()), box[2] - box[0], lines);
	}
	setCursorSprite(kCursorHeld);
	bool leftPrev = true;
	while (!shouldAbort()) {
		_screen.blitFrom(_still);
		_fontManager.setCurrentFont(1);
		_fontManager.setForeColor(_format.RGBToColor(0, 0, 0));
		for (uint i = 0; i < lines.size() && box[1] + 15 * (int)i < box[3]; i++) {
			_fontManager.displayStr(box[0], box[1] + 15 * i, lines[i]);
		}
		barPresent();
		const bool left = getCurrentMouseButton() == 1;
		if (left && !leftPrev) {
			break;
		}
		leftPrev = left;
	}
	// Back to the bar: the object goes to the first free slot and the place is re-entered
	_mixer->stopHandle(_voiceHandle); // Q-1101: the original stops sound channel 3
	const int slot = barFirstFreeSlot();
	_objects[id].state = 2;
	_objects[id].slot = slot;
	if (slot >= 0) {
		_slots[slot] = id;
	}
	_heldObject = kNoObject;
	_cursorId = -1;
	if (_place) {
		gotoPlace(_place->name);
	}
}

// The minutes screen (E-1104). Font, colour and line height are Q-1104: font 1, black, 15 px.
void CryOmni3DEngine_China::barNotebook() {
	Graphics::ManagedSurface fond;
	if (!loadStill("INTERF/FOND.HNM", fond)) {
		return;
	}
	Sprite exitNormal, exitHover, upNormal, upActive, downNormal, downActive;
	loadSprite("INTERF/SOM_SPIR.SPR", exitNormal);
	loadSprite("INTERF/I_SPRINV.SPR", exitHover);
	loadSprite("INTERF/FL_HAUTR.SPR", upNormal);
	loadSprite("INTERF/FL_HAUTJ.SPR", upActive);
	loadSprite("INTERF/FL_BASR.SPR", downNormal);
	loadSprite("INTERF/FL_BASJ.SPR", downActive);

	_fontManager.setCurrentFont(1);
	Common::Array<Common::String> lines;
	for (uint i = 0; i < _minutes.size() && i < 41; i++) {
		wrapText(_minuteTexts.getValOrDefault(_minutes[i]), 400, lines);
	}
	const int shown = 22;
	int first = MAX<int>(0, (int)lines.size() - shown); // opens scrolled to the end
	uint32 lastStep = g_system->getMillis();
	bool leftPrev = true;
	clearKeys();
	setCursorSprite(kCursorDefaultId);
	const Common::Rect exitRect(exitNormal.pos.x, exitNormal.pos.y, exitNormal.pos.x + exitNormal.surface.w,
	                            exitNormal.pos.y + exitNormal.surface.h);
	const Common::Rect upRect(upNormal.pos.x, upNormal.pos.y, upNormal.pos.x + upNormal.surface.w,
	                          upNormal.pos.y + upNormal.surface.h);
	const Common::Rect downRect(downNormal.pos.x, downNormal.pos.y, downNormal.pos.x + downNormal.surface.w,
	                            downNormal.pos.y + downNormal.surface.h);
	while (!shouldAbort()) {
		const Common::Point m = getMousePos();
		const bool up = upRect.contains(m) && first > 0;
		const bool down = downRect.contains(m) && first + shown < (int)lines.size();
		// One line per 10 ticks of the original's millisecond timer while an arrow is hovered
		const uint32 now = g_system->getMillis();
		if ((up || down) && now - lastStep >= 10) {
			first += up ? -1 : 1;
			lastStep = now;
		} else if (!up && !down) {
			lastStep = now;
		}
		_screen.blitFrom(fond);
		_fontManager.setCurrentFont(1);
		_fontManager.setForeColor(_format.RGBToColor(0, 0, 0));
		for (int i = 0; i < shown && first + i < (int)lines.size(); i++) {
			_fontManager.displayStr(180, 80 + 15 * i, lines[first + i]);
		}
		const Sprite &exitSprite = exitRect.contains(m) ? exitHover : exitNormal;
		_screen.transBlitFrom(exitSprite.surface, exitNormal.pos, exitSprite.keyColor);
		const Sprite &upSprite = up ? upActive : upNormal;
		_screen.transBlitFrom(upSprite.surface, upNormal.pos, upSprite.keyColor);
		const Sprite &downSprite = down ? downActive : downNormal;
		_screen.transBlitFrom(downSprite.surface, downNormal.pos, downSprite.keyColor);
		barPresent();

		bool leave = false;
		while (!_keysPressed.empty()) {
			if (_keysPressed.pop().keycode == Common::KEYCODE_ESCAPE) {
				leave = true;
			}
		}
		const bool left = getCurrentMouseButton() == 1;
		if (left && !leftPrev && exitRect.contains(getMousePos())) {
			leave = true;
		}
		leftPrev = left;
		if (leave) {
			break;
		}
	}
}

int32 CryOmni3DEngine_China::interfaceScreen() {
	_rightLatch = true; // E-1100: one open per right-button press
	barLoad();
	// The display is redrawn first and the frame saved (E-1100)
	drawView();
	_barSaved.copyFrom(_screen);
	_barWarp = _display == kDisplayWarp;
	barRebuildSlots();

	// The band: rows 415..479 blended halfway towards RGB565 0x39CA (E-1100)
	byte tr, tg, tb;
	_format.colorToRGB(textColor(0x39CA), tr, tg, tb);
	_barBand.create(640, kBandRows, _format);
	for (int y = 0; y < kBandRows; y++) {
		const uint16 *src = (const uint16 *)_barSaved.getBasePtr(0, kBandTop + y);
		uint16 *dst = (uint16 *)_barBand.getBasePtr(0, y);
		for (int x = 0; x < 640; x++) {
			byte r, g, b;
			_format.colorToRGB(src[x], r, g, b);
			dst[x] = _format.RGBToColor((r + tr) / 2, (g + tg) / 2, (b + tb) / 2);
		}
	}

	clearKeys();
	barSlide(true);
	int32 result = 1;
	bool wasBelow = false;
	// A button still down while the bar opens does nothing until it is released (E-1100)
	bool leftPrev = getCurrentMouseButton() == 1;
	bool rightPrev = getCurrentMouseButton() == 2;
	while (!shouldAbort()) {
		bool closeKey = false;
		while (!_keysPressed.empty()) {
			if (_keysPressed.pop().keycode == Common::KEYCODE_SPACE) {
				closeKey = true;
			}
		}
		const int buttons = getCurrentMouseButton();
		const bool left = buttons == 1, right = buttons == 2;
		const Common::Point m = getMousePos();
		const int hovered = barSlotAt(m);
		const bool held = _heldObject != kNoObject;

		if (closeKey || (right && !rightPrev)) {
			break;
		}
		if (left && !leftPrev) {
			if (hovered >= 0) {
				// Take, put back or swap (E-1102)
				const uint inSlot = _slots[hovered];
				if (held) {
					const uint old = _heldObject;
					_objects[old].state = 2;
					_objects[old].slot = hovered;
					_slots[hovered] = old;
					_heldObject = kNoObject;
					barObjectSprites(old);
				}
				if (inSlot != kNoObject) {
					if (!held) {
						_slots[hovered] = kNoObject;
					}
					barHold(inSlot);
				}
				_cursorId = -1;
			} else if (barVisible(kBarSpiral) && barRect(kBarSpiral).contains(m)) {
				if (held) {
					const int slot = barFirstFreeSlot();
					_objects[_heldObject].state = 2;
					_objects[_heldObject].slot = slot;
					if (slot >= 0) {
						_slots[slot] = _heldObject;
					}
					_heldObject = kNoObject;
				}
				_cursorId = -1;
				result = 0;
				break;
			} else if (held && _heldObject < kDocumentCount && barRect(kBarEye).contains(m)) {
				barDocument();
				leftPrev = true;
				continue;
			} else if (barVisible(kBarNote) && barRect(kBarNote).contains(m)) {
				barNotebook();
				leftPrev = true;
				continue;
			} else if (barVisible(kBarCompass) && barRect(kBarCompass).contains(m)) {
				warning("China: the map is not implemented (Q-1100)");
			}
		}
		leftPrev = left;
		rightPrev = right;

		// To use the held object on the scene: leave upwards after having been below y 400 (E-1100)
		if (_heldObject != kNoObject) {
			if (m.y > 400) {
				wasBelow = true;
			} else if (wasBelow) {
				break;
			}
		}
		int cursor = _heldObject != kNoObject ? (int)kCursorHeld : (int)kCursorDefaultId;
		if (_heldObject < kDocumentCount && barRect(kBarEye).contains(m)) {
			cursor = kCursorEye;
		}
		setCursorSprite(cursor);
		barDraw(kBandRows, 0, hovered);
		barPresent();
	}
	if (!shouldAbort()) {
		barSlide(false);
	}
	// Back in the frame loop the next warp draw is a plain draw (E-1100, E-0903)
	_skipFade = true;
	_spaceArmed = _spaceUp = false;
	_pressLatch = true;
	_cursorId = -1;
	clearKeys();
	return result;
}

} // End of namespace China
} // End of namespace CryOmni3D
