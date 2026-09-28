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
#include "common/endian.h"
#include "common/events.h"
#include "common/file.h"
#include "common/system.h"
#include "common/tokenizer.h"

#include "backends/keymapper/keymap.h"
#include "backends/keymapper/keymapper.h"

#include "engines/util.h"

#include "graphics/cursorman.h"

#include "graphics/pixelformat.h"

#include "image/png.h"

#include "peintre/bfg.h"
#include "peintre/detection.h"
#include "peintre/display.h"
#include "peintre/gfx.h"
#include "peintre/movie.h"
#include "peintre/obj3d.h"
#include "peintre/peintre.h"
#include "peintre/sound.h"
#include "peintre/world.h"

namespace Peintre {

PeintreEngine::PeintreEngine(OSystem *syst, const ADGameDescription *gameDesc)
	: Engine(syst), _gameDescription(gameDesc) {
	// The detector's directory globs (Data, Scenes_3D) are already in SearchMan.
}

PeintreEngine::~PeintreEngine() {
	for (auto &s : _sessions)
		delete s._value.script;
	delete _movies;
	delete _sound;
	delete _display;
	_screen.free();
}

void PeintreEngine::present() {
	_display->present(_screen);
}

void PeintreEngine::warpMouse(int x, int y) {
	const Common::Point w = _display->toWindow(Common::Point(x, y));
	_system->warpMouse(w.x, w.y);
	_mouse = Common::Point(x, y);
}

void PeintreEngine::captureMouse(bool capture) {
	if (capture == _captured)
		return;
	_captured = capture;
	_system->lockMouse(capture);
	_lookX = _lookY = 0;
	if (capture) {
		// The mouse stays at the centre, where the game sees it.
		_captureStart = _system->getMillis();
		warpMouse(320, 240);
	}
}

void PeintreEngine::takeLook(float &yaw, float &pitch) {
	yaw = _lookX * _lookScale;
	pitch = (_invertY ? _lookY : -_lookY) * _lookScale;
	_lookX = _lookY = 0;
}

void PeintreEngine::getHotspotPositions(Common::Array<Graphics::HotspotInfo> &hotspots) {
	if (_world)
		_world->hotspots(hotspots);
}

bool PeintreEngine::hotspotDirty() const {
	return _world && _world->hotspotsChanged();
}

void PeintreEngine::toggleHotspots() {
	// ScummVM's hotspot overlay, as a toggle; the cursor stays as it was.
	if (!ConfMan.getBool("enable_hotspots"))
		return;
	const bool cursor = CursorMan.isVisible();
	showHotspots(!_showHotspots);
	CursorMan.showMouse(cursor);
}

void PeintreEngine::drawHotspots() {
	if (!_showHotspots || !_world)
		return;
	Common::Array<Graphics::HotspotInfo> list;
	_world->hotspots(list);
	const bool changed = _world->hotspotsChanged() || _hotspotForceRedraw;
	Engine::drawHotspots();
	// The base class keeps its last markers when there are none.
	if (changed && list.empty() && _system->isOverlayVisible())
		_system->hideOverlay();
	_world->hotspotsDrawn();
}

static Common::KeyCode devKey(const Common::String &name, char &ascii) {
	static const struct {
		const char *name;
		Common::KeyCode code;
		char ascii;
	} kKeys[] = {
		{ "space", Common::KEYCODE_SPACE, ' ' }, { "esc", Common::KEYCODE_ESCAPE, 27 },
		{ "backspace", Common::KEYCODE_BACKSPACE, 8 }, { "enter", Common::KEYCODE_RETURN, 13 },
		{ "up", Common::KEYCODE_UP, 0 }, { "down", Common::KEYCODE_DOWN, 0 },
		{ "left", Common::KEYCODE_LEFT, 0 }, { "right", Common::KEYCODE_RIGHT, 0 },
		{ "pageup", Common::KEYCODE_PAGEUP, 0 }, { "pagedown", Common::KEYCODE_PAGEDOWN, 0 },
		// Pushed events skip the keymapper: its actions by name.
		{ "strafeleft", (Common::KeyCode)kKeyStrafeLeft, 0 }, { "straferight", (Common::KeyCode)kKeyStrafeRight, 0 },
		{ "hotspots", (Common::KeyCode)kKeyHotspots, 0 }
	};
	for (const auto &k : kKeys) {
		if (name.equalsIgnoreCase(k.name)) {
			ascii = k.ascii;
			return k.code;
		}
	}
	ascii = name.empty() ? 0 : name[0];
	return name.empty() ? Common::KEYCODE_INVALID : (Common::KeyCode)tolower(name[0]);
}

void PeintreEngine::devStep() {
	if (_devCommands.empty())
		return;
	const uint32 now = _system->getMillis() - _devStart;
	while (!_devCommands.empty() && _devCommands[0].time <= now) {
		const Common::String c = _devCommands[0].command;
		_devCommands.remove_at(0);
		Common::Array<Common::String> w;
		Common::String cur;
		for (uint i = 0; i <= c.size(); i++) {
			if (i == c.size() || c[i] == ' ') {
				if (!cur.empty())
					w.push_back(cur);
				cur.clear();
			} else {
				cur += c[i];
			}
		}
		if (w.empty())
			continue;
		debugC(1, kDebugInput, "dev_commands: %s", c.c_str());
		Common::EventManager *em = _system->getEventManager();
		Common::Event e;
		if ((w[0] == "click" || w[0] == "tap" || w[0] == "press" || w[0] == "move") && w.size() >= 3) {
			e.type = Common::EVENT_MOUSEMOVE;
			e.mouse = _display->toWindow(Common::Point(atoi(w[1].c_str()), atoi(w[2].c_str())));
			_system->warpMouse(e.mouse.x, e.mouse.y);
			em->pushEvent(e);
			if (w[0] == "press") {
				// The button stays down until "release x y" (drags).
				e.type = Common::EVENT_LBUTTONDOWN;
				em->pushEvent(e);
			} else if (w[0] == "click" || w[0] == "tap") {
				// A tap is down for exactly one poll: its release is due at the next one.
				e.type = Common::EVENT_LBUTTONDOWN;
				em->pushEvent(e);
				DevCommand up = { now + (w[0] == "tap" ? 1 : 150), Common::String::format("release %s %s", w[1].c_str(), w[2].c_str()) };
				_devCommands.insert_at(0, up);
			}
		} else if (w[0] == "look" && w.size() >= 3) {
			// Relative mouse motion (modern controls' mouse look).
			e.type = Common::EVENT_MOUSEMOVE;
			e.mouse = _display->toWindow(Common::Point(320, 240));
			e.relMouse = Common::Point(atoi(w[1].c_str()), atoi(w[2].c_str()));
			em->pushEvent(e);
		} else if (w[0] == "release" && w.size() >= 3) {
			e.type = Common::EVENT_LBUTTONUP;
			e.mouse = _display->toWindow(Common::Point(atoi(w[1].c_str()), atoi(w[2].c_str())));
			em->pushEvent(e);
		} else if ((w[0] == "key" || w[0] == "hold" || w[0] == "keyup") && w.size() >= 2) {
			char ascii;
			e.kbd.keycode = devKey(w[1], ascii);
			e.kbd.ascii = ascii;
			e.type = w[0] == "keyup" ? Common::EVENT_KEYUP : Common::EVENT_KEYDOWN;
			em->pushEvent(e);
			if (w[0] != "keyup") {
				const uint32 hold = w[0] == "hold" && w.size() >= 3 ? atoi(w[2].c_str()) : 100;
				DevCommand up = { now + hold, "keyup " + w[1] };
				uint at = 0;
				while (at < _devCommands.size() && _devCommands[at].time <= up.time)
					at++;
				_devCommands.insert_at(at, up);
			}
		} else if (w[0] == "type" && w.size() >= 2) {
			for (uint i = 0; i < w[1].size(); i++) {
				e.type = Common::EVENT_KEYDOWN;
				e.kbd.ascii = w[1][i];
				e.kbd.keycode = (Common::KeyCode)tolower(w[1][i]);
				em->pushEvent(e);
				e.type = Common::EVENT_KEYUP;
				em->pushEvent(e);
			}
		} else if (w[0] == "snap" && w.size() >= 2) {
			Common::DumpFile out;
			Graphics::Surface frame;
			if (out.open(Common::Path(w[1], '/')))
				Image::writePNG(out, _display->snapshot(frame) ? frame : _screen);
			frame.free();
		} else if (w[0] == "where") {
			if (_world) {
				const Camera &cam = _world->camera();
				debug("where: %d, %d, %d pitch %d yaw %d", cam.x, cam.y, cam.z, cam.pitch, cam.yaw);
			}
		} else if (w[0] == "quit") {
			quitGame();
		}
	}
}

void PeintreEngine::pollInput() {
	endTick();
	pollEvents();
}

void PeintreEngine::endTick() {
	_keysFired.clear();
	_typed.clear();
	_pressed = false;
}

void PeintreEngine::pollEvents() {
	devStep();
	Common::Event event;
	while (_eventMan->pollEvent(event)) {
		if (Common::isMouseEvent(event))
			event.mouse = _display->toLogical(event.mouse);
		if (_captured && Common::isMouseEvent(event)) {
			// Mouse look: the motion turns the view, the game's mouse stays at the centre.
			// Motion queued before the capture, or from its warp, does not turn.
			if (event.type == Common::EVENT_MOUSEMOVE && _system->getMillis() - _captureStart >= 100) {
				_lookX += event.relMouse.x;
				_lookY += event.relMouse.y;
			}
			event.mouse = Common::Point(320, 240);
		}
		switch (event.type) {
		case Common::EVENT_SCREEN_CHANGED:
			_display->updateSize();
			break;
		case Common::EVENT_MOUSEMOVE:
			_mouse = event.mouse;
			break;
		case Common::EVENT_LBUTTONDOWN:
			_mouse = event.mouse;
			_button = true;
			_pressed = true;
			break;
		case Common::EVENT_LBUTTONUP:
			_mouse = event.mouse;
			_button = false;
			break;
		case Common::EVENT_KEYDOWN:
			if (event.kbd.keycode == (Common::KeyCode)kKeyHotspots) { // dev_commands
				toggleHotspots();
				break;
			}
			if (Common::find(_keysDown.begin(), _keysDown.end(), event.kbd.keycode) == _keysDown.end())
				_keysDown.push_back(event.kbd.keycode);
			if (event.kbd.keycode == Common::KEYCODE_BACKSPACE)
				_typed += '\b';
			else if (event.kbd.ascii >= 32 && event.kbd.ascii < 256)
				_typed += (char)event.kbd.ascii;
			break;
		case Common::EVENT_CUSTOM_ENGINE_ACTION_START:
			if (event.customType == kKeyHotspots) {
				toggleHotspots();
				break;
			}
			// A keymapper action is the key it stands for (metaengine.cpp initKeymaps).
			if (Common::find(_keysDown.begin(), _keysDown.end(), (Common::KeyCode)event.customType) == _keysDown.end())
				_keysDown.push_back((Common::KeyCode)event.customType);
			break;
		case Common::EVENT_CUSTOM_ENGINE_ACTION_END: {
			if (event.customType == kKeyHotspots)
				break;
			Common::Array<Common::KeyCode>::iterator it = Common::find(_keysDown.begin(), _keysDown.end(), (Common::KeyCode)event.customType);
			if (it != _keysDown.end())
				_keysDown.erase(it);
			_keysFired.push_back((Common::KeyCode)event.customType);
			break;
		}
		case Common::EVENT_KEYUP: {
			// A key fires on the tick it is released (ui.md "Input").
			Common::Array<Common::KeyCode>::iterator it = Common::find(_keysDown.begin(), _keysDown.end(), event.kbd.keycode);
			if (it != _keysDown.end())
				_keysDown.erase(it);
			_keysFired.push_back(event.kbd.keycode);
			break;
		}
		default:
			break;
		}
	}
}

bool PeintreEngine::keyFired(Common::KeyCode key) const {
	return Common::find(_keysFired.begin(), _keysFired.end(), key) != _keysFired.end();
}

void PeintreEngine::waitTick(uint32 ms) {
	const uint32 now = _system->getMillis();
	if (_lastTick && now < _lastTick + ms)
		_system->delayMillis(_lastTick + ms - now);
	_lastTick = _system->getMillis();
}

Common::Error PeintreEngine::run() {
	// Enhancements (launcher options, all off by default: the original).
	ConfMan.registerDefault("high_fps", false);
	ConfMan.registerDefault("high_res", false);
	ConfMan.registerDefault("widescreen", false);
	ConfMan.registerDefault("filter_textures", false);
	ConfMan.registerDefault("fov", 67);
	ConfMan.registerDefault("turn_speed", 100);
	ConfMan.registerDefault("modern_controls", false);
	ConfMan.registerDefault("invert_y", false);
	ConfMan.registerDefault("mouse_sensitivity", 100);
	ConfMan.registerDefault("enable_hotspots", true);
	_modern = ConfMan.getBool("modern_controls");
	_invertY = ConfMan.getBool("invert_y");
	_lookScale *= ConfMan.getInt("mouse_sensitivity") / 100.0f;
	Common::Keymapper *keymapper = _system->getEventManager()->getKeymapper();
	if (Common::Keymap *k = keymapper->getKeymap("peintre"))
		k->setEnabled(!_modern);
	if (Common::Keymap *k = keymapper->getKeymap("peintre-modern"))
		k->setEnabled(_modern);
	_keymap = keymapper->getKeymap(_modern ? "peintre-modern" : "peintre");
	// The original draws on a 640x480 16-bit surface (boot.md step 5); with OpenGL the 3D is
	// drawn at the window's size and the page is scaled into it.
	_display = Display::create();
	const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
	_screen.create(640, 480, format);

	if (ConfMan.getBool("dev_load_all"))
		loadAllScenes();

	_devStart = _system->getMillis();
	if (ConfMan.hasKey("dev_commands")) {
		// ms:command;ms:command... (peintre.h devStep)
		const Common::String all = ConfMan.get("dev_commands");
		Common::String item;
		for (uint i = 0; i <= all.size(); i++) {
			if (i == all.size() || all[i] == ';') {
				const int colon = item.findFirstOf(':');
				if (colon > 0) {
					DevCommand d = { (uint32)atoi(item.substr(0, colon).c_str()), item.substr(colon + 1) };
					d.command.trim();
					_devCommands.push_back(d);
				}
				item.clear();
			} else {
				item += all[i];
			}
		}
	}
	_sound = new Sound(_mixer);
	_movies = new MoviePlayer(this);
	if (!_movies->loadTable())
		warning("Cannot read the movie table from mission.___");
	if (ConfMan.hasKey("dev_movie")) {
		// dev_movie=credits runs the end credits instead.
		if (ConfMan.get("dev_movie") == "credits")
			runEndCredits();
		else
			_movies->play(ConfMan.get("dev_movie"));
		return Common::kNoError;
	}

	if (ConfMan.hasKey("dev_scene")) {
		// Dev harness: straight into a 3D scene (from dev_prev_scene, default 0), as a
		// player with view size dev_view_size (default 0).
		if (_players.empty()) {
			PlayerRecord p;
			p.viewSize = ConfMan.hasKey("dev_view_size") ? ConfMan.getInt("dev_view_size") : 0;
			_players.push_back(p);
			_player = 0;
		}
		runWorld(ConfMan.getInt("dev_scene"), ConfMan.hasKey("dev_prev_scene") ? ConfMan.getInt("dev_prev_scene") : 0);
		return Common::kNoError;
	}

	// boot.md "Sequence": players, the player-name screen, resume state, loading, intro.
	loadPlayers();
	checkSessions();
	if (ConfMan.hasKey("save_slot")) {
		// A game chosen in the launcher: slot = player * 100 + object (metaengine.cpp).
		// Like Load3DGame it resumes inside the saved zone (save.md), then the 3D.
		const int slot = ConfMan.getInt("save_slot");
		if (slot / 100 < (int)_players.size() && readGame(slot / 100, slot % 100)) {
			_player = slot / 100;
			_sound->setVolume(_players[_player].volume);
			int code = enterZone(_state.currentZone());
			while (code >= 0 && readGame(code / 100, code % 100))
				code = enterZone(_state.currentZone());
			if (code != -2)
				runWorld(_state.block3D[0x3C], _state.block3D[0x3D], _state.currentZone(), code);
			return Common::kNoError;
		}
	}
	bool known = false;
	// The name is typed: the keymapper must leave Space and Backspace to the text.
	Common::Keymap *keymap = _keymap;
	if (keymap)
		keymap->setEnabled(false);
	const bool entered = runPlayerScreen(_player, known);
	if (keymap)
		keymap->setEnabled(true);
	if (!entered)
		return Common::kNoError;
	if (!known)
		deletePlayerSaves(_player);
	savePlayers();
	_sound->setVolume(_players[_player].volume);

	uint32 in2d = 0;
	_state.clear();
	if (!known || !readResume(_player, in2d)) {
		_state.clear();
		in2d = 0;
	}
	if (ConfMan.hasKey("dev_zone")) {
		// Dev harness: straight into a 2D zone holding the objects of dev_held ("all" or
		// "n,n,..."), then stop.
		Common::StringTokenizer ids(ConfMan.get("dev_held"), ",");
		while (!ids.empty()) {
			const Common::String id = ids.nextToken();
			for (uint o = 0; o < kNumObjects; o++)
				if (id == "all" || (uint)atoi(id.c_str()) == o)
					_state.setHeld(o, 1);
		}
		_state.block3D[0x3E] = ConfMan.getInt("dev_zone");
		int code = enterZone(_state.currentZone());
		while (code >= 0 && readGame(code / 100, code % 100))
			code = enterZone(_state.currentZone()); // a loaded game resumes in its zone
		debug("dev_zone: left with %d", code);
		return Common::kNoError;
	}
	if (!in2d) {
		Graphics::Surface loading;
		if (loadTga("loading", loading)) {
			_screen.copyRectToSurface(loading, 0, 0, Common::Rect(MIN<int>(loading.w, 640), MIN<int>(loading.h, 480)));
			loading.free();
			present();
		}
	}
	_movies->play("intro");

	if (in2d) {
		// boot.md step 9: resume inside the saved zone, then the 3D at the saved spot.
		int code = enterZone(_state.currentZone());
		while (code >= 0 && readGame(code / 100, code % 100))
			code = enterZone(_state.currentZone());
		if (code != -2)
			runWorld(_state.block3D[0x3C], _state.block3D[0x3D], _state.currentZone(), code);
		return Common::kNoError;
	}
	runWorld(_state.block3D[0x3C], _state.block3D[0x3D]);
	return Common::kNoError;
}

void PeintreEngine::runWorld(int scene, int prevScene, int zone, int zoneCode) {
	World world(this);
	_world = &world;
	// However the 3D ends: the 2D side draws its own cursor and has no hotspots.
	struct Leave {
		PeintreEngine *vm;
		~Leave() {
			vm->leave3D();
			vm->_world = nullptr;
		}
	} leave = { this };
	if (zone >= 0) {
		// The 3D after a zone: the saved spot, the zone's bookkeeping (scene.md case C).
		world.resumeFromZone(zone, zoneCode);
	} else if (!world.load(scene, prevScene, false)) {
		return;
	}
	// movement.md "The tick": 66 ms. The game runs in those steps and the original draws
	// one frame per tick; the high_fps option draws at the display's rate, blending the
	// last two ticks.
	const uint32 kTickMs = 66;
	const bool highFps = ConfMan.getBool("high_fps");
	uint32 last = _system->getMillis(), pending = kTickMs;
	uint32 frames = 0, fpsStart = last, renderMs = 0;
	while (!shouldQuit()) {
		pollEvents();
		const uint32 now = _system->getMillis();
		// A long stall (a debugger, a dragged window) is not replayed.
		pending += MIN<uint32>(now - last, 250);
		last = now;
		WorldExit exit = kExitNone;
		bool ticked = false;
		while (pending >= kTickMs && exit == kExitNone && !shouldQuit()) {
			pending -= kTickMs;
			exit = world.tick();
			world.endTick();
			endTick();
			ticked = true;
		}
		if (exit == kExitNone) {
			if (!highFps && !ticked) {
				_system->delayMillis(1);
				continue;
			}
			const uint32 r0 = _system->getMillis();
			world.render(highFps ? (float)pending / kTickMs : 1.0f);
			renderMs += _system->getMillis() - r0;
			// Vsync paces the frames; without it, a frame drawn in under 2 ms rests a little.
			if (_system->getMillis() - r0 < 2)
				_system->delayMillis(1);
			frames++;
			if (now - fpsStart >= 5000) {
				debugC(1, kDebugGraphics, "%u frames per second, %u ms drawing each", frames * 1000 / (now - fpsStart), renderMs / MAX<uint32>(frames, 1));
				frames = 0;
				renderMs = 0;
				fpsStart = now;
			}
			continue;
		}
		// The tick's own frame, then what it asked for (the 2D side draws its own cursor).
		world.render(1.0f);
		leave3D();
		switch (exit) {
		case kExitMovie: {
			const Common::String name = world.movieRequest();
			_movies->play(name);
			// After the end of the game (0x4aba5c) a movie's end leads to the credits
			// (0x42fbd6): cinefin first plays cinefin2 (boot.md "End of the game"); the
			// museum's `fin` (musee.md) goes straight there.
			if (name.equalsIgnoreCase("cinefin"))
				_movies->play("cinefin2");
			if (name.equalsIgnoreCase("cinefin") || name.equalsIgnoreCase("fin")) {
				runEndCredits();
				return;
			}
			world.afterMovie();
			break;
		}
		case kExitZone: {
			const int code = enterZone(world.zoneRequest());
			if (code == -2)
				return;
			int result = code;
			// Load game n: always resumes inside its saved zone (save.md).
			while (result >= 0 && readGame(result / 100, result % 100))
				result = enterZone(_state.currentZone());
			if (result == -2)
				return;
			world.resumeFromZone(_state.currentZone(), result >= 0 ? -1 : result);
			break;
		}
		case kExitOptions: {
			const int code = runOptionMenu(true);
			if (code == -2)
				return;
			world.afterOptions();
			if (code >= 0) {
				int result = code;
				while (result >= 0 && readGame(result / 100, result % 100))
					result = enterZone(_state.currentZone());
				if (result == -2)
					return;
				world.resumeFromZone(_state.currentZone(), result >= 0 ? -1 : result);
			}
			break;
		}
		default:
			break;
		}
		// Back in the 3D: the next tick comes a full tick later, as after the original's wait.
		world.cut();
		pending = 0;
		last = _system->getMillis();
	}
}

void PeintreEngine::leave3D() {
	CursorMan.showMouse(false);
	captureMouse(false);
	if (_system->isOverlayVisible() && _showHotspots)
		_system->hideOverlay();
	_hotspotForceRedraw = true;
}

void PeintreEngine::loadAllScenes() {
	// Dev check: every object of every BFG loads (the RE validators' counts, obj3d.py).
	static const char *const kBundles[] = {
		"auberge", "cafe", "chambreb", "chambrev", "champ", "eglise", "hopiext", "hopiint",
		"jardin", "maisonet", "maisonj", "mangeurs", "musee", "pont", "terrasse"
	};
	uint nodes = 0, polys = 0, textures = 0, anims = 0, boxes = 0, failures = 0;
	for (const char *name : kBundles) {
		Bfg bfg;
		if (!bfg.open(Common::Path(Common::String::format("Scenes_3D/%s.BFG", name)))) {
			warning("dev_load_all: cannot open %s.BFG", name);
			failures++;
			continue;
		}
		for (uint i = 0; i < bfg.entryCount(); i++) {
			Common::Array<byte> data;
			if (!bfg.readEntry(bfg.entryName(i), data) || data.size() < kObjectHeaderSize) {
				failures++;
				continue;
			}
			bool ok = true;
			switch (READ_LE_UINT32(data.data() + 8)) {
			case kObjScene: {
				Scene3D s;
				ok = s.load(data);
				nodes += s.nodes.size();
				for (const Node &n : s.nodes)
					for (const FaceGroup &g : n.faceGroups)
						polys += g.polys.size();
				break;
			}
			case kObjTexture: {
				Texture3D *t = new Texture3D();
				ok = t->load(data);
				delete t;
				textures++;
				break;
			}
			case kObjAnim: {
				Anim3D a;
				ok = a.load(data);
				anims++;
				break;
			}
			case kObjBoxes: {
				Boxes3D b;
				ok = b.load(data);
				boxes++;
				break;
			}
			default:
				ok = false;
			}
			if (!ok) {
				warning("dev_load_all: %s:%s failed", name, bfg.entryName(i).c_str());
				failures++;
			}
		}
	}
	debug("dev_load_all: %u nodes, %u polys, %u textures, %u animations, %u box sets, %u failures",
		  nodes, polys, textures, anims, boxes, failures);
}

} // End of namespace Peintre
