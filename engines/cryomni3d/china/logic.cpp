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

#include "audio/audiostream.h"
#include "audio/decoders/raw.h"

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

const char *CryOmni3DEngine_China::objectStem(uint id) {
	return id < kObjectCount ? kObjectStems[id] : "";
}

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
	for (uint i = 0; i < kObjectCount; i++) {
		_objLabelKey[i].clear();
		_objDocKey[i].clear();
	}
	resetPlayState();
	_gameRunning = true;
	gotoPlace("Script_Start");
}

// What a new game and a loaded game both start without: the frame's input and display state.
void CryOmni3DEngine_China::resetPlayState() {
	zonesReset();
	_labelText.clear();
	_fadePending = false;
	_pressLatch = false;
	_clickedZone = -1;
	_alphaSpeed = _betaSpeed = 0.;
	_endOfPlay = false;
	_puzzleMode = false;
	_rightLatch = false;
	_skipFade = false;
	_fightStart = 0;
	_display = kDisplayNone;
	_cursorId = -1;
	_mixer->stopHandle(_voiceHandle);
	_mixer->stopHandle(_soundHandle);
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
	// Q-0012: in automatic save mode the original also saves here, unless a place asked to
	// skip it once (E-0954); ScummVM's own saves replace that.
	_place = place;
	_entryPending = true;
	musicForPlace(place->name);
	_display = kDisplayNone;
	_clickedZone = -1;
}

void CryOmni3DEngine_China::tick() {
	if (!_place) {
		return;
	}
	const bool entry = _entryPending;
	_entryPending = false;
	_inPlaceCall = true;
	_place->proc(*this, entry);
	_inPlaceCall = false;
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
			setCursorSprite(holding ? kCursorHeld : kCursorTake);
			break;
		case kZoneUse:
			setCursorSprite(holding ? kCursorHeld : kCursorUse);
			break;
		case kZoneLabel:
			_labelText = label(zone.key.c_str());
			if (_labelText.empty()) {
				_labelText = "ACCES LEGENDE INCONNU"; // the original's text for a missing key (E-0902)
			}
			break;
		case kZoneDoc:
			if (!holding) {
				setCursorSprite(kCursorDoc);
				_labelText = label(zone.key.c_str());
				if (_labelText.empty()) {
					_labelText = "ACCES BASE DOCUMENTAIRE INCONNU";
				}
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
			_pressLatch = true;
			warning("China: documentation entry %s", zone.key.c_str());
			gotoPlace(_place->name);
			return true;
		}
		// Holding an object, the place's code reacts: showing an item to someone (Q-1302)
		_pressLatch = true;
		_clickedZone = z;
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
bool CryOmni3DEngine_China::image(const char *name) {
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
		return false;
	}
	_display = kDisplayStill;
	_fadePending = false;
	return true;
}

// Videos pause the music (spec/china-zones.md Place API)
void CryOmni3DEngine_China::video(const char *name) {
	_mixer->pauseHandle(_musicHandle, true);
	CursorMan.showMouse(false);
	playHNM(Common::Path(Common::String::format("HNM/%s.HNS", name)), Audio::Mixer::kMusicSoundType);
	CursorMan.showMouse(true);
	clearKeys();
	_mixer->pauseHandle(_musicHandle, false);
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
	loadSprite(Common::Path(Common::String::format("SPRITES/OBJETS/R_%s.SPR", objectStem(id))), _heldCursor);
	_cursorId = -1; // the held sprite changed: set it again even if the id is the same
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

// Music (E-0206): the place's first three letters choose a track; other places keep the
// current one. ZIK files are headerless 22050 Hz 16-bit stereo PCM, looped whole.
// Q-1000: the original fades the volume by 10 per tick between tracks; we switch at once.
void CryOmni3DEngine_China::musicForPlace(const char *place) {
	static const struct {
		const char *prefix;
		const char *track;
	} tracks[] = {
		{ "pne", "Allee" }, { "aie", "Allee" }, { "aio", "Allee" }, { "ctp", "Allee" }, { "cgc", "Allee" },
		{ "cpc", "Bureaux1" }, { "lge", "Bureaux1" }, { "spf", "Bureaux1" },
		{ "lga", "Bureaux2" }, { "bpi", "Bureaux2" }, { "esp", "Bureaux2" }, { "nwf", "Bureaux2" },
		{ "ban", "Bureaux2" }, { "bda", "Bureaux2" },
		{ "pdc", "Concub" }, { "jix", "Jardins" },
		{ "cth", "SalleHS" }, { "chs", "SalleHS" }, { "shs", "SalleHS" }
	};
	for (uint i = 0; i < ARRAYSIZE(tracks); i++) {
		if (!scumm_strnicmp(place, tracks[i].prefix, 3)) {
			playMusic(tracks[i].track);
			return;
		}
	}
}

void CryOmni3DEngine_China::playMusic(const char *name) {
	if (_musicName.equalsIgnoreCase(name)) {
		return;
	}
	_mixer->stopHandle(_musicHandle);
	_musicName = name;
	Common::File *file = new Common::File();
	if (!file->open(Common::Path(Common::String::format("MUSIC/%s.ZIK", name)))) {
		warning("China: no music %s", name);
		delete file;
		return;
	}
	Audio::SeekableAudioStream *raw = Audio::makeRawStream(file, 22050,
	        Audio::FLAG_16BITS | Audio::FLAG_STEREO | Audio::FLAG_LITTLE_ENDIAN, DisposeAfterUse::YES);
	_mixer->playStream(Audio::Mixer::kMusicSoundType, &_musicHandle, Audio::makeLoopingAudioStream(raw, 0));
}

// Key repeats are dropped, so a held key acts once.
bool CryOmni3DEngine_China::keyEvent(const Common::Event &event) {
	if (event.type == Common::EVENT_KEYDOWN && event.kbdRepeat) {
		return true;
	}
	if (event.type == Common::EVENT_KEYUP && event.kbd.keycode == Common::KEYCODE_SPACE) {
		_spaceUp = true;
	}
	return false;
}

// DirectInput scan code 57 is Space (E-0954), the only key a place tests.
bool CryOmni3DEngine_China::keyDown(int32 scanCode) {
	return scanCode == 57 && _spacePressed;
}

// The object label and examine settings (E-0954); stored in saves since version 2 (E-1107).
void CryOmni3DEngine_China::objectSetLabel(uint id, const char *key) {
	if (id < kObjectCount) {
		_objLabelKey[id] = key; // the object's +0x2c string (E-0954)
	}
}

void CryOmni3DEngine_China::objectSetExamine(uint id, const char *place) {
	if (id < kObjectCount) {
		_objDocKey[id] = place; // the object's +0x20 string (E-0954)
	}
}

// Screen fade (spec/china-zones.md Place API): the cross-fade's 19 steps towards black.
void CryOmni3DEngine_China::screenEffect() {
	fadeTo(nullptr);
}

uint32 CryOmni3DEngine_China::timeMs() const {
	return g_system->getMillis();
}

int32 CryOmni3DEngine_China::unknownCall(const char *name) {
	warning("China: call %s not implemented", name);
	return 0;
}

// Cursors (E-0901): the hot point is the sprite's centre, except `inter` (1, 45).
const Sprite &CryOmni3DEngine_China::cursorSprite(int id) const {
	if (id == kCursorHeld) {
		return _heldCursor;
	}
	if (id == kCursorEye) {
		return _objEye[MIN<uint>(_heldObject, kObjectCount - 1)];
	}
	return _cursors[id];
}

void CryOmni3DEngine_China::setCursorSprite(int id) {
	if (id == _cursorId) {
		return;
	}
	const Sprite &s = cursorSprite(id);
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
	if (view) {
		fadeTo(view);
	}
}

// The cross-fade step towards a picture, or towards black without one.
void CryOmni3DEngine_China::fadeTo(const Graphics::Surface *target) {
	Graphics::ManagedSurface old;
	old.copyFrom(_screen);
	for (uint k = 1; k <= 19 && !shouldAbort(); k++) {
		const int f = MIN<int>(16 * k, 256);
		for (int y = 0; y < 480; y++) {
			const uint16 *o = (const uint16 *)old.getBasePtr(0, y);
			const uint16 *n = target ? (const uint16 *)target->getBasePtr(0, y) : nullptr;
			uint16 *d = (uint16 *)_screen.getBasePtr(0, y);
			for (int x = k & 1; x < 640; x += 2) {
				byte orr, og, ob, nr = 0, ng = 0, nb = 0;
				_format.colorToRGB(o[x], orr, og, ob);
				if (n) {
					_format.colorToRGB(n[x], nr, ng, nb);
				}
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
	drawLabelBox(_labelText, _labelPos);
}

void CryOmni3DEngine_China::drawLabelBox(const Common::String &text, const Common::Point &pos) {
	_fontManager.setCurrentFont(0);
	const int width = _fontManager.getStrWidth(text);
	int x = pos.x;
	const int y = pos.y;
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
	_fontManager.displayStr(x + 1, y + 1, text);
	_fontManager.setForeColor(textColor(0xffff));
	_fontManager.displayStr(x, y, text);
}

void CryOmni3DEngine_China::drawView() {
	if (_display == kDisplayWarp) {
		const Graphics::Surface *view = _omni3D.getSurface();
		if (view) {
			_screen.copyRectToSurface(*view, 0, 0, Common::Rect(640, 480));
		}
	} else if (_display == kDisplayStill) {
		_screen.blitFrom(_still);
	}
}

void CryOmni3DEngine_China::drawFrame() {
	drawView();
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
	_spaceUp = _spaceArmed = false;
	_inPlay = true;
	while (!shouldQuit()) {
		pollEvents();
		bool toMenu = false;
		_spacePressed = false;
		while (!_keysPressed.empty()) {
			const Common::KeyCode key = _keysPressed.pop().keycode;
			if (key == Common::KEYCODE_ESCAPE) {
				toMenu = true;
			} else if (key == Common::KEYCODE_SPACE) {
				_spacePressed = true;
			}
		}
		if (toMenu || _endOfPlay) {
			_inPlay = false;
			if (_endOfPlay) {
				// The end of the story: the credits, then the menu (Q-1001)
				_gameRunning = false;
				_endOfPlay = false;
				credits();
			}
			return;
		}
		if (_pendingLoad >= 0) {
			applyLoad();
		}
		_pressed = getCurrentMouseButton() == 1;
		// The bar opens on Space or a right-button press edge (E-1100)
		const bool right = getCurrentMouseButton() == 2;
		if (!right) {
			_rightLatch = false;
		}
		// Space opens it on its release (E-1100)
		if (_spacePressed) {
			_spaceArmed = true;
		}
		bool spaceOpen = false;
		if (_spaceUp) {
			_spaceUp = false;
			spaceOpen = _spaceArmed;
			_spaceArmed = false;
		}
		if (spaceOpen || (right && !_rightLatch)) {
			interfaceScreen();
			_spacePressed = false;
			if (_pendingLoad >= 0 || shouldQuit()) {
				continue;
			}
		}
		updateCursor();
		const int hovered = zoneAt(getMousePos());
		if (hovered != _hoveredZone) {
			debug(2, "China: zone %d under (%d, %d)", hovered, getMousePos().x, getMousePos().y);
		}
		_hoveredZone = hovered;
		tick();
		const bool fade = _display == kDisplayWarp && _fadePending;
		if (fade) {
			_fadePending = false;
		}
		if (fade && !_skipFade) {
			crossFade();
		} else {
			if (_display == kDisplayWarp) {
				scrollByCursor();
			}
			drawFrame();
		}
		if (_display == kDisplayWarp) {
			_skipFade = false; // only the next warp draw after the bar is a plain draw (E-0903)
		}
		waitFrame();
	}
}

} // End of namespace China
} // End of namespace CryOmni3D
