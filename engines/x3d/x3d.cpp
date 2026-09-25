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


#include "image/bmp.h"

#include "video/avi_decoder.h"

#include "x3d/collision.h"
#include "x3d/interaction.h"
#include "x3d/player.h"
#include "x3d/renderer.h"
#include "x3d/sound.h"
#include "x3d/talk.h"
#include "x3d/scene.h"
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

	playScene(sceneName);
	return Common::kNoError;
}

void X3DEngine::playScene(const Common::String &sceneName) {
	Scene scene(_renderer);
	if (!scene.load(sceneName))
		error("Unable to load scene %s", sceneName.c_str());
	// Sound (sound.md): mode 0 group volumes, the scene's emitters, talkers
	_sound->setGroupVolume(Sound::kAmbient, 85);
	_sound->setGroupVolume(4, 80);
	_sound->setGroupVolume(5, 80);
	_sound->setScale(scene.scale);
	Talk talk(scene, *_sound, scene.dir());
	Interaction interaction(scene, *_sound, talk);
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
		talk.addTalker("U01_01", "$$$DUMMY.*01SParle");
		talk.addTalker("U01_02", "$$$DUMMY.*02SParle");
		// The unit's ambient loop, started after the prologue
		_sound->play(Common::Path(scene.dir() + "Sound/U01.WAV"), Sound::kAmbient, 85, true);
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
	uint32 last = _system->getMillis(), pending = 0, frames = 0, fpsStart = last, lastClick = 0, logicMs = 0;
	// Development shortcut: dev_click=x,y,ms[;x,y,ms...] clicks at game pixel (x, y) ms
	// after the scene starts, for testing without focus (SDL takes click positions from
	// the real cursor)
	Common::Array<int> devClicks;
	if (ConfMan.hasKey("dev_click")) {
		const char *c = ConfMan.get("dev_click").c_str();
		int x, y, ms, n;
		while (sscanf(c, "%d,%d,%d%n", &x, &y, &ms, &n) == 3) {
			devClicks.push_back(x);
			devClicks.push_back(y);
			devClicks.push_back(ms);
			c += n;
			if (*c == ';')
				c++;
		}
	}
	const uint32 sceneStart = last;
	while (!shouldQuit()) {
		if (!devClicks.empty() && _system->getMillis() - sceneStart >= (uint32)devClicks[2]) {
			mouse = Common::Point(devClicks[0], devClicks[1]);
			clickNow = true;
			devClicks.remove_at(0);
			devClicks.remove_at(0);
			devClicks.remove_at(0);
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
				_sound->emit(Sound::kEffectsEmitter, "SAUT.WAV", player.eye, false);
			logicMs += stepMs;
			talk.tick(logicMs);
			scene.update(stepMs / 1000.0f);
			_sound->updateVolumes(player.eye);
			interaction.eye = player.eye;

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
			if (scene.pick(camera, _renderer->width(), _renderer->height(), mouse.x, mouse.y, model, object, depth) &&
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

		scene.draw(camera, _renderer->width(), _renderer->height());
		_renderer->present();
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

	_renderer->clear();
	_renderer->drawImage(*bmp.getSurface(), (_renderer->width() - 640) / 2, 0, false);
	_renderer->present();
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
