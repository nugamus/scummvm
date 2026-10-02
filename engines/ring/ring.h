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
#include "common/hashmap.h"
#include "common/hash-str.h"
#include "common/ptr.h"
#include "common/random.h"
#include "common/rect.h"
#include "common/str.h"

#include "engines/advancedDetector.h"
#include "engines/engine.h"

#include "graphics/managed_surface.h"

#include "ring/rotation.h"

namespace Common {
class Serializer;
}

namespace Graphics {
class WinFont;
}

namespace Ring {

/** The keymapper's engine actions (metaengine.cpp). */
enum Action {
	kActionSkip = 1, ///< Escape
	kActionMenu      ///< F12
};

class Bag;
struct Image;
class Cursors;
class Resources;
class Sounds;
class World;
struct HotSpot;
struct Movability;
struct Animation;
struct Puzzle;
struct Rotation;

/** The drag control, app+0x99 (spec/cursor.md, "Dragging"). */
struct Drag {
	bool active = false;
	int object = 0, value = 0, puzzle = 0;
	bool onPuzzle = true;
	Common::Point press, current;
	Common::Point previous;  ///< the position before the last move (0x426140)
	Common::Point reference; ///< the press position, or a point a handler sets (0x4066b0)
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
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override;
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override;
	Common::Error saveGameStream(Common::WriteStream *stream, bool isAutosave = false) override;
	Common::Error loadGameStream(Common::SeekableReadStream *stream) override;
	/** In play: the game and the screen taken now, the description line built. */
	Common::Error saveGameState(int slot, const Common::String &desc, bool isAutosave = false) override;

	/** `StartMenu(1)`'s save (sy.md, "Flow"): the game and the screen as they are, kept for continue and the save screen. */
	void snapshot();
	/** The save screen's description, "<character>  <time>   <date>" (E-0258). */
	Common::String describeSave() const;
	/** The kept screen as the save screen's picture, 260 x 480 (E-0265); nullptr without one. */
	Image *savePicture() const;
	/** The save screen's OK: the kept game in the first free slot with its two lines and picture. */
	bool saveToFreeSlot(const Common::String &description, const Common::String &name);
	/** Continue: the kept game loaded again; false without one. */
	bool continueGame();
	/** A slot's description line, typed name and (when asked) picture, for the load screen. */
	bool readSave(int slot, Common::String &description, Common::String &name, Image **picture);
	/** The slots holding saves, ascending. */
	Common::Array<int> saveSlots() const;
	void deleteSave(int slot);
	const Graphics::ManagedSurface &snapScreen() const { return _snapScreen; }
	Graphics::WinFont *font() const { return _font.get(); }

	Graphics::ManagedSurface &screen() { return _screen; }
	Resources &resources() { return *_resources; }
	World &world() { return *_world; }
	Cursors &cursors() { return *_cursors; }
	Sounds &sounds() { return *_sounds; }
	Bag &bag() { return *_bag; }
	/** An object goes in hand with its cursors (0x40b860, spec/bag.md). */
	void holdObject(int object);
	/** 0x406570: the object in hand is dropped, its cursors go. */
	void dropObject();
	/** The right button (0x40afe0): the bag opens or closes. */
	void toggleBag();
	/** 0x419350 and 0x40ded0: the bag closes and the rotation it froze may be looked around again. */
	void hideBag();
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
	/** 0x40f6c0: one frame drawn and the sound ends checked, from inside a handler. */
	void renderFrame();
	/** Frames drawn for `ms` milliseconds (the handlers' `GetTickCount` loops). */
	void renderFor(uint32 ms);
	/** `RotSetRolTo` 0x405c20: the animated turn of the rotation (spec/rotation.md). */
	void rotSetRolTo(int rotation, float alpha, float beta, float ran);
	/** `PuzSet3DSouOn` / `RotSet3DSouOn` / `...Off` and the ambient ones: the item, started or stopped at once in the current place. */
	void setSoundItem(int owner, int sound, bool on);
	/** `PuzSet3DSouVol` (0x404d70) and the like: the item's volume, applied at once when it plays. */
	void setSoundItemVolume(int owner, int sound, int volume);
	/** 0x408db0: game over `n` (mode 4); the next frame shows End.bmp and opens the menu (games/ring/docs/ni.md). */
	void gameOver(int n);
	/** 0x431350: the credits, `cre_01.bma` .. `cre_11.bma` scrolled (games/ring/docs/sy.md). */
	void credits();
	/** `ScrollImage` 0x401260: 1 after a full scroll and `holdMs`, 2 when Escape ended it. */
	int scrollImage(const Common::String &name, uint32 holdMs);
	/** `SetCursorPos`. */
	void setMouse(int x, int y);
	/** Clears app+0x74 from a click handler: the clicked object (flag 8) does not go in hand (spec/bag.md "Taking"). */
	void keepHand() { _takeAllowed = false; }
	/** Clears app+0x78 from the bag-click event: the object picked in the bag is not kept in hand (spec/bag.md). */
	void keepBagObject() { _listAllowed = false; }
	/** The current puzzle when one is current, else the current rotation. */
	int currentPlace() const { return _mode == 2 ? _puzzle : _rotation; }
	/** `RotGetAlp` 0x405ab0: the stored alpha + 135, less 360 above 360. */
	float rotGetAlp(int rotation);
	Common::Point mouse() const { return _mouse; }
	/**
	 * `LoadSaveTimer(file, mode)` (spec/bag.md, Erda): the timers and the bag of a world
	 * left through Erda, kept by name (`alb`, `sie`, `log`, `bru`) until it is resumed.
	 */
	void saveWorldState(const Common::String &file);
	bool loadWorldState(const Common::String &file);
	/** 0x408bc0, 0x431040: every zone set-up runs again from scratch (a new game, a game over). */
	void resetWorld();
	/** `GoZone` 0x402280: leaves the place, stops the sounds, enters a zone at an entry point. */
	void goZone(int zone, int entry);
	/** `SetZone` 0x402210: the current zone changes; Erda's button is offered outside SY and AS. */
	void setZone(int zone);
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
	/** An accessibility clicked: the object-click event (flag bit 0), then taking it (bit 3). */
	void clickObject(int zone, int object, int value, int place);
	/** Erda's button in the bag: the world is left for the hub, where the player was kept (spec/bag.md). */
	void erda();
	/** The drag event (0x40c060) to the zone's handler. */
	void dragEvent(int phase);
	/** Through movability `index` of the current rotation or puzzle (spec/rotation.md). */
	void move(const Movability &m, int index);
	/** 0x4101c0: an animated turn of the current rotation, one step per frame. */
	void turn(Rotation &r, float alpha, float beta, float ran);
	/** Draws the current rotation (or puzzle) and puzzle 1, without the cursor. */
	void drawView();
	/** Saved games (save.cpp, spec/save.md). */
	void syncGame(Common::Serializer &s);
	/** A loaded game applied in the main loop: the set-ups again, the records, entry 1000. */
	void applyLoad();
	/** Raises a hold-on-frame event an animation left (0x40c910). */
	void holdEvent(Animation &anim);
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
	Common::ScopedPtr<Bag> _bag;
	int _bagRotation = 0;      ///< 0x495244: the rotation the open bag froze
	bool _bagWasFrozen = false; ///< 0x495570: its +0x67 before
	Common::ScopedPtr<Graphics::WinFont> _font;
	Common::String _messageTitle, _messageText;
	int _zone = 1;
	int _menuZone = 0;   ///< app+0x6f: the zone the menu returns to, 0 when the menu is down
	int _puzzle = 0;     ///< the current puzzle (app+0x81)
	int _rotation = 0;   ///< the current rotation (app+0x89)
	int _mode = 2;       ///< 1 rotation, 2 puzzle (0x40b7c0)
	RotationView _view;
	uint32 _panTime = 0; ///< looking around advances once per 1/60 s (Q-0011)
	uint32 _lastRotationFrame = 0; ///< 0x4a17b8
	Common::Point _mouse;
	struct Button {
		bool down;
		Common::Point pos;
		bool right;
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
	int _gameOver = 0; ///< app+0x70 while mode 4 is pending
	bool _takeAllowed = true; ///< app+0x74
	bool _listAllowed = true; ///< app+0x78
	Common::Array<byte> _pendingLoad;
	Common::Array<byte> _snapGame;        ///< the game as F12 (or a save in play) left it
	Graphics::ManagedSurface _snapScreen; ///< the screen then (0x49556c)
	Common::String _saveDescription, _saveName; ///< Windows-1252, as typed
	Common::Array<Common::Pair<int, bool> > _playingOnLoad;
	struct WorldState {
		Common::Array<int> bag;
		Common::Array<Timer> timers;
		uint32 tick = 0;
	};
	Common::HashMap<Common::String, WorldState> _worldStates;
	Common::RandomSource _random{ "ring" };
};

/** The running engine, for the zone code (ring/zone.h). */
extern RingEngine *g_engine;

} // End of namespace Ring

#endif // RING_RING_H
