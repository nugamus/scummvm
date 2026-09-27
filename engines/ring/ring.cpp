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
#include "common/events.h"
#include "common/file.h"
#include "common/fs.h"
#include "common/system.h"
#include "common/tokenizer.h"

#include "engines/util.h"

#include "graphics/fonts/winfont.h"

#include "ring/cursor.h"
#include "ring/detection.h"
#include "ring/movie.h"
#include "ring/resources.h"
#include "ring/ring.h"
#include "ring/sound.h"
#include "ring/world.h"
#include "ring/ring/zones.h"

namespace Ring {

RingEngine::RingEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst), _gameDescription(gameDesc) {
	const Common::FSNode gameDataDir(ConfMan.getPath("path"));
	SearchMan.addDirectory(gameDataDir, 0, 4);
}

RingEngine::~RingEngine() {
}

bool RingEngine::hasFeature(EngineFeature f) const {
	return f == kSupportsReturnToLauncher;
}

void RingEngine::present() {
	g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, _screen.w, _screen.h);
	g_system->updateScreen();
}

void RingEngine::pollEvents(uint32 ms) {
	Common::Event event;
	while (g_system->getEventManager()->pollEvent(event)) {
		if (event.type == Common::EVENT_KEYDOWN) {
			// WM_CHAR characters and Delete reach 0x40b060 (spec/events.md, "Keys").
			if (event.kbd.keycode == Common::KEYCODE_ESCAPE)
				_escapeDown = true;
			if (event.kbd.keycode == Common::KEYCODE_DELETE)
				_keys.push_back(0x2e);
			else if (event.kbd.ascii && event.kbd.ascii < 256)
				_keys.push_back(event.kbd.ascii);
		} else if (event.type == Common::EVENT_KEYUP && event.kbd.keycode == Common::KEYCODE_ESCAPE)
			_escapeDown = false;
		else if (event.type == Common::EVENT_MOUSEMOVE && !_scripted)
			_mouse = event.mouse;
		else if (event.type == Common::EVENT_LBUTTONDOWN || event.type == Common::EVENT_LBUTTONUP) {
			if (!_scripted)
				_mouse = event.mouse;
			_buttons.push_back(Button{ event.type == Common::EVENT_LBUTTONDOWN, event.mouse });
		}
	}
	if (ms)
		g_system->delayMillis(ms);
}

void RingEngine::wait(uint32 ms) {
	uint32 start = g_system->getMillis();
	while (g_system->getMillis() - start < ms && !_escapeDown && !shouldQuit())
		pollEvents(5);
}

// A 24-bit BMP's pixel bytes as stored (rows bottom-up, padded); DisFad works on them.
static bool readBmp24(const Common::Path &path, int &w, int &h, Common::Array<byte> &bytes) {
	Common::File f;
	if (!f.open(path) || f.readUint16BE() != MKTAG16('B', 'M'))
		return false;
	f.skip(8);
	uint32 offset = f.readUint32LE();
	f.readUint32LE();
	w = f.readSint32LE();
	h = f.readSint32LE();
	f.readUint16LE();
	if (f.readUint16LE() != 24 || w <= 0 || h <= 0)
		return false;
	uint32 stride = (w * 3 + 3) & ~3;
	bytes.resize(stride * h);
	f.seek(offset);
	return f.read(bytes.data(), bytes.size()) == bytes.size();
}

bool RingEngine::fade(const Common::String &from, const Common::String &to, uint frames, uint32 holdMs) {
	// DisFad with load-from 'e' and kind 2: the pictures come from DATA\SY\IMAGE (spec/resources.md).
	Common::Path dir = Common::Path("DATA").appendComponent("SY").appendComponent("IMAGE");
	int w1, h1, w2, h2;
	Common::Array<byte> a, b;
	if (!readBmp24(dir.appendComponent(from), w1, h1, a) || !readBmp24(dir.appendComponent(to), w2, h2, b) ||
		w1 != w2 || h1 != h2 || !frames) {
		warning("Ring: DisFad %s -> %s failed", from.c_str(), to.c_str());
		return false;
	}
	uint32 stride = (w1 * 3 + 3) & ~3;
	Common::Array<int16> step(a.size());
	for (uint32 i = 0; i < a.size(); i++)
		step[i] = (int16)(((int)a[i] - (int)b[i]) / (int)frames);

	auto show = [&](const Common::Array<byte> &pix) {
		for (int y = 0; y < h1 && 16 + y < _screen.h; y++) {
			const byte *src = &pix[(h1 - 1 - y) * stride];
			uint16 *dst = (uint16 *)_screen.getBasePtr(0, 16 + y);
			for (int x = 0; x < w1 && x < _screen.w; x++)
				dst[x] = _screen.format.RGBToColor(src[x * 3 + 2], src[x * 3 + 1], src[x * 3]);
		}
		present();
	};

	// The animation clock runs at 25 frames per second; each new frame subtracts one step.
	uint32 start = g_system->getMillis();
	for (uint f = 1; f <= frames && !_escapeDown && !shouldQuit(); f++) {
		for (uint32 i = 0; i < a.size(); i++)
			a[i] = (byte)(a[i] - step[i]);
		show(a);
		while (g_system->getMillis() - start < f * 40 && !shouldQuit())
			pollEvents(2);
	}
	show(b);
	wait(holdMs);
	return true;
}

void RingEngine::showStartupScreens() {
	// 0x4314a0 (spec/boot.md, "Start-up screens")
	while (_escapeDown && !shouldQuit())
		pollEvents(10);
	playMovie(this, Common::Path("DATA").appendComponent("SY").appendComponent("PLA").appendComponent("LOGO.CNM"), 0);
	static const struct {
		const char *from, *to;
		uint32 hold;
	} steps[] = {
		{ "beg0.bmp", "beg1.bmp", 3000 }, { "beg1.bmp", "beg0.bmp", 0 },
		{ "beg0.bmp", "beg2.bmp", 3000 }, { "beg2.bmp", "beg0.bmp", 0 },
		{ "beg0.bmp", "beg3.bmp", 3000 }, { "beg3.bmp", "beg0.bmp", 0 },
		{ "beg0.bmp", "beg4.bmp", 3000 }, { "beg4.bmp", "beg0.bmp", 0 },
		{ "beg0.bmp", "beg5.bmp", 3000 }, { "beg5.bmp", "beg0.bmp", 0 },
		{ "beg0.bmp", "beg6.bmp", 6000 },
	};
	for (const auto &s : steps) {
		pollEvents();
		if (_escapeDown || shouldQuit())
			break;
		fade(s.from, s.to, 20, s.hold);
	}
}

Common::Error RingEngine::run() {
	Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
	initGraphics(640, 480, &format);
	_screen.create(640, 480, format);

	switch (_gameDescription->language) {
	case Common::FR_FRA: _languageFolder = "FRA"; break;
	case Common::DE_DEU: _languageFolder = "GER"; break;
	case Common::NL_NLD: _languageFolder = "HOL"; break;
	case Common::IT_ITA: _languageFolder = "ITA"; break;
	case Common::ES_ESP: _languageFolder = "SPA"; break;
	case Common::SV_SWE: _languageFolder = "SWE"; break;
	default: _languageFolder = "ENG"; break;
	}
	_resources.reset(new Resources(format));
	_cursors.reset(new Cursors());
	_resources->setLanguageFolder(_languageFolder);

	// The window shows nothing for 3 s (timer 100), the set-up runs, then 2 s more (timer 101).
	present();
	wait(3000);
	_resources->openArchive(kZoneSY);
	addCursors();
	_sounds.reset(new Sounds(this));
	loadPreferences();
	_world.reset(new World());
	_world->setUp(_sounds.get(), _preferences[2]);
	_sounds->setTypeVolumes(_preferences[0], _preferences[1]);
	// Font 1: "ARX Pilgrim L" asked with a 12-pixel cell; the closest in arxrin.fon is 8 points (Q-0010).
	_font.reset(new Graphics::WinFont());
	if (_font->loadFromFON("arxrin.fon", Graphics::WinFontDirEntry("ARX Pilgrim L", 8)))
		_world->setFont(_font.get());
	else
		warning("Ring: cannot load font 1 from arxrin.fon");
	wait(2000);
	_escapeDown = false;

	// Development: dev_skip_startup=true in the game domain goes straight to the menu.
	if (!ConfMan.getBool("dev_skip_startup"))
		showStartupScreens();

	startMenu(false);
	_buttons.clear();
	// Development: dev_input="ms:move x y;ms:click x y;ms:key code 0;..." replays input at ms after the menu opens.
	Common::StringArray script;
	for (const Common::String &step : Common::StringTokenizer(ConfMan.get("dev_input"), ";").split())
		script.push_back(step);
	_scripted = !script.empty(); // the real mouse's moves are ignored while scripted
	uint32 menuStart = g_system->getMillis();
	while (!shouldQuit()) {
		pollEvents();
		while (!script.empty()) {
			uint ms, x, y;
			char what[8];
			if (sscanf(script[0].c_str(), "%u:%7s %u %u", &ms, what, &x, &y) != 4) {
				script.remove_at(0);
				continue;
			}
			if (g_system->getMillis() - menuStart < ms)
				break;
			if (!strcmp(what, "key")) {
				key(x);
			} else {
				// "down" and "up" press and release the left button; "click" does both.
				_mouse = Common::Point(x, y);
				if (!strcmp(what, "down") || !strcmp(what, "click"))
					_buttons.push_back(Button{ true, _mouse });
				if (!strcmp(what, "up") || !strcmp(what, "click"))
					_buttons.push_back(Button{ false, _mouse });
			}
			script.remove_at(0);
		}
		while (!_keys.empty()) {
			int k = _keys.remove_at(0);
			key(k);
		}
		while (!_buttons.empty()) {
			// The button state follows every event; handlers only see y < 465 (spec/boot.md, "Input").
			Button b = _buttons.remove_at(0);
			_buttonDown = b.down;
			if (b.pos.y >= 465)
				continue;
			if (b.down)
				buttonDown(b.pos.x, b.pos.y);
			else
				click(b.pos.x, b.pos.y);
		}
		frame();
		g_system->delayMillis(10);
	}
	return Common::kNoError;
}

void RingEngine::key(int code) {
	// ponytail: Escape's end of a playing dialogue, the visual object lists and SY's key
	// handler (the save name) come with dialogues and the save screens (spec/events.md).
	Puzzle *p1 = _world->puzzle(1);
	Puzzle *p = p1 && p1->mode == 2 ? p1 : _world->puzzle(_puzzle);
	if (!p)
		return;
	for (const auto &acc : p->accessibilities) {
		const HotSpot &h = acc->hotSpot;
		if (h.enabled && h.key == code) {
			click((h.rect.left + h.rect.right) / 2, (h.rect.top + h.rect.bottom) / 2);
			return;
		}
	}
}

void RingEngine::loadPreferences() {
	Common::String line = ConfMan.get("preferences");
	Common::File f;
	if (line.empty() && f.open("aPre.ini"))
		line = f.readLine();
	int v[4];
	if (sscanf(line.c_str(), "%d %d %d %d", &v[0], &v[1], &v[2], &v[3]) == 4)
		memcpy(_preferences, v, sizeof(v));
	else
		warning("Ring: cannot read the preferences (aPre.ini)");
}

void RingEngine::savePreferences(int volume, int dialogue, int stereo, int subtitles) {
	_preferences[0] = volume;
	_preferences[1] = dialogue;
	_preferences[2] = stereo;
	_preferences[3] = subtitles;
	ConfMan.set("preferences", Common::String::format("%d %d %d %d", volume, dialogue, stereo, subtitles));
	ConfMan.flushToDisk();
	_sounds->setTypeVolumes(volume, dialogue); // 0x4289e0; the stereo only matters for 3D pans set later
}

void RingEngine::message(const char *key) {
	_messageTitle.clear();
	_messageText.clear();
	Common::File f;
	if (!f.open("aMes.ini")) {
		warning("Ring: cannot open aMes.ini");
		return;
	}
	// The key is a whitespace-separated word; up to ten language lines follow it.
	bool found = false;
	while (!found && !f.eos() && !f.err()) {
		Common::String line = f.readLine();
		for (const Common::String &word : Common::StringTokenizer(line, " 	").split())
			if (word == key)
				found = true;
	}
	for (int i = 0; found && i < 10 && !f.eos(); i++) {
		Common::String line = f.readLine();
		if (line.size() < 3)
			break;
		if (!line.hasPrefix(_languageFolder.substr(0, 3)))
			continue;
		size_t last = line.findLastOf('#');
		if (last == Common::String::npos || last == 0)
			break;
		size_t first = line.substr(0, last).findLastOf('#');
		if (first == Common::String::npos)
			break;
		_messageTitle = line.substr(first + 1, last - first - 1);
		_messageText = line.substr(last + 1);
		return;
	}
	warning("Ring: no message %s for %s in aMes.ini", key, _languageFolder.c_str());
}

void RingEngine::addCursors() {
	// The game set-up's cursors, 0x430ed0 (spec/boot.md); kind 1 (id 0x36) is a Windows cursor.
	static const struct {
		int id;
		const char *name;
		int kind, frames, offX, offY;
	} cursors[] = {
		{ 0x33, "CUR_busy", 3, 0, 0, 0 }, { 10000, "ni_handsel", 3, 0, 15, 15 },
		{ 0x32, "cur_idle", 4, 15, 10, 6 }, { 0x35, "cur_muv", 4, 20, 10, 6 },
		{ 0x34, "CUR_Hotspot", 4, 19, 10, 6 }, { 0x37, "cur_back", 3, 0, 10, 20 },
		{ 0x38, "CUR_MenuIdle", 3, 0, 0, 0 }, { 0x39, "CUR_MenuActive", 3, 0, 0, 0 },
	};
	for (const auto &c : cursors) {
		_cursors->add(c.id, c.name, c.kind, c.frames, 12.5f);
		_cursors->setOffset(c.id, c.offX, c.offY);
	}
}

void RingEngine::leavePlace() {
	// 0x40b650: effects and dialogues stop when the player leaves a place.
	_puzzle = _rotation = 0;
	_sounds->stopType(kSoundEffect, kSoundLeft);
	_sounds->stopType(kSoundDialogue, kSoundLeft);
}

void RingEngine::puzSetAct(int puzzle, bool start, bool stop) {
	Puzzle *p = _world->puzzle(puzzle);
	if (!p)
		return;
	leavePlace();
	_puzzle = puzzle;
	_mode = 2;
	_sounds->enterPlace(&p->sounds, start, stop);
}

void RingEngine::rotSetAct(int rotation, bool start, bool stop) {
	Rotation *r = _world->rotation(rotation);
	if (!r)
		return;
	if (!r->panorama) {
		Common::File f;
		Common::Path path = Common::Path("DATA").appendComponent(zoneFolder(r->zone)).appendComponent("NODE").appendComponent(r->name + ".aqc");
		r->panorama.reset(new Panorama());
		if (!f.open(path) || !r->panorama->load(f)) {
			warning("Ring: cannot load the node %s", path.toString().c_str());
			r->panorama.reset();
			return;
		}
	}
	leavePlace();
	_rotation = rotation;
	_mode = 1;
	_mouse = Common::Point(320, 240);
	g_system->warpMouse(320, 240);
	_panTime = g_system->getMillis();
	_sounds->enterPlace(&r->sounds, start, stop, true, r->alpha + 135.0f, _preferences[2]);
}

void RingEngine::setZone(int zone, int entry) {
	// ponytail: the CD check, the zone's archive (ART_x) and the saved-game entry (1000) come with their specs
	_zone = zone;
	_menuZone = 0;
	if (zone == kZoneAS)
		AS::enter(this, entry);
	else
		warning("Ring: zone %d is not implemented yet", zone);
}

bool RingEngine::puzSetMod(int puzzle, int mode, int object) {
	Puzzle *p = _world->puzzle(puzzle);
	if (!p || (p->mode == 2 && mode == 2))
		return false;
	p->mode = mode;
	p->modeObject = object;
	return true;
}

void RingEngine::startMenu(bool fromGame) {
	if (_menuZone)
		return;
	// ponytail: from the game, the snapshot save and the thumbnail come with spec/save.md
	_sounds->stopAll(4); // 0x406ea0(4)
	_menuZone = _zone;
	_zone = kZoneSY;
	puzSetAct(90000);
	puzSetMod(1, 1, 0);
	for (int o = 1; o < 8; o++) {
		_world->setAccessibilities(o, false);
		_world->hideAndFree(o);
	}
	_world->setAccessibilities(90004, fromGame); // "continue"
}

void RingEngine::requestClose() {
	puzSetMod(1, 2, 2);
	_world->showPresentation(2, 0, true);
	_world->setAccessibilities(2, true);
}

void RingEngine::frame() {
	drawView();
	if (_buttonDown)
		dragMove(_mouse.x, _mouse.y);
	track(_mouse.x, _mouse.y);
	_sounds->dialogueFrame(_screen, _font.get(), _preferences[3] != 0);
	_cursors->draw(*_resources, _screen, _mouse.x, _mouse.y, g_system->getMillis());
	present();
	_sounds->checkEnds();
}

void RingEngine::drawView() {
	_screen.fillRect(Common::Rect(0, 0, 640, 16), 0);
	_screen.fillRect(Common::Rect(0, 464, 640, 480), 0);
	Rotation *r = _mode == 1 ? _world->rotation(_rotation) : nullptr;
	if (r && !r->paused && r->panorama) {
		// Looking around, per frame in the original (0x4107f0); here per 1/60 s (Q-0011).
		uint32 now = g_system->getMillis();
		for (int steps = 0; now - _panTime >= 17 && steps < 10; steps++, _panTime += 17) {
			float dx = _mouse.x / 640.0f - 0.5f, dy = _mouse.y / 480.0f - 0.5f;
			if (ABS(dx) > 0.25f)
				r->alpha += dx * (ABS(dx) - 0.25f) * 48.0f;
			if (ABS(dy) > 0.25f)
				r->beta += dy * (ABS(dy) - 0.25f) * 48.0f;
			_view.update(*r, *r->panorama);
		}
		if (now - _panTime >= 17)
			_panTime = now;
		_view.update(*r, *r->panorama);
		_view.draw(*r->panorama, _screen, 16);
	} else if (Puzzle *p = _world->puzzle(_puzzle)) {
		_world->draw(*p, *_resources, _screen);
	}
	if (Puzzle *p1 = _world->puzzle(1))
		_world->draw(*p1, *_resources, _screen);
}

void RingEngine::turn(Rotation &r, float alpha, float beta, float ran) {
	float a = alpha - 135.0f;
	if (a < 0.0f)
		a += 360.0f;
	float a0 = r.alpha, b0 = r.beta, r0 = r.ran;
	int da = ABS((int)(a - a0)), db = ABS((int)(beta - b0)), dr = ABS((int)(ran - r0));
	if (da > 180)
		da = 360 - da;
	int steps = (int)(MAX(da, MAX(db, dr)) * 0.8f);
	if (a - a0 > 180.0f)
		a -= 360.0f;
	else if (a - a0 < -180.0f)
		a += 360.0f;
	for (int i = 0; i < steps && !shouldQuit(); i++) {
		float t = steps == 1 ? 0.0f : (float)i / (steps - 1);
		r.alpha = a * t + a0 * (1 - t);
		if (r.alpha > 360.0f)
			r.alpha -= 360.0f;
		r.beta = beta * t + b0 * (1 - t);
		r.ran = ran * t + r0 * (1 - t);
		drawView();
		present();
		pollEvents(16); // one step per frame (Q-0011)
	}
}

void RingEngine::move(const Movability &m) {
	// ponytail: the zone's movability events (0x40c2b0, 0x40c420) come with the zone handlers;
	// Ctrl-clicks (no turn, no ride) with the key spec's modifiers
	Rotation *from = _mode == 1 ? _world->rotation(_rotation) : nullptr;
	if (from && m.turn == 0)
		turn(*from, m.alpha1, m.beta1, m.ran1);
	else if (from && m.turn == 1)
		from->setAngles(m.alpha1, m.beta1, m.ran1);
	Rotation *to = m.kind == 0 || m.kind == 2 ? _world->rotation(m.target) : nullptr;
	Puzzle *toPuzzle = to ? nullptr : _world->puzzle(m.target);
	// The target's sounds: 3D pans for the arrival angle, then the transition (spec/sound.md).
	if (to) {
		to->setAlpha(m.alpha2);
		for (auto &i : to->sounds)
			i->pan = i->pan3D(to->alpha + 135.0f, _preferences[2]); // ponytail: 0x41ee10 checks the sound's type 3
		_sounds->prepareTransition(&to->sounds);
	} else if (toPuzzle) {
		_sounds->prepareTransition(&toPuzzle->sounds);
	}
	int zone = from ? from->zone : _zone;
	if (m.ride.empty() || !playMovie(this, Common::Path("DATA").appendComponent(zoneFolder(zone)).appendComponent("PLA").appendComponent(m.ride + ".cnm"), 0))
		_sounds->finishTransition();
	if (to) {
		rotSetAct(m.target);
		to->setAngles(m.alpha2, m.beta2, m.ran2);
	} else {
		puzSetAct(m.target);
	}
}

// The zone's handlers; puzzle 1's events always go to SY (spec/events.md).
static void onAccessibility(RingEngine *vm, int zone, int object, int value) {
	if (zone == kZoneSY)
		SY::onAccessibility(vm, object, value);
}

static void onNothing(RingEngine *vm, int zone) {
	if (zone == kZoneSY)
		SY::onNothing(vm);
}

static void onClick(RingEngine *vm, int zone, int object, int value) {
	if (zone == kZoneSY)
		SY::onClick(vm, object, value);
}

void RingEngine::soundEvent(int id, int type, int reason) {
	// (id, type, reason without bit 0x1000, reason & 0x1000) to the zone's handler (0x40ced0).
	int why = reason & ~0x1000, ended = reason & 0x1000;
	if (_zone == kZoneSY)
		SY::onSound(this, id, type, why, ended);
	else if (_zone == kZoneAS)
		AS::onSound(this, id, type, why, ended);
}

void RingEngine::track(int x, int y) {
	const HotSpot *h = trackHit(x, y);
	if (_drag.active)
		_cursors->set(h == _drag.hotSpot ? 4 : 3);
}

const HotSpot *RingEngine::trackHit(int x, int y) {
	// ponytail: the inventory joins with its spec
	if (Puzzle *p1 = _world->puzzle(1)) {
		if (const Accessibility *acc = _world->hit(*p1, x, y)) {
			_cursors->set(acc->hotSpot.cursor);
			onAccessibility(this, kZoneSY, acc->object, acc->hotSpot.value);
			return &acc->hotSpot;
		}
		if (p1->mode == 2) {
			_cursors->set(0x32);
			SY::onNothing(this);
			return nullptr;
		}
	}
	Rotation *r = _mode == 1 ? _world->rotation(_rotation) : nullptr;
	if (r && !r->paused && r->panorama) {
		Common::Point pt = _view.toPanorama(*r->panorama, x, y);
		if (const Accessibility *acc = World::hit(r->accessibilities, pt.x, pt.y)) {
			_cursors->set(acc->hotSpot.cursor);
			onAccessibility(this, _zone, acc->object, acc->hotSpot.value);
			return &acc->hotSpot;
		}
		if (const Movability *m = World::hit(r->movabilities, pt.x, pt.y)) {
			_cursors->set(m->hotSpot.cursor); // ponytail: the "on a movability" event comes with the zone handlers
			return &m->hotSpot;
		}
	} else if (Puzzle *p = _world->puzzle(_puzzle)) {
		if (const Accessibility *acc = _world->hit(*p, x, y)) {
			_cursors->set(acc->hotSpot.cursor);
			onAccessibility(this, _zone, acc->object, acc->hotSpot.value);
			return &acc->hotSpot;
		}
		if (const Movability *m = World::hit(p->movabilities, x, y)) {
			_cursors->set(m->hotSpot.cursor);
			return &m->hotSpot;
		}
	}
	_cursors->set(0x32);
	onNothing(this, _zone);
	return nullptr;
}

void RingEngine::buttonDown(int x, int y) {
	// ponytail: not while the inventory is shown (its spec); the button-down event (bit 1)
	// has no handler in the zones done so far
	Puzzle *p1 = _world->puzzle(1);
	Puzzle *p = _world->puzzle(_puzzle);
	Rotation *r = _mode == 1 ? _world->rotation(_rotation) : nullptr;
	const Accessibility *acc = nullptr;
	int id = 0;
	bool onPuzzle = true;
	Common::Point at(x, y);
	if (p1 && (acc = _world->hit(*p1, x, y)))
		id = 1;
	else if (p1 && p1->mode == 2)
		return;
	else if (p && _mode == 2 && (acc = _world->hit(*p, x, y)))
		id = p->id;
	else if (r && !r->paused && r->panorama) {
		at = _view.toPanorama(*r->panorama, x, y);
		if ((acc = World::hit(r->accessibilities, at.x, at.y))) {
			id = r->id;
			onPuzzle = false;
		}
	}
	Object *o = acc ? _world->object(acc->object) : nullptr;
	if (o && (o->flags & 4)) {
		_drag = Drag();
		_drag.active = true;
		_drag.object = o->id;
		_drag.value = acc->hotSpot.value;
		_drag.puzzle = id;
		_drag.onPuzzle = onPuzzle;
		_drag.press = _drag.current = at;
		_drag.hotSpot = &acc->hotSpot;
		// Cursors 3 and 4: the object's passive and active drag cursors (0x40b9b0).
		for (int i = 0; i < 2; i++) {
			const DragCursor &d = o->dragCursors[i];
			_cursors->remove(3 + i);
			if (d.kind == 3)
				_cursors->add(3 + i, (o->icon.empty() ? "dummy" : o->icon) + (i ? "_da" : "_dp"), 3);
			else if (d.kind == 4)
				_cursors->add(3 + i, o->icon, 4, d.frames, d.fps);
			_cursors->setOffset(3 + i, d.offsetX, d.offsetY);
		}
		dragEvent(1);
	}
	track(x, y);
}

void RingEngine::dragMove(int x, int y) {
	// The screen position, also against a rotation's hot spot, as 0x409520 does.
	if (!_drag.active || !(_drag.mode == 2 ? _drag.limit : _drag.hotSpot->rect).contains(x, y))
		return;
	_drag.current = Common::Point(x, y);
	dragEvent(3);
}

void RingEngine::dragEvent(int phase) {
	if ((_drag.puzzle == 1 && _drag.onPuzzle) || _zone == kZoneSY)
		SY::onDrag(this, _drag.object, phase);
}

void RingEngine::click(int x, int y) {
	if (_drag.active) {
		int mode = _drag.mode;
		dragEvent(2);
		_drag = Drag();
		if (mode == 2)
			return;
	}
	Puzzle *p1 = _world->puzzle(1);
	Puzzle *p = _mode == 2 ? _world->puzzle(_puzzle) : nullptr;
	if (p1 && p1->mode != 2) {
		if (const Movability *m = World::hit(p1->movabilities, x, y)) {
			Movability copy = *m;
			move(copy);
			return;
		}
	}
	for (Puzzle *q : { p1, p }) {
		if (!q)
			continue;
		if (const Accessibility *acc = _world->hit(*q, x, y)) {
			Object *o = _world->object(acc->object);
			if (o && (o->flags & 1))
				onClick(this, q == p1 ? kZoneSY : _zone, acc->object, acc->hotSpot.value);
			track(x, y);
			return;
		}
		if (q == p1 && p1->mode == 2)
			return;
		if (q == p) {
			if (const Movability *m = World::hit(p->movabilities, x, y)) {
				Movability copy = *m;
				move(copy);
				return;
			}
		}
	}
	Rotation *r = _mode == 1 ? _world->rotation(_rotation) : nullptr;
	if (!r || r->paused || !r->panorama)
		return;
	Common::Point pt = _view.toPanorama(*r->panorama, x, y);
	if (const Accessibility *acc = World::hit(r->accessibilities, pt.x, pt.y)) {
		Object *o = _world->object(acc->object);
		if (o && (o->flags & 1))
			onClick(this, _zone, acc->object, acc->hotSpot.value);
		track(x, y);
		return;
	}
	if (const Movability *m = World::hit(r->movabilities, pt.x, pt.y)) {
		Movability copy = *m;
		move(copy);
	}
}

} // End of namespace Ring
