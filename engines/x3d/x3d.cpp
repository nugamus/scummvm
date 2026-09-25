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
#include "common/savefile.h"
#include "common/serializer.h"
#include "common/debug.h"
#include "common/events.h"
#include "common/file.h"
#include "common/system.h"

#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "audio/mixer.h"

#include "engines/metaengine.h"
#include "engines/util.h"

#include "graphics/cursorman.h"

#include "image/bmp.h"

#include "video/avi_decoder.h"

#include "x3d/collision.h"
#include "x3d/console.h"
#include "x3d/frame.h"
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
	_renderer = Renderer::create(ConfMan.hasKey("widescreen") && ConfMan.getBool("widescreen") ? 854 : 640, 480);
	_sound = new Sound(_mixer);
	_inventory = new Inventory();
	setDebugger(new Console(this));

	// Development shortcut: start_scene=<file.X3D> in the game's config skips the boot
	// sequence and the scene's entry video
	if (!ConfMan.hasKey("start_scene")) {
		// Boot sequence: docs/engine-spec/boot.md
		showBitmap("2dbit/Intro1.bmp");
		wait(3000);
		showBitmap("2dbit/Intro2.bmp");
		wait(2000);
	}

	// U00 shows the players screen and runs Monet's tutorial; the Option menu's New game
	// then goes to App.bin's start scene (ui.md, Boot to U01)
	Common::String sceneName = ConfMan.hasKey("start_scene") ? ConfMan.get("start_scene") : "U00.X3D";

	while (!shouldQuit() && !sceneName.empty()) {
		_nextScene.clear();
		playScene(sceneName);
		sceneName = _nextScene;
	}
	return Common::kNoError;
}

static const uint32 kSaveVersion = 1;

bool X3DEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	return _scene && _unit && _unit->gameStarted() && !_suspended;
}

bool X3DEngine::canLoadGameStateCurrently(Common::U32String *msg) {
	return true;
}

Common::Error X3DEngine::saveGameStream(Common::WriteStream *stream, bool isAutosave) {
	if (!_scene)
		return Common::kUnknownError;
	stream->writeUint32BE(MKTAG('X', '3', 'D', 'S'));
	Common::Serializer s(nullptr, stream);
	uint32 version = kSaveVersion;
	s.syncAsUint32LE(version);
	s.syncString(_sceneName);
	s.syncAsByte(_practice);
	_scene->syncState(s);
	_interaction->syncState(s);
	_collision->syncState(s);
	_player.syncState(s);
	_inventory->syncState(s);
	if (_unit)
		_unit->syncState(s);
	return Common::kNoError;
}

Common::Error X3DEngine::loadGameStream(Common::SeekableReadStream *stream) {
	if (stream->readUint32BE() != MKTAG('X', '3', 'D', 'S'))
		return Common::kReadingFailed;
	Common::Serializer s(stream, nullptr);
	uint32 version = 0;
	s.syncAsUint32LE(version);
	if (version != kSaveVersion)
		return Common::kReadingFailed;
	Common::String scene;
	s.syncString(scene);
	s.syncAsByte(_practice);
	_pendingLoad.resize(stream->size() - stream->pos());
	stream->read(_pendingLoad.data(), _pendingLoad.size());
	gotoScene(scene);
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

Common::StringArray X3DEngine::players() const {
	Common::StringArray names;
	const Common::String all = ConfMan.get("players");
	Common::String name;
	for (const char *p = all.c_str(); ; p++) {
		if (*p == '|' || !*p) {
			if (!name.empty())
				names.push_back(name);
			name.clear();
			if (!*p)
				break;
		} else {
			name += *p;
		}
	}
	return names;
}

bool X3DEngine::selectPlayer(const Common::String &name) {
	// ponytail: players share ScummVM's save slots; the original keeps saves per player
	Common::StringArray names = players();
	for (const Common::String &n : names)
		if (n == name)
			return false;
	names.push_back(name);
	Common::String all;
	for (const Common::String &n : names)
		all += (all.empty() ? "" : "|") + n;
	ConfMan.set("players", all);
	ConfMan.flushToDisk();
	return true;
}

void X3DEngine::gameOver() {
	_sound->stopAll();
	if (!loadMenu())
		afterOptionMenu(optionMenu());
}

void X3DEngine::playScene(const Common::String &sceneName) {
	_sceneName = sceneName;

	Scene scene(_renderer);
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

	U00 unit00(this, _practice);
	U01 unit01(this);
	U02 unit02(this);
	_unit = sceneName.hasPrefixIgnoreCase("U00") ? (Unit *)&unit00 :
	        sceneName.hasPrefixIgnoreCase("U01") ? (Unit *)&unit01 :
	        sceneName.hasPrefixIgnoreCase("U02") ? (Unit *)&unit02 : nullptr;
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
		scene.syncState(s);
		interaction.syncState(s);
		collision.syncState(s);
		_player.syncState(s);
		_inventory->syncState(s);
		if (_unit)
			_unit->syncState(s);
		_pendingLoad.clear();
		if (_unit)
			_unit->start(false, false);
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

	// A held item goes back to the bar before the unit changes (E-0251)
	if (!_interaction->heldItem().empty()) {
		_inventory->add(_interaction->heldItem() + "P");
		_interaction->holdItem("");
	}
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
	_scene->update(stepMs / 1000.0f);
	_collision->refresh();
	_sound->updateVolumes(_player.eye);
	_interaction->eye = _player.eye;
}

void X3DEngine::frame(bool input) {
	if (!_devClicks.empty() && _system->getMillis() - _devStart >= (uint32)_devClicks[2]) {
		_mouse = Common::Point(_devClicks[0], _devClicks[1]);
		_clickNow = true;
		for (int i = 0; i < 3; i++)
			_devClicks.remove_at(0);
	}

	// Due commands, up to the first click (one click per frame)
	while (input && !_clickNow && !_devCommands.empty() && _system->getMillis() - _devStart >= (uint32)atoi(_devCommands[0].c_str())) {
		const Common::String c = _devCommands.remove_at(0);
		debug(1, "dev command %s: %s", c.c_str(), command(c.substr(c.findFirstOf(':') + 1)).c_str());
	}

	Common::Event e;
	while (_system->getEventManager()->pollEvent(e)) {
		if (e.type == Common::EVENT_MOUSEMOVE) {
			_mouse = e.mouse;
			_hoverNow = true;
			continue;
		}
		if (e.type == Common::EVENT_LBUTTONDOWN) {
			debug(1, "click %d,%d", e.mouse.x, e.mouse.y);
			_mouse = e.mouse;
			_clickNow = true;
			continue;
		}
		if (e.type != Common::EVENT_KEYDOWN && e.type != Common::EVENT_KEYUP)
			continue;
		const bool down = e.type == Common::EVENT_KEYDOWN;
		debug(3, "key %d %s", e.kbd.keycode, down ? "down" : "up");
		if (!down) {
			debug(1, "camera %g,%g,%g,%g,%g", _player.eye.x(), _player.eye.y(), _player.eye.z(), _player.yaw, _player.pitch);
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
			if (down && input && !_suspended)
				_escapeNow = true;
			break;
		default: break;
		}
	}

	// Logic runs in fixed steps; rendering interpolates the camera between the last two
	// (movement.md, Engine model)
	const uint32 stepMs = 1000 / kStepsPerSecond;
	const uint32 now = _system->getMillis();
	_pending += now - _last;
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
			debug(2, "pick %s at depth %g: hotspot %d", names[0].c_str(), depth, _hotspot);
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
	_scene->draw(camera, _renderer->width(), _renderer->height());
	_inventory->draw(*_renderer, x2d);
	if (_unit)
		_unit->draw();
	_renderer->present();
	_system->delayMillis(1);

	_frames++;
	if (now - _fpsStart >= 5000) {
		debug(2, "%u frames per second", _frames * 1000 / (now - _fpsStart));
		_frames = 0;
		_fpsStart = now;
	}

	// Escape in a game: "Do you want to save?" over the frozen scene (ui.md, Escape)
	if (_escapeNow) {
		_escapeNow = false;
		const Common::String held = _interaction->heldItem();
		if (!held.empty()) {
			_inventory->add(held + "P");
			_interaction->holdItem("");
		}
		if (_unit && !_unit->gameStarted()) {
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
	if (len == 0)
		return;
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

Common::String X3DEngine::runMenu(const Common::String &name, MenuList *list, const Common::String &placeholder) {
	Frame frame;
	if (!frame.load(name))
		return "escape";
	if (!placeholder.empty())
		frame.setText(placeholder, true);
	if (list)
		frame.setList(list->rows, list->selected);
	const int x2d = (_renderer->width() - 640) / 2;
	CursorMan.showMouse(true);
	Common::String result;
	while (result.empty() && !shouldQuit()) {
		Common::Event e;
		while (_system->getEventManager()->pollEvent(e)) {
			if (e.type == Common::EVENT_MOUSEMOVE || e.type == Common::EVENT_LBUTTONDOWN)
				_mouse = e.mouse;
			// A click acts on press (ui.md, Events)
			if (e.type == Common::EVENT_LBUTTONDOWN) {
				const Common::Point p(_mouse.x - x2d, _mouse.y);
				const int row = list ? frame.listRowAt(p) : -1;
				if (row >= 0) {
					list->selected = row;
					frame.selectRow(row);
					if (row < (int)list->names.size())
						frame.setText(list->names[row]);
				} else {
					result = frame.commandAt(frame.viewAt(p));
				}
			}
			if (e.type == Common::EVENT_KEYDOWN) {
				if (e.kbd.keycode == Common::KEYCODE_ESCAPE)
					result = "escape";
				else if (e.kbd.keycode == Common::KEYCODE_RETURN || e.kbd.keycode == Common::KEYCODE_KP_ENTER)
					result = "enter";
				else if (e.kbd.keycode == Common::KEYCODE_BACKSPACE)
					frame.backspace();
				else if (e.kbd.ascii >= 32 && e.kbd.ascii < 127)
					frame.type(e.kbd.ascii);
			}
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
	_menuText = frame.text();
	debug(1, "menu %s: %s", name.c_str(), result.c_str());
	return result;
}

Common::String X3DEngine::optionMenu() {
	for (;;) {
		const Common::String c = runMenu("Option");
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
		} else if (!c.empty() && c != "escape" && c != "enter") {
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
		_practice = false;
		_inventory->clear();
		gotoScene(scene);
	} else if (command == "OptionEntrenement") {
		_practice = true;
		_inventory->clear();
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
			// The object's centre on screen, with the render projection
			const Math::Vector3d d = _scene->surfacePoint(a[1], _player.eye) - _player.eye;
			const float yaw = _player.yaw, pitch = _player.pitch;
			const Math::Vector3d right(-sinf(yaw), -cosf(yaw), 0);
			const Math::Vector3d up(cosf(pitch) * cosf(yaw), -cosf(pitch) * sinf(yaw), sinf(pitch));
			const Math::Vector3d fwd(sinf(pitch) * cosf(yaw), -sinf(pitch) * sinf(yaw), -cosf(pitch));
			const float z = Math::Vector3d::dotProduct(d, fwd);
			if (z <= 0)
				return "behind the camera";
			const float ky = (4.0f / 3.0f) / tan(_player.fov * M_PI / 360.0), kx = ky * _renderer->height() / _renderer->width();
			const float cx = _renderer->width() / 2.0f, cy = _renderer->height() / 2.0f;
			_mouse = Common::Point(cx + cx * Math::Vector3d::dotProduct(d, right) * kx / z,
			                       cy - cy * Math::Vector3d::dotProduct(d, up) * ky / z);
		}
		_clickNow = true;
		return Common::String::format("click %d,%d", _mouse.x, _mouse.y);
	}
	if (c == "save" && a.size() >= 2)
		return saveGameState(atoi(a[1].c_str()), "console").getCode() == Common::kNoError ? "saved" : "save failed";
	if (c == "load" && a.size() >= 2)
		return loadGameState(atoi(a[1].c_str())).getCode() == Common::kNoError ? "loading" : "load failed";
	if (c == "savemenu") {
		saveMenu();
		return "ok";
	}
	if (c == "loadmenu")
		return loadMenu() ? "loaded" : "no load";
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
			_scene->ambient[k] = MAX(0, (int)_scene->ambient[k] - start[k] / (int)n);
		runFor(10);
	}
}

void X3DEngine::playVideo(const Common::String &name, const Common::String &wav) {
	Video::AVIDecoder video;
	if (!video.loadFile(Common::Path("Video/" + name + ".avi"))) {
		warning("Unable to open video %s", name.c_str());
		return;
	}

	video.start();
	_sound->stopAll(); // a video stops every sound (sound.md)

	// The soundtrack is a separate WAV, started right after the video
	Audio::SoundHandle sound;
	Common::File *file = new Common::File();
	if (file->open(Common::Path("Video/" + (wav.empty() ? name : wav) + ".wav"))) {
		Audio::RewindableAudioStream *stream = Audio::makeWAVStream(file, DisposeAfterUse::YES);
		if (stream)
			_mixer->playStream(Audio::Mixer::kSFXSoundType, &sound, stream);
	} else {
		delete file;
	}

	bool skip = false;
	while (!shouldQuit() && !skip && !video.endOfVideo()) {
		Common::Event e;
		while (_system->getEventManager()->pollEvent(e)) {
			if (e.type == Common::EVENT_KEYDOWN &&
			    (e.kbd.keycode == Common::KEYCODE_RETURN || e.kbd.keycode == Common::KEYCODE_ESCAPE))
				skip = true;
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

	_mixer->stopHandle(sound);
}

} // End of namespace X3D
