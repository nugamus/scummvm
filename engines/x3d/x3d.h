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

#ifndef X3D_X3D_H
#define X3D_X3D_H

#include "common/array.h"
#include "common/events.h"
#include "common/hashmap.h"
#include "common/rect.h"
#include "common/scummsys.h"
#include "common/str.h"
#include "engines/engine.h"
#include "graphics/hotspot_renderer.h"

#include "x3d/detection.h"
#include "x3d/player.h"
#include "x3d/scene.h"

namespace Graphics {
struct Surface;
}

namespace Video {
class VideoDecoder;
}

namespace X3D {

class Collision;
class Interaction;
class Inventory;
class Frame;
class Renderer;
class Scene;
class Sound;
class Talk;
class Unit;

// Keymapper actions (metaengine.cpp): the original's keys, the strafes of the modern
// controls, and the hotspot overlay
enum Action {
	kActionNone,
	kActionForward, kActionBackward, kActionTurnLeft, kActionTurnRight, kActionLookUp,
	kActionLookDown, kActionRun, kActionJump, kActionInventory, kActionMenu, kActionSkip, kActionCrouch,
	kActionStrafeLeft, kActionStrafeRight, kActionToggleHotspots,
	kActionSaveMenu, kActionVolumeUp, kActionVolumeDown
};

class X3DEngine : public Engine {
public:
	X3DEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~X3DEngine() override;

	bool hasFeature(EngineFeature f) const override {
		return f == kSupportsReturnToLauncher || f == kSupportsLoadingDuringRuntime ||
		       f == kSupportsSavingDuringRuntime ||
		       (f == kSupportsArbitraryResolutions && _nativeResolution);
	}

	// Saves: the scene's state as the original stores it, in ScummVM's save files; a load
	// switches to the saved scene and restores it there
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override;
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override;
	Common::Error saveGameStream(Common::WriteStream *stream, bool isAutosave = false) override;
	Common::Error saveGameState(int slot, const Common::String &desc, bool isAutosave = false) override;
	Common::Error loadGameStream(Common::SeekableReadStream *stream) override;
	Common::Error loadGameState(int slot) override;
	// A game over: the load screen, else the Option menu
	void gameOver();
	// The scene gauge: a bar that empties over ms while visible; expired() is
	// true once, when it runs out, and stops it
	struct Gauge {
		uint32 ms = 0, start = 0;
		bool visible = false;
		Common::String label; // U04 runs four timers on it: Door, Paint, ecroule, PlusVite
	};
	void startGauge(uint32 ms, bool visible = true, const Common::String &label = "");
	void stopGauge() { _gauge.ms = 0; }
	bool gaugeRunning() const { return _gauge.ms != 0; }
	bool gaugeOver() const { return _gauge.ms && _logicMs - _gauge.start >= _gauge.ms; }
	const Common::String &gaugeLabel() const { return _gauge.label; }
	const Gauge &gauge() const { return _gauge; }
	void setGauge(const Gauge &g) { _gauge = g; }
	bool gaugeExpired();
	void syncGauge(Common::Serializer &s);
	void storeHeldItem(); // a held item back into the bar
	// Every event as the engine takes it: mouse positions in the logical frame, a resized
	// window resizes the frame, and the hotspot overlay toggle is handled (and dropped)
	void processEvent(Common::Event &e);
	// The game's text for a Message.txt line, or an English fallback for the few the engine uses
	Common::String message(uint id) const;
	// Autosaves into the autosave slot, named as the original names its automatic save
	void autosave();
	Unit *createUnit(const Common::String &sceneName); // the scene's unit code, or nullptr
	// The last view redrawn at thumbnail size (save thumbnails in 3D mode), or nullptr
	Graphics::Surface *thumbnail(int width, int height);
	// ScummVM's hotspot overlay (an enhancement): in free play, the hotspots a click would
	// reach, at a point of each that the pick finds
	void drawHotspots() override;
	void getHotspotPositions(Common::Array<Graphics::HotspotInfo> &hotspots) override;
	bool hotspotDirty() const override;
	void findHotspots(); // the markers and the objects to outline
	void refreshHotspots(); // findHotspots when due or when what is clickable changed
	void placeMarkers(); // their positions in the drawn view
	// The object's first surface point on screen that a pick takes to hotspot target (hit),
	// else its first on screen; false when none is. tries: surface points picked at most
	bool aimAt(const Scene::Model *model, uint object, int target, Common::Point &point, bool &hit, uint tries = 1000,
	           Math::Vector3d *world = nullptr); // world: the point hit
	bool toScreen(const Math::Vector3d &p, Common::Point &s) const; // the pick's projection
	bool picks(const Common::Point &s, int target); // a click at s reaches hotspot target

	// Script primitives for unit code (scripted camera moves). They run frames until done, like the original's blocking loops: animation, sound
	// and rendering go on, the player's camera input does not.
	// 0: one frame. walk: the camera keys and the unit's input hook stay on (the original's
	// "RunFor + generic input" waits), clicks and Escape do not.
	void runFor(uint32 ms, bool walk = false);
	// One frame with full input, clicks included (U04 waits for the glove to be taken)
	// Escape and F5 are ignored in it
	void frameWithInput() { _escapeBlocked = true; frame(true); _escapeBlocked = false; }
	// Moves the camera over ms; nullptr keeps the position, kKeep keeps an angle / the FOV
	void moveTo(uint32 ms, const float *position, float yaw, float pitch, float fov = kKeep);
	void lookAt(uint32 ms, const Math::Vector3d &target);
	void setView(const float *position, float yaw, float pitch); // a cut, no interpolation
	bool enterHeld() const { return _enterHeld; }
	uint32 logicMs() const { return _logicMs; } // logic time in the scene (ms)
	// Suspend: no cursor, camera keys or clicks
	void suspend(bool suspended);
	void startSkip();
	void endSkip();
	/** This step's camera belongs to a ride (the train, the boat): mouse look waits (enhancement). */
	void rideView() { _rideView = true; }
	bool suspended() const { return _suspended; }
	// The unit's start runs for a load: the saved state is already in place
	bool restoring() const { return _restoring; }
	// A pending load keeps its scene: a sequence that goes on after it cannot replace it
	void gotoScene(const Common::String &name) {
		if (_pendingLoad.empty())
			_nextScene = name;
	}
	bool sceneChanging() const { return !_nextScene.empty(); }
	void addUnitAction(const Common::String &name) { _unitActions.push_back(name); }
	// Video/<name>.avi with its soundtrack Video/<wav>.wav (the video's name when empty).
	// action: an INFOACT action run once the video starts (U33's film and its speech);
	// then only Enter ends it, stopping the voice, and sounds already playing go on.
	// keepSounds: sounds already playing go on (U33's films)
	void playVideo(const Common::String &name, const Common::String &wav = "", uint32 action = 0, bool keepSounds = false);
	void fadeToBlack(uint32 ms); // the ambient light down to 0 over ms

	// A frame's list view: its rows, the selected one, and the names a
	// row click copies into the edit (players list)
	struct MenuList {
		Common::StringArray rows, names;
		int selected = -1;
	};
	// Shows a frame until a command is chosen; returns the command, "escape", or "enter".
	// The scene, if any, stays frozen underneath. text: the edit's first text, the caret at
	// its end.
	Common::String runMenu(const Common::String &name, MenuList *list = nullptr, const Common::String &text = "");
	// The OptionSave and OptionLoad screens over ScummVM's save slots; load:
	// true when a game was loaded
	void saveMenu();
	// A frame already loaded, until a command; timeout (ms) returns "timeout", a key on a
	// frame without an edit "key"
	Common::String runFrame(Frame &frame, MenuList *list = nullptr, uint32 timeout = 0);
	void showPainting(const Common::String &name); // TableauJeu
	void credits();
	void settings();
	void gallery();
	bool paintingScreens(uint index, uint count);
	void magnifier(const Common::String &painting);
	void returnToPainting(const Common::String &painting); // Escape in the 3D view
	int playerUnit() const; // the unit of the player's last save (for the gallery)
	bool loadMenu();
	// Players (SelectUser): true when the name is new, which is then added
	bool selectPlayer(const Common::String &name);
	Common::StringArray players(Common::String *current = nullptr) const; // current: the last selected
	void readPlayers(Common::StringArray &names, Common::Array<int> &units, Common::String *current = nullptr) const;
	void writePlayers(const Common::StringArray &names, const Common::Array<int> &units);
	// The Option menu: its chosen command (OptionNouvelleP, OptionEntrenement), or
	// empty when quitting; afterOptionMenu goes where it leads
	Common::String optionMenu();
	void afterOptionMenu(const Common::String &command);
	const Common::String &menuText() const { return _menuText; }

	// A debugger command (console.h): where, goto, lookat, click, hotspots, give, hold, pos, act,
	// save <slot>, load <slot>, savemenu, loadmenu, page <credits|settings|gallery|loupe p|painting>, exhaust <id>, view3d <painting>
	Common::String command(const Common::String &line);
	void queueCommand(const Common::String &line) { _queuedCommands.push_back(line); } // runs on the next frame

	static constexpr float kKeep = 100.0f;
	bool u02Warned = false; // U02's gauge warning, said once per process

	// The running scene
	Scene *scene() { return _scene; }
	Player &player() { return _player; }
	Collision *collision() { return _collision; }
	Interaction *interaction() { return _interaction; }
	Inventory *inventory() { return _inventory; }
	Talk *talk() { return _talk; }
	Sound *sound() { return _sound; }
	Renderer *renderer() { return _renderer; }
	const Keys &keys() const { return _keys; }

	// Logic steps per second. The original ran one step per rendered frame; its frame rate
	// is not known, so this rate is an estimate, and it sets the turn speed.
	static const uint kStepsPerSecond = 60;

protected:
	Common::Error run() override;
	void pauseEngineIntern(bool pause) override;

private:
	void playScene(const Common::String &sceneName);
	// One pass of the main loop: events, the logic steps due, hover/click, rendering
	void frame(bool input);
	bool pollInput(bool input, const Common::Point &centre); // this frame's events; true when the captured mouse moved
	void updatePointer(const Camera &camera, uint32 now, bool input); // cursor, hover and click
	void drawFrame(const Camera &camera, float alpha);
	void handleEscape(); // the Escape menu over the frozen scene
	void runUnitCode(); // queued click actions and the unit's per-frame checks
	void logicStep(bool input);
	Camera viewCamera(float alpha) const; // the player's view, alpha of the way from the last step
	void showBitmap(const Common::Path &path);
	void wait(uint32 ms);

	const ADGameDescription *_gameDescription;
	Renderer *_renderer = nullptr;
	Sound *_sound = nullptr;

	// Scene state, valid inside playScene
	Scene *_scene = nullptr;
	Collision *_collision = nullptr;
	Interaction *_interaction = nullptr;
	Inventory *_inventory = nullptr;
	Talk *_talk = nullptr;
	Unit *_unit = nullptr;
	bool _nativeResolution = false; // the renderer draws at the window's size
	bool _inGameOver = false;       // no saves until the next scene or load
	Video::VideoDecoder *_video = nullptr; // the video playing, if any
	bool _practice = false; // Practice was chosen: U00 without the players screen
	Player _player, _previous;
	Keys _keys;
	bool _enterHeld = false, _suspended = false;
	Common::Point _mouse = Common::Point(320, 240);
	bool _hoverNow = false, _clickNow = false;
	int _hotspot = -1;
	uint32 _last = 0, _pending = 0, _logicMs = 0, _lastClick = 0, _frames = 0, _fpsStart = 0;
	Common::StringArray _queuedCommands; // console commands for the next frame
	bool _restoring = false;
	Common::String _nextScene, _sceneName;
	Common::Array<byte> _pendingLoad; // a save's scene state, restored by playScene
	uint32 _pendingVersion = 1;
	Common::StringArray _unitActions;
	Common::String _clickedHotspot; // the hotspot of this frame's click, for Unit::afterClick
	Camera _camera; // the last one drawn, for frames over a frozen scene
	bool _freePlay = false; // the last frame took clicks
	float _fovExtra = 0; // the fov option's degrees beyond the original's 90
	Camera _hotspotCamera; // the view of the last hotspot overlay
	uint32 _hotspotTime = 0;
	Common::Array<bool> _clickableNow; // per hotspot, at the last findHotspots (and free play last)
	// A marker: a point of the hotspot's object a click reaches, as an offset from its
	// origin. The object itself, not its name: names repeat (U02 has two *U02_02).
	struct Marker {
		int hotspot = -1;
		const Scene::Model *model = nullptr;
		uint object = 0;
		Math::Vector3d offset;
		Common::U32String label;
	};
	Common::Array<Marker> _markers;
	Common::Array<Graphics::HotspotInfo> _hotspots; // the markers placed, in window pixels
	Common::Array<Scene::Highlight> _highlight; // their objects, outlined
	Common::String _menuText; // the text edit of the last menu
	bool _escapeNow = false, _escapeBlocked = false;
	bool _posedBetween = false; // the last render posed the scene between two steps
	int _frameDepth = 0; // frames nested in blocking sequences: no saving or loading there
	bool _walk = false; // runFor with walking input
	bool _rideView = false;
	bool _skipping = false, _skipMutedSfx = false; // a scripted sequence being skipped
	Gauge _gauge;
	int _menuView = -1;  // the view index of the last frame click
	Common::String _playerName;
	mutable Common::HashMap<uint, Common::String> _messages; // Message.txt, loaded on first use
	mutable bool _messagesLoaded = false;
	Common::String _gallery3D; // the painting whose 3D scene the next scene is

	// Modern controls (an option, not in the original): in free play the mouse is captured,
	// turns the view, and clicks and hovers act at the centre of the screen
	bool _modern = false, _invertY = false, _mouseCaptured = false, _cursorHidden = false;
	bool _highFps = false; // the high_fps option: frames between steps
	float _lookScale = 0.0025f; // radians per mouse count, times mouse_sensitivity / 100
	uint32 _captureStart = 0; // mouse motion right after a capture is dropped
	const char *keymapName() const { return _modern ? "x3d-modern" : "x3d-default"; }
	void captureMouse(bool capture);
};

} // End of namespace X3D

#endif // X3D_X3D_H
