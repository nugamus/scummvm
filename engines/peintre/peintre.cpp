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
#include "common/system.h"

#include "backends/keymapper/keymap.h"
#include "backends/keymapper/keymapper.h"

#include "engines/util.h"

#include "graphics/cursorman.h"

#include "graphics/pixelformat.h"


#include "peintre/detection.h"
#include "peintre/display.h"
#include "peintre/gfx.h"
#include "peintre/movie.h"
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
			if (Common::find(_keysDown.begin(), _keysDown.end(), event.kbd.keycode) == _keysDown.end())
				_keysDown.push_back(event.kbd.keycode);
			if (event.kbd.keycode == Common::KEYCODE_BACKSPACE)
				_typed += '\b';
			else if (event.kbd.keycode == Common::KEYCODE_LEFT)
				_typed += (char)kTypedLeft;
			else if (event.kbd.keycode == Common::KEYCODE_RIGHT)
				_typed += (char)kTypedRight;
			else if (event.kbd.keycode == Common::KEYCODE_HOME)
				_typed += (char)kTypedHome;
			else if (event.kbd.keycode == Common::KEYCODE_END)
				_typed += (char)kTypedEnd;
			else if (event.kbd.keycode == Common::KEYCODE_DELETE)
				_typed += (char)kTypedDelete;
			else if ((event.kbd.ascii >= 32 && event.kbd.ascii < 127) || (event.kbd.ascii >= 160 && event.kbd.ascii < 256))
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

	_sound = new Sound(_mixer);
	_movies = new MoviePlayer(this);
	if (!_movies->loadTable())
		warning("Cannot read the movie table from mission.___");

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
			// 0x42edef pauses the stream (the static sounds go on behind the menu); 0x42f515
			// resumes it.
			_sound->pauseStream(true);
			const int code = runOptionMenu(true);
			_sound->pauseStream(false);
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
		// The key buffers are cleared on returning from 2D or the option menu (movement.md
		// "Keyboard"): the Escape that closed the menu must not open it again.
		endTick();
		_keysDown.clear();
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

} // End of namespace Peintre
