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
#include "common/debug.h"
#include "common/events.h"
#include "common/file.h"
#include "common/system.h"

#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "audio/mixer.h"

#include "engines/util.h"

#include "graphics/cursorman.h"

#include "image/bmp.h"

#include "video/avi_decoder.h"

#include "x3d/collision.h"
#include "x3d/interaction.h"
#include "x3d/inventory.h"
#include "x3d/player.h"
#include "x3d/renderer.h"
#include "x3d/scene.h"
#include "x3d/sound.h"
#include "x3d/talk.h"
#include "x3d/u01.h"
#include "x3d/x3d.h"

namespace X3D {

X3DEngine::X3DEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc) {
}

X3DEngine::~X3DEngine() {
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

	// Development shortcut: start_scene=<file.X3D> in the game's config skips the boot
	// sequence and the scene's entry video
	if (!ConfMan.hasKey("start_scene")) {
		// Boot sequence: docs/engine-spec/boot.md
		showBitmap("2dbit/Intro1.bmp");
		wait(3000);
		showBitmap("2dbit/Intro2.bmp");
		wait(2000);
	}

	// A new game starts at the scene named in App.bin #GAME# (E-0037). The original shows
	// the U00 menu scene and its OptionUser frame first; until that is specified, a new
	// game goes straight there.
	Common::String sceneName = "U01.X3D";
	if (ConfMan.hasKey("start_scene")) {
		sceneName = ConfMan.get("start_scene");
	} else if (Common::SeekableReadStream *game = openBinChunk("App.bin", "#GAME#")) {
		sceneName = game->readString(0, 30);
		delete game;
	}

	while (!shouldQuit() && !sceneName.empty()) {
		_nextScene.clear();
		playScene(sceneName);
		sceneName = _nextScene;
	}
	return Common::kNoError;
}

void X3DEngine::playScene(const Common::String &sceneName) {
	Scene scene(_renderer);
	if (!scene.load(sceneName))
		error("Unable to load scene %s", sceneName.c_str());
	_scene = &scene;

	// Sound (sound.md): mode 0 group volumes, the scene's emitters
	_sound->setGroupVolume(Sound::kAmbient, 85);
	_sound->setGroupVolume(4, 80);
	_sound->setGroupVolume(5, 80);
	_sound->setScale(scene.scale);
	Talk talk(scene, *_sound, scene.dir());
	_talk = &talk;
	Interaction interaction(scene, *_sound, talk);
	_interaction = &interaction;
	// The bar is opened, empty, when the player is chosen (ui.md, Boot to U01)
	Inventory inventory(interaction);
	_inventory = &inventory;
	interaction.inventory = &inventory;

	const bool u01 = sceneName.hasPrefixIgnoreCase("U01");
	U01 unit01(this);
	_u01 = u01 ? &unit01 : nullptr;
	if (_u01)
		_u01->afterLoad();
	interaction.load(scene.dir());

	_player = Player();
	_player.init(scene.scale);
	_player.fov = scene.camera.fov;
	Collision collision;
	collision.build(scene);
	_collision = &collision;

	_keys = Keys();
	_enterHeld = _suspended = _hoverNow = _clickNow = false;
	_hotspot = -1;
	_unitActions.clear();
	_last = _fpsStart = _sceneStart = _system->getMillis();
	_pending = _logicMs = _lastClick = _frames = 0;

	// Development shortcut: dev_click=x,y,ms[;x,y,ms...] clicks at game pixel (x, y) ms
	// after the scene starts, for testing without focus (SDL takes click positions from
	// the real cursor)
	_devClicks.clear();
	if (ConfMan.hasKey("dev_click")) {
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

	// Development shortcut: start_camera=x,y,z,yaw,pitch places the camera anywhere and
	// skips the unit's scripted start
	if (ConfMan.hasKey("start_camera")) {
		sscanf(ConfMan.get("start_camera").c_str(), "%f,%f,%f,%f,%f", &_player.eye.x(),
		       &_player.eye.y(), &_player.eye.z(), &_player.yaw, &_player.pitch);
		if (_u01)
			_u01->start(false);
	} else if (_u01) {
		// start_scene skips the prologue as well as the boot sequence
		_u01->start(true, !ConfMan.hasKey("start_scene"));
	}
	_previous = _player;

	while (!shouldQuit() && _nextScene.empty())
		frame(true);

	_sound->stopAll();
	_scene = nullptr;
	_collision = nullptr;
	_interaction = nullptr;
	_talk = nullptr;
	_inventory = nullptr;
	_u01 = nullptr;
}

void X3DEngine::logicStep(bool input) {
	const uint32 stepMs = 1000 / kStepsPerSecond;
	_previous = _player;
	if (input && !_suspended) {
		const bool handled = _u01 && _u01->input(stepMs / 1000.0f);
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
	if (!_devClicks.empty() && _system->getMillis() - _sceneStart >= (uint32)_devClicks[2]) {
		_mouse = Common::Point(_devClicks[0], _devClicks[1]);
		_clickNow = true;
		for (int i = 0; i < 3; i++)
			_devClicks.remove_at(0);
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
			if (down && input && !_suspended)
				_inventory->toggle();
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
		logicStep(input);
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
	}
	if (interactive && !overBar)
		_interaction->hover(_hotspot, now);

	_scene->draw(camera, _renderer->width(), _renderer->height());
	_inventory->draw(*_renderer, x2d);
	if (_u01)
		_u01->draw();
	_renderer->present();
	_system->delayMillis(1);

	_frames++;
	if (now - _fpsStart >= 5000) {
		debug(2, "%u frames per second", _frames * 1000 / (now - _fpsStart));
		_frames = 0;
		_fpsStart = now;
	}

	// The unit's code: queued click actions, then its per-frame checks. Both may run
	// blocking sequences (nested frames).
	if (input) {
		while (!_unitActions.empty() && !shouldQuit()) {
			const Common::String action = _unitActions.remove_at(0);
			if (!_u01 || !_u01->handle(action))
				warning("Unit action %s is not implemented", action.c_str());
		}
		if (_u01)
			_u01->afterFrame();
	}
}

void X3DEngine::runFor(uint32 ms) {
	const uint32 end = _logicMs + ms;
	do
		frame(false);
	while (_logicMs < end && !shouldQuit() && _nextScene.empty());
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

void X3DEngine::playVideo(const Common::String &name) {
	Video::AVIDecoder video;
	if (!video.loadFile(Common::Path("Video/" + name + ".avi"))) {
		warning("Unable to open video %s", name.c_str());
		return;
	}

	video.start();
	_sound->stopAll(); // a video stops every sound (sound.md)

	// The soundtrack is a separate WAV, started right after the video
	Audio::SoundHandle sound;
	Common::File *wav = new Common::File();
	if (wav->open(Common::Path("Video/" + name + ".wav"))) {
		Audio::RewindableAudioStream *stream = Audio::makeWAVStream(wav, DisposeAfterUse::YES);
		if (stream)
			_mixer->playStream(Audio::Mixer::kSFXSoundType, &sound, stream);
	} else {
		delete wav;
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
