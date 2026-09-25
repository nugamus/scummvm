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

#include "graphics/screen.h"
#include "graphics/tinygl/tinygl.h"

#include "image/bmp.h"

#include "video/avi_decoder.h"

#include "x3d/collision.h"
#include "x3d/interaction.h"
#include "x3d/player.h"
#include "x3d/scene.h"
#include "x3d/x3d.h"

namespace X3D {

X3DEngine::X3DEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc) {
}

X3DEngine::~X3DEngine() {
	delete _screen;
}

Common::Error X3DEngine::run() {
	// All original paths are relative to Data/, which the detector (kADFlagMatchFullPaths)
	// has already added to SearchMan
	const Graphics::PixelFormat format = g_system->getSupportedFormats().front();
	initGraphics(640, 480, &format);
	_screen = new Graphics::Screen();

	// Development shortcut: start_scene=<file.X3D> in the game's config skips the boot
	// sequence and the scene's entry video
	if (!ConfMan.hasKey("start_scene")) {
		// Boot sequence: docs/engine-spec/boot.md
		showBitmap("2dbit/Intro1.bmp");
		wait(3000);
		showBitmap("2dbit/Intro2.bmp");
		wait(2000);

		// The original shows the U00 menu scene and its OptionUser frame here. Until that is
		// specified, a new game goes straight to U01, whose normal entry plays the prologue.
		playVideo("Prologue");
	}

	// A new game starts at the scene named in App.bin #GAME# (E-0037)
	Common::String sceneName = "U01.X3D";
	if (ConfMan.hasKey("start_scene")) {
		sceneName = ConfMan.get("start_scene");
	} else if (Common::SeekableReadStream *game = openBinChunk("App.bin", "#GAME#")) {
		sceneName = game->readString(0, 30);
		delete game;
	}

	// U01 draws ~5,000 immediate-mode faces a frame, more than the default 5 MB of draw calls
	TinyGL::createContext(_screen->w, _screen->h, _screen->format, 256, false, false, 64 * 1024 * 1024);
	playScene(sceneName);
	TinyGL::destroyContext();
	return Common::kNoError;
}

void X3DEngine::playScene(const Common::String &sceneName) {
	Scene scene;
	if (!scene.load(sceneName))
		error("Unable to load scene %s", sceneName.c_str());
	Interaction interaction(this, scene);
	interaction.load(scene.dir());

	// U01 as its scripted entry leaves it (movement.md, U01 hand-over): the mayor holds out
	// the card (GiveCard) and the player can only click until TakeCard. The entry itself
	// is not implemented yet.
	// ponytail: only U01's values; other units need their Uxx_Start state
	Player player;
	player.init(scene.scale);
	player.fov = scene.camera.fov;
	Common::StringArray noCollision;
	const bool u01 = sceneName.hasPrefixIgnoreCase("U01");
	bool takingCard = false;
	if (u01) {
		player.eye.set(-466.36f, -452.495f, 30.48f);
		player.yaw = 4.7f;
		player.sphereOffset = 37.0f;
		player.canMove = player.canTurn = false;
		noCollision.push_back("Box203");
		scene.hideObject("Box203");
		scene.hideObject("Cylinder07");
		scene.playClip("*U01_02", "Anim/U01_02/Action03.A3D");
	}
	// Development shortcut: start_camera=x,y,z,yaw,pitch places the camera anywhere
	if (ConfMan.hasKey("start_camera")) {
		sscanf(ConfMan.get("start_camera").c_str(), "%f,%f,%f,%f,%f", &player.eye.x(),
		       &player.eye.y(), &player.eye.z(), &player.yaw, &player.pitch);
		player.canMove = player.canTurn = true;
	}

	Collision collision;
	collision.build(scene, noCollision);

	// Logic runs in fixed steps; rendering runs every loop iteration and interpolates
	// the camera between the last two steps (movement.md, Engine model)
	const uint32 stepMs = 1000 / kStepsPerSecond;
	Keys keys;
	Player previous = player;
	Camera camera;
	int hotspot = -1;
	Common::Point mouse(320, 240);
	bool hoverNow = false, clickNow = false;
	uint32 last = _system->getMillis(), pending = 0, frames = 0, fpsStart = last, lastClick = 0;
	// Development shortcut: dev_click=x,y,ms clicks at game pixel (x, y) ms after the scene
	// starts, for testing without focus (SDL takes click positions from the real cursor)
	int devClick[3] = { -1, -1, -1 };
	if (ConfMan.hasKey("dev_click"))
		sscanf(ConfMan.get("dev_click").c_str(), "%d,%d,%d", &devClick[0], &devClick[1], &devClick[2]);
	const uint32 sceneStart = last;
	while (!shouldQuit()) {
		if (devClick[2] >= 0 && _system->getMillis() - sceneStart >= (uint32)devClick[2]) {
			mouse = Common::Point(devClick[0], devClick[1]);
			clickNow = true;
			devClick[2] = -1;
		}
		Common::Event e;
		while (_system->getEventManager()->pollEvent(e)) {
			if (e.type == Common::EVENT_MOUSEMOVE) {
				mouse = e.mouse;
				hoverNow = true;
				continue;
			}
			if (e.type == Common::EVENT_LBUTTONDOWN) {
				debug(1, "click %d,%d", e.mouse.x, e.mouse.y);
				mouse = e.mouse;
				clickNow = true;
				continue;
			}
			if (e.type != Common::EVENT_KEYDOWN && e.type != Common::EVENT_KEYUP)
				continue;
			const bool down = e.type == Common::EVENT_KEYDOWN;
			if (!down) {
				debug(1, "camera %g,%g,%g,%g,%g", player.eye.x(), player.eye.y(), player.eye.z(), player.yaw, player.pitch);
				hoverNow = true; // the original re-hovers on every key release
			}
			switch (e.kbd.keycode) {
			case Common::KEYCODE_UP: keys.up = down; break;
			case Common::KEYCODE_DOWN: keys.down = down; break;
			case Common::KEYCODE_LEFT: keys.left = down; break;
			case Common::KEYCODE_RIGHT: keys.right = down; break;
			case Common::KEYCODE_PAGEUP: keys.pageUp = down; break;
			case Common::KEYCODE_PAGEDOWN: keys.pageDown = down; break;
			case Common::KEYCODE_LCTRL:
			case Common::KEYCODE_RCTRL: keys.ctrl = down; break;
			default: break;
			}
		}

		const uint32 now = _system->getMillis();
		pending += now - last;
		last = now;
		while (pending >= stepMs) {
			pending -= stepMs;
			previous = player;
			if (player.tick(stepMs / 1000.0f, keys, collision))
				playSound("SAUT.WAV");
			scene.update(stepMs / 1000.0f);

			// TakeCard waits for the card animation, then allows walking (E-0050, E-0057)
			if (takingCard && !scene.clipPlaying("*U01_02")) {
				takingCard = false;
				player.canMove = player.canTurn = true;
			}
		}

		const float alpha = (float)pending / stepMs;
		for (int k = 0; k < 3; k++)
			camera.position[k] = previous.eye.getData()[k] + (player.eye.getData()[k] - previous.eye.getData()[k]) * alpha;
		camera.yaw = previous.yaw + (player.yaw - previous.yaw) * alpha;
		camera.pitch = previous.pitch + (player.pitch - previous.pitch) * alpha;
		camera.fov = player.fov;
		camera.roll = player.roll;

		// Hover and click (interaction.md): clicks closer together than one frame + 10 ms
		// are ignored; nothing counts beyond 4 scene units of depth
		if (clickNow && now - lastClick < 1000 / kStepsPerSecond + 10)
			clickNow = false;
		if (hoverNow || clickNow) {
			hotspot = -1;
			const Scene::Model *model;
			uint object;
			float depth;
			if (scene.pick(camera, _screen->w, _screen->h, mouse.x, mouse.y, model, object, depth) &&
			    depth <= 4 * scene.scale) {
				Common::StringArray names;
				for (int o = object; o >= 0; o = model->file.objects[o].parent)
					names.push_back(model->file.objects[o].name);
				hotspot = interaction.hotspotFor(names);
				debug(2, "pick %s at depth %g: hotspot %d", names[0].c_str(), depth, hotspot);
			}
			hoverNow = false;
		}
		if (clickNow) {
			lastClick = now;
			clickNow = false;
			Common::StringArray unitActions;
			interaction.click(hotspot, unitActions);
			for (const Common::String &action : unitActions) {
				// U01's unit code (interaction.md, Click -> action)
				if (u01 && action.equalsIgnoreCase("TakeCard")) {
					scene.rewindClip("*U01_02");
					takingCard = true;
					interaction.setCursorKind("*U01_02", 3);
				} else {
					warning("Unit action %s is not implemented", action.c_str());
				}
			}
		}
		interaction.hover(hotspot, now);

		scene.draw(camera, _screen->w, _screen->h);
		TinyGL::presentBuffer();
		Graphics::Surface frame;
		TinyGL::getSurfaceRef(frame);
		_system->copyRectToScreen(frame.getPixels(), frame.pitch, 0, 0, frame.w, frame.h);
		_system->updateScreen();
		_system->delayMillis(1);

		frames++;
		if (now - fpsStart >= 5000) {
			debug(2, "%u frames per second", frames * 1000 / (now - fpsStart));
			frames = 0;
			fpsStart = now;
		}
	}
}

void X3DEngine::showBitmap(const Common::Path &path) {
	Common::File file;
	Image::BitmapDecoder bmp;
	if (!file.open(path) || !bmp.loadStream(file))
		error("Unable to load %s", path.toString().c_str());

	_screen->simpleBlitFrom(*bmp.getSurface());
	_screen->update();
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

void X3DEngine::playSound(const Common::String &name) {
	Common::File *file = new Common::File();
	if (!file->open(Common::Path(name))) {
		delete file;
		warning("Unable to open sound %s", name.c_str());
		return;
	}
	Audio::RewindableAudioStream *stream = Audio::makeWAVStream(file, DisposeAfterUse::YES);
	if (stream)
		_mixer->playStream(Audio::Mixer::kSFXSoundType, nullptr, stream);
}

void X3DEngine::playVideo(const Common::String &name) {
	Video::AVIDecoder video;
	if (!video.loadFile(Common::Path("Video/" + name + ".avi"))) {
		warning("Unable to open video %s", name.c_str());
		return;
	}

	video.start();

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
				_screen->simpleBlitFrom(*frame);
				_screen->update();
			}
		}
		_system->delayMillis(5);
	}

	_mixer->stopHandle(sound);
}

} // End of namespace X3D
