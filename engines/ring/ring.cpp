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
#include "common/fs.h"
#include "common/system.h"

#include "engines/util.h"


#include "ring/detection.h"
#include "ring/movie.h"
#include "ring/resources.h"
#include "ring/ring.h"

namespace Ring {

RingEngine::RingEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst), _gameDescription(gameDesc) {
	const Common::FSNode gameDataDir(ConfMan.getPath("path"));
	SearchMan.addDirectory(gameDataDir, 0, 4);
}

RingEngine::~RingEngine() {
}

bool RingEngine::hasFeature(EngineFeature f) const {
	return f == kSupportsReturnToLauncher;
}

void RingEngine::present() {
	g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, _screen.w, _screen.h);
	g_system->updateScreen();
}

void RingEngine::pollEvents(uint32 ms) {
	Common::Event event;
	while (g_system->getEventManager()->pollEvent(event)) {
		if (event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_ESCAPE)
			_escapeDown = true;
		else if (event.type == Common::EVENT_KEYUP && event.kbd.keycode == Common::KEYCODE_ESCAPE)
			_escapeDown = false;
	}
	if (ms)
		g_system->delayMillis(ms);
}

void RingEngine::wait(uint32 ms) {
	uint32 start = g_system->getMillis();
	while (g_system->getMillis() - start < ms && !_escapeDown && !shouldQuit())
		pollEvents(5);
}

// A 24-bit BMP's pixel bytes as stored (rows bottom-up, padded); DisFad works on them.
static bool readBmp24(const Common::Path &path, int &w, int &h, Common::Array<byte> &bytes) {
	Common::File f;
	if (!f.open(path) || f.readUint16BE() != MKTAG16('B', 'M'))
		return false;
	f.skip(8);
	uint32 offset = f.readUint32LE();
	f.readUint32LE();
	w = f.readSint32LE();
	h = f.readSint32LE();
	f.readUint16LE();
	if (f.readUint16LE() != 24 || w <= 0 || h <= 0)
		return false;
	uint32 stride = (w * 3 + 3) & ~3;
	bytes.resize(stride * h);
	f.seek(offset);
	return f.read(bytes.data(), bytes.size()) == bytes.size();
}

bool RingEngine::fade(const Common::String &from, const Common::String &to, uint frames, uint32 holdMs) {
	// DisFad with load-from 'e' and kind 2: the pictures come from DATA\SY\IMAGE (spec/resources.md).
	Common::Path dir = Common::Path("DATA").appendComponent("SY").appendComponent("IMAGE");
	int w1, h1, w2, h2;
	Common::Array<byte> a, b;
	if (!readBmp24(dir.appendComponent(from), w1, h1, a) || !readBmp24(dir.appendComponent(to), w2, h2, b) ||
		w1 != w2 || h1 != h2 || !frames) {
		warning("Ring: DisFad %s -> %s failed", from.c_str(), to.c_str());
		return false;
	}
	uint32 stride = (w1 * 3 + 3) & ~3;
	Common::Array<int16> step(a.size());
	for (uint32 i = 0; i < a.size(); i++)
		step[i] = (int16)(((int)a[i] - (int)b[i]) / (int)frames);

	auto show = [&](const Common::Array<byte> &pix) {
		for (int y = 0; y < h1 && 16 + y < _screen.h; y++) {
			const byte *src = &pix[(h1 - 1 - y) * stride];
			uint16 *dst = (uint16 *)_screen.getBasePtr(0, 16 + y);
			for (int x = 0; x < w1 && x < _screen.w; x++)
				dst[x] = _screen.format.RGBToColor(src[x * 3 + 2], src[x * 3 + 1], src[x * 3]);
		}
		present();
	};

	// The animation clock runs at 25 frames per second; each new frame subtracts one step.
	uint32 start = g_system->getMillis();
	for (uint f = 1; f <= frames && !_escapeDown && !shouldQuit(); f++) {
		for (uint32 i = 0; i < a.size(); i++)
			a[i] = (byte)(a[i] - step[i]);
		show(a);
		while (g_system->getMillis() - start < f * 40 && !shouldQuit())
			pollEvents(2);
	}
	show(b);
	wait(holdMs);
	return true;
}

void RingEngine::showStartupScreens() {
	// 0x4314a0 (spec/boot.md, "Start-up screens")
	while (_escapeDown && !shouldQuit())
		pollEvents(10);
	playMovie(this, Common::Path("DATA").appendComponent("SY").appendComponent("PLA").appendComponent("LOGO.CNM"), 0);
	static const struct {
		const char *from, *to;
		uint32 hold;
	} steps[] = {
		{ "beg0.bmp", "beg1.bmp", 3000 }, { "beg1.bmp", "beg0.bmp", 0 },
		{ "beg0.bmp", "beg2.bmp", 3000 }, { "beg2.bmp", "beg0.bmp", 0 },
		{ "beg0.bmp", "beg3.bmp", 3000 }, { "beg3.bmp", "beg0.bmp", 0 },
		{ "beg0.bmp", "beg4.bmp", 3000 }, { "beg4.bmp", "beg0.bmp", 0 },
		{ "beg0.bmp", "beg5.bmp", 3000 }, { "beg5.bmp", "beg0.bmp", 0 },
		{ "beg0.bmp", "beg6.bmp", 6000 },
	};
	for (const auto &s : steps) {
		pollEvents();
		if (_escapeDown || shouldQuit())
			break;
		fade(s.from, s.to, 20, s.hold);
	}
}

Common::Error RingEngine::run() {
	Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
	initGraphics(640, 480, &format);
	_screen.create(640, 480, format);

	switch (_gameDescription->language) {
	case Common::FR_FRA: _languageFolder = "FRA"; break;
	case Common::DE_DEU: _languageFolder = "GER"; break;
	case Common::NL_NLD: _languageFolder = "HOL"; break;
	case Common::IT_ITA: _languageFolder = "ITA"; break;
	case Common::ES_ESP: _languageFolder = "SPA"; break;
	case Common::SV_SWE: _languageFolder = "SWE"; break;
	default: _languageFolder = "ENG"; break;
	}
	_resources.reset(new Resources(format));
	_resources->setLanguageFolder(_languageFolder);

	// The window shows nothing for 3 s (timer 100), the set-up runs, then 2 s more (timer 101).
	present();
	wait(3000);
	_resources->openArchive(kZoneSY);
	wait(2000);
	_escapeDown = false;

	showStartupScreens();

	// StartMenu(0) shows puzzle 90000 (games/ring/docs, to come); for now its background.
	_screen.clear();
	Common::ScopedPtr<Image> menu(_resources->loadImage(kZoneSY, "GenMen.bmp", true));
	if (menu)
		menu->draw(_screen, 0, 16, 1);
	present();
	while (!shouldQuit())
		pollEvents(10);
	return Common::kNoError;
}

} // End of namespace Ring
