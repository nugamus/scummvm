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

#ifndef PEINTRE_PEINTRE_H
#define PEINTRE_PEINTRE_H

#include "common/algorithm.h"
#include "common/array.h"
#include "common/keyboard.h"
#include "common/rect.h"
#include "common/scummsys.h"
#include "common/str.h"

#include "engines/advancedDetector.h"
#include "engines/engine.h"

#include "graphics/surface.h"

namespace Peintre {

class MoviePlayer;
class Sound;

/** A player record (save.md "USERS.BIN"). */
struct PlayerRecord {
	Common::String name;
	int32 volume = 0;     ///< DirectSound attenuation, 0 = full, -5000 = silent
	uint32 viewSize = 0;  ///< 0..3 = 640x480, 512x384, 400x300, 320x240
};

const uint kMaxPlayers = 5;
const uint kNumObjects = 35;
const uint kNumZones = 25;
const uint k3DBlockSize = 0x36C;
const uint k2DBlockSize = 0x100;

/** Everything a save holds (save.md): the 3D state block and the 2D block. */
struct GameState {
	byte block3D[k3DBlockSize];
	uint32 zoneDone[kNumZones];
	uint32 placed[kNumObjects];
	uint32 counters[4];

	GameState() { clear(); }
	void clear();
	/** The 35 object-held flags live in the 3D block at +0x40. */
	uint32 held(uint object) const;
	void setHeld(uint object, uint32 value);
	byte currentZone() const { return block3D[0x3E]; }
};

class PeintreEngine : public Engine {
public:
	PeintreEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~PeintreEngine() override;

	Common::Error run() override;

	// Screen: the original's 640x480 RGB565 page.
	Graphics::Surface &screen() { return _screen; }
	void present();

	// Input, sampled once per tick (ui.md "Input").
	void pollInput();
	Common::Point mouse() const { return _mouse; }
	bool buttonDown() const { return _button; }
	/** True on the tick a key is released (the original's "fires"). */
	bool keyFired(Common::KeyCode key) const;
	/** True while a key is down (the panorama reads the arrows held, ui.md "Views"). */
	bool keyHeld(Common::KeyCode key) const {
		return Common::find(_keysDown.begin(), _keysDown.end(), key) != _keysDown.end();
	}
	/** Characters typed since the last poll. */
	const Common::String &typed() const { return _typed; }
	/** Waits until `ms` after the previous tick. */
	void waitTick(uint32 ms);
	/**
	 * Dev harness: dev_commands=ms:command[;ms:command...] from the start of run():
	 * click x y, move x y, key <name>, hold <name> ms, type <text>, snap <file.png>, quit.
	 * Input is pushed as events, so movies and every screen see it.
	 */
	void devStep();

	// Players and saves (players.cpp, save.md).
	Common::Array<PlayerRecord> &players() { return _players; }
	uint currentPlayer() const { return _player; }
	GameState &state() { return _state; }
	void loadPlayers();
	void savePlayers();
	/** The integrity check: drops players without a resume file (save.md). */
	void checkSessions();
	void deletePlayerSaves(uint player);
	bool writeGame(uint slot);
	bool writeResume(uint32 in2d);
	bool readGame(uint player, uint slot);
	bool readResume(uint player, uint32 &in2d);
	bool gameExists(uint player, uint slot) const;

	/** The zone's sunflower count when the 2D side left it (passed to 0x42f2c2). */
	uint32 zoneLeaveCount = 0;

	MoviePlayer *movies() { return _movies; }
	Sound *sound() { return _sound; }

	// The 2D side (shell.cpp and zones/, ui.md). Codes as in ui.md "Leaving a zone":
	// -1 back to 3D, -2 quit, -3 back to 3D and start over from the museum, n >= 0 load
	// game n (player * 100 + slot).
	int enterZone(uint zone);
	/** The option menu; from 3D or from a zone. Returns -1 resume, -2 quit, n load. */
	int runOptionMenu(bool from3D);
	/** The credits after the end of the game (ui.md "Credits after the end"). */
	void runEndCredits();

private:
	/** The player-name screen (accueil.cpp). Returns false when the player quits. */
	bool runPlayerScreen(uint &player, bool &known);
	/** The 3D world until the player quits (world.cpp). */
	void runWorld(int scene, int prevScene, int zone = -1, int zoneCode = 0);
	void loadAllScenes();

	const ADGameDescription *_gameDescription;
	Graphics::Surface _screen;
	MoviePlayer *_movies = nullptr;
	Sound *_sound = nullptr;

	Common::Point _mouse;
	bool _button = false;
	Common::Array<Common::KeyCode> _keysDown, _keysFired;
	Common::String _typed;
	uint32 _lastTick = 0;
	struct DevCommand {
		uint32 time;
		Common::String command;
	};
	Common::Array<DevCommand> _devCommands;
	uint32 _devStart = 0;

	Common::Array<PlayerRecord> _players;
	uint _player = 0;
	GameState _state;
};

} // End of namespace Peintre

#endif // PEINTRE_PEINTRE_H
