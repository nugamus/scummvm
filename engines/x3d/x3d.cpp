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
#include "common/memstream.h"
#include "common/ptr.h"
#include "common/savefile.h"
#include "common/serializer.h"
#include "common/util.h"
#include "common/debug.h"
#include "common/events.h"
#include "common/file.h"
#include "common/system.h"
#include "common/tokenizer.h"
#include "common/translation.h"

#include "backends/keymapper/keymap.h"
#include "backends/keymapper/keymapper.h"

#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "audio/mixer.h"

#include "engines/metaengine.h"
#include "engines/util.h"

#include "graphics/cursorman.h"
#include "graphics/hotspot_renderer.h"
#include "graphics/surface.h"

#include "gui/message.h"

#include "video/avi_decoder.h"

#include "x3d/detection.h"
#include "x3d/collision.h"
#include "x3d/console.h"
#include "x3d/frame.h"
#include "x3d/monet/gallery3d.h"
#include "x3d/interaction.h"
#include "x3d/inventory.h"
#include "x3d/player.h"
#include "x3d/renderer.h"
#include "x3d/scene.h"
#include "x3d/sound.h"
#include "x3d/talk.h"
#include "x3d/x3d.h"

namespace X3D {

X3DEngine::X3DEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc) {
}

X3DEngine::~X3DEngine() {
	if (_skipping)
		endSkip(); // the mixer's effects are muted while a sequence is skipped
	delete _inventory;
	delete _sound;
	delete _renderer;
}

Common::Error X3DEngine::run() {
	// All original paths are relative to Data/, which the detector (kADFlagMatchFullPaths)
	// has already added to SearchMan
	// The original frames every mode as 4:3; widescreen keeps its height and view and
	// shows more to the sides. 2D images stay 640x480, centred.
	ConfMan.registerDefault("high_fps", false);
	ConfMan.registerDefault("high_res", false);
	_highFps = ConfMan.getBool("high_fps");
	ConfMan.registerDefault("widescreen", false);
	ConfMan.registerDefault("max_detail", false);
	ConfMan.registerDefault("filter_textures", false);
	ConfMan.registerDefault("run_toggle", false);
	ConfMan.registerDefault("crouch_toggle", false);
	ConfMan.registerDefault("turn_speed", 100);
	ConfMan.registerDefault("fov", 90);
	// Modern controls; mouse_sensitivity is a percent of the default look speed
	ConfMan.registerDefault("modern_controls", false);
	ConfMan.registerDefault("invert_y", false);
	ConfMan.registerDefault("mouse_sensitivity", 100);
	_modern = ConfMan.getBool("modern_controls");
	_invertY = ConfMan.getBool("invert_y");
	_lookScale *= ConfMan.getInt("mouse_sensitivity") / 100.0f;
	Common::Keymapper *keymapper = _system->getEventManager()->getKeymapper();
	keymapper->getKeymap("x3d-default")->setEnabled(!_modern);
	keymapper->getKeymap("x3d-modern")->setEnabled(_modern);
	_renderer = Renderer::create(ConfMan.getBool("widescreen") ? 854 : 640, 480, _nativeResolution);
	_renderer->updateSize(ConfMan.getBool("widescreen"));
	_renderer->setFilterTextures(ConfMan.getBool("filter_textures"));
	_fovExtra = ConfMan.getInt("fov") - 90;
	_sound = new Sound(_mixer);
	_inventory = new Inventory();
	setDebugger(new Console(this));

	if (!ConfMan.hasKey("save_slot")) {
		// Boot sequence: the two intro pictures
		showBitmap("2dbit/Intro1.bmp");
		wait(3000);
		showBitmap("2dbit/Intro2.bmp");
		wait(2000);
	}

	// The cursors are in the game's EXE, which a copy of Data/ alone lacks
	if (!Common::File::exists("MissionMonet.exe") && !Common::File::exists("INSTALL/02_PR/MissionMonet.exe")) {
		GUI::MessageDialog dialog(_("MissionMonet.exe was not found, so the game's own mouse cursors are missing. "
		                            "Copy the CD's INSTALL folder (or INSTALL/02_PR/MissionMonet.exe) next to the Data folder."));
		dialog.runModal();
	}

	// U00 shows the players screen and runs Monet's tutorial; the Option menu's New game
	// then goes to App.bin's start scene
	Common::String sceneName = "U00.X3D";
	// A game chosen in the launcher's load dialog
	if (ConfMan.hasKey("save_slot"))
		loadGameState(ConfMan.getInt("save_slot"));
	// A load (that one, or one from the global menu during the intro) names the scene
	if (!_nextScene.empty())
		sceneName = _nextScene;

	while (!shouldQuit() && !sceneName.empty()) {
		_nextScene.clear();
		playScene(sceneName);
		sceneName = _nextScene;
	}
	return Common::kNoError;
}

void X3DEngine::processEvent(Common::Event &e) {
	if (Common::isMouseEvent(e))
		e.mouse = _renderer->toLogical(e.mouse);
	if (e.type == Common::EVENT_SCREEN_CHANGED)
		_renderer->updateSize(ConfMan.getBool("widescreen"));
	// ScummVM's hotspot overlay, as a toggle instead of while held; the cursor stays as
	// it was (the base class hides it while the overlay shows)
	if ((e.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START || e.type == Common::EVENT_CUSTOM_ENGINE_ACTION_END) &&
	    e.customType == kActionToggleHotspots) {
		if (e.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START && ConfMan.getBool("enable_hotspots")) {
			const bool cursor = CursorMan.isVisible();
			showHotspots(!_showHotspots);
			CursorMan.showMouse(cursor);
		}
		e.type = Common::EVENT_INVALID;
	}
}

Common::String X3DEngine::message(uint id) const {
	// Message.txt: lines "<id> <text>;", beside the game's EXE (INSTALL/02_PR on the CD)
	if (!_messagesLoaded) {
		_messagesLoaded = true;
		Common::File file;
		if (file.open("Message.txt") || file.open("INSTALL/02_PR/Message.txt")) {
			while (!file.eos() && !file.err()) {
				const Common::String line = file.readLine();
				const char *text = strchr(line.c_str(), ' ');
				if (!text || !Common::isDigit(line[0]))
					continue;
				const char *end = strchr(++text, ';');
				_messages[atoi(line.c_str())] = end ? Common::String(text, end) : Common::String(text);
			}
		}
	}
	if (_messages.contains(id))
		return _messages[id];
	switch (id) {
	case 1:
		return "Empty";
	case 300:
		return "Save without name";
	case 301:
		return "Player's name";
	case 1003:
		return "Automatic save";
	default:
		return "";
	}
}

void X3DEngine::autosave() {
	saveGameState(getAutosaveSlot(), message(1003), true);
}

Common::Error X3DEngine::loadGameState(int slot) {
	// The base class autosaves before every load, into the autosave slot: loading that
	// slot would first overwrite it with the current position
	if (slot != getAutosaveSlot())
		return Engine::loadGameState(slot);
	Common::InSaveFile *file = _saveFileMan->openForLoading(getSaveStateName(slot));
	if (!file)
		return Common::kReadingFailed;
	Common::Error result = loadGameStream(file);
	ExtendedSavegameHeader header;
	if (result.getCode() == Common::kNoError && MetaEngine::readSavegameHeader(file, &header))
		setTotalPlayTime(header.playtime);
	delete file;
	return result;
}

static const uint32 kSaveVersion = 6; // 2: numbered clip slots; 3: the scene gauge; 4: U01 on it; 5: U05/U33 clocks; 6: more unit state

bool X3DEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	// Only between the unit's sequences: a save inside one could not replay its end. Not
	// in the air or during a game over either: ScummVM autosaves before every load, and a
	// save of the fall would replay the death on each load
	const bool can = _scene && _unit && _unit->gameStarted() && !_suspended && _frameDepth <= 1 &&
	                 !_inGameOver && !_player.falling() && !_player.jumping();
	if (!can && msg)
		*msg = _("The game can only be saved while you are free to walk around.");
	return can;
}

bool X3DEngine::canLoadGameStateCurrently(Common::U32String *msg) {
	const bool can = _frameDepth <= 1;
	if (!can && msg)
		*msg = _("A game cannot be loaded during a sequence.");
	return can;
}

Common::Error X3DEngine::saveGameStream(Common::WriteStream *stream, bool isAutosave) {
	if (!_scene)
		return Common::kUnknownError;
	stream->writeUint32BE(MKTAG('X', '3', 'D', 'S'));
	Common::Serializer s(nullptr, stream);
	uint32 version = kSaveVersion;
	s.syncAsUint32LE(version);
	s.setVersion(kSaveVersion);
	s.syncString(_sceneName);
	s.syncAsByte(_practice);
	_scene->syncState(s);
	_interaction->syncState(s);
	_collision->syncState(s);
	_player.syncState(s);
	_inventory->syncState(s);
	if (_unit)
		_unit->syncState(s);
	syncGauge(s);
	return Common::kNoError;
}

Common::Error X3DEngine::loadGameStream(Common::SeekableReadStream *stream) {
	if (stream->readUint32BE() != MKTAG('X', '3', 'D', 'S'))
		return Common::kReadingFailed;
	Common::Serializer s(stream, nullptr);
	uint32 version = 0;
	s.syncAsUint32LE(version);
	if (version < 1 || version > kSaveVersion)
		return Common::kReadingFailed;
	_pendingVersion = version;
	Common::String scene;
	s.syncString(scene);
	s.syncAsByte(_practice);
	_pendingLoad.resize(stream->size() - stream->pos());
	stream->read(_pendingLoad.data(), _pendingLoad.size());
	_nextScene = scene; // not gotoScene: the pending load keeps this scene
	return Common::kNoError;
}

Graphics::Surface *X3DEngine::thumbnail(int width, int height) {
	if (!_scene)
		return nullptr;
	// The player's view, not the last one drawn: a unit's entry autosave comes after its
	// setView, before any frame
	_scene->draw(viewCamera(1), _renderer->width(), _renderer->height());
	// A wider view is scaled to the thumbnail's height and its centre cut out
	const int fullWidth = MAX(width, height * _renderer->width() / _renderer->height());
	Graphics::Surface *full = _renderer->thumbnail(fullWidth, height);
	if (!full || fullWidth == width)
		return full;
	const int x = (fullWidth - width) / 2;
	Graphics::Surface *small = new Graphics::Surface();
	small->copyFrom(full->getSubArea(Common::Rect(x, 0, x + width, height)));
	full->free();
	delete full;
	return small;
}

void X3DEngine::storeHeldItem() {
	if (_interaction && !_interaction->heldItem().empty()) {
		_inventory->add(_interaction->heldItem() + "P");
		_interaction->holdItem("");
	}
}

void X3DEngine::pauseEngineIntern(bool pause) {
	Engine::pauseEngineIntern(pause);
	if (pause)
		captureMouse(false); // the next frame in free play captures it again
	if (_video)
		_video->pauseVideo(pause);
	if (!pause)
		_last = _system->getMillis(); // the paused time is not game time
}

void X3DEngine::playScene(const Common::String &sceneName) {
	_sceneName = sceneName;

	Scene scene(_renderer);
	scene.maxDetail = ConfMan.getBool("max_detail");
	if (!scene.load(sceneName))
		error("Unable to load scene %s", sceneName.c_str());
	_scene = &scene;

	// Sound: the group volumes of sound mode 0, the scene's emitters
	_sound->setGroupVolume(Sound::kAmbient, 85);
	_sound->setGroupVolume(4, 80);
	_sound->setGroupVolume(5, 80);
	_sound->setScale(scene.scale);
	Talk talk(scene, *_sound, scene.dataDir());
	_talk = &talk;
	Interaction interaction(scene, *_sound, talk);
	_interaction = &interaction;
	// The bar is opened, empty, when the player is chosen
	_inventory->attach(&interaction);
	interaction.inventory = _inventory;

	Common::ScopedPtr<Unit> unit(createUnit(sceneName));
	_unit = unit.get();
	if (_unit)
		_unit->afterLoad();

	_player = Player();
	_player.init(scene.scale);
	_player.fov = scene.camera.fov;
	Collision collision;
	collision.build(scene);
	_collision = &collision;
	interaction.collision = &collision;
	interaction.load(scene.dataDir()); // after the collision: hidden hotspots leave it

	_keys = Keys();
	_enterHeld = _suspended = _hoverNow = _clickNow = false;
	_hotspot = -1;
	_unitActions.clear();
	_last = _fpsStart = _system->getMillis();
	_pending = _logicMs = _lastClick = _frames = 0;
	_gauge = Gauge();
	_inGameOver = false;

	if (!_pendingLoad.empty()) {
		// A load: the unit's load hook has run; restore the saved state, then the unit's
		// start without its entry
		Common::MemoryReadStream stream(_pendingLoad.data(), _pendingLoad.size());
		Common::Serializer s(&stream, nullptr);
		s.setVersion(_pendingVersion);
		scene.syncState(s);
		interaction.syncState(s);
		collision.syncState(s);
		_player.syncState(s);
		_inventory->syncState(s);
		if (_unit)
			_unit->syncState(s);
		syncGauge(s);
		_pendingLoad.clear();
		_restoring = true;
		if (_unit)
			_unit->start(false, false);
		_restoring = false;
	} else if (_unit) {
		_unit->start(true, true);
	}
	_previous = _player;

	while (!shouldQuit() && _nextScene.empty())
		frame(true);

	// A held item goes back to the bar before the unit changes
	storeHeldItem();
	_clickedHotspot.clear();

	_sound->stopAll();
	_scene = nullptr;
	_highlight.clear();
	_collision = nullptr;
	_interaction = nullptr;
	_talk = nullptr;
	_inventory->attach(nullptr);
	_unit = nullptr;
}

void X3DEngine::logicStep(bool input) {
	const uint32 stepMs = 1000 / kStepsPerSecond;
	// The last render may have left the scene posed between two steps: the step reads
	// object positions from its own pose (the train and bike rides follow them)
	if (_posedBetween) {
		_posedBetween = false;
		_scene->interpolate(1);
		if (_unit)
			_unit->afterAnimate();
		_scene->poseAll();
	}
	_scene->beginStep();
	_previous = _player;
	if (input && !_suspended) {
		_rideView = false; // set again by the unit while a ride drives the camera
		const bool handled = _unit && _unit->input(stepMs / 1000.0f);
		if (!handled && _player.tick(stepMs / 1000.0f, _keys, *_collision))
			_sound->emit(Sound::kEffectsEmitter, "SAUT.WAV", _player.eye, false);
		_keys.mouseTurn = false;
	}
	_logicMs += stepMs;
	_inventory->tick(_logicMs);
	_talk->tick(_logicMs);
	_scene->advance(stepMs / 1000.0f);
	if (_unit)
		_unit->afterAnimate();
	_scene->poseAll();
	if (_unit)
		_unit->afterStep();
	_collision->refresh();
	_sound->updateVolumes(_player.eye);
	_interaction->eye = _player.eye;
}

void X3DEngine::frame(bool input) {
	struct Depth {
		int &d;
		explicit Depth(int &depth) : d(depth) { d++; }
		~Depth() { d--; }
	} nesting(_frameDepth);
	// A console command that runs frames of its own, up to the first click
	while (input && !_clickNow && !_queuedCommands.empty()) {
		const Common::String c = _queuedCommands.remove_at(0);
		debugC(1, kDebugScript, "command %s: %s", c.c_str(), command(c).c_str());
	}

	// Modern controls: the mouse is captured in free play, the bar closed; a locked view
	// (no turning, as in close-ups) points with a free cursor. Short sequences that are not
	// suspended keep it captured but do not look. A click at a given point (the console's
	// click) keeps its point.
	captureMouse(_modern && !_suspended && !_inventory->shown() && _player.canTurn);
	const bool pointClick = _clickNow;
	const Common::Point centre(_renderer->width() / 2, _renderer->height() / 2);
	const bool looked = pollInput(input, centre);

	// Logic runs in fixed steps; rendering interpolates the camera between the last two
	const uint32 stepMs = 1000 / kStepsPerSecond;
	const uint32 now = _system->getMillis();
	// Skipping a scripted sequence: one step per frame, as fast as it runs, undrawn and
	// silent, until free play or the next scene (enhancement)
	if (_skipping && ((input && !_suspended) || !_nextScene.empty()))
		endSkip();
	if (_skipping) {
		_sound->stopGroup(Sound::kVoice);
		_pending = stepMs;
	} else {
		// A long gap (a stall, a debugger) is not replayed as game time
		_pending += MIN<uint32>(now - _last, 250);
	}
	_last = now;
	bool stepped = false;
	while (_pending >= stepMs && !shouldQuit()) {
		_pending -= stepMs;
		logicStep(input || _walk);
		stepped = true;
	}

	// The original draws one frame per step; the high_fps option draws at the display's
	// rate, interpolating between the last two steps.
	const float alpha = _highFps ? (float)_pending / stepMs : 1.0f;
	const bool drawNow = !_skipping && (_highFps || stepped);
	const Camera camera = viewCamera(alpha);
	if (_mouseCaptured && looked) // the cursor stays at the centre, as the crosshair
		_system->warpMouse(_system->getWidth() / 2, _system->getHeight() / 2);
	if (_mouseCaptured && !pointClick) {
		// Hover again whenever the view moved
		_hoverNow |= _mouse != centre || camera.yaw != _camera.yaw || camera.pitch != _camera.pitch ||
		             memcmp(camera.position, _camera.position, sizeof(camera.position));
		_mouse = centre;
	}
	updatePointer(camera, now, input);

	_camera = camera;
	if (drawNow)
		drawFrame(camera, alpha);
	if (!_skipping) {
		// Without high_fps nothing is drawn before the next step: sleep until it is due,
		// 10 ms at most so that input stays responsive
		const uint32 due = _pending + (_system->getMillis() - now);
		_system->delayMillis(!_highFps && due < stepMs ? CLIP<uint32>(stepMs - due, 1, 10) : 1);
	}

	if (now - _fpsStart >= 5000) {
		debugC(2, kDebugGraphics, "%u frames per second", _frames * 1000 / (now - _fpsStart));
		_frames = 0;
		_fpsStart = now;
	}

	if (_escapeNow) {
		_escapeNow = false;
		handleEscape();
	}
	if (input)
		runUnitCode();
}

bool X3DEngine::pollInput(bool input, const Common::Point &centre) {
	// The game's keys come as keymapper actions, so they follow the player's remapping
	bool looked = false;
	Common::Event e;
	while (_system->getEventManager()->pollEvent(e)) {
		processEvent(e);
		if (e.type == Common::EVENT_MOUSEMOVE && _mouseCaptured) {
			// Mouse look, within the original's pitch limits; the last step turns too, so
			// the drawn view follows at once. Motion queued before the capture, or from its
			// warp, does not turn.
			looked |= e.relMouse.x || e.relMouse.y;
			if ((input || _walk) && _player.canTurn && !_rideView && _system->getMillis() - _captureStart >= 100 &&
			    (e.relMouse.x || e.relMouse.y)) {
				const float dYaw = e.relMouse.x * _lookScale;
				const float pitch = _player.pitch - (_invertY ? -1 : 1) * e.relMouse.y * _lookScale;
				// Nearly straight down or up: the original's 0.6..2.7 keeps items at the
				// feet out of reach of a crosshair
				const float dPitch = CLIP(pitch, MIN(_player.pitch, 0.05f), MAX(_player.pitch, (float)M_PI - 0.05f)) - _player.pitch;
				_player.yaw += dYaw;
				_previous.yaw += dYaw;
				_player.pitch += dPitch;
				_previous.pitch += dPitch;
				_keys.mouseTurn = true;
			}
			continue;
		}
		if (e.type == Common::EVENT_MOUSEMOVE) {
			_mouse = e.mouse;
			_hoverNow = true;
			continue;
		}
		if (e.type == Common::EVENT_LBUTTONDOWN) {
			debugC(1, kDebugInput, "click %d,%d", e.mouse.x, e.mouse.y);
			_mouse = _mouseCaptured ? centre : e.mouse;
			_clickNow = true;
			continue;
		}
		if (e.type == Common::EVENT_KEYUP) {
			_hoverNow = true; // the original re-hovers on every key release
			continue;
		}
		if (e.type != Common::EVENT_CUSTOM_ENGINE_ACTION_START && e.type != Common::EVENT_CUSTOM_ENGINE_ACTION_END)
			continue;
		const bool down = e.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START;
		debugC(3, kDebugInput, "action %d %s", e.customType, down ? "down" : "up");
		if (!down) {
			debugC(2, kDebugInput, "camera %g,%g,%g,%g,%g", _player.eye.x(), _player.eye.y(), _player.eye.z(), _player.yaw, _player.pitch);
			_hoverNow = true;
		}
		switch (e.customType) {
		case kActionForward:
			_keys.up = down;
			break;
		case kActionBackward:
			_keys.down = down;
			break;
		case kActionTurnLeft:
			_keys.left = down;
			break;
		case kActionTurnRight:
			_keys.right = down;
			break;
		case kActionLookUp:
			_keys.pageUp = down;
			break;
		case kActionLookDown:
			_keys.pageDown = down;
			break;
		case kActionRun:
			_keys.ctrl = down;
			break;
		case kActionJump:
			_keys.shift = down;
			break;
		case kActionCrouch:
			_keys.crouch = down;
			break;
		case kActionStrafeLeft:
			_keys.strafeLeft = down;
			break;
		case kActionStrafeRight:
			_keys.strafeRight = down;
			break;
		case kActionSkip:
			_enterHeld = down;
			if (down && (!input || _suspended))
				startSkip();
			break;
		case kActionInventory:
			_keys.space = down;
			if (down && input && !_suspended)
				_inventory->toggle();
			break;
		case kActionMenu:
		case kActionSaveMenu: // the same in play; only the menu key skips a sequence
			if (down && input && !_suspended && !_escapeBlocked)
				_escapeNow = true;
			else if (down && (!input || _suspended) && e.customType == kActionMenu)
				startSkip();
			break;
		default:
			break;
		}
	}
	return looked;
}

void X3DEngine::updatePointer(const Camera &camera, uint32 now, bool input) {
	// Hover and click: clicks closer together than one frame + 10 ms are ignored; nothing
	// counts beyond 4 scene units of depth
	const bool interactive = input && !_suspended;
	_freePlay = interactive;
	// Modern controls: no cursor while it has no use (scripted scenes, dialogue), so none
	// sits in the middle of the view (not in the original, which keeps its cursor)
	const bool hideCursor = _modern && !interactive && !_inventory->shown();
	if (hideCursor)
		CursorMan.showMouse(false); // every frame: suspend() and hold changes show it
	else if (_cursorHidden)
		CursorMan.showMouse(true);
	_cursorHidden = hideCursor;
	if (_clickNow && (!interactive || now - _lastClick < 1000 / kStepsPerSecond + 10))
		_clickNow = false;
	_interaction->setCursorScale(_renderer->pixelScale());
	// The inventory bar takes the mouse before the scene
	const int x2d = (_renderer->width() - 640) / 2;
	const Common::Point mouse2d(_mouse.x - x2d, _mouse.y);
	const bool overBar = interactive && _inventory->contains(mouse2d);
	if (overBar) {
		if (_clickNow) {
			_lastClick = now;
			_clickNow = false;
			_inventory->click(mouse2d);
		}
		_interaction->showCursor(_inventory->cursorAt(mouse2d));
		_hoverNow = false;
	}
	if (interactive && !overBar && _clickNow && _unit && _unit->beforeClick()) {
		_lastClick = now;
		_clickNow = false;
	}
	if (interactive && !overBar && (_hoverNow || _clickNow)) {
		_hotspot = -1;
		const Scene::Model *model;
		uint object;
		float depth;
		if (_scene->pick(camera, _renderer->width(), _renderer->height(), _mouse.x, _mouse.y, model, object, depth) &&
		    depth <= 4 * _scene->scale) {
			Common::StringArray names;
			for (int o = object; o >= 0; o = model->file.objects[o].parent)
				names.push_back(model->file.objects[o].name);
			_hotspot = _interaction->hotspotFor(names);
			debugC(2, kDebugInput, "pick %s at depth %g: hotspot %d", names[0].c_str(), depth, _hotspot);
		}
		_hoverNow = false;
	}
	if (_clickNow) {
		_lastClick = now;
		_clickNow = false;
		_interaction->click(_hotspot, _unitActions);
		if (_hotspot >= 0)
			_clickedHotspot = _interaction->hotspotName(_hotspot);
	}
	if (interactive && !overBar)
		_interaction->hover(_hotspot, now);
}

void X3DEngine::drawFrame(const Camera &camera, float alpha) {
	// At alpha 0 too: the camera is then at the step's start, and so must the objects be
	if (_scene->interpolate(alpha)) {
		if (_unit)
			_unit->afterAnimate();
		_scene->poseAll();
		_posedBetween = true;
	}
	_scene->draw(camera, _renderer->width(), _renderer->height());
	refreshHotspots();
	if (_showHotspots && _freePlay)
		_scene->drawHighlight(_highlight);
	_inventory->draw(*_renderer, (_renderer->width() - 640) / 2);
	if (_unit)
		_unit->draw();
	if (_gauge.ms && _gauge.visible)
		drawGauge(_renderer, MIN(1.0f, (_logicMs - _gauge.start) / (float)_gauge.ms));
	drawHotspots();
	_renderer->present();
	_frames++;
}

void X3DEngine::handleEscape() {
	// Escape in a game: "Do you want to save?" over the frozen scene. In a game a held item
	// goes back to the bar first; before one (U00) it stays on the cursor.
	if (!_unit || _unit->gameStarted())
		storeHeldItem();
	if (_unit && _unit->escape()) {
		// The unit's own Escape (the gallery's 3D view)
	} else if (_unit && !_unit->gameStarted()) {
		// No game yet (U00): the Option menu at once, the ambient stopped
		_sound->stopGroup(Sound::kAmbient);
		afterOptionMenu(optionMenu());
	} else {
		const Common::String c = runMenu("Save");
		if (c == "SaveOui")
			saveMenu();
		else if (c == "SaveNon")
			afterOptionMenu(optionMenu());
	}
	_last = _system->getMillis();
}

void X3DEngine::runUnitCode() {
	// The unit's code: queued click actions, then its per-frame checks. Both may run
	// blocking sequences (nested frames).
	while (!_unitActions.empty() && !shouldQuit()) {
		const Common::String action = _unitActions.remove_at(0);
		if (!_unit || !_unit->handle(action))
			warning("Unit action %s is not implemented", action.c_str());
	}
	// The click handler's check, after the queue
	if (!_clickedHotspot.empty()) {
		const Common::String hotspot = _clickedHotspot;
		_clickedHotspot.clear();
		if (_unit)
			_unit->afterClick(hotspot);
	}
	if (_unit)
		_unit->afterFrame();
}

Camera X3DEngine::viewCamera(float alpha) const {
	Camera camera;
	for (int k = 0; k < 3; k++)
		camera.position[k] = _previous.eye.getData()[k] + (_player.eye.getData()[k] - _previous.eye.getData()[k]) * alpha;
	// Angles the short way round: a step from 6.28 to 0.01 (U04's boat) is not a full turn
	const float twoPi = 2 * (float)M_PI;
	const float dYaw = _player.yaw - _previous.yaw, dPitch = _player.pitch - _previous.pitch;
	camera.yaw = _previous.yaw + (dYaw - twoPi * floorf(dYaw / twoPi + 0.5f)) * alpha;
	camera.pitch = _previous.pitch + (dPitch - twoPi * floorf(dPitch / twoPi + 0.5f)) * alpha;
	// The fov option widens free play's 90 degrees; scripted views (70 and below) keep
	// theirs, and zooms between blend
	camera.fov = _player.fov + _fovExtra * CLIP((_player.fov - 70) / 20, 0.0f, 1.0f);
	camera.roll = _player.roll;
	return camera;
}

void X3DEngine::runFor(uint32 ms, bool walk) {
	// 0 = one frame, which includes an animation tick
	const uint32 end = _logicMs + MAX<uint32>(ms, 1);
	const bool outer = _walk;
	_walk = walk;
	do
		frame(false);
	while (_logicMs < end && !shouldQuit() && _nextScene.empty());
	_walk = outer;
}

// Reduces an angle to [0, 2pi)
static float wrapAngle(float a) {
	a = fmodf(a, 2 * (float)M_PI);
	return a < 0 ? a + 2 * (float)M_PI : a;
}

void X3DEngine::moveTo(uint32 ms, const float *position, float yaw, float pitch, float fov) {
	// N steps of 1/N of the difference each, then the targets; angles the short way
	// round, 100.0 (kKeep) keeps the current value
	const uint n = MAX<uint>(1, ms * kStepsPerSecond / 1000);
	Math::Vector3d target = position ? Math::Vector3d(position[0], position[1], position[2]) : _player.eye;
	float dYaw = 0, dPitch = 0;
	if (yaw != kKeep) {
		_player.yaw = wrapAngle(_player.yaw);
		dYaw = wrapAngle(yaw) - _player.yaw;
		if (dYaw > (float)M_PI)
			dYaw -= 2 * (float)M_PI;
		else if (dYaw < -(float)M_PI)
			dYaw += 2 * (float)M_PI;
	}
	if (pitch != kKeep) {
		_player.pitch = wrapAngle(_player.pitch);
		dPitch = wrapAngle(pitch) - _player.pitch;
		if (dPitch > (float)M_PI)
			dPitch -= 2 * (float)M_PI;
		else if (dPitch < -(float)M_PI)
			dPitch += 2 * (float)M_PI;
	}
	const float dFov = fov != kKeep ? fov - _player.fov : 0;
	const Math::Vector3d dEye = (target - _player.eye) * (1.0f / n);
	const float endYaw = _player.yaw + dYaw, endPitch = _player.pitch + dPitch, endFov = _player.fov + dFov;
	for (uint i = 0; i < n && !shouldQuit(); i++) {
		_player.eye += dEye;
		_player.yaw += dYaw / n;
		_player.pitch += dPitch / n;
		_player.fov += dFov / n;
		const uint32 t = _logicMs;
		while (_logicMs == t && !shouldQuit())
			frame(false);
	}
	_player.eye = target;
	_player.yaw = endYaw;
	_player.pitch = endPitch;
	_player.fov = endFov;
}

void X3DEngine::lookAt(uint32 ms, const Math::Vector3d &target) {
	// The yaw and pitch whose view direction points at the target
	const Math::Vector3d d = target - _player.eye;
	const float len = d.getMagnitude();
	if (len == 0) {
		runFor(0); // still one frame: callers loop on it
		return;
	}
	const float yaw = atan2f(-d.y(), d.x());
	const float pitch = acosf(CLIP(-d.z() / len, -1.0f, 1.0f));
	if (ms == 0) {
		_player.yaw = wrapAngle(yaw);
		_player.pitch = pitch;
		runFor(0);
	} else {
		moveTo(ms, nullptr, yaw, pitch);
	}
}

void X3DEngine::setView(const float *position, float yaw, float pitch) {
	if (position)
		_player.eye.set(position[0], position[1], position[2]);
	_player.yaw = yaw;
	_player.pitch = pitch;
	_previous = _player;
}

Common::String X3DEngine::runMenu(const Common::String &name, MenuList *list, const Common::String &text) {
	Frame frame;
	if (!frame.load(name))
		return "escape";
	if (!text.empty())
		frame.setText(text);
	const Common::String result = runFrame(frame, list);
	debugC(1, kDebugMenu, "menu %s: %s", name.c_str(), result.c_str());
	return result;
}

Common::String X3DEngine::runFrame(Frame &frame, MenuList *list, uint32 timeout) {
	if (_skipping) // a menu is where a skipped sequence ends
		endSkip();
	if (list)
		frame.setList(list->rows, list->selected, list->names);
	const int x2d = (_renderer->width() - 640) / 2;
	uint32 lastPress = 0; // a second press on a players row within 500 ms and 4 px selects
	Common::Point lastPoint;
	// A text edit takes every key as typed (Space is the inventory action otherwise)
	captureMouse(false);
	Common::Keymap *keymap = _system->getEventManager()->getKeymapper()->getKeymap(keymapName());
	const bool keymapOff = keymap && keymap->isEnabled() && frame.hasEdit();
	if (keymapOff)
		keymap->setEnabled(false);
	CursorMan.showMouse(true);
	_cursorHidden = false;
	const uint32 start = _system->getMillis();
	_menuView = -1;
	Common::String result;
	while (result.empty() && !shouldQuit()) {
		Common::Event e;
		while (_system->getEventManager()->pollEvent(e)) {
			processEvent(e);
			if (e.type == Common::EVENT_MOUSEMOVE || e.type == Common::EVENT_LBUTTONDOWN) {
				_mouse = e.mouse;
				frame.drag(Common::Point(_mouse.x - x2d, _mouse.y));
			}
			if (e.type == Common::EVENT_LBUTTONUP)
				frame.release();
			// A click acts on press
			if (e.type == Common::EVENT_LBUTTONDOWN) {
				const Common::Point p(_mouse.x - x2d, _mouse.y);
				const int row = list ? frame.listRowAt(p) : -1;
				if (row >= 0) {
					frame.selectRow(row);
					// Windows' double click on the players list: SelectUser
					const uint32 now = _system->getMillis();
					if (!list->names.empty() && lastPress && now - lastPress <= 500 &&
						ABS(p.x - lastPoint.x) <= 2 && ABS(p.y - lastPoint.y) <= 2 && frame.selectedRow() >= 0) {
						result = "SelectUser";
						lastPress = 0;
					} else {
						lastPress = now;
						lastPoint = p;
					}
				} else if (!frame.press(p, _system->getEventManager()->getModifierState() & Common::KBD_SHIFT)) {
					_menuView = frame.viewAt(p);
					result = frame.commandAt(_menuView);
				}
			}
			// Keys come as keymapper actions, except on a frame with a text edit, which
			// turns the keymap off and reads the keys as typed
			int volume = 0;
			if (e.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START) {
				if (e.customType == kActionMenu)
					result = "escape";
				else if (e.customType == kActionSkip)
					result = "enter";
				else if (e.customType == kActionVolumeUp || e.customType == kActionVolumeDown)
					volume = e.customType == kActionVolumeUp ? 10 : -10;
				else
					result = "key";
			}
			if (e.type == Common::EVENT_KEYDOWN) {
				if (!frame.hasEdit()) {
					result = "key"; // a key without an action
				} else if (e.kbd.keycode == Common::KEYCODE_ESCAPE) {
					result = "escape";
				} else if (e.kbd.keycode == Common::KEYCODE_RETURN || e.kbd.keycode == Common::KEYCODE_KP_ENTER) {
					result = "enter";
				} else if (e.kbd.keycode == Common::KEYCODE_KP_PLUS || e.kbd.keycode == Common::KEYCODE_KP_MINUS) {
					volume = e.kbd.keycode == Common::KEYCODE_KP_PLUS ? 10 : -10;
				} else if (e.kbd.keycode == Common::KEYCODE_BACKSPACE) {
					frame.backspace();
				} else if (e.kbd.keycode == Common::KEYCODE_LEFT || e.kbd.keycode == Common::KEYCODE_RIGHT) {
					frame.moveCaret(e.kbd.keycode == Common::KEYCODE_LEFT ? -1 : 1);
				} else if (e.kbd.ascii >= 32 && e.kbd.ascii < 127) {
					frame.type(e.kbd.ascii);
				}
			}
			// Every sound group +-10 while a frame is open
			for (int g = 1; volume && g <= 6; g++)
				_sound->setGroupVolume(g, _sound->groupVolume(g) + volume);
		}
		if (timeout && result.empty() && _system->getMillis() - start >= timeout)
			result = "timeout";

		const int hovered = frame.viewAt(Common::Point(_mouse.x - x2d, _mouse.y));
		if (_interaction)
			_interaction->showCursor(MAX(0, frame.cursorAt(hovered)));
		if (_scene)
			_scene->draw(_camera, _renderer->width(), _renderer->height());
		else
			_renderer->clear();
		frame.draw(*_renderer, x2d, hovered);
		_renderer->present();
		_system->delayMillis(10);
	}
	if (keymapOff)
		keymap->setEnabled(true);
	_menuText = frame.text();
	if (list)
		list->selected = frame.selectedRow();
	_last = _system->getMillis();
	return result;
}

bool X3DEngine::toScreen(const Math::Vector3d &p, Common::Point &s) const {
	// The render projection without the roll, as the pick uses it; false when off screen
	const float yaw = _camera.yaw, pitch = _camera.pitch;
	const Math::Vector3d d = p - Math::Vector3d(_camera.position[0], _camera.position[1], _camera.position[2]);
	const Math::Vector3d right(-sinf(yaw), -cosf(yaw), 0);
	const Math::Vector3d up(cosf(pitch) * cosf(yaw), -cosf(pitch) * sinf(yaw), sinf(pitch));
	const Math::Vector3d fwd(sinf(pitch) * cosf(yaw), -sinf(pitch) * sinf(yaw), -cosf(pitch));
	const float ky = (4.0f / 3.0f) / tanf(_camera.fov * (float)M_PI / 360), kx = ky * _renderer->height() / _renderer->width();
	const float cx = _renderer->width() / 2.0f, cy = _renderer->height() / 2.0f;
	const float z = Math::Vector3d::dotProduct(d, fwd);
	if (z <= 0)
		return false;
	const float x = cx + cx * Math::Vector3d::dotProduct(d, right) * kx / z, y = cy - cy * Math::Vector3d::dotProduct(d, up) * ky / z;
	// In float first: a point far off screen does not fit a Common::Point
	if (!(x >= 0 && y >= 0 && x < _renderer->width() && y < _renderer->height()))
		return false;
	s = Common::Point((int16)x, (int16)y);
	return true;
}

bool X3DEngine::picks(const Common::Point &s, int target) {
	const Scene::Model *model;
	uint o;
	float depth;
	// At any distance, unlike a click (4 scene units): the overlay shows everything that
	// works, near or far
	if (!_scene->pick(_camera, _renderer->width(), _renderer->height(), s.x, s.y, model, o, depth))
		return false;
	Common::StringArray names;
	for (int k = o; k >= 0; k = model->file.objects[k].parent)
		names.push_back(model->file.objects[k].name);
	return _interaction->hotspotFor(names) == target;
}

bool X3DEngine::aimAt(const Scene::Model *model, uint object, int target, Common::Point &point, bool &hit, uint tries, Math::Vector3d *world) {
	// The object's surface points on screen, nearest first; the picks are spread over all
	// of them, so a partly covered object is tried where it shows
	const Math::Vector3d eye(_camera.position[0], _camera.position[1], _camera.position[2]);
	Common::Array<Math::Vector3d> surface;
	Common::Array<Common::Point> screen;
	for (const Math::Vector3d &p : _scene->surfacePoints(model, object, eye)) {
		Common::Point s;
		if (toScreen(p, s)) {
			surface.push_back(p);
			screen.push_back(s);
		}
	}
	hit = false;
	if (screen.empty())
		return false;
	point = screen[0];
	const uint step = MAX<uint>(1, screen.size() / MAX<uint>(1, tries));
	for (uint i = 0; target >= 0 && i < screen.size(); i += step)
		if (picks(screen[i], target)) {
			point = screen[i];
			hit = true;
			if (world)
				*world = surface[i];
			break;
		}
	return true;
}

static Math::Vector3d origin(const Scene::Model *m, uint o) {
	const float *w = m->file.objects[o].world;
	return Math::Vector3d(w[12], w[13], w[14]);
}

void X3DEngine::drawHotspots() {
	// Which hotspots are marked, and where on them, is found again when the view moves
	// (at most every 100 ms) and a few times a second; the markers follow their points on
	// every frame. ScummVM's overlay (a full-window upload) is redrawn when they changed.
	if (!_showHotspots)
		return;
	const Common::Array<Graphics::HotspotInfo> last = _hotspots;
	placeMarkers();
	bool same = !_hotspotForceRedraw && last.size() == _hotspots.size();
	for (uint i = 0; same && i < last.size(); i++)
		same = last[i].position == _hotspots[i].position && last[i].name == _hotspots[i].name;
	if (same)
		return;
	_hotspotForceRedraw = true; // the base class draws only when dirty
	Engine::drawHotspots();
	if (_hotspots.empty() && _system->isOverlayVisible())
		_system->hideOverlay(); // the base class keeps its last markers when there are none
}

void X3DEngine::refreshHotspots() {
	// Markers and outlines are found again when the view moved (every 100 ms at most), a few
	// times a second for animations, and at once when a hotspot becomes clickable or stops
	// being so (taken, used up, a scene's script) or free play starts or ends: no outline
	// stays on what a click no longer reaches.
	if (!_showHotspots)
		return;
	Common::Array<bool> now;
	if (_freePlay && _scene) {
		const uint n = _interaction->hotspotCount();
		now.resize(n + 1);
		for (uint i = 0; i < n; i++)
			now[i] = _interaction->clickable(i);
		now[n] = true;
	}
	if (now != _clickableNow || hotspotDirty() || _hotspotForceRedraw) {
		_clickableNow = now;
		findHotspots();
	}
}

void X3DEngine::placeMarkers() {
	// Each marker's point, moved with its object's origin, in the drawn view (with its
	// roll), to window pixels without rounding to the logical frame
	_hotspots.clear();
	const float a = _camera.yaw, e = _camera.pitch, r = _camera.roll * (float)M_PI / 180;
	const Math::Vector3d eye(_camera.position[0], _camera.position[1], _camera.position[2]);
	const Math::Vector3d right0(-sinf(a), -cosf(a), 0), up0(cosf(e) * cosf(a), -cosf(e) * sinf(a), sinf(e));
	const Math::Vector3d right = right0 * cosf(r) + up0 * sinf(r), up = up0 * cosf(r) - right0 * sinf(r);
	const Math::Vector3d fwd(sinf(e) * cosf(a), -sinf(e) * sinf(a), -cosf(e));
	const float w = _renderer->width(), h = _renderer->height();
	const float ky = (4.0f / 3.0f) / tanf(_camera.fov * (float)M_PI / 360), kx = ky * h / w;
	const Common::Point o = _renderer->toWindow(Common::Point(0, 0)), c = _renderer->toWindow(Common::Point(w, h));
	const Common::Array<Scene::Model *> &models = _scene->models();
	for (const Marker &m : _markers) {
		if (Common::find(models.begin(), models.end(), m.model) == models.end()) // removed since
			continue;
		const Math::Vector3d d = origin(m.model, m.object) + m.offset - eye;
		const float z = Math::Vector3d::dotProduct(d, fwd);
		if (z <= 0)
			continue;
		const float x = w / 2 * (1 + Math::Vector3d::dotProduct(d, right) * kx / z);
		const float y = h / 2 * (1 - Math::Vector3d::dotProduct(d, up) * ky / z);
		if (x < 0 || y < 0 || x >= w || y >= h)
			continue;
		_hotspots.push_back(Graphics::HotspotInfo(Common::Point(o.x + (int)(x * (c.x - o.x) / w + 0.5f),
		                                                        o.y + (int)(y * (c.y - o.y) / h + 0.5f)), m.label));
	}
}

void X3DEngine::getHotspotPositions(Common::Array<Graphics::HotspotInfo> &hotspots) {
	hotspots = _hotspots;
}

void X3DEngine::findHotspots() {
	// Each clickable hotspot once, at a point where a click reaches it; its INFOOBJ name
	// (the data has no other)
	const Common::Array<Marker> last = _markers;
	_markers.clear();
	_highlight.clear();
	if (!_freePlay || !_scene)
		return;
	_hotspotCamera = _camera;
	_hotspotTime = _system->getMillis();
	Common::Array<bool> done;
	for (const Scene::Model *m : _scene->models()) {
		if (m->hidden)
			continue;
		for (uint o = 0; o < m->file.objects.size(); o++) {
			const Common::String &name = m->file.objects[o].name;
			if (!name.contains('*') || (m->hiddenObjects[o] && !m->pickWhenHidden[o]) || m->unpickable[o])
				continue;
			const int h = _interaction->hotspotFor(Common::StringArray(1, name));
			if (h < 0)
				continue;
			if ((uint)h >= done.size())
				done.resize(h + 1);
			if (done[h] || !_interaction->clickable(h))
				continue;
			// The marker stays on its point while a click still reaches the hotspot there
			Marker marker;
			Common::Point s;
			bool hit = false;
			for (const Marker &l : last)
				if (l.model == m && l.object == o && toScreen(origin(m, o) + l.offset, s) && picks(s, h)) {
					marker = l;
					hit = true;
				}
			if (!hit) {
				Math::Vector3d p;
				// At most 24 picks per hotspot, so a sliver of an object may get no marker
				if (!aimAt(m, o, h, s, hit, 24, &p) || !hit)
					continue;
				const Common::String &label = _interaction->hotspotName(h);
				marker.hotspot = h;
				marker.model = m;
				marker.object = o;
				marker.offset = p - origin(m, o);
				marker.label = label.substr(label.findFirstOf('*') + 1);
			}
			done[h] = true;
			_markers.push_back(marker);
		}
	}
	// Every object a pick takes to a marked hotspot (its name or an ancestor's), for the outline
	for (const Scene::Model *m : _scene->models()) {
		if (m->hidden)
			continue;
		for (uint o = 0; o < m->file.objects.size(); o++) {
			if ((m->hiddenObjects[o] && !m->pickWhenHidden[o]) || m->unpickable[o])
				continue;
			Common::StringArray names;
			for (int k = o; k >= 0; k = m->file.objects[k].parent)
				names.push_back(m->file.objects[k].name);
			const int h = _interaction->hotspotFor(names);
			if (h >= 0 && (uint)h < done.size() && done[h])
				_highlight.push_back(Scene::Highlight(m, o, h));
		}
	}
}

bool X3DEngine::hotspotDirty() const {
	// Again when the view moves, and a few times a second for animations and state
	const uint32 age = _system->getMillis() - _hotspotTime;
	return (age >= 100 && memcmp(&_camera, &_hotspotCamera, sizeof(Camera))) || age >= 250;
}

Common::String X3DEngine::command(const Common::String &line) {
	if (!_scene)
		return "no scene";
	const Common::StringArray a = Common::StringTokenizer(line, " ").split();
	if (a.empty())
		return "";
	const Common::String &c = a[0];
	if (c == "where") {
		_player.probeGround(*_collision);
		float t = 1;
		_collision->cast(_player.eye, _player.eye - Math::Vector3d(0, 0, 10000), t);
		return Common::String::format("%g,%g,%g,%g,%g fov %g, over %s at z %g", _player.eye.x(), _player.eye.y(), _player.eye.z(),
		                              _player.yaw, _player.pitch, _player.fov, _player.groundObject.c_str(), _player.eye.z() - 10000 * t);
	}
	if (c == "goto" && a.size() >= 4) {
		const float p[3] = { (float)atof(a[1].c_str()), (float)atof(a[2].c_str()), (float)atof(a[3].c_str()) };
		setView(p, a.size() > 4 ? atof(a[4].c_str()) : _player.yaw, a.size() > 5 ? atof(a[5].c_str()) : _player.pitch);
		return "ok";
	}
	if (c == "lookat" && a.size() >= 2) {
		lookAt(a.size() > 2 ? atoi(a[2].c_str()) : 0, _scene->objectCenter(a[1]));
		return "ok";
	}
	if (c == "click") {
		if (a.size() >= 3) {
			_mouse = Common::Point(atoi(a[1].c_str()), atoi(a[2].c_str()));
		} else if (a.size() == 2) {
			Common::Point s;
			bool hit;
			Scene::Model *model;
			uint object;
			if (!_scene->findObject(a[1], model, object) ||
			    !aimAt(model, object, _interaction->hotspotFor(Common::StringArray(1, a[1])), s, hit))
				return "behind the camera";
			_mouse = s;
			if (!hit)
				debugC(1, kDebugScript, "click: no visible point of %s", a[1].c_str());
		}
		_clickNow = true;
		return Common::String::format("click %d,%d", _mouse.x, _mouse.y);
	}
	if (c == "save" && a.size() >= 2)
		return saveGameState(atoi(a[1].c_str()), "console").getCode() == Common::kNoError ? "saved" : "save failed";
	if (c == "load" && a.size() >= 2)
		return loadGameState(atoi(a[1].c_str())).getCode() == Common::kNoError ? "loading" : "load failed";
	if (c == "page" && a.size() >= 2) {
		if (a[1] == "credits")
			credits();
		else if (a[1] == "settings")
			settings();
		else if (a[1] == "gallery")
			gallery();
		else if (a[1] == "loupe" && a.size() >= 3)
			magnifier(a[2]);
		else
			showPainting(a[1]);
		return "ok";
	}
	if (c == "savemenu") {
		saveMenu();
		return "ok";
	}
	if (c == "loadmenu")
		return loadMenu() ? "loaded" : "no load";
	if (c == "view3d" && a.size() >= 2) {
		if (Gallery3D::sceneFor(a[1]).empty())
			return "no 3D scene";
		_gallery3D = a[1];
		gotoScene(Gallery3D::sceneFor(a[1]));
		return "ok";
	}
	if (c == "gauge" && a.size() >= 2) {
		// Development: the running gauge ends after ms
		if (_gauge.ms)
			_gauge.start = _logicMs + atoi(a[1].c_str()) - _gauge.ms;
		return _gauge.ms ? "ok" : "no gauge";
	}
	if (c == "exhaust" && a.size() >= 2) {
		_interaction->exhaust(atoi(a[1].c_str()));
		return "ok";
	}
	if (c == "act" && a.size() >= 2) {
		_interaction->runAction(atoi(a[1].c_str()), _unitActions); // Mnn, bypassing the click
		return "ok";
	}
	if (c == "pos" && a.size() >= 2) {
		const Math::Vector3d o = _scene->objectPosition(a[1]), m = _scene->objectCenter(a[1]);
		Scene::Model *model;
		uint i;
		const bool hidden = _scene->findObject(a[1], model, i) && model->hiddenObjects[i];
		return Common::String::format("origin %g,%g,%g centre %g,%g,%g%s", o.x(), o.y(), o.z(), m.x(), m.y(), m.z(), hidden ? " hidden" : "");
	}
	if (c == "overlay") { // the hotspot overlay on or off, as the key does
		showHotspots(!_showHotspots);
		return Common::String::format("%u markers", _markers.size());
	}
	if (c == "hotspots") {
		Common::String out;
		for (const Common::String &h : _interaction->hotspotNames()) {
			const Common::String name = h.substr(0, h.findFirstOf(' '));
			const Math::Vector3d p = _scene->objectCenter(name);
			out += Common::String::format("%s at %g,%g,%g\n", h.c_str(), p.x(), p.y(), p.z());
		}
		return out;
	}
	if (c == "give" && a.size() >= 2) {
		_inventory->add(a[1]);
		return "ok";
	}
	if (c == "objs" && a.size() >= 2) { // every object whose name contains the text
		Common::String out;
		for (uint mi = 0; mi < _scene->models().size(); mi++) {
			const Scene::Model *m = _scene->models()[mi];
			for (uint o = 0; o < m->file.objects.size(); o++)
				if (m->file.objects[o].name.contains(a[1]))
					out += Common::String::format("m%u o%u %s hid %d mhid %d at %g,%g,%g;", mi, o, m->file.objects[o].name.c_str(), (int)m->hiddenObjects[o], (int)m->hidden, m->file.objects[o].world[12], m->file.objects[o].world[13], m->file.objects[o].world[14]);
		}
		return out;
	}
	if (c == "node" && a.size() >= 2) // an animation node's frame
		return Common::String::format("frame %g running %d", _scene->nodeFrame(a[1]), (int)_scene->nodeRunning(a[1]));
	if (c == "probe" && a.size() >= 4) { // the ground below a point, without moving
		const Math::Vector3d p(atof(a[1].c_str()), atof(a[2].c_str()), atof(a[3].c_str()));
		float t = 1;
		const bool hit = _collision->cast(p, p - Math::Vector3d(0, 0, 10000), t);
		return Common::String::format("%d z %g", (int)hit, p.z() - 10000 * t);
	}
	if (c == "bar") {
		_inventory->toggle(); // as Space
		return "ok";
	}
	if (c == "hold" && a.size() >= 2) {
		_interaction->holdItem(a[1] == "-" ? "" : a[1]); // "-": nothing
		return "ok";
	}
	return "unknown command " + c;
}

void X3DEngine::captureMouse(bool capture) {
	if (capture == _mouseCaptured)
		return;
	_mouseCaptured = capture;
	_system->lockMouse(capture);
	if (capture) {
		_captureStart = _system->getMillis();
		_system->warpMouse(_system->getWidth() / 2, _system->getHeight() / 2);
		_mouse = Common::Point(_renderer->width() / 2, _renderer->height() / 2);
		_hoverNow = true;
	}
}

void X3DEngine::startSkip() {
	// Any scripted sequence can be skipped (an enhancement): voices stop, effects are
	// muted until it is over
	if (_skipping)
		return;
	debugC(1, kDebugScript, "skipping the sequence");
	_skipping = true;
	_sound->stopGroup(Sound::kVoice);
	_skipMutedSfx = !_mixer->isSoundTypeMuted(Audio::Mixer::kSFXSoundType);
	_mixer->muteSoundType(Audio::Mixer::kSFXSoundType, true);
}

void X3DEngine::endSkip() {
	_skipping = false;
	if (_skipMutedSfx)
		_mixer->muteSoundType(Audio::Mixer::kSFXSoundType, false);
	_skipMutedSfx = false;
	_last = _system->getMillis();
}

void X3DEngine::suspend(bool suspended) {
	_suspended = suspended;
	CursorMan.showMouse(!suspended);
}

void X3DEngine::showBitmap(const Common::Path &path) {
	Graphics::Surface *image = loadBitmap(path);
	if (!image)
		error("Unable to load %s", path.toString().c_str());

	_renderer->clear();
	_renderer->drawImage(*image, (_renderer->width() - 640) / 2, 0, false);
	_renderer->present();
	image->free();
	delete image;
}

void X3DEngine::wait(uint32 ms) {
	const uint32 start = _system->getMillis();
	while (!shouldQuit() && _system->getMillis() - start < ms) {
		Common::Event e;
		while (_system->getEventManager()->pollEvent(e))
			processEvent(e); // a resized window still resizes the frame
		_system->delayMillis(10);
	}
}

void X3DEngine::fadeToBlack(uint32 ms) {
	const uint n = MAX<uint>(1, ms * kStepsPerSecond / 1000);
	const byte start[3] = { _scene->ambient[0], _scene->ambient[1], _scene->ambient[2] };
	for (uint i = 0; i < n && !shouldQuit(); i++) {
		for (int k = 0; k < 3; k++)
			_scene->ambient[k] = start[k] * (n - 1 - i) / n; // black at the last step
		runFor(10);
	}
}

void X3DEngine::startGauge(uint32 ms, bool visible, const Common::String &label) {
	_gauge.ms = ms;
	_gauge.start = _logicMs;
	_gauge.visible = visible;
	_gauge.label = label;
}

void X3DEngine::syncGauge(Common::Serializer &s) {
	// The gauge as the original saves it (JAUGE): duration and elapsed; a restore resumes
	// with the remainder
	uint32 elapsed = _logicMs - _gauge.start;
	s.syncAsUint32LE(_gauge.ms, 3);
	s.syncAsUint32LE(elapsed, 3);
	s.syncAsByte(_gauge.visible, 3);
	s.syncString(_gauge.label, 3);
	if (s.isLoading())
		_gauge.start = _logicMs - elapsed;
}

bool X3DEngine::gaugeExpired() {
	if (!gaugeOver())
		return false;
	_gauge.ms = 0;
	return true;
}

void X3DEngine::playVideo(const Common::String &name, const Common::String &wav, uint32 action, bool keepSounds) {
	Video::AVIDecoder video;
	if (!video.loadFile(Common::Path("Video/" + name + ".avi"))) {
		warning("Unable to open video %s", name.c_str());
		return;
	}

	video.start();
	if (!action && !keepSounds) {
		_sound->stopAll(); // a video stops every sound
	} else if (action && _interaction) {
		Common::StringArray ignored;
		_interaction->runAction(action, ignored);
	}

	// The soundtrack is a separate WAV, started right after the video
	Audio::SoundHandle sound;
	Common::File *file = new Common::File();
	if (file->open(Common::Path("Video/" + (wav.empty() ? name : wav) + ".wav"))) {
		Audio::RewindableAudioStream *stream = Audio::makeWAVStream(file, DisposeAfterUse::YES);
		if (stream)
			_mixer->playStream(Audio::Mixer::kSpeechSoundType, &sound, stream);
	} else {
		delete file;
	}

	debugC(1, kDebugGraphics, "video %s", name.c_str());
	_video = &video; // paused with the engine
	captureMouse(false);
	bool skip = _skipping;
	while (!shouldQuit() && !skip && !video.endOfVideo()) {
		Common::Event e;
		while (_system->getEventManager()->pollEvent(e)) {
			processEvent(e);
			if (e.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START &&
			    (e.customType == kActionSkip || (!action && e.customType == kActionMenu))) {
				skip = true;
				if (action)
					_sound->stopGroup(Sound::kVoice);
			}
		}

		if (video.needsUpdate()) {
			const Graphics::Surface *frame = video.decodeNextFrame();
			if (frame) {
				_renderer->clear();
				_renderer->drawImage(*frame, (_renderer->width() - 640) / 2, 0, false);
				_renderer->present();
			}
		}
		_system->delayMillis(5);
	}

	debugC(1, kDebugGraphics, "video %s done", name.c_str());
	_video = nullptr;
	_mixer->stopHandle(sound);
}

} // End of namespace X3D
