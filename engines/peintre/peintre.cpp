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

#include "engines/util.h"

#include "graphics/pixelformat.h"

#include "peintre/bfg.h"
#include "peintre/gfx.h"
#include "peintre/movie.h"
#include "peintre/obj3d.h"
#include "peintre/peintre.h"
#include "peintre/sound.h"

namespace Peintre {

PeintreEngine::PeintreEngine(OSystem *syst, const ADGameDescription *gameDesc)
	: Engine(syst), _gameDescription(gameDesc) {
	// The detector's directory globs (Data, Scenes_3D) are already in SearchMan.
}

PeintreEngine::~PeintreEngine() {
	delete _movies;
	delete _sound;
	_screen.free();
}

void PeintreEngine::present() {
	_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, _screen.w, _screen.h);
	_system->updateScreen();
}

void PeintreEngine::pollInput() {
	_keysFired.clear();
	_typed.clear();
	Common::Event event;
	while (_eventMan->pollEvent(event)) {
		switch (event.type) {
		case Common::EVENT_MOUSEMOVE:
			_mouse = event.mouse;
			break;
		case Common::EVENT_LBUTTONDOWN:
			_mouse = event.mouse;
			_button = true;
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
			else if (event.kbd.ascii >= 32 && event.kbd.ascii < 256)
				_typed += (char)event.kbd.ascii;
			break;
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
	// The original draws on a 640x480 16-bit surface (boot.md step 5).
	const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
	initGraphics(640, 480, &format);
	_screen.create(640, 480, format);

	if (ConfMan.getBool("dev_load_all"))
		loadAllScenes();

	_sound = new Sound(_mixer);
	_movies = new MoviePlayer(this);
	if (!_movies->loadTable())
		warning("Cannot read the movie table from mission.___");
	if (ConfMan.hasKey("dev_movie")) {
		_movies->play(ConfMan.get("dev_movie"));
		return Common::kNoError;
	}

	// boot.md "Sequence": players, the player-name screen, resume state, loading, intro.
	loadPlayers();
	checkSessions();
	bool known = false;
	if (!runPlayerScreen(_player, known))
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

	// Not specced into the engine yet: the 3D world and the 2D zones.
	while (!shouldQuit()) {
		pollInput();
		present();
		waitTick(40);
	}
	return Common::kNoError;
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
