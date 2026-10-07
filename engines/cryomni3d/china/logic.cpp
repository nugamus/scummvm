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

#include "graphics/cursorman.h"
#include "image/tga.h"

#include "cryomni3d/china/engine.h"

namespace CryOmni3D {
namespace China {

// Object sprite stems, after c_/r_/i_ (spec/china-zones.md Objects, E-0905)
static const char *const kObjectStems[CryOmni3DEngine_China::kObjectCount] = {
	"feuill", "origi", "letpos", "conf1", "conf2", "conf3", "conf4", "indic1", "indic2", "indic3",
	"indic4", "listvi", "procla", "edit", "papier", "rebus", "plantr", "indics", "indics", "soclsc",
	"tourne", "cire", "ruyi", "posepi", "burin", "martea", "clef", "cured", "pincea", "monn",
	"mandat", "mandat", "mandat", "mandat", "clef", ""
};

// Cursor sprite ids (E-0901)
enum {
	kCursorGoWarp = 8,
	kCursorLook = 9,
	kCursorTake = 10,
	kCursorDefault = 11,
	kCursorGoStill = 12,
	kCursorUse = 14,
	kCursorDoc = 15,
	kCursorTalk = 16
};

// New game (E-0506): every object back to its initial state, MANDAT1 in the inventory,
// then the start procedure.
void CryOmni3DEngine_China::newGame() {
	memset(_vars, 0, sizeof(_vars));
	for (uint i = 0; i < kObjectCount; i++) {
		_objects[i].state = 0;
		_objects[i].slot = -1;
	}
	_objects[30].state = 2;
	_heldObject = kNoObject;
	_minutes.clear();
	_zones.clear();
	_gameRunning = true;
	gotoPlace("Script_Start");
}

const CryOmni3DEngine_China::PlaceDef *CryOmni3DEngine_China::findPlace(const Common::String &name) const {
	for (const PlaceDef *p = kPlaces; p->name; p++) {
		if (name.equalsIgnoreCase(p->name)) {
			return p;
		}
	}
	return nullptr;
}

// Goto (E-0903, E-0700): the target becomes current, its entry part runs on the next tick;
// the display keeps its last frame until then.
void CryOmni3DEngine_China::gotoPlace(const char *name) {
	const PlaceDef *place = findPlace(name);
	if (!place) {
		warning("China: no place %s", name);
		return;
	}
	debug(1, "China: goto %s", place->name);
	_place = place;
	_entryPending = true;
	_display = kDisplayNone;
	_clickedZone = -1;
}

void CryOmni3DEngine_China::tick() {
	if (!_place) {
		return;
	}
	const bool entry = _entryPending;
	_entryPending = false;
	_place->proc(*this, entry);
}

// Zones (spec/china-zones.md Zones, E-0904)
void CryOmni3DEngine_China::addZone(byte type, int top, int left, int bottom, int right, bool disabled,
                                    const char *target, const char *key, double alpha, double beta) {
	if (_zones.size() >= 40) {
		warning("China: more than 40 zones");
		return;
	}
	Zone z;
	z.rect = Common::Rect(left, top, right + 1, bottom + 1);
	z.disabled = disabled;
	z.type = type;
	z.target = target ? target : "";
	z.key = key ? key : "";
	z.alpha = alpha;
	z.beta = beta;
	_zones.push_back(z);
}

void CryOmni3DEngine_China::zoneGo(int top, int left, int bottom, int right, bool disabled, const char *target,
                                   int arg, double alpha, double beta) {
	addZone(kZoneGo, top, left, bottom, right, disabled, target, nullptr, alpha, beta);
}

void CryOmni3DEngine_China::zoneLook(int top, int left, int bottom, int right, bool disabled, const char *target,
                                     int arg) {
	addZone(kZoneLook, top, left, bottom, right, disabled || visitMode(), target, nullptr, -1., -1.);
}

void CryOmni3DEngine_China::zoneTake(int top, int left, int bottom, int right, bool disabled, const char *target) {
	addZone(kZoneTake, top, left, bottom, right, disabled || visitMode(), target, nullptr, -1., -1.);
}

void CryOmni3DEngine_China::zoneUse(int top, int left, int bottom, int right, bool disabled) {
	addZone(kZoneUse, top, left, bottom, right, disabled || visitMode(), nullptr, nullptr, -1., -1.);
}

void CryOmni3DEngine_China::zoneLabel(int top, int left, int bottom, int right, bool disabled, const char *key) {
	addZone(kZoneLabel, top, left, bottom, right, disabled || !visitMode(), nullptr, key, -1., -1.);
}

void CryOmni3DEngine_China::zoneDoc(int top, int left, int bottom, int right, bool disabled, const char *key) {
	addZone(kZoneDoc, top, left, bottom, right, disabled, nullptr, key, -1., -1.);
}

void CryOmni3DEngine_China::zoneTalk(int top, int left, int bottom, int right, bool disabled) {
	addZone(kZoneTalk, top, left, bottom, right, disabled || visitMode(), nullptr, nullptr, -1., -1.);
}

void CryOmni3DEngine_China::zoneEnable(uint i) {
	if (i >= _zones.size()) {
		return;
	}
	const byte type = _zones[i].type;
	if (visitMode() && (type == kZoneUse || type == kZoneLabel || type == kZoneTalk)) {
		return;
	}
	_zones[i].disabled = false;
}

void CryOmni3DEngine_China::zoneDisable(uint i) {
	if (i < _zones.size()) {
		_zones[i].disabled = true;
	}
}

int CryOmni3DEngine_China::zoneAt(const Common::Point &hot) {
	Common::Point p = hot;
	if (_display == kDisplayWarp) {
		// The hit test returns y as 767 - image row (E-0604)
		p = _omni3D.mapMouseCoords(hot);
		p.y = 767 - p.y;
	} else if (_display != kDisplayStill) {
		return -1;
	}
	for (uint i = 0; i < _zones.size(); i++) {
		if (_zones[i].rect.contains(p)) {
			return _zones[i].disabled ? -1 : (int)i;
		}
	}
	return -1;
}

// The zone handler (spec/china-zones.md Hover, Click and transitions; E-0900)
bool CryOmni3DEngine_China::zoneHandler() {
	_labelText.clear();
	_clickedZone = -1;
	if (!_pressed) {
		_pressLatch = false;
	}
	const int z = _hoveredZone;
	if (z < 0 || z >= (int)_zones.size()) {
		return false;
	}
	const Zone &zone = _zones[z];
	const bool holding = _heldObject != kNoObject;
	const bool press = _pressed && (!_pressLatch || zone.type == kZoneLabel || zone.type == kZoneTalk);
	if (!press) {
		switch (zone.type) {
		case kZoneGo:
			setCursorSprite(_display == kDisplayWarp ? kCursorGoWarp : kCursorGoStill);
			break;
		case kZoneLook:
			setCursorSprite(kCursorLook);
			break;
		case kZoneTake:
			if (!holding) {
				setCursorSprite(kCursorTake);
			}
			break;
		case kZoneUse:
			if (!holding) {
				setCursorSprite(kCursorUse);
			}
			break;
		case kZoneLabel:
			_labelText = label(zone.key.c_str());
			break;
		case kZoneDoc:
			if (!holding) {
				setCursorSprite(kCursorDoc);
				_labelText = label(zone.key.c_str());
			}
			break;
		case kZoneTalk:
			setCursorSprite(kCursorTalk);
			break;
		default:
			break;
		}
		if (!_labelText.empty()) {
			_labelPos = cursorTopLeft() + Common::Point(20, 20);
		}
		return false;
	}

	switch (zone.type) {
	case kZoneGo:
	case kZoneLook:
	case kZoneTake: {
		_pressLatch = true;
		const Zone copy = zone;
		if (_display == kDisplayWarp) {
			turnToPoint(cursorTopLeft());
			// zoomIn(arg): arg is 0 in all data, so no zoom frames (E-0903)
			if (copy.alpha >= 0.) {
				setAngles(copy.alpha, copy.beta);
			}
		}
		if (!copy.target.empty()) {
			gotoPlace(copy.target.c_str());
			return true;
		}
		_clickedZone = z;
		return false;
	}
	case kZoneUse:
		_pressLatch = true;
		_clickedZone = z;
		return false;
	case kZoneDoc:
		if (!holding) {
			// Q-0903: the documentation screen is not implemented yet; the place is re-entered
			warning("China: documentation entry %s", zone.key.c_str());
			gotoPlace(_place->name);
			return true;
		}
		return false;
	default:
		_clickedZone = z;
		return false;
	}
}

// Place API (spec/china-zones.md Place API)
void CryOmni3DEngine_China::warp(const char *name) {
	if (!loadStill(Common::Path(Common::String::format("WARP/%s.HNM", name)), _warpImage)) {
		return;
	}
	_omni3D.setSourceSurface(_warpImage.surfacePtr());
	_alphaSpeed = _betaSpeed = 0.;
	_display = kDisplayWarp;
	_fadePending = true;
}

// Still lookup (E-0510): Images\<stem>.hnm, then Images\<stem>.tga, then Loc\<stem>.
void CryOmni3DEngine_China::image(const char *name) {
	const Common::String stem(name);
	bool ok = false;
	if (Common::File::exists(Common::Path("IMAGES/" + stem + ".HNM"))) {
		ok = loadStill(Common::Path("IMAGES/" + stem + ".HNM"), _still);
	} else {
		Common::File file;
		if (file.open(Common::Path("IMAGES/" + stem + ".TGA"))) {
			Image::TGADecoder tga;
			if (tga.loadStream(file) && tga.getSurface()) {
				Graphics::Surface *conv = tga.getSurface()->convertTo(_format);
				_still.copyFrom(*conv);
				conv->free();
				delete conv;
				ok = true;
			}
		} else {
			ok = loadStill(Common::Path("LOC/" + stem + ".HNM"), _still);
		}
	}
	if (!ok) {
		warning("China: no image %s", name);
		return;
	}
	_display = kDisplayStill;
	_fadePending = false;
}

void CryOmni3DEngine_China::video(const char *name) {
	CursorMan.showMouse(false);
	playHNM(Common::Path(Common::String::format("HNM/%s.HNS", name)), Audio::Mixer::kMusicSoundType);
	CursorMan.showMouse(true);
	clearKeys();
}

void CryOmni3DEngine_China::setAngles(double alpha, double beta) {
	_omni3D.setAlpha(alpha);
	_omni3D.setBeta(beta);
}

// Objects (E-0905)
void CryOmni3DEngine_China::objectToInventory(uint id) {
	if (id < kObjectCount && _objects[id].state == 0) {
		_objects[id].state = 2;
	}
}

void CryOmni3DEngine_China::objectToCursor(uint id) {
	if (id >= kObjectCount || _objects[id].state == 1 || _objects[id].state == 2) {
		return;
	}
	if (_heldObject != kNoObject) {
		_objects[_heldObject].state = 2;
	}
	_objects[id].state = 1;
	_heldObject = id;
	loadSprite(Common::Path(Common::String::format("SPRITES/OBJETS/R_%s.SPR", kObjectStems[id])), _heldCursor);
}

void CryOmni3DEngine_China::objectDestroy(uint id) {
	if (id >= kObjectCount) {
		return;
	}
	_objects[id].state = 3;
	if (_heldObject == id) {
		_heldObject = kNoObject;
	}
}

void CryOmni3DEngine_China::minutesAdd(const char *key) {
	for (uint i = 0; i < _minutes.size(); i++) {
		if (_minutes[i].equalsIgnoreCase(key)) {
			return;
		}
	}
	_minutes.push_back(key);
}

// Q-0011: the synced dialogue player is not implemented yet.
void CryOmni3DEngine_China::dialogue(const char *line, const char *stemOther, const char *stemPlayer) {
	warning("China: dialogue %s (%s, %s) not played", line, stemOther, stemPlayer);
}

// Q-0950: these calls are not specced yet; they log and do nothing.
void CryOmni3DEngine_China::voice(const char *line) {
	warning("China: voice %s not played", line);
}

void CryOmni3DEngine_China::soundQueue(const char *name) {
	warning("China: sound %s not played", name);
}

void CryOmni3DEngine_China::soundPlayWait(const char *name) {
	warning("China: sound %s not played", name);
}

void CryOmni3DEngine_China::soundStop() {
}

void CryOmni3DEngine_China::screenEffect() {
}

void CryOmni3DEngine_China::interfaceScreen() {
	warning("China: interface screen not implemented");
}

int32 CryOmni3DEngine_China::puzzle(int32 number, int32 arg) {
	warning("China: puzzle %d (%d) not implemented", number, arg);
	return 0;
}

uint32 CryOmni3DEngine_China::timeMs() const {
	return g_system->getMillis();
}

int32 CryOmni3DEngine_China::unknownCall(uint32 address, int32 a, int32 b) {
	warning("China: call 0x%x(%d, %d) not implemented", address, a, b);
	return 0;
}

// Cursors (E-0901): the hot point is the sprite's centre, except `inter` (1, 45).
void CryOmni3DEngine_China::setCursorSprite(int id) {
	if (id == _cursorId) {
		return;
	}
	const Sprite &s = id == kCursorHeld ? _heldCursor : _cursors[id];
	if (s.surface.empty()) {
		return;
	}
	const int hotX = id == 13 ? 1 : s.surface.w / 2;
	const int hotY = id == 13 ? 45 : s.surface.h / 2;
	CursorMan.replaceCursor(s.surface.rawSurface(), hotX, hotY, s.keyColor);
	_cursorId = id;
	_cursorHot = Common::Point(hotX, hotY);
}

Common::Point CryOmni3DEngine_China::cursorTopLeft() {
	return getMousePos() - _cursorHot;
}

// Default cursor before the place runs (spec/china-zones.md Hover step 2)
void CryOmni3DEngine_China::updateCursor() {
	const int defaultId = _heldObject != kNoObject ? (int)kCursorHeld : (int)kCursorDefault;
	if (_display != kDisplayWarp) {
		setCursorSprite(defaultId);
		return;
	}
	const Common::Point tl = cursorTopLeft();
	const int col = tl.x < 100 ? 0 : (tl.x > 540 ? 2 : 1);
	const int row = tl.y < 100 ? 0 : (tl.y > 380 ? 2 : 1);
	static const int bands[3][3] = {
		{ 4, 0, 7 }, // x < 100: tri315, tri270, tri225
		{ 2, -1, 3 }, // middle: tri0, default, tri180
		{ 5, 1, 6 }  // x > 540: tri45, tri90, tri135
	};
	const int id = bands[col][row];
	setCursorSprite(id < 0 ? defaultId : id);
}

// Edge scrolling (E-0509, E-0605) from the cursor centre; velocities decay by 0.8 per frame.
void CryOmni3DEngine_China::scrollByCursor() {
	const Common::Point c = getMousePos();
	double pushX = 0., pushY = 0.;
	if (c.x < 100) {
		pushX = 100 - c.x;
	} else if (c.x > 540) {
		pushX = 540 - c.x;
	}
	if (c.y < 100) {
		pushY = c.y - 100;
	} else if (c.y > 380) {
		pushY = c.y - 380;
	}
	const double k = 5. - _panoramaSpeed;
	_alphaSpeed += pushX / (1250. * k);
	_betaSpeed += pushY / (1500. * k);
	if (_alphaSpeed != 0. || _betaSpeed != 0.) {
		setAngles(_omni3D.getAlpha() + _alphaSpeed, _omni3D.getBeta() + _betaSpeed);
		_alphaSpeed *= 0.8;
		_betaSpeed *= 0.8;
	}
}

static double wrapAngle(double a) {
	while (a > M_PI) {
		a -= 2. * M_PI;
	}
	while (a <= -M_PI) {
		a += 2. * M_PI;
	}
	return a;
}

// turnToPoint (E-0606): 32 frames, each closing 20% of the remaining (wrapped) difference.
void CryOmni3DEngine_China::turnToPoint(const Common::Point &topLeft) {
	const double targetAlpha = _omni3D.getAlpha() + atan2(320. - topLeft.x, 416.);
	const double targetBeta = _omni3D.getBeta() + atan2(topLeft.y - 240., 416.);
	for (uint i = 0; i < 32 && !shouldAbort(); i++) {
		const double alpha = _omni3D.getAlpha();
		setAngles(alpha + wrapAngle(targetAlpha - alpha) * 0.2,
		          _omni3D.getBeta() + (targetBeta - _omni3D.getBeta()) * 0.2);
		drawFrame();
		waitFrame();
		pollEvents();
	}
}

// Cross-fade (E-0903): 19 frames, alternate columns, each channel moves min(16k, 256)/256 of
// the way from the old screen to the new warp.
void CryOmni3DEngine_China::crossFade() {
	const Graphics::Surface *view = _omni3D.getSurface();
	if (!view) {
		return;
	}
	Graphics::ManagedSurface old;
	old.copyFrom(_screen);
	for (uint k = 1; k <= 19 && !shouldAbort(); k++) {
		const int f = MIN<int>(16 * k, 256);
		for (int y = 0; y < 480; y++) {
			const uint16 *o = (const uint16 *)old.getBasePtr(0, y);
			const uint16 *n = (const uint16 *)view->getBasePtr(0, y);
			uint16 *d = (uint16 *)_screen.getBasePtr(0, y);
			for (int x = k & 1; x < 640; x += 2) {
				byte orr, og, ob, nr, ng, nb;
				_format.colorToRGB(o[x], orr, og, ob);
				_format.colorToRGB(n[x], nr, ng, nb);
				d[x] = _format.RGBToColor(orr - (orr - nr) * f / 256, og - (og - ng) * f / 256,
				                          ob - (ob - nb) * f / 256);
			}
		}
		g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, 640, 480);
		g_system->updateScreen();
		waitFrame();
		pollEvents();
	}
}

// Label (E-0902): a darkened box, then the text in slot 0, black shadow then white.
void CryOmni3DEngine_China::drawLabel() {
	if (_labelText.empty() || _display != kDisplayWarp) {
		return;
	}
	_fontManager.setCurrentFont(0);
	const int width = _fontManager.getStrWidth(_labelText);
	int x = _labelPos.x;
	const int y = _labelPos.y;
	if (x + width + 2 >= 640) {
		x = 638 - width;
	}
	if (y + 20 >= 479) {
		return;
	}
	Common::Rect box(x - 2, y - 2, x - 1 + width, y + 13);
	box.clip(Common::Rect(640, 480));
	for (int yy = box.top; yy < box.bottom; yy++) {
		uint16 *p = (uint16 *)_screen.getBasePtr(box.left, yy);
		for (int xx = box.left; xx < box.right; xx++, p++) {
			byte r, g, b;
			_format.colorToRGB(*p, r, g, b);
			*p = _format.RGBToColor((r + 82) / 2, (g + 82) / 2, (b + 82) / 2);
		}
	}
	_fontManager.setForeColor(_format.RGBToColor(0, 0, 0));
	_fontManager.displayStr(x + 1, y + 1, _labelText);
	_fontManager.setForeColor(textColor(0xffff));
	_fontManager.displayStr(x, y, _labelText);
}

void CryOmni3DEngine_China::drawFrame() {
	if (_display == kDisplayWarp) {
		const Graphics::Surface *view = _omni3D.getSurface();
		if (view) {
			_screen.copyRectToSurface(*view, 0, 0, Common::Rect(640, 480));
		}
	} else if (_display == kDisplayStill) {
		_screen.blitFrom(_still);
	}
	drawLabel();
	g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, 640, 480);
	g_system->updateScreen();
}

// The original never paces its frames (E-0804) and turns the view per frame; we run 25 frames/s (Q-0800).
void CryOmni3DEngine_China::waitFrame() {
	const uint32 now = g_system->getMillis();
	if (_nextFrame > now) {
		g_system->delayMillis(_nextFrame - now);
	}
	_nextFrame = MAX(_nextFrame, now) + 40;
}

// One frame (E-0508): keys, cursor, zone under the hot point, the place, then the draw.
void CryOmni3DEngine_China::playLoop() {
	clearKeys();
	while (!shouldAbort()) {
		pollEvents();
		bool toMenu = false;
		while (!_keysPressed.empty()) {
			if (_keysPressed.pop().keycode == Common::KEYCODE_ESCAPE) {
				toMenu = true;
			}
		}
		if (toMenu) {
			return;
		}
		_pressed = getCurrentMouseButton() == 1;
		updateCursor();
		const int hovered = zoneAt(getMousePos());
		if (hovered != _hoveredZone) {
			debug(2, "China: zone %d under (%d, %d)", hovered, getMousePos().x, getMousePos().y);
		}
		_hoveredZone = hovered;
		tick();
		if (_display == kDisplayWarp && _fadePending) {
			_fadePending = false;
			crossFade();
		} else {
			if (_display == kDisplayWarp) {
				scrollByCursor();
			}
			drawFrame();
		}
		waitFrame();
	}
}

} // End of namespace China
} // End of namespace CryOmni3D
