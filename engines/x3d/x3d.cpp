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
#include "common/debug.h"
#include "common/events.h"
#include "common/file.h"
#include "common/system.h"

#include "backends/keymapper/keymap.h"
#include "backends/keymapper/keymapper.h"

#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "audio/mixer.h"

#include "engines/metaengine.h"
#include "engines/util.h"

#include "graphics/cursorman.h"

#include "image/bmp.h"

#include "video/avi_decoder.h"

#include "x3d/detection.h"
#include "x3d/collision.h"
#include "x3d/console.h"
#include "x3d/frame.h"
#include "x3d/gallery3d.h"
#include "x3d/interaction.h"
#include "x3d/inventory.h"
#include "x3d/player.h"
#include "x3d/renderer.h"
#include "x3d/scene.h"
#include "x3d/sound.h"
#include "x3d/talk.h"
#include "x3d/u00.h"
#include "x3d/u01.h"
#include "x3d/u02.h"
#include "x3d/u03.h"
#include "x3d/u04.h"
#include "x3d/u05.h"
#include "x3d/u06.h"
#include "x3d/u07.h"
#include "x3d/u33.h"
#include "x3d/x3d.h"

namespace X3D {

X3DEngine::X3DEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc) {
}

X3DEngine::~X3DEngine() {
	delete _inventory;
	delete _sound;
	delete _renderer;
}

Common::Error X3DEngine::run() {
	// All original paths are relative to Data/, which the detector (kADFlagMatchFullPaths)
	// has already added to SearchMan
	// The original frames every mode as 4:3 (E-0040); widescreen keeps its height and view
	// and shows more to the sides. 2D images stay 640x480, centred.
	ConfMan.registerDefault("widescreen", false);
	ConfMan.registerDefault("max_detail", false);
	ConfMan.registerDefault("filter_textures", false);
	_renderer = Renderer::create(ConfMan.getBool("widescreen") ? 854 : 640, 480);
	_renderer->filterTextures = ConfMan.getBool("filter_textures");
	_sound = new Sound(_mixer);
	_inventory = new Inventory();
	setDebugger(new Console(this));

	// Development shortcut: start_scene=<file.X3D> in the game's config skips the boot
	// sequence and the scene's entry video
	if (!ConfMan.hasKey("start_scene") && !ConfMan.hasKey("save_slot")) {
		// Boot sequence: docs/engine-spec/boot.md
		showBitmap("2dbit/Intro1.bmp");
		wait(3000);
		showBitmap("2dbit/Intro2.bmp");
		wait(2000);
	}

	// U00 shows the players screen and runs Monet's tutorial; the Option menu's New game
	// then goes to App.bin's start scene (ui.md, Boot to U01)
	Common::String sceneName = ConfMan.hasKey("start_scene") ? ConfMan.get("start_scene") : "U00.X3D";
	// A game chosen in the launcher's load dialog
	if (ConfMan.hasKey("save_slot") && loadGameState(ConfMan.getInt("save_slot")).getCode() == Common::kNoError)
		sceneName = _nextScene;

	while (!shouldQuit() && !sceneName.empty()) {
		_nextScene.clear();
		playScene(sceneName);
		sceneName = _nextScene;
	}
	return Common::kNoError;
}

// A keymapper action event as the key it stands for, so key handling stays in one place
static void actionToKey(Common::Event &e) {
	if (e.type != Common::EVENT_CUSTOM_ENGINE_ACTION_START && e.type != Common::EVENT_CUSTOM_ENGINE_ACTION_END)
		return;
	static const Common::KeyCode keys[] = {
		Common::KEYCODE_INVALID, Common::KEYCODE_UP, Common::KEYCODE_DOWN, Common::KEYCODE_LEFT,
		Common::KEYCODE_RIGHT, Common::KEYCODE_PAGEUP, Common::KEYCODE_PAGEDOWN, Common::KEYCODE_LCTRL,
		Common::KEYCODE_LSHIFT, Common::KEYCODE_SPACE, Common::KEYCODE_ESCAPE, Common::KEYCODE_RETURN
	};
	if (e.customType <= kActionNone || e.customType > kActionSkip)
		return;
	const bool down = e.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START;
	e.kbd = Common::KeyState(keys[e.customType]);
	e.type = down ? Common::EVENT_KEYDOWN : Common::EVENT_KEYUP;
}

static const uint32 kSaveVersion = 4; // 2: numbered clip slots; 3: the scene gauge; 4: U01 on it

bool X3DEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	// Only between the unit's sequences: a save inside one could not replay its end
	return _scene && _unit && _unit->gameStarted() && !_suspended && _frameDepth <= 1;
}

bool X3DEngine::canLoadGameStateCurrently(Common::U32String *msg) {
	return _frameDepth <= 1;
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
	_scene->draw(_camera, _renderer->width(), _renderer->height());
	return _renderer->thumbnail(width, height);
}

void X3DEngine::saveMenu() {
	// OptionSave (save.md): slots 1..98, the first free one selected; OK saves and stays
	for (;;) {
		Common::StringArray names;
		names.resize(99);
		for (const SaveStateDescriptor &d : getMetaEngine()->listSaves(_targetName.c_str()))
			if (d.getSaveSlot() >= 1 && d.getSaveSlot() <= 98)
				names[d.getSaveSlot()] = d.getDescription();
		MenuList list;
		for (int s = 1; s <= 98; s++) {
			list.rows.push_back(Common::String::format("%d - %s", s + 1, names[s].empty() ? "Empty" : names[s].c_str()));
			if (list.selected < 0 && names[s].empty())
				list.selected = s - 1;
		}
		if (list.selected < 0)
			list.selected = 0;
		// ponytail: the original appends typing to "Save without name"; here typing replaces it
		const Common::String c = runMenu("OptionSave", &list, "Save without name");
		if (shouldQuit())
			return;
		if (c == "OptionSelectSave" || c == "enter") {
			const Common::String name = _menuText.empty() ? "Save without name" : _menuText;
			saveGameState(list.selected + 1, name);
			continue;
		}
		if (c == "OptionSaveSommaire" || c == "escape") {
			afterOptionMenu(optionMenu());
			return;
		}
		if (c == "SaveQuit") {
			if (runMenu("OptionQuitter") == "QuitterOK")
				quitGame();
			continue;
		}
		return; // OptionSave3D: back to the game
	}
}

bool X3DEngine::loadMenu() {
	// OptionLoad (save.md): the used slots only, nothing selected; OK loads
	MenuList list;
	Common::Array<int> slots;
	for (const SaveStateDescriptor &d : getMetaEngine()->listSaves(_targetName.c_str())) {
		slots.push_back(d.getSaveSlot());
		list.rows.push_back(Common::String::format("%d - %s", d.getSaveSlot() + 1, d.getDescription().c_str()));
	}
	for (;;) {
		const Common::String c = runMenu("OptionLoad", &list);
		if (shouldQuit())
			return false;
		if ((c == "OptionSelectGame" || c == "enter") && list.selected >= 0)
			return loadGameState(slots[list.selected]).getCode() == Common::kNoError;
		if (c == "OptionScreen" || c == "escape")
			return false;
	}
}

// The players and the unit of each one's last save, one "unit name" line per player, in
// the savefile <target>.players; "*" before the unit marks the current player (ui.md)
void X3DEngine::readPlayers(Common::StringArray &names, Common::Array<int> &units, Common::String *current) const {
	Common::ScopedPtr<Common::InSaveFile> in(_saveFileMan->openForLoading(_targetName + ".players"));
	while (in && !in->eos() && !in->err()) {
		const Common::String line = in->readLine();
		const char *space = strchr(line.c_str(), ' ');
		if (!space || !space[1])
			continue;
		const bool isCurrent = line.hasPrefix("*");
		units.push_back(atoi(line.c_str() + (isCurrent ? 1 : 0)));
		names.push_back(space + 1);
		if (isCurrent && current)
			*current = names.back();
	}
}

void X3DEngine::writePlayers(const Common::StringArray &names, const Common::Array<int> &units) {
	Common::ScopedPtr<Common::OutSaveFile> out(_saveFileMan->openForSaving(_targetName + ".players", false));
	if (!out)
		return;
	for (uint i = 0; i < names.size(); i++)
		out->writeString(Common::String::format("%s%d %s\n", names[i] == _playerName ? "*" : "", units[i], names[i].c_str()));
	out->finalize();
}

Common::StringArray X3DEngine::players(Common::String *current) const {
	Common::StringArray names;
	Common::Array<int> units;
	readPlayers(names, units, current);
	return names;
}

bool X3DEngine::selectPlayer(const Common::String &name) {
	_playerName = name;
	// ponytail: players share ScummVM's save slots; the original keeps saves per player
	Common::StringArray names;
	Common::Array<int> units;
	readPlayers(names, units);
	// The selected player becomes the current one, which the players screen offers next time
	const bool isNew = Common::find(names.begin(), names.end(), name) == names.end();
	if (isNew) {
		names.push_back(name);
		units.push_back(0);
	}
	writePlayers(names, units);
	return isNew;
}

Common::Error X3DEngine::saveGameState(int slot, const Common::String &desc, bool isAutosave) {
	const Common::Error result = Engine::saveGameState(slot, desc, isAutosave);
	// The player's unit number follows every save: it unlocks the gallery (ui.md)
	if (result.getCode() == Common::kNoError && !_playerName.empty()) {
		Common::StringArray names;
		Common::Array<int> units;
		readPlayers(names, units);
		for (uint i = 0; i < names.size(); i++)
			if (names[i] == _playerName)
				units[i] = atoi(_sceneName.c_str() + 1);
		writePlayers(names, units);
	}
	return result;
}

void X3DEngine::storeHeldItem() {
	if (_interaction && !_interaction->heldItem().empty()) {
		_inventory->add(_interaction->heldItem() + "P");
		_interaction->holdItem("");
	}
}

void X3DEngine::pauseEngineIntern(bool pause) {
	Engine::pauseEngineIntern(pause);
	if (_video)
		_video->pauseVideo(pause);
	if (!pause)
		_last = _system->getMillis(); // the paused time is not game time
}

void X3DEngine::gameOver() {
	_sound->stopAll();
	storeHeldItem(); // the caught path stores it (E-0210)
	if (!loadMenu())
		afterOptionMenu(optionMenu());
}

Unit *X3DEngine::createUnit(const Common::String &sceneName) {
	if (sceneName.size() < 3 || toupper(sceneName[0]) != 'U' || !Common::isDigit(sceneName[1]))
		return nullptr;
	const int n = atoi(sceneName.c_str() + 1);
	switch (n) {
	case 0:
		return new U00(this, _practice);
	case 1:
		return new U01(this);
	case 2:
		return new U02(this);
	case 3:
		return new U03(this);
	case 4:
		return new U04(this);
	case 5:
		return new U05(this);
	case 6:
		return new U06(this);
	case 7:
		return new U07(this);
	case 33:
		return new U33(this);
	default:
		return nullptr;
	}
}

void X3DEngine::playScene(const Common::String &sceneName) {
	_sceneName = sceneName;

	Scene scene(_renderer);
	scene.maxDetail = ConfMan.getBool("max_detail");
	if (!scene.load(sceneName))
		error("Unable to load scene %s", sceneName.c_str());
	_scene = &scene;

	// Sound (sound.md): mode 0 group volumes, the scene's emitters
	_sound->setGroupVolume(Sound::kAmbient, 85);
	_sound->setGroupVolume(4, 80);
	_sound->setGroupVolume(5, 80);
	_sound->setScale(scene.scale);
	Talk talk(scene, *_sound, scene.dataDir());
	_talk = &talk;
	Interaction interaction(scene, *_sound, talk);
	_interaction = &interaction;
	// The bar is opened, empty, when the player is chosen (ui.md, Boot to U01)
	_inventory->attach(&interaction);
	interaction.inventory = _inventory;

	const bool view3d = !_gallery3D.empty() && sceneName.equalsIgnoreCase(Gallery3D::sceneFor(_gallery3D));
	Common::ScopedPtr<Unit> unit(view3d ? new Gallery3D(this, _gallery3D) : createUnit(sceneName));
	_gallery3D.clear();
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
	_last = _fpsStart = _sceneStart = _system->getMillis();
	_pending = _logicMs = _lastClick = _frames = 0;
	_gauge = Gauge();

	// Development shortcut: dev_click=x,y,ms[;x,y,ms...] clicks at game pixel (x, y) ms
	// after the first scene starts, for testing without focus (SDL takes click positions
	// from the real cursor). Parsed once: the schedule runs on across scene changes.
	const bool parseDev = !_devParsed;
	if (parseDev) {
		_devParsed = true;
		_devStart = _sceneStart;
	}
	if (parseDev && ConfMan.hasKey("dev_click")) {
		const Common::String clicks = ConfMan.get("dev_click");
		const char *c = clicks.c_str();
		int x, y, ms, n;
		while (sscanf(c, "%d,%d,%d%n", &x, &y, &ms, &n) == 3) {
			_devClicks.push_back(x);
			_devClicks.push_back(y);
			_devClicks.push_back(ms);
			c += n;
			if (*c == ';')
				c++;
		}
	}

	// Development shortcut: dev_commands=ms:command[;ms:command...] runs console commands
	// at those times after the first scene starts (console.h)
	if (parseDev && ConfMan.hasKey("dev_commands")) {
		const Common::String commands = ConfMan.get("dev_commands");
		Common::String c;
		for (const char *p = commands.c_str(); ; p++) {
			if (*p == ';' || !*p) {
				c.trim();
				if (!c.empty())
					_devCommands.push_back(c);
				c.clear();
				if (!*p)
					break;
			} else {
				c += *p;
			}
		}
	}

	// Development shortcut: start_camera=x,y,z,yaw,pitch places the camera anywhere and
	// skips the unit's scripted start
	if (!_pendingLoad.empty()) {
		// A load: the unit's load hook has run; restore the saved state, then the unit's
		// start without its entry (save.md, Loading)
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
	} else if (ConfMan.hasKey("start_camera")) {
		sscanf(ConfMan.get("start_camera").c_str(), "%f,%f,%f,%f,%f", &_player.eye.x(),
		       &_player.eye.y(), &_player.eye.z(), &_player.yaw, &_player.pitch);
		if (_unit)
			_unit->start(false, false);
	} else if (_unit) {
		// start_scene skips the prologue as well as the boot sequence
		_unit->start(true, !ConfMan.hasKey("start_scene"));
	}
	_previous = _player;

	while (!shouldQuit() && _nextScene.empty())
		frame(true);

	// A held item goes back to the bar before the unit changes (E-0251, E-0210)
	storeHeldItem();
	_clickedHotspot.clear();

	_sound->stopAll();
	_scene = nullptr;
	_collision = nullptr;
	_interaction = nullptr;
	_talk = nullptr;
	_inventory->attach(nullptr);
	_unit = nullptr;
}

void X3DEngine::logicStep(bool input) {
	const uint32 stepMs = 1000 / kStepsPerSecond;
	_previous = _player;
	if (input && !_suspended) {
		const bool handled = _unit && _unit->input(stepMs / 1000.0f);
		if (!handled && _player.tick(stepMs / 1000.0f, _keys, *_collision))
			_sound->emit(Sound::kEffectsEmitter, "SAUT.WAV", _player.eye, false);
	}
	_logicMs += stepMs;
	_inventory->tick(_logicMs);
	_talk->tick(_logicMs);
	_scene->advance(stepMs / 1000.0f);
	if (_unit)
		_unit->afterAnimate();
	_scene->poseAll();
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
	if (!_devClicks.empty() && _system->getMillis() - _devStart >= (uint32)_devClicks[2]) {
		_mouse = Common::Point(_devClicks[0], _devClicks[1]);
		_clickNow = true;
		for (int i = 0; i < 3; i++)
			_devClicks.remove_at(0);
	}

	// Due commands, up to the first click (one click per frame)
	while (input && !_clickNow && !_devCommands.empty() && _system->getMillis() - _devStart >= (uint32)atoi(_devCommands[0].c_str())) {
		const Common::String c = _devCommands.remove_at(0);
		debugC(1, kDebugScript, "dev command %s: %s", c.c_str(), command(c.substr(c.findFirstOf(':') + 1)).c_str());
	}

	Common::Event e;
	while (_system->getEventManager()->pollEvent(e)) {
		actionToKey(e);
		if (e.type == Common::EVENT_MOUSEMOVE) {
			_mouse = e.mouse;
			_hoverNow = true;
			continue;
		}
		if (e.type == Common::EVENT_LBUTTONDOWN) {
			debugC(1, kDebugInput, "click %d,%d", e.mouse.x, e.mouse.y);
			_mouse = e.mouse;
			_clickNow = true;
			continue;
		}
		if (e.type != Common::EVENT_KEYDOWN && e.type != Common::EVENT_KEYUP)
			continue;
		const bool down = e.type == Common::EVENT_KEYDOWN;
		debugC(3, kDebugInput, "key %d %s", e.kbd.keycode, down ? "down" : "up");
		if (!down) {
			debugC(2, kDebugInput, "camera %g,%g,%g,%g,%g", _player.eye.x(), _player.eye.y(), _player.eye.z(), _player.yaw, _player.pitch);
			_hoverNow = true; // the original re-hovers on every key release
		}
		switch (e.kbd.keycode) {
		case Common::KEYCODE_UP: _keys.up = down; break;
		case Common::KEYCODE_DOWN: _keys.down = down; break;
		case Common::KEYCODE_LEFT: _keys.left = down; break;
		case Common::KEYCODE_RIGHT: _keys.right = down; break;
		case Common::KEYCODE_PAGEUP: _keys.pageUp = down; break;
		case Common::KEYCODE_PAGEDOWN: _keys.pageDown = down; break;
		case Common::KEYCODE_LCTRL:
		case Common::KEYCODE_RCTRL: _keys.ctrl = down; break;
		case Common::KEYCODE_RETURN:
		case Common::KEYCODE_KP_ENTER: _enterHeld = down; break;
		case Common::KEYCODE_SPACE:
			_keys.space = down;
			if (down && input && !_suspended)
				_inventory->toggle();
			break;
		case Common::KEYCODE_LSHIFT:
		case Common::KEYCODE_RSHIFT: _keys.shift = down; break;
		case Common::KEYCODE_ESCAPE:
		case Common::KEYCODE_F5: // the same in play (ui.md, Escape)
			if (down && input && !_suspended && !_escapeBlocked)
				_escapeNow = true;
			break;
		default: break;
		}
	}

	// Logic runs in fixed steps; rendering interpolates the camera between the last two
	// (movement.md, Engine model)
	const uint32 stepMs = 1000 / kStepsPerSecond;
	const uint32 now = _system->getMillis();
	if (_devUp) {
		_keys.up = now < _devUp;
		if (!_keys.up)
			_devUp = 0;
	}
	if (_devDown) {
		_keys.down = now < _devDown;
		if (!_keys.down)
			_devDown = 0;
	}
	if (_devShift) {
		_keys.shift = now < _devShift;
		if (!_keys.shift)
			_devShift = 0;
	}
	// A long gap (a stall, a debugger) is not replayed as game time
	_pending += MIN<uint32>(now - _last, 250);
	_last = now;
	while (_pending >= stepMs && !shouldQuit()) {
		_pending -= stepMs;
		logicStep(input || _walk);
	}

	Camera camera;
	const float alpha = (float)_pending / stepMs;
	for (int k = 0; k < 3; k++)
		camera.position[k] = _previous.eye.getData()[k] + (_player.eye.getData()[k] - _previous.eye.getData()[k]) * alpha;
	camera.yaw = _previous.yaw + (_player.yaw - _previous.yaw) * alpha;
	camera.pitch = _previous.pitch + (_player.pitch - _previous.pitch) * alpha;
	camera.fov = _player.fov;
	camera.roll = _player.roll;

	// Hover and click (interaction.md): clicks closer together than one frame + 10 ms are
	// ignored; nothing counts beyond 4 scene units of depth
	const bool interactive = input && !_suspended;
	if (_clickNow && (!interactive || now - _lastClick < 1000 / kStepsPerSecond + 10))
		_clickNow = false;
	// The inventory bar takes the mouse before the scene (ui.md, Frame manager)
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

	_camera = camera;
	if (alpha > 0 && _scene->interpolate(alpha)) {
		if (_unit)
			_unit->afterAnimate();
		_scene->poseAll();
	}
	_scene->draw(camera, _renderer->width(), _renderer->height());
	_inventory->draw(*_renderer, x2d);
	if (_unit)
		_unit->draw();
	if (_gauge.ms && _gauge.visible)
		drawGauge(_renderer, MIN(1.0f, (_logicMs - _gauge.start) / (float)_gauge.ms));
	_renderer->present();
	_system->delayMillis(1);

	_frames++;
	if (now - _fpsStart >= 5000) {
		debugC(2, kDebugGraphics, "%u frames per second", _frames * 1000 / (now - _fpsStart));
		_frames = 0;
		_fpsStart = now;
	}

	// Escape in a game: "Do you want to save?" over the frozen scene (ui.md, Escape)
	if (_escapeNow) {
		_escapeNow = false;
		// In a game a held item goes back to the bar first; in U00 it stays on the
		// cursor (E-0210)
		if (!_unit || _unit->gameStarted())
			storeHeldItem();
		if (_unit && _unit->escape()) {
			// The unit's own Escape (the gallery's 3D view)
		} else if (_unit && !_unit->gameStarted()) {
			// No game yet (U00): the Option menu at once, the ambient stopped (ui.md, Escape)
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

	// The unit's code: queued click actions, then its per-frame checks. Both may run
	// blocking sequences (nested frames).
	if (input) {
		while (!_unitActions.empty() && !shouldQuit()) {
			const Common::String action = _unitActions.remove_at(0);
			if (!_unit || !_unit->handle(action))
				warning("Unit action %s is not implemented", action.c_str());
		}
		// The click handler's check after the queue (u01.md, E-0250)
		if (!_clickedHotspot.empty()) {
			const Common::String hotspot = _clickedHotspot;
			_clickedHotspot.clear();
			if (_unit)
				_unit->afterClick(hotspot);
		}
		if (_unit)
			_unit->afterFrame();
	}
}

void X3DEngine::runFor(uint32 ms, bool walk) {
	const uint32 end = _logicMs + ms;
	const bool outer = _walk;
	_walk = walk;
	do
		frame(false);
	while (_logicMs < end && !shouldQuit() && _nextScene.empty());
	_walk = outer;
}

// Reduces an angle to [0, 2pi)
static float wrapAngle(float a) {
	a = fmod(a, 2 * (float)M_PI);
	return a < 0 ? a + 2 * (float)M_PI : a;
}

void X3DEngine::moveTo(uint32 ms, const float *position, float yaw, float pitch, float fov) {
	// N steps of 1/N of the difference each, then the targets; angles the short way
	// round, 100.0 keeps (movement.md, E-0050)
	const uint n = MAX<uint>(1, ms * kStepsPerSecond / 1000);
	Math::Vector3d target = position ? Math::Vector3d(position[0], position[1], position[2]) : _player.eye;
	float dYaw = 0, dPitch = 0;
	if (yaw != kKeep) {
		_player.yaw = wrapAngle(_player.yaw);
		dYaw = wrapAngle(yaw) - _player.yaw;
		if (dYaw > M_PI)
			dYaw -= 2 * M_PI;
		else if (dYaw < -M_PI)
			dYaw += 2 * M_PI;
	}
	if (pitch != kKeep) {
		_player.pitch = wrapAngle(_player.pitch);
		dPitch = wrapAngle(pitch) - _player.pitch;
		if (dPitch > M_PI)
			dPitch -= 2 * M_PI;
		else if (dPitch < -M_PI)
			dPitch += 2 * M_PI;
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
	// The yaw and pitch whose view direction points at the target (E-0040)
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
	if (list)
		frame.setList(list->rows, list->selected, list->names);
	const int x2d = (_renderer->width() - 640) / 2;
	uint32 lastPress = 0; // a second press on a players row within 500 ms and 4 px selects
	Common::Point lastPoint;
	// A text edit takes every key as typed (Space is the inventory action otherwise)
	Common::Keymap *keymap = _system->getEventManager()->getKeymapper()->getKeymap("x3d-default");
	const bool keymapOff = keymap && keymap->isEnabled() && frame.hasEdit();
	if (keymapOff)
		keymap->setEnabled(false);
	CursorMan.showMouse(true);
	const uint32 start = _system->getMillis();
	_menuView = -1;
	Common::String result;
	// Development shortcut: dev_menu=cmd,cmd,... answers the next menus in order after
	// 1.5 s each ("key" for any key, "text:<name>" types into the edit then Enter)
	if (!_devMenuParsed) {
		_devMenuParsed = true;
		const Common::String all = ConfMan.get("dev_menu");
		Common::String c;
		for (const char *p = all.c_str(); ; p++) {
			if (*p == ',' || !*p) {
				if (!c.empty())
					_devMenu.push_back(c);
				c.clear();
				if (!*p)
					break;
			} else {
				c += *p;
			}
		}
	}
	while (result.empty() && !shouldQuit()) {
		Common::Event e;
		while (_system->getEventManager()->pollEvent(e)) {
			actionToKey(e);
			if (e.type == Common::EVENT_MOUSEMOVE || e.type == Common::EVENT_LBUTTONDOWN) {
				_mouse = e.mouse;
				frame.drag(Common::Point(_mouse.x - x2d, _mouse.y));
			}
			if (e.type == Common::EVENT_LBUTTONUP)
				frame.release();
			// A click acts on press (ui.md, Events)
			if (e.type == Common::EVENT_LBUTTONDOWN) {
				const Common::Point p(_mouse.x - x2d, _mouse.y);
				const int row = list ? frame.listRowAt(p) : -1;
				if (row >= 0) {
					frame.selectRow(row);
					// Windows' double click on the players list: SelectUser (ui.md)
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
			if (e.type == Common::EVENT_KEYDOWN) {
				if (e.kbd.keycode == Common::KEYCODE_ESCAPE)
					result = "escape";
				else if (e.kbd.keycode == Common::KEYCODE_RETURN || e.kbd.keycode == Common::KEYCODE_KP_ENTER)
					result = "enter";
				else if (e.kbd.keycode == Common::KEYCODE_KP_PLUS || e.kbd.keycode == Common::KEYCODE_KP_MINUS) {
					// Every sound group +-10 while a frame is open (E-0604)
					for (int g = 1; g <= 6; g++)
						_sound->setGroupVolume(g, _sound->groupVolume(g) + (e.kbd.keycode == Common::KEYCODE_KP_PLUS ? 10 : -10));
				} else if (!frame.hasEdit())
					result = "key";
				else if (e.kbd.keycode == Common::KEYCODE_BACKSPACE)
					frame.backspace();
				else if (e.kbd.keycode == Common::KEYCODE_LEFT || e.kbd.keycode == Common::KEYCODE_RIGHT)
					frame.moveCaret(e.kbd.keycode == Common::KEYCODE_LEFT ? -1 : 1);
				else if (e.kbd.ascii >= 32 && e.kbd.ascii < 127)
					frame.type(e.kbd.ascii);
			}
		}
		if (timeout && result.empty() && _system->getMillis() - start >= timeout)
			result = "timeout";
		if (result.empty() && !_devMenu.empty() && _system->getMillis() - start >= 1500) {
			const Common::String c = _devMenu.remove_at(0);
			if (c.hasPrefix("text:")) {
				frame.setText(c.substr(5));
				result = "enter";
			} else {
				result = c;
			}
			debugC(1, kDebugMenu, "dev menu: %s", c.c_str());
		}

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

void X3DEngine::showPainting(const Common::String &name) {
	// TableauJeu (ui.md, Other frames): full screen until a click, Escape blocked; then
	// the voice stops
	Frame frame;
	if (!frame.load("TableauJeu"))
		return;
	frame.setBitmap(1, name);
	for (;;) {
		const Common::String c = runFrame(frame);
		if (shouldQuit() || (c != "escape" && c != "key" && c != "enter"))
			break;
	}
	_sound->stopGroup(Sound::kVoice);
}

void X3DEngine::credits() {
	// Credits (ui.md): a click or 6 s turns the page; page 1 shows twice (the frame's
	// first name has no extension), a key leaves
	Frame frame;
	if (!frame.load("Credits"))
		return;
	for (int page : { 1, 2, 3, 4, 5 }) {
		frame.setBitmap(1, Common::String::format("Credit%02d", page));
		for (int shown = 0; shown < (page == 1 ? 2 : 1); shown++) {
			const Common::String c = runFrame(frame, nullptr, 6000);
			if (shouldQuit() || (c != "timeout" && c != "MoveCredit"))
				return;
		}
	}
}

void X3DEngine::settings() {
	// OptionReglages (ui.md Settings): music is group 1, voice groups 2 and 3. The sliders
	// are ScummVM's music and speech/effects volumes, so they agree with the launcher's
	Frame frame;
	if (!frame.load("OptionReglages"))
		return;
	const int max = frame.sliderMax(4);
	frame.setSliderValue(4, ConfMan.getInt("music_volume") * max / Audio::Mixer::kMaxMixerVolume);
	frame.setSliderValue(5, ConfMan.getInt("speech_volume") * max / Audio::Mixer::kMaxMixerVolume);
	for (;;) {
		const Common::String c = runFrame(frame);
		if (shouldQuit() || c == "ReglageAnnuler" || c == "escape")
			return;
		if (c == "ReglageOK" || c == "enter") {
			const int voice = frame.sliderValue(5) * Audio::Mixer::kMaxMixerVolume / MAX(1, max);
			ConfMan.setInt("music_volume", frame.sliderValue(4) * Audio::Mixer::kMaxMixerVolume / MAX(1, max));
			ConfMan.setInt("speech_volume", voice);
			ConfMan.setInt("sfx_volume", voice);
			syncSoundSettings();
			return;
		}
	}
}

// The gallery's paintings in unlock order and the saved unit that unlocks up to each
// (ui.md Gallery)
static const char *const kPaintings[] = {
	"U11_01", "U11_02", "U11_03", "U12_03", "U12_04", "U13_14", "U13_05", "U13_13", "U13_03",
	"U13_01", "U13_12", "U13_11", "U13_06", "U13_04", "U14_01", "U13_15", "U14_02", "U14_05",
	"U14_03", "U14_07"
};

static uint unlockedPaintings(int unit) {
	switch (unit) {
	case 1: return 1;
	case 2: return 3;
	case 3: return 4;
	case 33: return 5;
	case 4: return 14;
	case 5: case 6: case 7: return 20;
	default: return 0;
	}
}

// The Galerie frame's thumbnail view per painting
static int thumbnailView(const Common::String &painting) {
	static const struct { const char *painting; int id; } views[] = {
		{ "U11_01", 5 }, { "U11_02", 22 }, { "U11_03", 6 }, { "U12_03", 7 }, { "U12_04", 40 },
		{ "U13_01", 8 }, { "U13_03", 9 }, { "U13_04", 10 }, { "U13_05", 11 }, { "U13_06", 12 },
		{ "U13_11", 13 }, { "U13_12", 14 }, { "U13_13", 15 }, { "U13_14", 30 }, { "U14_01", 16 },
		{ "U13_15", 17 }, { "U14_02", 18 }, { "U14_03", 19 }, { "U14_05", 20 }, { "U14_07", 21 }
	};
	for (const auto &v : views)
		if (painting == v.painting)
			return v.id;
	return -1;
}

int X3DEngine::playerUnit() const {
	Common::StringArray names;
	Common::Array<int> units;
	readPlayers(names, units);
	for (uint i = 0; i < names.size(); i++)
		if (names[i] == _playerName)
			return units[i];
	return 0;
}

void X3DEngine::gallery() {
	const uint count = unlockedPaintings(playerUnit());
	for (;;) {
		Frame galerie;
		if (!galerie.load("Galerie"))
			return;
		for (uint i = 0; i < ARRAYSIZE(kPaintings); i++)
			galerie.setVisible(thumbnailView(kPaintings[i]), i < count);
		const Common::String c = runFrame(galerie);
		if (shouldQuit() || c != "GoToTableau")
			return;
		const Common::String painting = galerie.bitmapName(_menuView).substr(0, 6);
		uint index = 0;
		while (index < count && painting != kPaintings[index])
			index++;
		if (index < count && !paintingScreens(index, count))
			return;
	}
}

// Tableau, Taille and Loupe of the unlocked painting index; false to leave the gallery
bool X3DEngine::paintingScreens(uint index, uint count) {
	Common::String screen = "Tableau";
	for (;;) {
		const Common::String p = kPaintings[index];
		Frame frame;
		if (!frame.load(screen))
			return false;
		frame.setBitmap(1, p + (screen == "Tableau" ? "TAB" : "_Size"));
		if (screen == "Tableau" && (p == "U14_02" || p == "U14_05"))
			frame.setVisible(30, false);
		const Common::String c = runFrame(frame);
		if (shouldQuit() || c == "escape")
			return false;
		if (c == "GoBack")
			return true;
		if (c == "GoPrev")
			index = (index + count - 1) % count;
		else if (c == "GoNext")
			index = (index + 1) % count;
		else if (c == "GoTaille")
			screen = "Taille";
		else if (c == "GoEcranTableau")
			screen = "Tableau";
		else if (c == "GoLoupe")
			magnifier(p);
		else if (c == "GotoScene3D" && !Gallery3D::sceneFor(p).empty()) {
			_gallery3D = p; // the Option menu hands over to the scene
			return false;
		}
	}
}

void X3DEngine::returnToPainting(const Common::String &painting) {
	// The painting's Tableau, then the gallery and the Option menu as the player leaves
	const uint count = unlockedPaintings(playerUnit());
	uint index = 0;
	while (index < ARRAYSIZE(kPaintings) && painting != kPaintings[index])
		index++;
	if (index >= count || paintingScreens(index, count))
		gallery();
	afterOptionMenu(_gallery3D.empty() ? optionMenu() : Common::String("Gallery3D"));
}

void X3DEngine::magnifier(const Common::String &painting) {
	// Loupe (ui.md): the parts listed in Media.txt stitched together, panned from the
	// edges, left on a click
	Common::File media;
	int cols = 0, rows = 0;
	if (media.open("2dbit/Media.txt")) {
		while (!media.eos()) {
			const Common::String line = media.readLine();
			if (line.hasPrefixIgnoreCase(painting + "Loupe;")) {
				const char *c = strchr(line.c_str() + painting.size() + 6, ';');
				if (c)
					sscanf(c + 1, "%d,%d", &cols, &rows);
				break;
			}
		}
	}
	if (cols <= 0 || rows <= 0)
		return;
	Common::Array<Graphics::Surface *> parts;
	int width = 0, height = 0;
	for (int i = 0; i < cols * rows; i++) {
		parts.push_back(loadBitmap(Common::Path(Common::String::format("2dbit/%sLoupe%d.BMP", painting.c_str(), i + 1))));
		if (!parts.back())
			warning("Missing magnifier part %d of %s", i + 1, painting.c_str());
		if (parts.back() && i / cols == 0)
			width += parts.back()->w;
		if (parts.back() && i % cols == 0)
			height += parts.back()->h;
	}
	Graphics::Surface image;
	image.create(MAX(width, 640), MAX(height, 480), Graphics::PixelFormat::createFormatRGBA32());
	for (int i = 0; i < cols * rows; i++) {
		if (!parts[i])
			continue;
		Graphics::Surface *rgba = parts[i]->convertTo(image.format);
		image.copyRectToSurface(*rgba, (i % cols) * 640, (i / cols) * 480, Common::Rect(rgba->w, rgba->h));
		rgba->free();
		delete rgba;
		parts[i]->free();
		delete parts[i];
	}

	const int x2d = (_renderer->width() - 640) / 2;
	int ox = 0, oy = 0;
	uint32 lastPan = 0;
	bool done = false;
	while (!done && !shouldQuit()) {
		Common::Event e;
		while (_system->getEventManager()->pollEvent(e)) {
			actionToKey(e);
			if (e.type == Common::EVENT_MOUSEMOVE)
				_mouse = e.mouse;
			if (e.type == Common::EVENT_LBUTTONDOWN || (e.type == Common::EVENT_KEYDOWN && e.kbd.keycode == Common::KEYCODE_ESCAPE))
				done = true;
		}
		const int mx = _mouse.x - x2d, my = _mouse.y;
		const int left = mx < 30, right = mx >= 610, top = my < 30, bottom = my >= 450;
		static const int kinds[3][3] = { { 12, 13, 9 }, { 10, 0, 7 }, { 11, 6, 8 } }; // [v][h]
		if (_interaction)
			_interaction->showCursor(kinds[top ? 0 : bottom ? 2 : 1][left ? 0 : right ? 2 : 1]);
		const uint32 now = _system->getMillis();
		if (now - lastPan >= 80) {
			lastPan = now;
			if (left)
				ox -= (30 - mx) * 50 / 30;
			if (right)
				ox += (mx - 609) * 50 / 30;
			if (top)
				oy -= (30 - my) * 50 / 30;
			if (bottom)
				oy += (my - 449) * 50 / 30;
			ox = CLIP(ox, 0, image.w - 640);
			oy = CLIP(oy, 0, image.h - 480);
		}
		_renderer->clear();
		const Graphics::Surface view = image.getSubArea(Common::Rect(ox, oy, ox + 640, oy + 480));
		_renderer->drawImage(view, x2d, 0, false);
		_renderer->present();
		_system->delayMillis(10);
	}
	image.free();
}

Common::String X3DEngine::optionMenu() {
	for (;;) {
		// Load and Gallery are greyed when empty and still react (ui.md, Q-0063)
		Frame frame;
		if (!frame.load("Option"))
			return "";
		if (getMetaEngine()->listSaves(_targetName.c_str()).empty())
			frame.setBitmap(3, "SomB2");
		if (!unlockedPaintings(playerUnit()))
			frame.setBitmap(6, "SomE2");
		const Common::String c = runFrame(frame);
		if (shouldQuit())
			return "";
		if (c == "OptionNouvelleP" || c == "OptionEntrenement")
			return c;
		if (c == "OptionLoad" && loadMenu())
			return c; // the load has chosen the next scene
		if (c == "OptionQuitter") {
			if (runMenu("OptionQuitter") == "QuitterOK") {
				quitGame();
				return "";
			}
		} else if (c == "OptionCredits") {
			credits();
		} else if (c == "OptionReglage") {
			settings();
		} else if (c == "OptionGalerie") {
			gallery();
			if (!_gallery3D.empty())
				return "Gallery3D";
		} else if (!c.empty() && c != "escape" && c != "enter" && c != "key") {
			warning("Menu command %s is not implemented", c.c_str());
		}
	}
}

void X3DEngine::afterOptionMenu(const Common::String &command) {
	if (command == "OptionNouvelleP") {
		// A new game starts at the scene named in App.bin #GAME# (E-0037)
		Common::String scene = "U01.X3D";
		if (Common::SeekableReadStream *game = openBinChunk("App.bin", "#GAME#")) {
			scene = game->readString(0, 30);
			delete game;
		}
		// A new game drops a held item and empties the bar (E-0212)
		_practice = false;
		if (_interaction)
			_interaction->holdItem("");
		_inventory->clear();
		gotoScene(scene);
	} else if (command == "Gallery3D") {
		gotoScene(Gallery3D::sceneFor(_gallery3D));
	} else if (command == "OptionEntrenement") {
		// Practice keeps the bar (E-0212); a held item is stored by the scene switch
		_practice = true;
		gotoScene("U00.X3D");
	}
}

Common::String X3DEngine::command(const Common::String &line) {
	if (!_scene)
		return "no scene";
	Common::StringArray a;
	Common::String w;
	for (uint i = 0; i <= line.size(); i++) {
		if (i == line.size() || line[i] == ' ') {
			if (!w.empty())
				a.push_back(w);
			w.clear();
		} else {
			w += line[i];
		}
	}
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
			// The first of the object's surface points that the pick reaches, else the first
			// on screen, with the render projection
			const float yaw = _player.yaw, pitch = _player.pitch;
			const Math::Vector3d right(-sinf(yaw), -cosf(yaw), 0);
			const Math::Vector3d up(cosf(pitch) * cosf(yaw), -cosf(pitch) * sinf(yaw), sinf(pitch));
			const Math::Vector3d fwd(sinf(pitch) * cosf(yaw), -sinf(pitch) * sinf(yaw), -cosf(pitch));
			const float ky = (4.0f / 3.0f) / tan(_player.fov * M_PI / 360.0), kx = ky * _renderer->height() / _renderer->width();
			const float cx = _renderer->width() / 2.0f, cy = _renderer->height() / 2.0f;
			const int target = _interaction->hotspotFor(Common::StringArray(1, a[1]));
			bool found = false, hit = false;
			for (const Math::Vector3d &p : _scene->surfacePoints(a[1], _player.eye)) {
				const Math::Vector3d d = p - _player.eye;
				const float z = Math::Vector3d::dotProduct(d, fwd);
				if (z <= 0)
					continue;
				const Common::Point s(cx + cx * Math::Vector3d::dotProduct(d, right) * kx / z,
				                      cy - cy * Math::Vector3d::dotProduct(d, up) * ky / z);
				if (s.x < 0 || s.y < 0 || s.x >= _renderer->width() || s.y >= _renderer->height())
					continue;
				if (!found)
					_mouse = s;
				found = true;
				const Scene::Model *model;
				uint object;
				float depth;
				if (target < 0 || !_scene->pick(_camera, _renderer->width(), _renderer->height(), s.x, s.y, model, object, depth))
					continue;
				Common::StringArray names;
				for (int o = object; o >= 0; o = model->file.objects[o].parent)
					names.push_back(model->file.objects[o].name);
				if (_interaction->hotspotFor(names) == target) {
					_mouse = s;
					hit = true;
					break;
				}
			}
			if (!found)
				return "behind the camera";
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
	if (c == "press" && a.size() >= 3) { // hold up, down or shift for ms, for scripted play
		const uint32 until = _system->getMillis() + atoi(a[2].c_str());
		(a[1] == "shift" ? _devShift : a[1] == "down" ? _devDown : _devUp) = until;
		return "ok";
	}
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
		while (_system->getEventManager()->pollEvent(e)) {
		}
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
	// save.md JAUGE: duration and elapsed; a restore resumes with the remainder
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
		_sound->stopAll(); // a video stops every sound (sound.md)
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

	_video = &video; // paused with the engine
	bool skip = false;
	while (!shouldQuit() && !skip && !video.endOfVideo()) {
		Common::Event e;
		while (_system->getEventManager()->pollEvent(e)) {
			actionToKey(e);
			if (e.type == Common::EVENT_KEYDOWN &&
			    (e.kbd.keycode == Common::KEYCODE_RETURN || (!action && e.kbd.keycode == Common::KEYCODE_ESCAPE))) {
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

	_video = nullptr;
	_mixer->stopHandle(sound);
}

} // End of namespace X3D
