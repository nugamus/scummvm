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

#include "common/config-manager.h"
#include "common/system.h"

#include "peintre/movie.h"
#include "peintre/peintre.h"
#include "peintre/shell.h"
#include "peintre/sound.h"

namespace Peintre {

// The 2D shell: the zone screen, its inventory bar, magnifier, slots and sunflower
// (ui.md "Entering a zone" .. "Leaving a zone"). State numbers are the original's
// (0x4e27e4) so that the spec's table reads against the code.

// The zone table 0x4a6b18 (ui.md "Zone table"). The handlers are in zones/*.cpp.
const ZoneDef kZones[kNumZones] = {
	{ "A14_031a", 1, { 0, -1, -1 }, 325, 430, 1, 0, false },
	{ "A01_02", 1, { 1, -1, -1 }, 436, 290, 2, 1, true },
	{ "A01_03", 2, { 3, 2, -1 }, 436, 373, 0, 1, true },
	{ "A11_01", 1, { 4, -1, -1 }, 436, 398, 0, 1, true },
	{ "A03_01", 3, { 5, 6, 7 }, 441, 417, 0, 2, true },
	{ "A13_08", 1, { 8, -1, -1 }, 329, 430, 0, 2, true },
	{ "A13_04", 1, { 9, -1, -1 }, 436, 413, 0, 2, true },
	{ "A03_02", 3, { 10, 12, 11 }, 436, 412, 0, 2, true },
	{ "A13_03", 2, { 13, 14, -1 }, 436, 404, 0, 2, true },
	{ "A13_02", 1, { 15, -1, -1 }, 436, 331, 2, 2, true },
	{ "A13_05", 2, { 16, 17, -1 }, 333, 430, 0, 2, true },
	{ "A13_07", 1, { 18, -1, -1 }, 326, 430, 0, 2, true },
	{ "A03_03", 1, { 19, -1, -1 }, 436, 406, 0, 2, true },
	{ "A03_04", 2, { 21, 20, -1 }, 436, 411, 0, 2, true },
	{ "A13_01", 2, { 22, 23, -1 }, 436, 412, 0, 2, true },
	{ "A13_06", 2, { 24, 25, -1 }, 333, 430, 0, 2, true },
	{ "A03_05", 1, { 26, -1, -1 }, 331, 430, 0, 2, true },
	{ "A13_09", 1, { 27, -1, -1 }, 332, 430, 0, 2, true },
	{ "A03_06", 1, { 28, -1, -1 }, 441, 429, 0, 2, true },
	{ "A04_01", 2, { 29, 30, -1 }, 436, 271, 2, 3, true },
	{ "A14_01", 1, { 31, -1, -1 }, 338, 430, 0, 3, true },
	{ "A14_032a", 0, { -1, -1, -1 }, 325, 430, 0, 3, false },
	{ "A04_02", 1, { 32, -1, -1 }, 436, 270, 2, 3, true },
	{ "A04_03", 1, { 33, -1, -1 }, 331, 430, 0, 3, true },
	{ "A14_02", 1, { 34, -1, -1 }, 436, 417, 0, 3, true }
};

/** The object table's sound (0x4a75a8 +4): OP<nn> for some objects, else OP_GENE. */
static Common::String objectSound(uint object) {
	static const byte kOwn[] = { 3, 6, 8, 12, 14, 15, 18, 19, 21, 22, 25, 27, 28, 29, 32, 34 };
	for (byte o : kOwn)
		if (o == object)
			return Common::String::format("OP%02u", object);
	return "OP_GENE";
}

// Geometry (ui.md "The zone screen", E-0410).
static const int kSlotDrawY[3] = { 29, 116, 222 };
static const int kSlotRectY[3] = { 53, 143, 233 };   // (12, y, 59, 58)
static const int kFlyFromY[3] = { 83, 172, 263 };    // x 40
static const Common::Rect kRetourRect(605, 437, 605 + 24, 437 + 29);
static const Common::Rect kRetourMRect(11, 437, 11 + 24, 437 + 29);
static const Common::Rect kArrowRects[2] = {
	Common::Rect(13, 435, 13 + 23, 435 + 30), Common::Rect(509, 435, 509 + 22, 435 + 30)
};
static const Common::Rect kTournRects[3] = {
	Common::Rect(127, 313, 127 + 30, 313 + 26), Common::Rect(111, 244, 111 + 24, 244 + 32),
	Common::Rect(148, 290, 148 + 30, 290 + 26)
};
// Where TOURN rests: dragged, it is centred on this point moved with the cursor (0x410ce4, E-0442).
static const Common::Point kTournGrab[3] = {
	Common::Point(122, 344), Common::Point(132, 295), Common::Point(166, 333)
};
static const Common::Rect kPotArea(590, 400, 590 + 45, 400 + 80);

static Common::Rect slotRect(uint s) {
	return Common::Rect(12, kSlotRectY[s], 12 + 59, kSlotRectY[s] + 58);
}

void drawCursor(Graphics::Surface &dst, const SpriteBank &bank, uint n, const Common::Point &pos) {
	// Table 0x4a64e8 {frame, dx, dy}; the frame's centre goes to the cursor - (dx, dy) (E-0416).
	static const int8 kOffsets[14][2] = {
		{ -7, -10 }, { -9, 0 }, { 8, 0 }, { 0, -9 }, { 0, 8 }, { -9, -9 }, { 8, -9 },
		{ -9, 9 }, { 8, 9 }, { 0, 0 }, { 2, -8 }, { 0, -9 }, { 0, -6 }, { 0, -6 }
	};
	if (n < 14)
		bank.draw(dst, n, pos.x - kOffsets[n][0], pos.y - kOffsets[n][1]);
}

MovieSlot::MovieSlot(Shell &shell, uint movie, bool skippable) : SlotRun(shell) {
	_s.movie(movie, skippable);
}

bool MovieSlot::run(int &result) {
	result = 1;
	return !_s.moviePlaying();
}

VoiceSlot::VoiceSlot(Shell &shell, const char *voice) : SlotRun(shell) {
	_s.voice(voice);
}

bool VoiceSlot::run(int &result) {
	result = 1;
	return _s.voiceWait();
}

Shell::Shell(PeintreEngine *vm) : _vm(vm) {
	const Graphics::PixelFormat format = vm->screen().format;
	_page.create(640, 480, format);
	_bg.create(640, 480, format);
	_page.fillRect(Common::Rect(640, 480), 0);
	_bg.fillRect(Common::Rect(640, 480), 0);
}

Shell::~Shell() {
	delete _run;
	_page.free();
	_bg.free();
	_panorama.free();
}

uint32 &Shell::counter(uint i) {
	return _vm->state().counters[i & 3];
}

bool Shell::background(const Common::String &name) {
	Graphics::Surface s;
	if (!loadTgp(name, s))
		return false;
	const Common::Rect r(MIN<int>(s.w, 640), MIN<int>(s.h, 480));
	_bg.copyRectToSurface(s, 0, 0, r);
	_page.copyRectToSurface(s, 0, 0, r);
	s.free();
	return true;
}

void Shell::restore(const Common::Rect &r) {
	Common::Rect c(r);
	c.clip(Common::Rect(640, 480));
	if (!c.isEmpty())
		_page.copyRectToSurface(_bg, c.left, c.top, c);
}

void Shell::drawSlots() {
	// CapsE when not placed, CapsAC frame 0 for the slot being run, else CapsAO (0x41170e).
	for (uint s = 0; s < _def->count; s++) {
		const SpriteBank &bank = (int)s == _runSlot ? _capsAC[s]
			: _vm->state().placed[_def->objects[s]] ? _capsAO[s] : _capsE[s];
		bank.draw(_page, 0, 0, kSlotDrawY[s]);
	}
}

void Shell::drawMagnifier() {
	const SpriteBank &bank = _mag == kMagOpening || _mag == kMagOpen ? _loupeOut : _loupeIn;
	bank.draw(_page, _magFrame, _def->magX, _def->magY);
}

void Shell::drawMagnifierOpen() {
	_mag = kMagOpen;
	_magFrame = MAX<int>(0, _loupeOut.frameCount() - 1);
	drawMagnifier();
}

void Shell::openMagnifier() {
	if (_mag == kMagOpen || _mag == kMagOpening)
		return;
	_mag = kMagOpening;
	_magFrame = 0;
	drawMagnifier();
}

void Shell::closeMagnifier() {
	if (_mag == kMagClosed || _mag == kMagClosing)
		return;
	_mag = kMagClosing;
	_magFrame = 0;
	drawMagnifier();
	sound("bar_outi");
}

bool Shell::overViewA() const {
	return _mag == kMagOpen && Common::Rect(_def->magX + 78, _def->magY, _def->magX + 78 + 65, _def->magY + 36).contains(_mouse);
}

bool Shell::overViewB() const {
	return _mag == kMagOpen && Common::Rect(_def->magX + 2, _def->magY, _def->magX + 2 + 65, _def->magY + 36).contains(_mouse);
}

void Shell::openBar() {
	_barReq = 1;
}

void Shell::closeBar() {
	_barReq = -1;
}

int Shell::barY() const {
	// y = 479 - trunc(60 sin(3° i)) while sliding, 420 open, 480 closed (0x40fb86).
	if (_barStep < 0)
		return 480;
	if (_barStep >= 30)
		return 420;
	return 479 - (int)(60.0 * sin(M_PI * 3 * _barStep / 180.0));
}

void Shell::moveBar() {
	if (_barReq > 0 && _barStep < 30)
		_barStep++;
	else if (_barReq < 0 && _barStep > -1)
		_barStep--;
}

int Shell::barHit() const {
	if (!barOpen())
		return -1;
	for (uint i = 0; i < 6 && _first + i < _list.size(); i++)
		if (Common::Rect(88 + 70 * i, 436, 88 + 70 * i + 28, 436 + 27).contains(_mouse))
			return i;
	return -1;
}

void Shell::drawBar(Graphics::Surface &dst) {
	const int y = barY();
	_invent.draw(dst, 0, 0, y);
	_invent.draw(dst, _arrowHeld == 0 ? 3 : 1, 13, y);
	_invent.draw(dst, _arrowHeld == 1 ? 4 : 2, 504, y);
	for (uint i = 0; i < 6 && _first + i < _list.size(); i++)
		if (((int)(_first + i) != _dragIndex || _dragObject < 0) && !(_flying && _first + i == _placedIndex))
			_opi.draw(dst, _list[_first + i], 67 + 70 * i, y);
	_pot.pos = Common::Point(558, y + 2);
	_pot.draw(dst);
	const uint32 c = counter(_def->counter);
	if (c > 0)
		_count.draw(dst, c, 558, y + 21);
}

void Shell::syncHeld() {
	// The 35 held flags are built from the list (0x410b80).
	for (uint o = 0; o < kNumObjects; o++)
		_vm->state().setHeld(o, 0);
	for (uint o : _list)
		_vm->state().setHeld(o, 1);
}

int Shell::slotOf(uint object) const {
	for (uint s = 0; s < _def->count; s++)
		if (_def->objects[s] == (int)object)
			return s;
	return -1;
}

void Shell::startCaps(uint slot, const SpriteBank *bank, int rate) {
	_capsBank = bank;
	_capsSlot = slot;
	_capsFrame = 0;
	_capsRate = rate;
}

void Shell::startRetour() {
	// The Retour buttons are drawn; they animate only under the cursor (E-0440).
	_retourOn = true;
	_retour.draw(_page, _retourFrame, 600, 435);
	_retourM.draw(_page, 0, 8, 432);
}

void Shell::place(uint index) {
	// Placed (E-0412): off the list, CapsOP, the bar closes, the object's sound.
	_object = _list[index];
	_slot = slotOf(_object);
	_list.remove_at(index);
	_placedIndex = index;
	_first = MIN<uint>(_first, _list.size() > 6 ? _list.size() - 6 : 0);
	_vm->state().placed[_object] = 1;
	_justPlaced = true;
	_retourOn = false;
	startCaps(_slot, &_capsOP[_slot], 0); // CapsOP every tick (E-0440)
	closeBar();
	sound(objectSound(_object));
	_state = kPlacing;
}

void Shell::movie(uint n, bool skippable) {
	_movieOn = _vm->movies()->open(n);
	_movieSkippable = skippable;
}

bool Shell::moviePlaying() const {
	return _movieOn;
}

void Shell::voice(const Common::String &name) {
	_vm->sound()->playStream(name);
}

bool Shell::voiceWait() {
	if (_vm->sound()->isStreamPlaying() && !_down)
		return false;
	_vm->sound()->stopStream();
	return true;
}

void Shell::stopVoice() {
	_vm->sound()->stopStream();
}

void Shell::sound(const Common::String &name, bool loop) {
	_vm->sound()->playStatic(name, loop);
}

void Shell::stopSound(const Common::String &name) {
	_vm->sound()->stopStatic(name);
}

bool Shell::soundPlaying(const Common::String &name) const {
	return _vm->sound()->isStaticPlaying(name);
}

void Shell::startView(const Common::String &name, bool panorama) {
	// ui.md "Views" (0x40dc22 / 0x409310).
	_viewing = true;
	_viewWait = true;
	_viewPanorama = panorama;
	if (panorama) {
		_panorama.free();
		if (!loadTgp(name, _panorama))
			_panorama.create(640, 480, _page.format);
		_viewPos = Common::Point(MAX(0, _panorama.w - 640) / 2, MAX(0, _panorama.h - 480) / 2);
		_viewSpeed = Common::Point();
		_mouse = Common::Point(320, 240);
		_vm->warpMouse(320, 240);
	} else {
		background(name);
		_viewAnim.load(name);
		_viewAnim.play();
		_viewAnim.step();
		sound("pas_VG2", true);
	}
}

bool Shell::viewStep() {
	if (_viewWait) {
		// Each view first waits for the button to be released.
		_viewWait = _down;
		if (_viewPanorama)
			_page.copyRectToSurface(_panorama, 0, 0, Common::Rect(_viewPos.x, _viewPos.y,
				_viewPos.x + MIN<int>(640, _panorama.w), _viewPos.y + MIN<int>(480, _panorama.h)));
		return false;
	}
	if (!_viewPanorama) {
		if (clicked()) {
			stopSound("pas_VG2");
			_viewing = false;
			_viewAnim.hide();
			return true;
		}
		if (odd() && !_down) {
			_viewAnim.step();
			if (_viewAnim.frame == _viewAnim.last() - 10)
				stopSound("pas_VG2");
		}
		return false;
	}

	if (clicked()) {
		sound("clic_1");
		_viewing = false;
		_panorama.free();
		return true;
	}
	// Held arrows build a speed of 1 per tick up to 32 per axis; else the cursor's margins.
	const bool left = _vm->keyHeld(Common::KEYCODE_LEFT), right = _vm->keyHeld(Common::KEYCODE_RIGHT);
	const bool up = _vm->keyHeld(Common::KEYCODE_UP), down = _vm->keyHeld(Common::KEYCODE_DOWN);
	Common::Point d;
	if (left || right || up || down) {
		_viewSpeed.x = left ? MAX(_viewSpeed.x - 1, -32) : right ? MIN(_viewSpeed.x + 1, 32) : 0;
		_viewSpeed.y = up ? MAX(_viewSpeed.y - 1, -32) : down ? MIN(_viewSpeed.y + 1, 32) : 0;
		d = _viewSpeed;
		_mouse = Common::Point(320, 240);
		_vm->warpMouse(320, 240);
		setCursor(kCursorDefault);
	} else {
		_viewSpeed = Common::Point();
		if (_mouse.x < 64)
			d.x = -(64 - _mouse.x) / 2;
		else if (_mouse.x > 575)
			d.x = (_mouse.x - 575) / 2;
		if (_mouse.y < 64)
			d.y = -(64 - _mouse.y) / 2;
		else if (_mouse.y > 415)
			d.y = (_mouse.y - 415) / 2;
		static const byte kDirCursor[3][3] = { { 5, 3, 6 }, { 1, 0, 2 }, { 7, 4, 8 } };
		setCursor(kDirCursor[(d.y > 0) - (d.y < 0) + 1][(d.x > 0) - (d.x < 0) + 1]);
	}
	_viewPos.x = CLIP<int>(_viewPos.x + d.x, 0, MAX(0, _panorama.w - 640));
	_viewPos.y = CLIP<int>(_viewPos.y + d.y, 0, MAX(0, _panorama.h - 480));
	_page.copyRectToSurface(_panorama, 0, 0, Common::Rect(_viewPos.x, _viewPos.y,
		_viewPos.x + MIN<int>(640, _panorama.w), _viewPos.y + MIN<int>(480, _panorama.h)));
	return false;
}

void Shell::redrawZone() {
	background(_def->background);
	drawSlots();
	if (_mag == kMagOpen)
		drawMagnifierOpen();
}

void Shell::leave(int code) {
	_code = code;
	_state = kLeave;
}

static void centreCursor(Common::Point &mouse) {
	mouse = Common::Point(320, 240);
	static_cast<PeintreEngine *>(g_engine)->warpMouse(320, 240);
}

static SlotRun *placeObject(Shell &s, uint zone, uint object) {
	switch (zone) {
	case 1: case 2: case 3:
		return placeA01(s, zone, object);
	case 4: case 7: case 12: case 13: case 16: case 18:
		return placeA03(s, zone, object);
	case 19: case 22: case 23:
		return placeA04(s, zone, object);
	case 0: case 20: case 24:
		return placeA14(s, zone, object);
	default:
		return placeA13(s, zone, object);
	}
}

void Shell::init() {
	_cursors.load("Curseurs");
	// The inventory list: the held objects in increasing order (0x4e2688).
	for (uint o = 0; o < kNumObjects; o++)
		if (_vm->state().held(o))
			_list.push_back(o);
	if (_zone == 21) {
		// The ending: no object, no bar (a14.md); Entry2D writes GGAME with flag 0.
		_vm->writeResume(0);
		_run = createEnding(*this);
		_state = kEnding;
		return;
	}
	for (uint s = 0; s < _def->count; s++) {
		const uint o = _def->objects[s];
		_capsE[s].load(Common::String::format("CapsE%02u", o));
		_capsOP[s].load(Common::String::format("CapsOP%02u", o));
		_capsAO[s].load(Common::String::format("CapsAO%02u", o));
		_capsAC[s].load(Common::String::format("CapsAC%02u", o));
	}
	_loupeOut.load("LoupeOut");
	_loupeIn.load("LoupeIn");
	_retour.load("Retour");
	_retourM.load("RetourM");
	_invent.load("Invent");
	_opi.load("OPI");
	_op.load("OP");
	_count.load("count");
	background(_def->background);
	_state = kInit;
}

int Shell::run(uint zone) {
	_zone = zone;
	_def = &kZones[zone];
	init();
	_mouse = _vm->mouse();
	_down = _vm->buttonDown();
	while (_state != kLeave) {
		if (_vm->shouldQuit()) {
			leave(-2);
			break;
		}
		_vm->pollInput();
		_wasDown = _down;
		_down = _vm->buttonDown();
		_mouse = _vm->mouse();
		_tick = (_tick + 1) & 3;
		_ticks++;

		if (_movieOn) {
			// Movie_Step: a click ends a skippable movie (E-0417).
			_movieOn = _vm->movies()->step(_page);
			if (_movieOn && _movieSkippable && _down) {
				_vm->movies()->close();
				_movieOn = false;
			}
		}
		// The slot animation (CapsOP every tick, CapsAO on even ticks, CapsAC on odd ticks).
		if (_capsBank && _capsFrame < (int)_capsBank->frameCount() &&
			(_capsRate == 0 || (_capsRate == 1) == odd()))
			_capsBank->draw(_page, _capsFrame++, 0, kSlotDrawY[_capsSlot]);

		step();
		if (_state == kLeave)
			break;

		// On even ticks the magnifier animation advances (1 -> 2, 0 -> 3 at its end).
		if (!odd() && (_mag == kMagOpening || _mag == kMagClosing)) {
			const SpriteBank &bank = _mag == kMagOpening ? _loupeOut : _loupeIn;
			if (_magFrame + 1 < (int)bank.frameCount()) {
				_magFrame++;
				drawMagnifier();
			} else {
				_mag = _mag == kMagOpening ? kMagOpen : kMagClosed;
			}
		}
		// Retour loops on odd ticks while the cursor is over it, idle with the bar closed (E-0440).
		if (_retourOn && odd() && _state == kIdle && barClosed() && kRetourRect.contains(_mouse)) {
			_retourFrame = (_retourFrame + 1) % MAX<uint>(1, _retour.frameCount());
			_retour.draw(_page, _retourFrame, 600, 435);
		}
		moveBar();
		compose();
		_vm->waitTick(40);
	}

	_vm->movies()->close();
	_vm->sound()->stopStream();
	_vm->sound()->stopAllStatic();
	delete _run;
	_run = nullptr;
	syncHeld();
	// The zone's sunflower count goes to the 3D side with the code (0x42f2c2, E-0414).
	_vm->zoneLeaveCount = counter(_def->counter);
	return _code;
}

void Shell::compose() {
	Graphics::Surface &dst = _vm->screen();
	dst.copyRectToSurface(_page, 0, 0, Common::Rect(640, 480));
	if (_viewing && !_viewPanorama)
		_viewAnim.draw(dst);
	if (_run)
		_run->draw(dst);
	_pa.draw(dst);
	if (!barClosed())
		drawBar(dst);
	if (_dragObject >= 0)
		_op.draw(dst, _dragObject, _dragOrigin.x + _mouse.x - _grab.x, _dragOrigin.y + _mouse.y - _grab.y);
	if (_flying) {
		// p = from (1 - t) + to t, t = (sin(3π/2 - π n / 32) + 1) / 2 (E-0412).
		const double t = (sin(1.5 * M_PI - M_PI * _flyN / 32.0) + 1.0) / 2.0;
		_op.draw(dst, _object, (int)(_flyFrom.x * (1 - t) + _flyTo.x * t), (int)(_flyFrom.y * (1 - t) + _flyTo.y * t));
	}
	if (_tournDrag)
		_tourn.draw(dst, 0, _tournPos.x + _mouse.x - _grab.x, _tournPos.y + _mouse.y - _grab.y);
	drawCursor(dst, _cursors, _cursor, _mouse);
	_vm->present();
}

void Shell::step() {
	// The shell's own states show the busy cursor (0x40e25a(0, 9) in MainWndProc).
	switch (_state) {
	case kIdle: case kRun: case kSunGrab: case kSunDrag: case kDragOwn: case kDragOther:
	case kEnding: case kViewARun: case kViewBRun: case kMenu:
		break;
	default:
		setCursor(kCursorBusy);
		break;
	}
	switch (_state) {
	case kInit:
		// The slots and the open magnifier; the first zone then opens the bar (E-0440).
		drawSlots();
		drawMagnifierOpen();
		if (_zone == 0) {
			closeMagnifier();
			_state = kBarOpen;
		} else {
			startRetour();
			_state = kIdle;
		}
		if (ConfMan.hasKey("dev_place")) {
			// Dev harness: the object is dropped on its slot at once (the bar would have
			// closed the magnifier).
			_mag = kMagClosed;
			for (uint i = 0; i < _list.size(); i++)
				if (_list[i] == (uint)ConfMan.getInt("dev_place") && slotOf(_list[i]) >= 0)
					place(i);
		}
		break;

	case kIdle:
		stepIdle();
		break;

	case kViewB:
	case kViewA:
		_retourOn = false;
		startView(Common::String(_def->background) + (_state == kViewA ? "a" : "b"), _state == kViewB);
		_state = _state == kViewA ? kViewARun : kViewBRun;
		break;

	case kViewBRun:
	case kViewARun:
		if (viewStep()) {
			redrawZone();
			startRetour();
			setCursor(kCursorDefault);
			_state = kIdle;
		}
		break;

	case kBarClose:
		if (barClosed()) {
			openMagnifier();
			startRetour();
			_state = kIdle;
		}
		break;

	case kBarOpen:
		if (_mag == kMagClosed) {
			openBar();
			sound("bar_obj");
			_state = kIdle;
		}
		break;

	case kDragOwn:
	case kDragOther:
		setCursor(kCursorDrag);
		if (_down)
			break;
		{
			const Common::Point drop(_dragOrigin.x + _mouse.x - _grab.x, _dragOrigin.y + _mouse.y - _grab.y);
			const int s = slotOf(_dragObject);
			if (_state == kDragOwn && s >= 0 && slotRect(s).contains(drop)) {
				place(_dragIndex);
			} else {
				sound("cf_clic3");
				_state = kIdle;
			}
			_dragObject = -1;
			_dragIndex = -1;
			setCursor(kCursorDefault);
		}
		break;

	case kPlacing:
		if (capsDone() && barClosed())
			_state = kPlace;
		break;

	case kReplay:
		if (capsDone() && barClosed() && _mag == kMagClosed)
			_state = kPlace;
		break;

	case kPlace:
		_runSlot = _slot;
		_run = placeObject(*this, _zone, _object);
		drawSlots();
		if (_run) {
			_state = kRun;
		} else {
			startCaps(_slot, &_capsAC[_slot], 1);
			sound("fermcaps");
			if (_mag == kMagClosed)
				openMagnifier();
			_state = kCapsClose;
		}
		break;

	case kRun:
		if (_vm->keyFired(Common::KEYCODE_BACKSPACE))
			_run->onAbort();
		if (_run->run(_result)) {
			delete _run;
			_run = nullptr;
			_state = kResult;
		}
		break;

	case kResult:
		_vm->movies()->close();
		_movieOn = false;
		// The end of the run redraws the zone with the magnifier closed (0x411807(-1, slot, 0)).
		if (_zone != 0) {
			background(_def->background);
			drawSlots();
			_mag = kMagClosed;
		}
		if (_result == 0 && _justPlaced) {
			// Back to the bar: the bar opens, then the object flies to its old place (E-0441).
			_vm->state().placed[_object] = 0;
			_runSlot = -1;
			_justPlaced = false;
			drawSlots();
			openBar();
			sound("bar_obj");
			_state = kFlyWait;
			break;
		}
		startCaps(_slot, &_capsAC[_slot], 1);
		sound("fermcaps");
		if (_zone != 0 && !_vm->state().zoneDone[_zone]) {
			bool all = true;
			for (uint s = 0; s < _def->count; s++)
				all = all && _vm->state().placed[_def->objects[s]];
			if (all) {
				// The sunflower; the magnifier reopens only after the autosave.
				_vm->state().zoneDone[_zone] = 1;
				_state = kSunStart;
				break;
			}
		}
		if (_mag == kMagClosed)
			openMagnifier();
		_state = kCapsClose;
		break;

	case kFlyWait:
		if (barMoving())
			break;
		// Back in the list at its old index, scrolled into view; cf_clic3; the flight starts.
		_list.insert_at(MIN<uint>(_placedIndex, _list.size()), _object);
		_placedIndex = MIN<uint>(_placedIndex, _list.size() - 1);
		if (_placedIndex > _first + 5)
			_first = _placedIndex - 5;
		sound("cf_clic3");
		_flyFrom = Common::Point(40, kFlyFromY[_slot]);
		_flyTo = Common::Point(101 + 70 * (MIN<int>(_placedIndex, _first + 5) - (int)_first), 450);
		_flyN = 0;
		_flying = true;
		_state = kFlyBack;
		break;

	case kFlyBack:
		if (++_flyN > 32) {
			_flying = false;
			closeBar();
			_state = kBackIdle;
		}
		break;

	case kBackIdle:
		if (barClosed()) {
			openMagnifier();
			startRetour();
			_state = kIdle;
		}
		break;

	case kCapsClose:
		// Zone 0 saves as soon as CapsAC ends; the others wait for the open magnifier (E-0441).
		if (capsDone() && (_zone == 0 || _mag == kMagOpen)) {
			_runSlot = -1;
			if (_justPlaced) {
				_state = kSave;
			} else {
				startRetour();
				_state = kIdle;
			}
		}
		break;

	case kSave:
		// Autosave GAME<player><object> (save.md), then zone 0 leaves.
		syncHeld();
		_vm->writeGame(_object);
		_justPlaced = false;
		_runSlot = -1;
		if (_zone == 0) {
			leave(-1);
		} else {
			startRetour();
			if (_mag == kMagClosed)
				openMagnifier();
			_state = kIdle;
		}
		break;

	case kMenuWait:
		if (barClosed())
			_state = kMenu;
		break;

	case kMenu: {
		syncHeld();
		const int code = _vm->runOptionMenu(false);
		if (code == -1)
			_state = kIdle;
		else
			leave(code);
		break;
	}

	case kLeaveWait:
		if (barClosed()) {
			centreCursor(_mouse);
			leave(-1);
		}
		break;

	case kEnding:
		if (_run->run(_result))
			leave(-1);
		break;

	default:
		stepSunflower();
		break;
	}
}

void Shell::stepIdle() {
	uint cursor = kCursorDefault;
	const bool retour = barClosed() && (kRetourRect.contains(_mouse) || kRetourMRect.contains(_mouse));
	if (overViewA() || overViewB() || retour)
		cursor = kCursorButton;
	else if (barHit() >= 0)
		cursor = kCursorTake;
	setCursor(cursor);

	if (_vm->keyFired(Common::KEYCODE_BACKSPACE) && !barMoving()) {
		if (barClosed()) {
			centreCursor(_mouse);
			leave(-1);
		} else {
			closeBar();
			_state = kLeaveWait;
		}
		return;
	}
	if (_vm->keyFired(Common::KEYCODE_ESCAPE)) {
		if (barClosed()) {
			_state = kMenu;
		} else {
			closeBar();
			_state = kMenuWait;
		}
		return;
	}
	if (_vm->keyFired(Common::KEYCODE_SPACE) && (_mag == kMagOpen || _mag == kMagClosed) && !barMoving()) {
		if (barClosed()) {
			closeMagnifier();
			_state = kBarOpen;
		} else {
			closeBar();
			_state = kBarClose;
		}
		return;
	}
	if (barMoving())
		return;

	// The bar's arrows scroll while held, every 8th tick (0x411514).
	_arrowHeld = -1;
	if (barOpen() && _down) {
		for (int a = 0; a < 2; a++) {
			if (!kArrowRects[a].contains(_mouse))
				continue;
			_arrowHeld = a;
			if ((_ticks & 7) == 0) {
				if (a == 0 && _first > 0) {
					_first--;
					sound("fleche");
				} else if (a == 1 && _list.size() - _first > 6) {
					_first++;
					sound("fleche");
				}
			}
		}
	}
	if (!clicked())
		return;

	const int i = barHit();
	if (i >= 0) {
		_dragIndex = _first + i;
		_dragObject = _list[_dragIndex];
		_dragOrigin = Common::Point(101 + 70 * i, barY() + 30);
		_grab = _mouse;
		setCursor(kCursorDrag);
		_state = slotOf(_dragObject) >= 0 ? kDragOwn : kDragOther;
	} else if (overViewB()) {
		sound("clic_1");
		_state = kViewB;
	} else if (overViewA()) {
		sound("clic_1");
		_state = kViewA;
	} else if (barClosed() && kRetourRect.contains(_mouse)) {
		centreCursor(_mouse);
		leave(-1);
	} else if (barClosed() && kRetourMRect.contains(_mouse)) {
		centreCursor(_mouse);
		leave(-3);
	} else {
		for (uint s = 0; s < _def->count; s++) {
			if (!_vm->state().placed[_def->objects[s]] || !slotRect(s).contains(_mouse))
				continue;
			// Replay a placed object's sequence.
			_object = _def->objects[s];
			_slot = s;
			_justPlaced = false;
			_retourOn = false;
			startCaps(s, &_capsAO[s], 2);
			sound("ouvrcaps");
			closeBar();
			closeMagnifier();
			_state = kReplay;
			break;
		}
	}
}

void Shell::stepSunflower() {
	// ui.md "The sunflower" (E-0413); Van Gogh's sprites move on odd ticks.
	const Common::String pa = Common::String("P") + _def->background;
	switch (_state) {
	case kSunStart:
		if (!capsDone())
			break;
		_pa.load(pa + "a");
		sound("pas_VG", true);
		_pa.play();
		_state = kSunWalk;
		break;

	case kSunWalk:
		if (!odd())
			break;
		_pa.step();
		if (_pa.frame == 12)
			stopSound("pas_VG");
		if (!_pa.playing) {
			_tourn.load(Common::String::format("TOURN%u", _def->tourn));
			_pot.load("POT");
			_state = kSunGrab;
		}
		break;

	case kSunGrab: {
		const bool over = kTournRects[_def->tourn].contains(_mouse);
		setCursor(over ? kCursorTake : kCursorDefault);
		if (over && clicked()) {
			_pa.load(pa + "b");
			_pa.show(0);
			openBar();
			sound("bar_obj");
			_tournDrag = true;
			_tournPos = kTournGrab[_def->tourn];
			_grab = _mouse;
			sound("tourneso");
			setCursor(kCursorDrag);
			_state = kSunDrag;
		}
		break;
	}

	case kSunDrag:
		if (_down)
			break;
		_tournDrag = false;
		// The dragged sprite's point is tested, not the cursor (0x410dfa, E-0442).
		if (kPotArea.contains(Common::Point(_tournPos.x + _mouse.x - _grab.x, _tournPos.y + _mouse.y - _grab.y))) {
			_pot.play();
			sound("vase");
			_state = kSunPot;
		} else {
			// Van Gogh holds the sunflower again: PA..a's last frame (E-0442).
			sound("cf_clic3");
			_pa.load(pa + "a");
			_pa.show(_pa.last());
			_state = kSunGrab;
		}
		setCursor(kCursorDefault);
		break;

	case kSunPot:
		if (odd()) // POT on odd ticks (E-0440)
			_pot.step();
		if (!_pot.playing) {
			counter(_def->counter)++;
			closeBar();
			_pa.load(pa + "c");
			_pa.play();
			_state = kSunLeave;
		}
		break;

	case kSunLeave:
		if (!odd())
			break;
		_pa.step();
		if (_pa.frame == 12)
			sound("pas_VG", true);
		if (!_pa.playing)
			_state = kSunFree;
		break;

	case kSunFree:
		stopSound("pas_VG");
		_pa.hide();
		_pot.hide();
		_state = kSave;
		break;

	default:
		warning("Shell: unknown state %d", _state);
		leave(-1);
		break;
	}
}

int PeintreEngine::enterZone(uint zone) {
	if (zone >= kNumZones) {
		// Entry2D accepts 25, which reads past the table (Q-0250).
		warning("enterZone: no zone %d", zone);
		return -1;
	}
	Shell shell(this);
	return shell.run(zone);
}

} // End of namespace Peintre
