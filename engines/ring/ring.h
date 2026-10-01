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

#ifndef RING_RING_H
#define RING_RING_H

#include "common/array.h"
#include "common/ptr.h"
#include "common/random.h"
#include "common/rect.h"
#include "common/str.h"

#include "engines/advancedDetector.h"
#include "engines/engine.h"

#include "graphics/managed_surface.h"

#include "ring/rotation.h"

namespace Graphics {
class WinFont;
}

namespace Ring {

class Cursors;
class Resources;
class Sounds;
class World;
struct HotSpot;
struct Movability;
struct Puzzle;
struct Rotation;

/** The drag control, app+0x99 (spec/cursor.md, "Dragging"). */
struct Drag {
	bool active = false;
	int object = 0, value = 0, puzzle = 0;
	bool onPuzzle = true;
	Common::Point press, current;
	int mode = 1; ///< 2: moves count inside `limit` instead of the hot spot
	Common::Rect limit = Common::Rect(0, 16, 640, 464);
	const HotSpot *hotSpot = nullptr;
};

/**
 * The Ring engine (Arxel Tribe, 1999). Behaviour follows the specs in the research
 * repository (engines/ring/docs/spec/): boot.md for the start-up, video.md, drawing.md,
 * resources.md.
 */
class RingEngine : public Engine {
public:
	RingEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~RingEngine() override;

	Common::Error run() override;
	bool hasFeature(EngineFeature f) const override;

	Graphics::ManagedSurface &screen() { return _screen; }
	Resources &resources() { return *_resources; }
	World &world() { return *_world; }
	Cursors &cursors() { return *_cursors; }
	Sounds &sounds() { return *_sounds; }
	/** The zones' rand(). */
	Common::RandomSource &rnd() { return _random; }
	/** The current zone (app+0x6e). */
	int zone() const { return _zone; }
	/** The sound event (0x40ced0, spec/sound.md) to the current zone's handler. */
	void soundEvent(int id, int type, int reason);

	/** `PuzSetAct` (games/ring/docs/sy.md): the puzzle becomes the current one; its sounds start / the old ones stop. */
	void puzSetAct(int puzzle, bool start = true, bool stop = true);
	/** `RotSetAct` (0x4025b0, spec/rotation.md): the rotation becomes the current view. */
	void rotSetAct(int rotation, bool start = true, bool stop = true);
	/** `GoZone` 0x402280: leaves the place, stops the sounds, enters a zone at an entry point. */
	void goZone(int zone, int entry);
	/** `SetZone`: only the current zone changes. */
	void setZone(int zone) { _zone = zone; }
	/** The current rotation, 0 when a puzzle is shown (0x402730). */
	int currentRotation() const { return _mode == 1 ? _rotation : 0; }
	/** `PlyCin` 0x401490: `DATA\<zone>\PLA\<name>.cnm` with sound channel 0 (spec/video.md). */
	void plyCin(const Common::String &name, int channel = 0);
	/** `PlyCinMul` 0x4016a0: effects and dialogues stop, then the video plays with the language's channel. */
	void plyCinMul(const Common::String &name);
	/** `GetLanID` / `GetLanCha` (spec/boot.md, `AddLanguage`). */
	int languageId() const;
	int languageChannel() const;
	/** `TimSta` / `TimSto` / `TimStoAll` / 0x406640 (spec/api.md, "Timers"). */
	void timSta(int id, uint32 ms);
	void timSto(int id);
	void timStoAll() { _timers.clear(); }
	bool timerRunning(int id) const;
	/** `PuzSetMod`: refused (false) when mode 2 is asked of a puzzle already in mode 2. */
	bool puzSetMod(int puzzle, int mode, int object);
	/** `StartMenu` (sy.md, "Flow"). */
	void startMenu(bool fromGame);
	/** What `WM_CLOSE` does: the exit dialogue (spec/boot.md, "Input"). */
	void requestClose();

	/** Copies the screen to the backend and updates it. */
	void present();
	/** Handles pending events, then sleeps `ms`. */
	void pollEvents(uint32 ms = 0);
	/** True while Escape is held (the original polls GetAsyncKeyState(VK_ESCAPE)). */
	bool escapePressed() const { return _escapeDown; }
	/** Waits for `ms` milliseconds or until Escape is held (0x402890). */
	void wait(uint32 ms);

	/** `DisFad` (spec/video.md): fades picture `from` into `to` over `frames` at 25 fps, holds `holdMs`. */
	bool fade(const Common::String &from, const Common::String &to, uint frames, uint32 holdMs);

	const Common::String &languageFolder() const { return _languageFolder; }

	Drag &drag() { return _drag; }

	/** The preferences (`aPreFer`, sy.md "Preferences"): volume, dialogue volume, stereo, subtitles. */
	const int *preferences() const { return _preferences; }
	/** `aPreFer::Save`: stored and written (the game domain's `preferences`, in aPre.ini's format). */
	void savePreferences(int volume, int dialogue, int stereo, int subtitles);

	/** `GetMultiLanMes` (spec/text.md): loads the key's title and text from aMes.ini. */
	void message(const char *key);
	const Common::String &messageTitle() const { return _messageTitle; }
	const Common::String &messageText() const { return _messageText; }

private:
	void showStartupScreens();
	/** aPre.ini's four values: the game domain's `preferences` once saved, else the game's aPre.ini. */
	void loadPreferences();
	void addCursors();
	/** 0x40b650: the current place is left. */
	void leavePlace();
	/** One idle-loop frame (spec/boot.md, "Frame"). */
	void frame();
	/** Hot-spot tracking, 0x408dd0 (spec/cursor.md); a drag replaces the cursor last. */
	void track(int x, int y);
	/** The tracking search; returns the hot spot under the mouse, if any. */
	const HotSpot *trackHit(int x, int y);
	/** Left button down (0x409630): button-down events and drag starts (spec/cursor.md, "Dragging"). */
	void buttonDown(int x, int y);
	/** 0x409520, every frame while the button is down: the drag follows the mouse. */
	void dragMove(int x, int y);
	/** `MouseLeftEvent` on release (spec/cursor.md). */
	void click(int x, int y);
	/** The drag event (0x40c060) to the zone's handler. */
	void dragEvent(int phase);
	/** Through movability `index` of the current rotation or puzzle (spec/rotation.md). */
	void move(const Movability &m, int index);
	/** 0x4101c0: an animated turn of the current rotation, one step per frame. */
	void turn(Rotation &r, float alpha, float beta, float ran);
	/** Draws the current rotation (or puzzle) and puzzle 1, without the cursor. */
	void drawView();
	/** The puzzle's animations advance (their events) before it is drawn (spec/animation.md). */
	void advanceAnimations(Puzzle &p);
	/** 0x410610's layer part: animations advance (their events), layers follow, patches apply. */
	void updateLayers(Rotation &r);
	/** `WM_TIMER`: due timers go to the zone's handler (0x40b4a0). */
	void runTimers();
	/** A key (0x40b060): clicks the hot spot that has it (spec/events.md, "Keys"). */
	void key(int code);

	const ADGameDescription *_gameDescription;
	Graphics::ManagedSurface _screen;
	Common::ScopedPtr<Resources> _resources;
	Common::ScopedPtr<World> _world;
	Common::ScopedPtr<Cursors> _cursors;
	Common::ScopedPtr<Sounds> _sounds;
	Common::ScopedPtr<Graphics::WinFont> _font;
	Common::String _messageTitle, _messageText;
	int _zone = 1;
	int _menuZone = 0;   ///< app+0x6f: the zone the menu returns to, 0 when the menu is down
	int _puzzle = 0;     ///< the current puzzle (app+0x81)
	int _rotation = 0;   ///< the current rotation (app+0x89)
	int _mode = 2;       ///< 1 rotation, 2 puzzle (0x40b7c0)
	RotationView _view;
	uint32 _panTime = 0; ///< looking around advances once per 1/60 s (Q-0011)
	Common::Point _mouse;
	struct Button {
		bool down;
		Common::Point pos;
	};
	Common::Array<Button> _buttons; ///< left button presses and releases not handled yet
	bool _buttonDown = false;
	Drag _drag;
	int _preferences[4] = { 100, 100, -1, 1 };
	Common::Array<int> _keys; ///< key codes not handled yet
	Common::String _languageFolder;
	bool _escapeDown = false;
	bool _scripted = false; ///< dev_input drives the mouse
	struct Timer {
		int id;
		uint32 period, due;
	};
	Common::Array<Timer> _timers;
	Common::RandomSource _random{ "ring" };
};

} // End of namespace Ring

#endif // RING_RING_H
