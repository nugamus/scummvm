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
#include "common/formats/ini-file.h"
#include "common/memstream.h"
#include "common/system.h"

#include "engines/util.h"

#include "graphics/font.h"
#include "graphics/fonts/ttf.h"
#include "graphics/pixelformat.h"

#include "video/mpegps_decoder.h"

#include "gilbert/database.h"
#include "gilbert/detection.h"
#include "gilbert/gilbert.h"
#include "gilbert/menu.h"
#include "gilbert/sound.h"

namespace Gilbert {

// The clip rectangle of every picture draw (boot.md "Conventions").
static const Common::Rect kClip(64, 50, 576, 430);
// Menu tick (boot.md "Main loop", Q-0207).
static const uint32 kTickMs = 16;
// The loading panel's busy wait (boot.md "Loading panel"); its length on the original's
// machines is not known (Q-0200).
static const uint32 kLoadingWaitMs = 250;

// Everything the game reads is below Data (boot.md "Conventions"): the detection's
// directory globs put Data in the search path.
GilbertEngine::GilbertEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst), _gameDesc(gameDesc) {
}

GilbertEngine::~GilbertEngine() {
	delete _menu;
	delete _logic;
	delete _sound;
	for (auto &f : _fonts)
		delete f._value;
}

Common::Error GilbertEngine::run() {
	const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
	initGraphics(640, 480, &format);
	_screen.create(640, 480, format);
	_clip = kClip;
	_sound = new Sound(_mixer);
	_menu = new Menu(this);
	_logic = new Logic(this);
	loadSettings();
	loadLanguage();
	_mouse = Common::Point(320, 240);

	boot();

	uint32 next = _system->getMillis();
	while (!shouldQuit()) {
		pollEvents();
		if (_menu->tick() == Menu::kQuit) {
			exitGame();
			return Common::kNoError;
		}
		next += kTickMs;
		const uint32 now = _system->getMillis();
		if (next > now)
			_system->delayMillis(next - now);
		else
			next = now;
	}
	saveSettings();
	return Common::kNoError;
}

// boot::Run (boot.md "The boot").
void GilbertEngine::boot() {
	// The CD check: ScummVM runs from the game's folder, which detection found.
	if (!Common::File::exists("misc/gilbert.nfo"))
		warning("Gilbert: Data/misc/gilbert.nfo is missing");

	for (int i = 0; i < 2; i++) {
		clear();
		present();
	}
	clear();
	playFilm("logo1.mpg");
	for (int i = 0; i < 2; i++) {
		clear();
		present();
	}
	clear();
	playFilm("logo2.mpg");
	for (int i = 0; i < 2; i++) {
		clear();
		present();
	}
	clear();
	if (shouldQuit())
		return;

	_sound->loadList(1, "menu.wxs");
	_sound->loadList(2, "2.wxs");
	applyVolumes();
	_interface1.load("maps/!global/interface1.wxi");
	_map.load("maps/!global/map.wxi");
	_interface2.load("maps/!global/interface2.wxi");
	_cursors.load("maps/!global/cursor.wxi");

	loadingStep(2, 11);
	_inventory.load("maps/!global/inventory.wxi");
	_gilbert.load("anims/gilbert.wxi");
	loadingStep(3, 12);
	loadingStep(4, 13);
	readSlotNames();
	loadingStep(5, 14);
	loadingStep(6, 15);
	checkDatabase();
	// The menu music is opened and looped, and starts on the menu's 10th frame.
	_sound->openStream(Sound::kMusic, "menu1", true, false);
}

// A debugging check (-d2 --debugflags=Load): default.dat reads, and writes back byte for byte.
void GilbertEngine::checkDatabase() {
	if (!DebugMan.isDebugChannelEnabled(kDebugLoad) || gDebugLevel < 2)
		return;
	Common::File f;
	if (!f.open("game/default.dat"))
		return;
	Database db;
	const bool ok = db.load(f);
	Common::MemoryWriteStreamDynamic out(DisposeAfterUse::YES);
	db.save(out);
	f.seek(0);
	Common::Array<byte> orig(f.size());
	f.read(orig.data(), orig.size());
	const bool same = out.size() == orig.size() && memcmp(out.getData(), orig.data(), orig.size()) == 0;
	debugC(2, kDebugLoad, "Database check: load %s, %d bytes written, %s", ok ? "ok" : "FAILED",
	       (int)out.size(), same ? "identical" : "DIFFERENT");
}

// boot::LoadingStep (boot.md "Loading panel").
void GilbertEngine::loadingStep(int step, uint line) {
	drawPicture(_interface2[0x3a], 235, 178);
	static const struct {
		int item, x;
	} lamps[] = { { 0x60, 251 }, { 0x61, 296 }, { 0x62, 341 } };
	static const byte lit[7] = { 0, 0, 3, 2, 6, 4, 4 }; // bit 0 red, 1 orange, 2 green
	for (int i = 0; i < 3; i++)
		if (lit[step] & (1 << i))
			drawPicture(_interface2[lamps[i].item], lamps[i].x, 244);
	const Common::U32String text = languageLine(line);
	const int x = 320 - textWidth(text, 8) / 2;
	drawText(_screen, text, x, 201, 8, kColourBlack);
	drawText(_screen, text, x, 200, 8, kColourTan);
	present();
	const uint32 end = _system->getMillis() + kLoadingWaitMs;
	while (!shouldQuit() && _system->getMillis() < end) {
		pollEvents();
		_system->delayMillis(10);
	}
}

// movie::Play (boot.md "Films").
void GilbertEngine::playFilm(const Common::String &name, bool fromIntro) {
	_sound->stopAll();
	Common::File *file = new Common::File();
	if (!file->open(Common::Path("mpg/").appendComponent(name))) {
		warning("Gilbert: no film %s", name.c_str());
		delete file;
		return;
	}
	Video::MPEGPSDecoder decoder;
	if (!decoder.loadStream(file)) {
		warning("Gilbert: cannot play %s (needs the mpeg2 component)", name.c_str());
		return;
	}
	decoder.setOutputPixelFormat(_screen.format);
	const bool fullscreen = _settings.fullscreenVideo;
	const bool logo = name == "logo1.mpg" || name == "logo2.mpg" || name == "logo3.mpg";
	if (fullscreen) {
		clear();
		present();
		clear();
	} else if (!logo) {
		drawPicture(_interface2[0x97], 64, 50);
	}
	decoder.start();
	_escHeld = false;
	while (!shouldQuit() && !decoder.endOfVideo()) {
		pollEvents();
		if (decoder.needsUpdate()) {
			const Graphics::Surface *frame = decoder.decodeNextFrame();
			if (frame) {
				if (fullscreen)
					_screen.blitFrom(*frame, Common::Rect(frame->w, frame->h), Common::Rect(640, 480));
				else
					_screen.blitFrom(*frame, Common::Point(127, 80));
				present();
			}
			if (_escHeld)
				break;
		}
		_system->delayMillis(5);
	}
	decoder.close();
	if (fullscreen) {
		clear();
		present();
		clear();
	}
	if (fromIntro)
		_menu->musicAfterFilm();
	// Room music restarts here once rooms exist (boot.md "Films" step 5).
}

bool GilbertEngine::newGame() {
	Common::File f;
	if (!f.open("game/default.dat") || !_logic->load(f)) {
		warning("Gilbert: cannot load Data/game/default.dat");
		return false;
	}
	_logic->startNewGame();
	return true;
}

void GilbertEngine::gotoWalkmap(uint32 id, int x, int y, int direction) {
	debugC(1, kDebugScript, "GotoWalkmap %d at (%d, %d) facing %d", id, x, y, direction);
	_gilbertPos = Common::Point(x, y);
}

void GilbertEngine::gotoCua(uint32 id) {
	debugC(1, kDebugScript, "GotoCUA %d", id);
}

void GilbertEngine::showDialog() {
	debugC(1, kDebugScript, "Dialog: %s", _logic->dialogTitle().c_str());
}

void GilbertEngine::playWave(int list, int index, bool loop) {
	_sound->playWave(list, index, loop);
}

void GilbertEngine::playStream(const Common::String &name, bool loop, int kind) {
	debugC(1, kDebugSound, "Stream %s loop %d kind %d", name.c_str(), loop, kind);
}

void GilbertEngine::newTopic(bool shown) {
	_sound->playWave(1, 10);
}

// boot::Exit (boot.md "Exit").
void GilbertEngine::exitGame() {
	saveSettings();
	_settings.fullscreenVideo = false;
	for (int i = 0; i < 2; i++) {
		clear();
		present();
	}
	clear();
	playFilm("logo3.mpg");
	_sound->stopAll();
	quitGame();
}

void GilbertEngine::clear() {
	_screen.fillRect(Common::Rect(640, 480), 0);
}

void GilbertEngine::present() {
	_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, 640, 480);
	_system->updateScreen();
}

void GilbertEngine::drawPicture(Picture *pic, int x, int y) {
	if (!pic)
		return;
	pic->last = Common::Rect(x, y, x + pic->surface.w, y + pic->surface.h);
	Common::Rect dst = pic->last;
	dst.clip(_clip);
	if (dst.isEmpty())
		return;
	const Common::Rect src(dst.left - x, dst.top - y, dst.right - x, dst.bottom - y);
	if (pic->transparent)
		_screen.transBlitFrom(pic->surface, src, Common::Point(dst.left, dst.top), pic->key);
	else
		_screen.blitFrom(pic->surface, src, Common::Point(dst.left, dst.top));
}

void GilbertEngine::drawSurface(const Graphics::ManagedSurface &src, const Common::Rect &srcRect, int x, int y) {
	Common::Rect dst(x, y, x + srcRect.width(), y + srcRect.height());
	dst.clip(_clip);
	if (dst.isEmpty())
		return;
	const Common::Rect s(srcRect.left + dst.left - x, srcRect.top + dst.top - y,
	                     srcRect.left + dst.right - x, srcRect.top + dst.bottom - y);
	_screen.transBlitFrom(src, s, Common::Point(dst.left, dst.top), 0);
}

void GilbertEngine::fillAlpha(const Common::Rect &r, uint32 rgb, int alpha) {
	Common::Rect area = r;
	area.clip(Common::Rect(640, 480));
	const byte cr = rgb >> 16, cg = (rgb >> 8) & 0xFF, cb = rgb & 0xFF;
	for (int y = area.top; y < area.bottom; y++) {
		uint16 *p = (uint16 *)_screen.getBasePtr(area.left, y);
		for (int x = area.left; x < area.right; x++, p++) {
			byte r0, g0, b0;
			_screen.format.colorToRGB(*p, r0, g0, b0);
			*p = _screen.format.RGBToColor(r0 + (cr - r0) * alpha / 255, g0 + (cg - g0) * alpha / 255,
			                               b0 + (cb - b0) * alpha / 255);
		}
	}
}

const Graphics::Font *GilbertEngine::font(int size, bool bold) {
	const int key = size * 2 + (bold ? 1 : 0);
	if (!_fonts.contains(key)) {
		// Arial at 96 dpi; Liberation Sans has Arial's metrics.
		Graphics::Font *f = Graphics::loadTTFFontFromArchive(bold ? "LiberationSans-Bold.ttf" : "LiberationSans-Regular.ttf",
		                                                     size, Graphics::kTTFSizeModeCharacter, 96, 96,
		                                                     Graphics::kTTFRenderModeMonochrome);
		if (!f)
			error("Gilbert: cannot load Liberation Sans from fonts.dat");
		_fonts[key] = f;
	}
	return _fonts[key];
}

int GilbertEngine::textWidth(const Common::U32String &text, int size, bool bold) {
	return font(size, bold)->getStringWidth(text);
}

void GilbertEngine::drawText(Graphics::ManagedSurface &dst, const Common::U32String &text, int x, int y, int size, uint32 rgb, bool bold) {
	const uint32 colour = dst.format.RGBToColor(rgb >> 16, (rgb >> 8) & 0xFF, rgb & 0xFF);
	font(size, bold)->drawString(&dst, text, x, y, dst.w - x, colour);
}

Common::U32String GilbertEngine::fromWindows1252(const Common::String &s) {
	return Common::U32String(s, Common::kWindows1252);
}

void GilbertEngine::loadLanguage() {
	Common::File f;
	if (!f.open("misc/language.txt")) {
		warning("Gilbert: Data/misc/language.txt is missing");
		return;
	}
	Common::String line;
	while (!f.eos()) {
		const char c = f.readByte();
		if (f.eos())
			break;
		if (c == '\r') {
			line.trim();
			_language.push_back(line);
			line.clear();
		} else {
			line += c;
		}
	}
}

Common::U32String GilbertEngine::languageLine(uint n) const {
	return n < _language.size() ? fromWindows1252(_language[n]) : Common::U32String();
}

void GilbertEngine::loadSettings() {
	_settings.fullscreenVideo = ConfMan.hasKey("fullscreen_video") && ConfMan.getBool("fullscreen_video");
	_settings.installationType = ConfMan.hasKey("installation_type") ? ConfMan.getInt("installation_type") : -1;
	const int sound = ConfMan.hasKey("sound_volume") ? ConfMan.getInt("sound_volume") : 4;
	const int music = ConfMan.hasKey("music_volume") ? ConfMan.getInt("music_volume") : 5;
	_settings.soundVolume = sound >= 1 && sound <= 6 ? sound : 4;
	_settings.musicVolume = music >= 1 && music <= 6 ? music : 5;
}

void GilbertEngine::saveSettings() {
	ConfMan.setBool("fullscreen_video", _settings.fullscreenVideo);
	ConfMan.setInt("sound_volume", _settings.soundVolume);
	ConfMan.setInt("music_volume", _settings.musicVolume);
	ConfMan.flushToDisk();
}

void GilbertEngine::applyVolumes() {
	_sound->setVolumes(_settings.musicVolume, _settings.soundVolume);
}

void GilbertEngine::readSlotNames() {
	Common::INIFile ini;
	ini.allowNonEnglishCharacters();
	const bool ok = ini.loadFromFile("gilbert.ini");
	for (int i = 1; i <= 50; i++) {
		Common::String name;
		if (!ok || !ini.getKey("name", Common::String::format("SLOT%d", i), name))
			name = "default";
		_slotNames[i] = name;
	}
}

void GilbertEngine::pollEvents() {
	Common::Event event;
	while (_eventMan->pollEvent(event)) {
		switch (event.type) {
		case Common::EVENT_MOUSEMOVE:
			_mouse = event.mouse;
			break;
		case Common::EVENT_LBUTTONDOWN:
			_mouse = event.mouse;
			_leftPress = true;
			break;
		case Common::EVENT_LBUTTONUP:
		case Common::EVENT_RBUTTONUP:
			// Releasing any button clears the button state (boot.md "Mouse").
			_leftPress = false;
			break;
		case Common::EVENT_KEYDOWN:
			if (event.kbd.keycode == Common::KEYCODE_ESCAPE)
				_escHeld = true;
			_keys.push_back(event.kbd);
			break;
		case Common::EVENT_KEYUP:
			if (event.kbd.keycode == Common::KEYCODE_ESCAPE)
				_escHeld = false;
			break;
		default:
			break;
		}
	}
}

bool GilbertEngine::takeLeftPress() {
	const bool p = _leftPress;
	_leftPress = false;
	return p;
}

Common::Array<Common::KeyState> GilbertEngine::takeKeys() {
	Common::Array<Common::KeyState> keys;
	keys.swap(_keys);
	return keys;
}

void GilbertEngine::warpMouse(int x, int y) {
	_mouse = Common::Point(x, y);
	_system->warpMouse(x, y);
}

} // End of namespace Gilbert
