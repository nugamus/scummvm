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

#include "backends/keymapper/keymap.h"
#include "backends/keymapper/keymapper.h"

#include "common/config-manager.h"
#include "common/events.h"
#include "common/file.h"
#include "common/fs.h"
#include "common/system.h"
#include "common/tokenizer.h"

#include "engines/util.h"

#include "graphics/fonts/winfont.h"
#include "image/png.h"

#include "ring/bag.h"
#include "ring/cursor.h"
#include "ring/detection.h"
#include "ring/movie.h"
#include "ring/resources.h"
#include "ring/ring.h"
#include "ring/sound.h"
#include "ring/world.h"
#include "ring/ring/zones.h"

namespace Ring {

// F12 (VK_F12 0x7b, a WM_KEYDOWN, not a character): StartMenu(1) (spec/boot.md, "Input").
static const int kKeyF12 = 0x100 | 0x7b;

// The zone's handlers; puzzle 1's events always go to SY (spec/events.md).
static void onAccessibility(RingEngine *vm, int zone, int object, int value) {
	if (zone == kZoneSY)
		SY::onAccessibility(vm, object, value);
	else if (zone == kZoneNI)
		NI::onAccessibility(vm, object, value);
	else if (zone == kZoneN2)
		N2::onAccessibility(vm, object, value);
}

static void onNothing(RingEngine *vm, int zone) {
	if (zone == kZoneSY)
		SY::onNothing(vm);
}

static void onClick(RingEngine *vm, int zone, int object, int value, int place) {
	if (zone == kZoneSY)
		SY::onClick(vm, object, value);
	else if (zone == kZoneAS)
		AS::onClick(vm, object, value);
	else if (zone == kZoneNI)
		NI::onClick(vm, object, value, place);
	else if (zone == kZoneRH)
		RH::onClick(vm, object, value);
	else if (zone == kZoneN2)
		N2::onClick(vm, object, value);
	else if (zone == kZoneFO)
		FO::onClick(vm, object, value);
	else if (zone == kZoneRO)
		RO::onClick(vm, object, value);
	else if (zone == kZoneWA)
		WA::onClick(vm, object, value);
}

static void onBagClick(RingEngine *vm, int zone, int object) {
	if (zone == kZoneFO)
		FO::onBagClick(vm, object);
}

static void onButtonDown(RingEngine *vm, int zone, int object, int value) {
	if (zone == kZoneNI)
		NI::onButtonDown(vm, object, value);
	else if (zone == kZoneN2)
		N2::onButtonDown(vm, object, value);
	else if (zone == kZoneRO)
		RO::onButtonDown(vm, object, value);
}

static void onTimer(RingEngine *vm, int zone, int id) {
	if (zone == kZoneAS)
		AS::onTimer(vm, id);
	else if (zone == kZoneNI)
		NI::onTimer(vm, id);
	else if (zone == kZoneRH)
		RH::onTimer(vm, id);
	else if (zone == kZoneN2)
		N2::onTimer(vm, id);
	else if (zone == kZoneFO)
		FO::onTimer(vm, id);
	else if (zone == kZoneRO)
		RO::onTimer(vm, id);
}

static void onAnimation(RingEngine *vm, int zone, int id, int frame) {
	if (zone == kZoneAS)
		AS::onAnimation(vm, id, frame);
	else if (zone == kZoneNI)
		NI::onAnimation(vm, id, frame);
	else if (zone == kZoneRH)
		RH::onAnimation(vm, id, frame);
	else if (zone == kZoneN2)
		N2::onAnimation(vm, id, frame);
	else if (zone == kZoneFO)
		FO::onAnimation(vm, id, frame);
	else if (zone == kZoneRO)
		RO::onAnimation(vm, id, frame);
	else if (zone == kZoneWA)
		WA::onAnimation(vm, id, frame);
}

static void onBeforeMove(RingEngine *vm, int zone, int from, int to, int index, int value, int kind) {
	if (zone == kZoneAS)
		AS::onBeforeMove(vm, from, to, kind);
	else if (zone == kZoneNI)
		NI::onBeforeMove(vm, from, to, value, kind);
	else if (zone == kZoneRH)
		RH::onBeforeMove(vm, from);
	else if (zone == kZoneN2)
		N2::onBeforeMove(vm, from, to, kind);
	else if (zone == kZoneFO)
		FO::onBeforeMove(vm, from, to, kind);
	else if (zone == kZoneRO)
		RO::onBeforeMove(vm, from, to, kind);
	else if (zone == kZoneWA)
		WA::onBeforeMove(vm, from, to, kind);
}

static void onAfterMove(RingEngine *vm, int zone, int to, int from, int index, int value, int kind) {
	if (zone == kZoneAS)
		AS::onAfterMove(vm, to, from, kind);
	else if (zone == kZoneNI)
		NI::onAfterMove(vm, to, from, value, kind);
	else if (zone == kZoneRH)
		RH::onAfterMove(vm, to, kind);
	else if (zone == kZoneN2)
		N2::onAfterMove(vm, to, value, kind);
	else if (zone == kZoneFO)
		FO::onAfterMove(vm, to, from, kind);
	else if (zone == kZoneRO)
		RO::onAfterMove(vm, to, from, kind);
	else if (zone == kZoneWA)
		WA::onAfterMove(vm, to, from, kind);
}

RingEngine *g_engine = nullptr;

RingEngine::RingEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst), _gameDescription(gameDesc) {
	g_engine = this;
	const Common::FSNode gameDataDir(ConfMan.getPath("path"));
	SearchMan.addDirectory(gameDataDir, 0, 5); // DATA/<zone>/DIA/<language>/<file>
}

RingEngine::~RingEngine() {
	g_engine = nullptr;
}

bool RingEngine::hasFeature(EngineFeature f) const {
	return f == kSupportsReturnToLauncher || f == kSupportsLoadingDuringRuntime || f == kSupportsSavingDuringRuntime;
}

void RingEngine::present() {
	g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, _screen.w, _screen.h);
	drawHotspots();
	g_system->updateScreen();
}

void RingEngine::pollEvents(uint32 ms) {
	Common::Event event;
	while (g_system->getEventManager()->pollEvent(event)) {
		if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START) {
			// Escape and F12 come through the keymapper (metaengine.cpp).
			if (event.customType == kActionSkip) {
				_escapeDown = true;
				_keys.push_back(27);
			} else if (event.customType == kActionMenu) {
				_keys.push_back(kKeyF12);
			} else if (event.customType == kActionHotspots) {
				toggleHotspots();
			}
		} else if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_END && event.customType == kActionSkip) {
			_escapeDown = false;
		} else if (event.type == Common::EVENT_KEYDOWN) {
			// WM_CHAR characters and Delete reach 0x40b060 (spec/events.md, "Keys").
			if (event.kbd.keycode == Common::KEYCODE_DELETE)
				_keys.push_back(0x2e);
			else if (event.kbd.ascii && event.kbd.ascii < 256)
				_keys.push_back(event.kbd.ascii);
		} else if (event.type == Common::EVENT_MOUSEMOVE && !_scripted)
			_mouse = event.mouse;
		else if ((event.type == Common::EVENT_LBUTTONDOWN || event.type == Common::EVENT_LBUTTONUP) && !_scripted) {
			// While dev_input drives the mouse the real buttons are ignored.
			_mouse = event.mouse;
			_buttons.push_back(Button{ event.type == Common::EVENT_LBUTTONDOWN, event.mouse, false });
		} else if (event.type == Common::EVENT_RBUTTONUP && !_scripted) {
			_buttons.push_back(Button{ false, event.mouse, true });
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
	_world->loadNames(_languageFolder);
	_bag.reset(new Bag(*_world, *_resources));
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
	// A game chosen in the launcher.
	if (ConfMan.hasKey("save_slot"))
		loadGameState(ConfMan.getInt("save_slot"));
	// Development: dev_bag=<id>,<id>... puts objects in the bag.
	for (const Common::String &id : Common::StringTokenizer(ConfMan.get("dev_bag"), ",").split())
		_bag->add(atoi(id.c_str()));
	// Development: dev_place=<id> starts on that rotation (alpha 90, ran 85.3), dev_place=p<id> on
	// that puzzle, in its zone (the ids of puzzles and rotations overlap).
	Common::String placeId = ConfMan.get("dev_place");
	bool isPuzzle = placeId.hasPrefix("p");
	int place = atoi(placeId.c_str() + (isPuzzle ? 1 : 0));
	if (Rotation *r = isPuzzle ? nullptr : _world->rotation(place)) {
		_zone = r->zone;
		_menuZone = 0;
		r->setAlpha(90.0f);
		r->ran = 85.3f;
		rotSetAct(r->id);
	} else if (Puzzle *p = isPuzzle ? _world->puzzle(place) : nullptr) {
		_zone = p->zone;
		_menuZone = 0;
		puzSetAct(p->id);
	}
	// Development: dev_input="ms:move x y;ms:click x y;ms:key code 0;..." replays input at ms after the menu opens.
	Common::StringArray script;
	for (const Common::String &step : Common::StringTokenizer(ConfMan.get("dev_input"), ";").split())
		script.push_back(step);
	_scripted = !script.empty(); // the real mouse (moves and buttons) is ignored while scripted
	uint32 menuStart = g_system->getMillis();
	uint32 lastRun = menuStart; // "+ms:..." steps run ms after the previous step ran
	while (!shouldQuit()) {
		pollEvents();
		while (!script.empty()) {
			if (script[0].hasPrefix("+")) {
				size_t colon = script[0].findFirstOf(':');
				uint dt = atoi(script[0].c_str() + 1);
				script[0] = Common::String::format("%u", lastRun + dt - menuStart) + (colon == Common::String::npos ? "" : script[0].substr(colon));
			}
			uint ms, x, y;
			char what[8], file[256];
			if (sscanf(script[0].c_str(), "%u:snap %255s", &ms, file) == 2) {
				// "snap C:/tmp/x.png" saves the screen as it was last presented
				if (g_system->getMillis() - menuStart < ms)
					break;
				Common::DumpFile out;
				if (!out.open(Common::FSNode(Common::Path(file, '/'))) || !::Image::writePNG(out, *_screen.surfacePtr()))
					warning("Ring: cannot write %s", file);
				script.remove_at(0);
				lastRun = g_system->getMillis();
				continue;
			}
			int n = sscanf(script[0].c_str(), "%u:%7s %u %u", &ms, what, &x, &y);
			if (n < 2) {
				script.remove_at(0);
				continue;
			}
			if (g_system->getMillis() - menuStart < ms)
				break;
			// Handler-level commands: "obj <object> <unk_19>" clicks an object's accessibility in
			// the current place, "mov <index>" takes one of its movabilities, "hold <object>" puts
			// an object in hand, "where" logs the zone, place, object in hand and bag.
			int here = _mode == 1 ? _rotation : _puzzle;
			Rotation *hereR = _mode == 1 ? _world->rotation(_rotation) : nullptr;
			Puzzle *hereP = hereR ? nullptr : _world->puzzle(_puzzle);
			if (!strcmp(what, "obj") && n == 4) {
				const Common::Array<Common::SharedPtr<Accessibility> > *accs = hereR ? &hereR->accessibilities : hereP ? &hereP->accessibilities : nullptr;
				const Accessibility *found = nullptr;
				for (uint i = 0; accs && i < accs->size() && !found; i++)
					if ((*accs)[i]->object == (int)x && (*accs)[i]->hotSpot.value == (int)y && (*accs)[i]->hotSpot.enabled)
						found = (*accs)[i].get();
				if (found)
					clickObject(_zone, x, y, here);
				else
					warning("Ring: dev obj %u %u: no enabled accessibility in %d", x, y, here);
			} else if (!strcmp(what, "mov") && n >= 3) {
				Common::Array<Movability> *list = hereR ? &hereR->movabilities : hereP ? &hereP->movabilities : nullptr;
				if (list && x < list->size() && (*list)[x].hotSpot.enabled) {
					Movability copy = (*list)[x];
					move(copy, x);
				} else {
					warning("Ring: dev mov %u: no such enabled movability in %d", x, here);
				}
			} else if (!strcmp(what, "save") && n >= 3) {
				saveGameState(x, "dev"); // "save <slot>", "load <slot>"
			} else if (!strcmp(what, "load") && n >= 3) {
				loadGameState(x);
			} else if (!strcmp(what, "zone") && n == 4) {
				goZone(x, y); // "zone <zone> <entry>": GoZone
			} else if (!strcmp(what, "hold") && n >= 3) {
				dropObject();
				holdObject(x);
			} else if (!strcmp(what, "pres") && n >= 3) {
				// "pres <object>": its presentations, shown or not, and their animations' state
				if (Object *o = _world->object(x)) {
					Common::String line;
					for (uint i = 0; i < o->presentations.size(); i++) {
						const Presentation &pr = o->presentations[i];
						line += Common::String::format(" %u:%s", i, pr.shown ? "shown" : "-");
						for (auto &a : pr.puzzleAnimations)
							line += Common::String::format("[%s f%d%s]", a->active ? "on" : "off", a->frame, a->paused ? " paused" : "");
					}
					debug("Ring: object %u%s", x, line.c_str());
				}
			} else if ((!strcmp(what, "varb") || !strcmp(what, "varw") || !strcmp(what, "vard")) && n >= 3) {
				World::VarType t = what[3] == 'b' ? World::kVarByte : what[3] == 'w' ? World::kVarWord : World::kVarDword;
				debug("Ring: %s %u = %d", what, x, _world->var(t, x));
			} else if (!strcmp(what, "hot")) {
				toggleHotspots();
			} else if (!strcmp(what, "where")) {
				Common::String bag;
				for (int id : _bag->contents())
					bag += Common::String::format(" %d", id);
				debug("Ring: zone %d %s %d, held %d, bag%s", _zone, _mode == 1 ? "rotation" : "puzzle", here, _bag->held(), bag.c_str());
			} else if (n < 4) {
				warning("Ring: dev_input: cannot read '%s'", script[0].c_str());
			} else if (!strcmp(what, "key")) {
				key(x);
			} else {
				// "down" and "up" press and release the left button, "click" does both, "rclick" is the right button.
				_mouse = Common::Point(x, y);
				if (!strcmp(what, "down") || !strcmp(what, "click"))
					_buttons.push_back(Button{ true, _mouse, false });
				if (!strcmp(what, "up") || !strcmp(what, "click"))
					_buttons.push_back(Button{ false, _mouse, false });
				if (!strcmp(what, "rclick"))
					_buttons.push_back(Button{ false, _mouse, true });
			}
			script.remove_at(0);
			lastRun = g_system->getMillis();
		}
		while (!_keys.empty()) {
			int k = _keys.remove_at(0);
			key(k);
		}
		while (!_buttons.empty()) {
			// The button state follows every event; handlers only see y < 465 (spec/boot.md, "Input").
			Button b = _buttons.remove_at(0);
			if (b.right) {
				toggleBag(); // WM_RBUTTONUP
				continue;
			}
			_buttonDown = b.down;
			if (b.pos.y >= 465)
				continue;
			if (b.down)
				buttonDown(b.pos.x, b.pos.y);
			else
				click(b.pos.x, b.pos.y);
		}
		if (!_pendingLoad.empty())
			applyLoad();
		runTimers();
		if (_gameOver) {
			// Mode 4 (spec/boot.md "Frame"): the set-ups again, then 0x431190(2, n): SetZone(1),
			// End.bmp at (0, 16) for 4 s (0x401000), StartMenu(0).
			_gameOver = 0;
			resetWorld();
			setZone(kZoneSY);
			if (Image *end = _resources->loadImage(kZoneSY, "End.bmp", true)) {
				end->draw(_screen, 0, 16, 1);
				delete end;
				present();
				wait(4000);
			}
			startMenu(false);
		}
		frame();
		g_system->delayMillis(10);
	}
	return Common::kNoError;
}

void RingEngine::key(int code) {
	// Escape ends a playing dialogue (0x406e40(5, 0x1002): its end event, so chains go on).
	if (code == 27)
		_sounds->stopType(kSoundDialogue, 0x1002);
	if (code == kKeyF12) {
		startMenu(true);
		return;
	}
	// The lists take no keys (0x46f030); SY's key handler runs unless a dialogue is up.
	Puzzle *p1 = _world->puzzle(1);
	bool dialogue = p1 && p1->mode == 2;
	if (!dialogue && _zone == kZoneSY)
		SY::onKey(this, code);
	Puzzle *p = dialogue ? p1 : _world->puzzle(_puzzle);
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
		if (!f.open(path) || !r->panorama->load(f, r->layers.size())) {
			warning("Ring: cannot load the node %s", path.toString().c_str());
			r->panorama.reset();
			return;
		}
		// 0x410410: the effects start at strength 0; the juggle's weights (spec/rotation.md).
		r->strength = 0.0f;
		r->loadTick = g_system->getMillis();
		r->jugWeights.resize(32 * 32);
		for (float &w : r->jugWeights)
			w = _random.getRandomNumber(32767) * r->jugAmplitude * (1.0f / 32767.0f);
	}
	leavePlace();
	_rotation = rotation;
	_mode = 1;
	_mouse = Common::Point(320, 240);
	g_system->warpMouse(320, 240);
	_panTime = g_system->getMillis();
	_sounds->enterPlace(&r->sounds, start, stop, true, r->alpha + 135.0f, _preferences[2]);
}

void RingEngine::goZone(int zone, int entry) {
	// ponytail: the CD check, the zone's archive (ART_x) and the saved-game entry (1000) come with their specs
	leavePlace();
	_sounds->stopAll(8);
	_sounds->clearPlaces();
	setZone(zone);
	_menuZone = 0;
	if (zone == kZoneAS)
		AS::enter(this, entry);
	else if (zone == kZoneNI)
		NI::enter(this, entry);
	else if (zone == kZoneRH)
		RH::enter(this, entry);
	else if (zone == kZoneN2)
		N2::enter(this, entry);
	else if (zone == kZoneFO)
		FO::enter(this, entry);
	else if (zone == kZoneRO)
		RO::enter(this, entry);
	else if (zone == kZoneWA)
		WA::enter(this, entry);
	else
		warning("Ring: zone %d is not implemented yet", zone);
}

void RingEngine::plyCin(const Common::String &name, int channel) {
	playMovie(this, Common::Path("DATA").appendComponent(zoneFolder(_zone)).appendComponent("PLA").appendComponent(name + ".cnm"), channel);
}

void RingEngine::plyCinMul(const Common::String &name) {
	_sounds->stopType(kSoundEffect, 0x100);
	_sounds->stopType(kSoundDialogue, 0x100);
	plyCin(name, languageChannel());
}

int RingEngine::languageId() const {
	switch (_gameDescription->language) {
	case Common::FR_FRA: return 2;
	case Common::DE_DEU: return 3;
	case Common::IT_ITA: return 4;
	case Common::ES_ESP: return 5;
	case Common::SV_SWE: return 6;
	case Common::NL_NLD: return 7;
	default: return 1;
	}
}

int RingEngine::languageChannel() const {
	static const int channels[] = { 1, 1, 2, 3, 1, 2, 1, 3 }; // by language id 1..7
	return channels[languageId()];
}

void RingEngine::timSta(int id, uint32 ms) {
	if (timerRunning(id))
		return; // aTimer::StartTimer refuses a running id
	_timers.push_back(Timer{ id, ms, g_system->getMillis() + ms });
}

void RingEngine::timSto(int id) {
	for (uint i = 0; i < _timers.size(); i++)
		if (_timers[i].id == id)
			_timers.remove_at(i--);
}

bool RingEngine::timerRunning(int id) const {
	for (const Timer &t : _timers)
		if (t.id == id)
			return true;
	return false;
}

void RingEngine::runTimers() {
	// One WM_TIMER per due timer and loop, as Windows posts at most one pending per timer.
	uint32 now = g_system->getMillis();
	Common::Array<int> due;
	for (Timer &t : _timers) {
		if ((int32)(now - t.due) >= 0) {
			t.due = now + t.period;
			due.push_back(t.id);
		}
	}
	for (int id : due)
		if (timerRunning(id))
			onTimer(this, _zone, id);
}

void RingEngine::renderFrame() {
	pollEvents();
	frame();
}

void RingEngine::renderFor(uint32 ms) {
	uint32 start = g_system->getMillis();
	while (g_system->getMillis() - start < ms && !shouldQuit()) {
		renderFrame();
		g_system->delayMillis(10);
	}
}

void RingEngine::rotSetRolTo(int rotation, float alpha, float beta, float ran) {
	if (Rotation *r = _world->rotation(rotation))
		turn(*r, alpha, beta, ran);
}

void RingEngine::setSoundItem(int owner, int sound, bool on) {
	// 0x41a220 / 0x41a280: started or stopped at once when the owner is the current place.
	Rotation *r = _world->rotation(owner);
	Puzzle *p = r ? nullptr : _world->puzzle(owner);
	SoundItems *items = r ? &r->sounds : p ? &p->sounds : nullptr;
	if (!items)
		return;
	bool current = (r && _mode == 1 && _rotation == owner) || (p && _puzzle == owner);
	for (auto &i : *items) {
		if (i->sound != sound)
			continue;
		i->active = on;
		if (current && on)
			_sounds->startItem(*i);
		else if (current)
			_sounds->stopItem(*i);
	}
}

void RingEngine::setSoundItemVolume(int owner, int sound, int volume) {
	Rotation *r = _world->rotation(owner);
	Puzzle *p = r ? nullptr : _world->puzzle(owner);
	SoundItems *items = r ? &r->sounds : p ? &p->sounds : nullptr;
	if (!items)
		return;
	for (auto &i : *items) {
		if (i->sound == sound) {
			i->volume = volume;
			if (_sounds->playing(sound))
				_sounds->setVolume(sound, volume);
		}
	}
}

void RingEngine::gameOver(int n) {
	_gameOver = n;
	_sounds->stopAll(0x40);
}

void RingEngine::saveWorldState(const Common::String &file) {
	WorldState &s = _worldStates[file];
	s.bag = _bag->contents();
	s.timers = _timers;
	s.tick = g_system->getMillis();
}

bool RingEngine::loadWorldState(const Common::String &file) {
	if (!_worldStates.contains(file))
		return false;
	const WorldState &s = _worldStates[file];
	_bag->removeAll();
	for (int i = (int)s.bag.size() - 1; i >= 0; i--)
		_bag->add(s.bag[i]);
	// The timers keep the time they had left (aTimer::LoadSave with the tick count).
	uint32 now = g_system->getMillis();
	_timers.clear();
	for (Timer t : s.timers) {
		t.due = now + (t.due - s.tick);
		_timers.push_back(t);
	}
	// ponytail: the sounds playing when the world was left (0x469790, 0x4696f0) are not replayed
	return true;
}

float RingEngine::rotGetAlp(int rotation) {
	Rotation *r = _world->rotation(rotation);
	if (!r)
		return 0.0f;
	float a = r->alpha + 135.0f;
	return a > 360.0f ? a - 360.0f : a;
}

void RingEngine::credits() {
	_sounds->stopAll(0x400);
	setZone(kZoneWA); // 51002 is WA's sound
	_sounds->play(51002, true);
	setZone(kZoneSY);
	for (int i = 1; i <= 11; i++)
		if (scrollImage(Common::String::format("cre_%02d.bma", i), i == 11 ? 5000 : 0) == 2)
			break;
	_sounds->stop(51002, 0x400);
}

int RingEngine::scrollImage(const Common::String &name, uint32 holdMs) {
	while (_escapeDown && !shouldQuit())
		pollEvents(10);
	Common::ScopedPtr<Image> img(_resources->loadImage(kZoneSY, name, false));
	if (!img)
		return 0;
	// One row per frame (Q-0080: 1/60 s); the window's top is row i.
	int h = img->surface.h, i = 0;
	bool escaped = false;
	uint32 next = g_system->getMillis();
	for (; i < h - 448 && !shouldQuit(); i++) {
		if (_escapeDown) {
			while (_escapeDown && !shouldQuit())
				pollEvents(10);
			escaped = true;
			break;
		}
		_screen.blitFrom(img->surface, Common::Rect(0, i, MIN<int>(640, img->surface.w), i + 448), Common::Point(0, 16));
		present();
		next += 17;
		while ((int32)(next - g_system->getMillis()) > 0 && !shouldQuit())
			pollEvents(2);
	}
	if (i == h - 448)
		wait(holdMs);
	return escaped ? 2 : 1;
}

void RingEngine::setMouse(int x, int y) {
	_mouse = Common::Point(x, y);
	g_system->warpMouse(x, y);
}

void RingEngine::resetWorld() {
	timStoAll();
	_sounds->stopAll(0x400);
	_sounds.reset(new Sounds(this));
	_sounds->setTypeVolumes(_preferences[0], _preferences[1]);
	_bag.reset();
	_world.reset(new World());
	_world->setUp(_sounds.get(), _preferences[2]);
	_world->loadNames(_languageFolder);
	_world->setFont(_font.get());
	_bag.reset(new Bag(*_world, *_resources));
	_puzzle = _rotation = 0;
	_cursors->remove(1);
	_cursors->remove(2);
	_worldStates.clear(); // the worlds left through Erda belong to the old game
}

void RingEngine::setZone(int zone) {
	_zone = zone;
	_bag->setErda(zone != kZoneSY && zone != kZoneAS);
}

void RingEngine::holdObject(int id) {
	Object *o = _world->object(id);
	if (!o)
		return;
	_bag->setHeld(id);
	// 0x40b860: cursors 1 (passive) and 2 (active) become the object's; image kind 4 is LSTICON.
	for (int i = 0; i < 2; i++) {
		const DragCursor &d = o->handCursors[i];
		const char *folder = d.imageKind == 4 ? "LSTICON" : "CURSOR";
		_cursors->remove(1 + i);
		if (d.kind == 3)
			_cursors->add(1 + i, o->icon + (i ? "_a" : "_p"), 3, 0, 0.0f, folder);
		else if (d.kind == 4)
			_cursors->add(1 + i, o->icon, 4, d.frames, d.fps, folder);
		_cursors->setOffset(1 + i, d.offsetX, d.offsetY);
	}
}

void RingEngine::dropObject() {
	_bag->setHeld(0);
	_cursors->remove(1);
	_cursors->remove(2);
}

void RingEngine::toggleBag() {
	Puzzle *p1 = _world->puzzle(1);
	if (_drag.active || _menuZone || (p1 && p1->mode == 2))
		return;
	if (_bag->shown()) {
		hideBag();
		return;
	}
	dropObject();
	_bag->show(g_system->getMillis());
	// 0x40de90: the current rotation stops following the mouse; its old state is kept for Erda.
	Rotation *r = _mode == 1 ? _world->rotation(_rotation) : nullptr;
	_bagRotation = r ? r->id : 0;
	_bagWasFrozen = r && r->frozen;
	if (r)
		r->frozen = true;
}

void RingEngine::hideBag() {
	_bag->hide();
	if (Rotation *r = _world->rotation(_bagRotation))
		r->frozen = false;
}

void RingEngine::erda() {
	// The world in SY's variables: NI and RH 1, RO and N2 2, FO 3, WA 4 (spec/bag.md).
	static const int world[] = { 0, 0, 1, 1, 3, 2, 4, 0, 2 };
	int n = _zone >= 0 && _zone <= 8 ? world[_zone] : 0;
	if (!n)
		return;
	static const char *const files[] = { "", "", "alb", "alb", "sie", "log", "bru", "", "log" };
	saveWorldState(files[_zone]);
	_world->setVar(World::kVarDword, 90012 + n, _zone);
	_world->setVar(World::kVarByte, 90008 + n, 1);
	bool onPuzzle = _mode == 2;
	_world->setVar(World::kVarByte, 90016 + n, onPuzzle);
	_world->setVar(World::kVarDword, 90020 + n, onPuzzle ? _puzzle : _rotation);
	if (!onPuzzle)
		_world->setVar(World::kVarByte, 90024 + n, _bagWasFrozen);
	AS::returnFromWorld(this, 13);
}

void RingEngine::clickObject(int zone, int object, int value, int place) {
	debugC(1, kDebugInput, "click: object %d unk_19 %d in %d", object, value, place);
	Object *o = _world->object(object);
	if (!o)
		return;
	int before = _zone;
	_takeAllowed = true;
	if (o->flags & 1)
		onClick(this, zone, object, value, place); // 0x40bbb0
	if (_zone != before)
		return; // a zone change is pending (mode 4)
	if (o->flags & 8) {
		// 0x40bed0 (only WA acts on it), then, unless a handler cleared app+0x74, the
		// clicked object goes in hand.
		if (zone == kZoneWA)
			WA::onTake(this, object, value);
		if (_takeAllowed) {
			dropObject();
			holdObject(object);
		}
		_takeAllowed = true;
	}
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
	if (fromGame) {
		// Busy cursor, one frame, the bag hidden, the game kept as `SaveGame` and the screen copied.
		_cursors->set(0x33);
		renderFrame();
		_bag->hide();
		snapshot();
	}
	_bag->hide();
	dropObject();
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
	if (_bag->shown())
		_bag->draw(_screen, g_system->getMillis());
	if (_buttonDown)
		dragMove(_mouse.x, _mouse.y);
	track(_mouse.x, _mouse.y);
	drawHotspotRegions();
	// H types a letter on the save screen, so the overlay's key is off there.
	if (Common::Keymap *keymap = g_system->getEventManager()->getKeymapper()->getKeymap("ring-play"))
		keymap->setEnabled(!(_mode == 2 && _puzzle == 90003));
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
		for (int steps = 0; now - _panTime >= 17 && steps < 10 && !r->frozen; steps++, _panTime += 17) {
			float dx = _mouse.x / 640.0f - 0.5f, dy = _mouse.y / 480.0f - 0.5f;
			if (ABS(dx) > 0.25f)
				r->alpha += dx * (ABS(dx) - 0.25f) * 48.0f;
			if (ABS(dy) > 0.25f)
				r->beta += dy * (ABS(dy) - 0.25f) * 48.0f;
			_view.update(*r, *r->panorama);
		}
		if (now - _panTime >= 17)
			_panTime = now;
		// The effects' strength grows by the frame time up to 1 (0x410610).
		if (r->strength < 1.0f)
			r->strength = MIN(1.0f, r->strength + (now - _lastRotationFrame) * 0.001f);
		_lastRotationFrame = now;
		_view.update(*r, *r->panorama);
		if (r->juggle)
			_view.juggle(*r, (now - r->loadTick) * 0.001f);
		updateLayers(*r);
		_view.draw(*r->panorama, _screen, 16);
		// 3D sounds follow the view (0x41ed80, every frame).
		float alpha = r->alpha + 135.0f;
		if (alpha > 360.0f)
			alpha -= 360.0f;
		for (auto &i : r->sounds) {
			if (_sounds->typeOf(i->sound) == kSoundAmbientEffect) {
				i->pan = i->pan3D(alpha, _preferences[2]);
				_sounds->setPan(i->sound, i->pan);
			}
		}
	} else if (Puzzle *p = _world->puzzle(_puzzle)) {
		advanceAnimations(*p);
		_world->draw(*p, *_resources, _screen);
		if (_zone == kZoneSY)
			SY::draw(this, _screen);
	}
	if (Puzzle *p1 = _world->puzzle(1)) {
		advanceAnimations(*p1);
		_world->draw(*p1, *_resources, _screen);
	}
}

void RingEngine::holdEvent(Animation &anim) {
	// 0x40c910: a hold on a frame starts (1) or ends (2); only WA handles it (E-0224).
	int phase = anim.holdEvent;
	anim.holdEvent = 0;
	if (phase && _zone == kZoneWA)
		WA::onHold(this, phase, anim.id);
}

void RingEngine::advanceAnimations(Puzzle &p) {
	// aPuzzle::Update 0x41c320, before the pictures are drawn.
	uint32 now = g_system->getMillis();
	for (uint i = 0; i < p.animations.size(); i++) {
		Common::SharedPtr<Animation> anim = p.animations[i];
		if (anim->advance(now))
			onAnimation(this, _zone, anim->id, anim->frame + 1);
		holdEvent(*anim);
	}
}

void RingEngine::updateLayers(Rotation &r) {
	uint32 now = g_system->getMillis();
	for (uint i = 0; i < r.layers.size(); i++) {
		Common::SharedPtr<Animation> anim = r.layers[i].animation;
		if (anim && anim->advance(now))
			onAnimation(this, _zone, anim->id, anim->frame + 1);
		if (anim)
			holdEvent(*anim);
	}
	// An animated layer takes its animation's state: stopped hides it (0x4103d0), running sets its frame (0x411530).
	for (uint i = 0; i < r.layers.size(); i++) {
		Rotation::Layer &l = r.layers[i];
		if (!l.animation || !r.panorama->animated(i))
			continue;
		if (!l.animation->active) {
			if (l.shown) {
				l.shown = false;
				l.dirty = true;
			}
		} else if (l.frame != l.animation->frame) {
			l.frame = l.animation->frame;
			if (l.shown)
				l.dirty = true;
		}
	}
	// 0x4114c0
	for (uint i = 0; i < r.layers.size(); i++) {
		Rotation::Layer &l = r.layers[i];
		if (l.dirty) {
			r.panorama->patch(i, l.shown ? l.frame : -1);
			l.dirty = false;
		}
	}
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
		r.strength = 1 - t;
		drawView();
		present();
		pollEvents(16); // one step per frame (Q-0011)
	}
}

void RingEngine::move(const Movability &m, int index) {
	// ponytail: Ctrl-clicks (no turn, no ride) come with the key spec's modifiers
	Rotation *from = _mode == 1 ? _world->rotation(_rotation) : nullptr;
	int fromId = from ? _rotation : _puzzle, zone = _zone;
	onBeforeMove(this, _zone, fromId, m.target, index, m.hotSpot.value, m.kind); // 0x40c2b0
	if (_zone != zone)
		return; // the zone changed (mode 4)
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
			if (_sounds->typeOf(i->sound) == kSoundAmbientEffect)
				i->pan = i->pan3D(to->alpha + 135.0f, _preferences[2]);
		_sounds->prepareTransition(&to->sounds);
	} else if (toPuzzle) {
		_sounds->prepareTransition(&toPuzzle->sounds);
	}
	int rideZone = from ? from->zone : _zone;
	if (m.ride.empty() || !playMovie(this, Common::Path("DATA").appendComponent(zoneFolder(rideZone)).appendComponent("PLA").appendComponent(m.ride + ".cnm"), 0))
		_sounds->finishTransition();
	if (to) {
		rotSetAct(m.target);
		to->setAngles(m.alpha2, m.beta2, m.ran2);
	} else {
		puzSetAct(m.target);
	}
	onAfterMove(this, _zone, m.target, fromId, index, m.hotSpot.value, m.kind); // 0x40c420
	dropObject(); // app+0x75 is always set
}

void RingEngine::soundEvent(int id, int type, int reason) {
	// (id, type, reason without bit 0x1000, reason & 0x1000) to the zone's handler (0x40ced0).
	int why = reason & ~0x1000, ended = reason & 0x1000;
	if (_zone == kZoneSY)
		SY::onSound(this, id, type, why, ended);
	else if (_zone == kZoneAS)
		AS::onSound(this, id, type, why, ended);
	else if (_zone == kZoneNI)
		NI::onSound(this, id, type, why, ended);
	else if (_zone == kZoneRH)
		RH::onSound(this, id, type, why, ended);
	else if (_zone == kZoneN2)
		N2::onSound(this, id, type, why, ended);
	else if (_zone == kZoneFO)
		FO::onSound(this, id, type, why, ended);
	else if (_zone == kZoneRO)
		RO::onSound(this, id, type, why, ended);
	else if (_zone == kZoneWA)
		WA::onSound(this, id, type, why, ended);
}

void RingEngine::track(int x, int y) {
	if (_bag->shown()) {
		// Only the bag is tested (0x418a70); the cursor changes on its menu band, Erda and nothing.
		int h = _bag->track(x, y, g_system->getMillis(), _screen, _font.get());
		if (h == Bag::kMenu || h == Bag::kErdaButton || h == Bag::kNone)
			_cursors->set(_drag.active ? 3 : _bag->held() ? 1 : 0x32);
		onNothing(this, _zone);
		return;
	}
	const HotSpot *h = trackHit(x, y);
	if (_drag.active)
		_cursors->set(h == _drag.hotSpot ? 4 : 3);
}

const HotSpot *RingEngine::trackHit(int x, int y) {
	// With an object in hand: cursor 2 on an accessibility, 1 on nothing (spec/cursor.md).
	int held = _bag->held();
	if (Puzzle *p1 = _world->puzzle(1)) {
		if (const Accessibility *acc = _world->hit(*p1, x, y)) {
			_cursors->set(held ? 2 : acc->hotSpot.cursor);
			onAccessibility(this, kZoneSY, acc->object, acc->hotSpot.value);
			return &acc->hotSpot;
		}
		if (p1->mode == 2) {
			_cursors->set(held ? 1 : 0x32);
			SY::onNothing(this);
			return nullptr;
		}
	}
	Rotation *r = _mode == 1 ? _world->rotation(_rotation) : nullptr;
	if (r && !r->paused && r->panorama) {
		Common::Point pt = _view.toPanorama(*r->panorama, x, y);
		if (const Accessibility *acc = World::hit(r->accessibilities, pt.x, pt.y)) {
			_cursors->set(held ? 2 : acc->hotSpot.cursor);
			onAccessibility(this, _zone, acc->object, acc->hotSpot.value);
			return &acc->hotSpot;
		}
		if (const Movability *m = World::hit(r->movabilities, pt.x, pt.y)) {
			_cursors->set(m->hotSpot.cursor); // ponytail: the "on a movability" event comes with the zone handlers
			return &m->hotSpot;
		}
	} else if (Puzzle *p = _world->puzzle(_puzzle)) {
		// Its visual object lists first (0x41d7a0).
		if (_zone == kZoneSY && SY::listTrack(this, x, y))
			return nullptr;
		if (const Accessibility *acc = _world->hit(*p, x, y)) {
			_cursors->set(held ? 2 : acc->hotSpot.cursor);
			onAccessibility(this, _zone, acc->object, acc->hotSpot.value);
			return &acc->hotSpot;
		}
		if (const Movability *m = World::hit(p->movabilities, x, y)) {
			_cursors->set(m->hotSpot.cursor);
			return &m->hotSpot;
		}
	}
	_cursors->set(held ? 1 : 0x32);
	onNothing(this, _zone);
	return nullptr;
}

void RingEngine::buttonDown(int x, int y) {
	if (_bag->shown())
		return;
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
	debugC(1, kDebugInput, "button down at (%d, %d): object %d unk_19 %d in %d", x, y, o ? o->id : 0, acc ? acc->hotSpot.value : 0, id);
	if (o && (o->flags & 2)) // 0x40bd40: puzzle 1's go to SY
		onButtonDown(this, id == 1 && onPuzzle ? kZoneSY : _zone, o->id, acc->hotSpot.value);
	if (o && (o->flags & 4)) {
		_drag = Drag();
		_drag.active = true;
		_drag.object = o->id;
		_drag.value = acc->hotSpot.value;
		_drag.puzzle = id;
		_drag.onPuzzle = onPuzzle;
		_drag.press = _drag.current = _drag.previous = _drag.reference = at;
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
	_drag.previous = _drag.current;
	_drag.current = Common::Point(x, y);
	dragEvent(3);
}

void RingEngine::dragEvent(int phase) {
	if ((_drag.puzzle == 1 && _drag.onPuzzle) || _zone == kZoneSY)
		SY::onDrag(this, _drag.object, phase);
	else if (_zone == kZoneNI)
		NI::onDrag(this, _drag.object, _drag.value, phase);
	else if (_zone == kZoneN2)
		N2::onDrag(this, _drag.object, _drag.value, phase);
	else if (_zone == kZoneFO)
		FO::onDrag(this, _drag.object, phase);
	else if (_zone == kZoneRO)
		RO::onDrag(this, _drag.object, _drag.value, phase);
}

void RingEngine::click(int x, int y) {
	if (_drag.active) {
		int mode = _drag.mode;
		dragEvent(2);
		_drag = Drag();
		if (mode == 2)
			return;
	}
	if (_bag->shown()) {
		// Only the bag (0x418520).
		int h = _bag->click(x, y, g_system->getMillis());
		if (h == Bag::kMenu) {
			startMenu(true);
		} else if (h == Bag::kErdaButton) {
			erda();
		} else if (h > 0) {
			// In hand, the bag-click event (0x40c1f0, only FO), the bag closed; a handler that
			// cleared app+0x78 drops it again, else its cursors come (spec/bag.md).
			_bag->setHeld(h);
			_listAllowed = true;
			onBagClick(this, _zone, h);
			hideBag();
			if (!_listAllowed) {
				_listAllowed = true;
				dropObject();
			} else {
				holdObject(h);
			}
			_mouse = Common::Point(320, 240);
			g_system->warpMouse(320, 240);
		}
		return;
	}
	Puzzle *p1 = _world->puzzle(1);
	Puzzle *p = _mode == 2 ? _world->puzzle(_puzzle) : nullptr;
	if (p1 && p1->mode != 2) {
		if (const Movability *m = World::hit(p1->movabilities, x, y)) {
			Movability copy = *m;
			move(copy, m - p1->movabilities.begin());
			return;
		}
	}
	for (Puzzle *q : { p1, p }) {
		if (!q)
			continue;
		if (q == p && _zone == kZoneSY && SY::listClick(this, x, y))
			return;
		if (const Accessibility *acc = _world->hit(*q, x, y)) {
			clickObject(q == p1 ? kZoneSY : _zone, acc->object, acc->hotSpot.value, q->id);
			track(x, y);
			return;
		}
		if (q == p1 && p1->mode == 2)
			return;
		if (q == p) {
			if (const Movability *m = World::hit(p->movabilities, x, y)) {
				Movability copy = *m;
				move(copy, m - p->movabilities.begin());
				return;
			}
		}
	}
	Rotation *r = _mode == 1 ? _world->rotation(_rotation) : nullptr;
	if (!r || r->paused || !r->panorama)
		return;
	Common::Point pt = _view.toPanorama(*r->panorama, x, y);
	if (const Accessibility *acc = World::hit(r->accessibilities, pt.x, pt.y)) {
		clickObject(_zone, acc->object, acc->hotSpot.value, r->id);
		track(x, y);
		return;
	}
	if (const Movability *m = World::hit(r->movabilities, pt.x, pt.y)) {
		Movability copy = *m;
		move(copy, m - r->movabilities.begin());
	}
}

} // End of namespace Ring
