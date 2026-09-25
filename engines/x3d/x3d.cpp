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
	{
		Scene scene;
		if (!scene.load(sceneName))
			error("Unable to load scene %s", sceneName.c_str());

		// U01's normal entry holds this first shot for 1.5 s (E-0041)
		// ponytail: only U01's start camera, other units need their Uxx_Start values
		Camera camera = scene.camera;
		if (sceneName.hasPrefixIgnoreCase("U01")) {
			camera.position[0] = -258.44f;
			camera.position[1] = -508.20f;
			camera.position[2] = 29.546f;
			camera.yaw = 1.31f;
			camera.pitch = 1.5707960f;
		}
		// Development shortcut: start_camera=x,y,z,yaw,pitch places the camera anywhere
		if (ConfMan.hasKey("start_camera"))
			sscanf(ConfMan.get("start_camera").c_str(), "%f,%f,%f,%f,%f", &camera.position[0],
			       &camera.position[1], &camera.position[2], &camera.yaw, &camera.pitch);

		// Game logic ticks will run here at a fixed rate once there is logic (movement, scripts);
		// rendering stays once per loop iteration
		while (!shouldQuit()) {
			Common::Event e;
			while (_system->getEventManager()->pollEvent(e)) {
			}

			scene.draw(camera, _screen->w, _screen->h);
			TinyGL::presentBuffer();
			Graphics::Surface frame;
			TinyGL::getSurfaceRef(frame);
			_system->copyRectToScreen(frame.getPixels(), frame.pitch, 0, 0, frame.w, frame.h);
			_system->updateScreen();
			_system->delayMillis(10);
		}
	}

	TinyGL::destroyContext();
	return Common::kNoError;
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
