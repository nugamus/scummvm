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
#include "common/savefile.h"
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
#include "gilbert/book.h"
#include "gilbert/cua.h"
#include "gilbert/dialog.h"
#include "gilbert/room.h"
#include "gilbert/sound.h"

namespace Gilbert {

// The clip rectangle of every picture draw (boot.md "Conventions").
static const Common::Rect kClip(64, 50, 576, 430);
// Menu tick (boot.md "Main loop", Q-0207).
static const uint32 kTickMs = 16;
// The loading panel's busy wait (boot.md "Loading panel"); its length on the original's
// machines is not known (Q-0200).
static const uint32 kLoadingWaitMs = 250;

// The slots live in `<target>.ini`, the original's gilbert.ini, in the save folder; until
// the first save the game folder's gilbert.ini (all empty) stands in for it.
static bool loadSlotIni(Common::INIFile &ini, const Common::String &target) {
	ini.allowNonEnglishCharacters();
	Common::ScopedPtr<Common::InSaveFile> in(g_system->getSavefileManager()->openForLoading(target + ".ini"));
	if (in)
		return ini.loadFromStream(*in);
	return ini.loadFromFile("gilbert.ini");
}

// Everything the game reads is below Data (boot.md "Conventions"): the detection's
// directory globs put Data in the search path.
GilbertEngine::GilbertEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst), _gameDesc(gameDesc) {
}

GilbertEngine::~GilbertEngine() {
	delete _menu;
	delete _room;
	delete _cua;
	delete _book;
	delete _dialog;
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
	_room = new Room(this);
	_cua = new CloseUp(this);
	_book = new Book(this);
	_dialog = new DialogBox(this);
	loadSettings();
	loadLanguage();
	_mouse = Common::Point(320, 240);

	boot();
	// A save picked in the launcher.
	if (ConfMan.hasKey("save_slot") && !shouldQuit())
		loadGameState(ConfMan.getInt("save_slot"));

	uint32 next = _system->getMillis();
	uint32 previous = next;
	while (!shouldQuit()) {
		pollEvents();
		// TgMain.DXTimer1Timer (boot.md "Main loop").
		const uint32 now0 = _system->getMillis();
		_logic->tick(now0);
		if (_logic->variable(198) != 0 && _mode != kModeMenu)
			_menu->gameOver();
		switch (_mode) {
		case kModeMenu:
			if (_menu->tick() == Menu::kQuit) {
				exitGame();
				return Common::kNoError;
			}
			break;
		case kModeRoom:
			_room->tick(now0 - previous);
			break;
		case kModeCua:
			_cua->tick();
			break;
		case kModeBook:
			_book->tick();
			break;
		default:
			break;
		}
		previous = now0;
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
	const uint32 filmStart = _system->getMillis();
	int frames = 0;
	_escHeld = false;
	while (!shouldQuit() && !decoder.endOfVideo()) {
		pollEvents();
		if (decoder.needsUpdate()) {
			const Graphics::Surface *frame = decoder.decodeNextFrame();
			if (frame) {
				frames++;
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
	debugC(1, kDebugGraphics, "Film %s: %d frames in %d ms (%d ms long)", name.c_str(), frames, _system->getMillis() - filmStart, decoder.getDuration().msecs());
	decoder.close();
	if (fullscreen) {
		clear();
		present();
		clear();
	}
	if (fromIntro)
		_menu->musicAfterFilm();
	else
		_room->restartMusic();
}

// Saving slot n (boot.md "Save slots"): file=game<n>.dat and the name in the slot list,
// then GESaveFile into `<target>.game<n>.dat`, the original's file byte for byte.
bool GilbertEngine::saveSlot(int n, const Common::String &name) {
	if (n < 1 || n > 50)
		return false;
	const Common::String file = Common::String::format("game%d.dat", n);
	Common::ScopedPtr<Common::OutSaveFile> out(_saveFileMan->openForSaving(_targetName + "." + file, false));
	if (!out)
		return false;
	_logic->save(*out);
	out->finalize();
	Common::INIFile ini;
	loadSlotIni(ini, _targetName);
	ini.setKey("total", "SAVEDGAMES", "50");
	const Common::String section = Common::String::format("SLOT%d", n);
	ini.setKey("file", section, file);
	ini.setKey("name", section, name);
	Common::ScopedPtr<Common::OutSaveFile> iniOut(_saveFileMan->openForSaving(_targetName + ".ini", false));
	if (!iniOut || !ini.saveToStream(*iniOut))
		return false;
	iniOut->finalize();
	_slotNames[n] = name;
	return true;
}

// Loading slot n: the file named in the slot list, then ResetState and GEContinueGame.
bool GilbertEngine::loadSlot(int n) {
	Common::INIFile ini;
	Common::String file;
	if (n < 1 || n > 50 || !loadSlotIni(ini, _targetName) ||
	    !ini.getKey("file", Common::String::format("SLOT%d", n), file) || file.empty())
		return false;
	Common::ScopedPtr<Common::InSaveFile> in(_saveFileMan->openForLoading(_targetName + "." + file));
	if (!in || !_logic->load(*in))
		return false;
	resetState();
	_logic->continueGame();
	return true;
}

bool GilbertEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	return _mode == kModeRoom && _menu->canSave();
}

bool GilbertEngine::canLoadGameStateCurrently(Common::U32String *msg) {
	return _mode == kModeRoom || _mode == kModeMenu;
}

Common::Error GilbertEngine::saveGameState(int slot, const Common::String &desc, bool isAutosave) {
	return saveSlot(slot, desc) ? Common::kNoError : Common::kWritingFailed;
}

Common::Error GilbertEngine::loadGameState(int slot) {
	if (!loadSlot(slot))
		return Common::kReadingFailed;
	_menu->gameLoaded(true);
	return Common::kNoError;
}

void GilbertEngine::resetState() {
	// ResetState (rooms.md "State").
	_room->reset();
	_cua->reset();
	_dialog->close();
	_settings.musicVolume = 5;
	_settings.soundVolume = 4;
	_settings.fullscreenVideo = false;
	applyVolumes();
}

bool GilbertEngine::newGame() {
	Common::File f;
	if (!f.open("game/default.dat") || !_logic->load(f)) {
		warning("Gilbert: cannot load Data/game/default.dat");
		return false;
	}
	resetState();
	_logic->startNewGame();
	return true;
}

void GilbertEngine::gotoWalkmap(uint32 id, int x, int y, int direction) {
	_room->load(id, x, y, direction);
}

void GilbertEngine::refreshWalkmap() {
	_room->refreshObjects();
}

Common::Point GilbertEngine::gilbertPosition() {
	return _room->gilbertPosition();
}

int GilbertEngine::mapWidth() {
	return _room->mapWidth();
}

int GilbertEngine::mapHeight() {
	return _room->mapHeight();
}

int GilbertEngine::mapCell(int x, int y) {
	return _room->mapCell(x, y);
}

void GilbertEngine::walk(int direction) {
	_room->walk(direction);
}

bool GilbertEngine::ctrlHeld() const {
	return (_eventMan->getModifierState() & Common::KBD_CTRL) != 0;
}

void GilbertEngine::drawCursor(int n) {
	Common::Point m = _mouse;
	if (m.x < 72 || m.x > 568 || m.y < 58 || m.y > 422) {
		m.x = CLIP<int16>(m.x, 72, 568);
		m.y = CLIP<int16>(m.y, 58, 422);
		warpMouse(m.x, m.y);
	}
	drawPicture(_cursors[n], m.x - 16, m.y - 16);
}

void GilbertEngine::gotoCua(uint32 id) {
	_cua->load(id);
}

void GilbertEngine::refreshCua() {
	_cua->refreshObjects();
}

void GilbertEngine::refreshInventory() {
	_cua->refreshInventory();
}

void GilbertEngine::showDialog() {
	debugC(1, kDebugScript, "Dialog: %s", _logic->dialogTitle().c_str());
	_dialog->open();
}

// Call-back 7 (rooms.md "Call-backs used in rooms").
void GilbertEngine::playWave(int list, int index, bool loop) {
	_sound->playListWave(list, index, loop);
}

// Call-back 10: room music, a dialogue voice, or another sound.
void GilbertEngine::playStream(const Common::String &name, bool loop, int kind) {
	debugC(1, kDebugSound, "Stream %s loop %d kind %d", name.c_str(), loop, kind);
	switch (kind) {
	case 0:
		_room->roomMusic(name);
		break;
	case 1:
		_sound->stopAll();
		_sound->openFile(Sound::kDialog, Common::Path("Sounds/Dialog/").appendComponent(name + ".wav"), loop, false);
		break;
	default:
		_sound->stopAll();
		_sound->openFile(Sound::kOther, Common::Path("Sounds/misc/").appendComponent(name + ".wav"), loop, true);
		break;
	}
}

void GilbertEngine::newTopic(bool shown) {
	_room->newTopic();
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
	present(_brightness);
}

void GilbertEngine::present(int brightness) {
	_brightness = brightness;
	if (brightness >= 256) {
		_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, 640, 480);
	} else {
		Graphics::ManagedSurface dim(640, 480, _screen.format);
		for (int y = 0; y < 480; y++) {
			const uint16 *s = (const uint16 *)_screen.getBasePtr(0, y);
			uint16 *d = (uint16 *)dim.getBasePtr(0, y);
			for (int x = 0; x < 640; x++) {
				byte r, g, b;
				_screen.format.colorToRGB(s[x], r, g, b);
				d[x] = _screen.format.RGBToColor(r * brightness / 256, g * brightness / 256, b * brightness / 256);
			}
		}
		_system->copyRectToScreen(dim.getPixels(), dim.pitch, 0, 0, 640, 480);
	}
	_system->updateScreen();
}

void GilbertEngine::drawPicture(Picture *pic, int x, int y) {
	drawPattern(pic, 0, x, y);
}

void GilbertEngine::drawPattern(Picture *pic, int k, int x, int y) {
	if (!pic)
		return;
	const Common::Rect pat = pic->pattern(k);
	pic->last = Common::Rect(x, y, x + pat.width(), y + pat.height());
	Common::Rect dst = pic->last;
	dst.clip(_clip);
	if (dst.isEmpty())
		return;
	const Common::Rect src(pat.left + dst.left - x, pat.top + dst.top - y, pat.left + dst.right - x, pat.top + dst.bottom - y);
	if (pic->transparent)
		_screen.transBlitFrom(pic->surface, src, Common::Point(dst.left, dst.top), pic->key);
	else
		_screen.blitFrom(pic->surface, src, Common::Point(dst.left, dst.top));
}

void GilbertEngine::blendPattern(Picture *pic, int k, const Common::Rect &dst, int alpha) {
	if (!pic)
		return;
	const Common::Rect pat = pic->pattern(k);
	if (pat.isEmpty() || dst.isEmpty())
		return;
	Common::Rect area = dst;
	area.clip(_clip);
	for (int y = area.top; y < area.bottom; y++) {
		const int sy = pat.top + (y - dst.top) * pat.height() / dst.height();
		uint16 *d = (uint16 *)_screen.getBasePtr(area.left, y);
		for (int x = area.left; x < area.right; x++, d++) {
			const int sx = pat.left + (x - dst.left) * pat.width() / dst.width();
			const uint16 c = *(const uint16 *)pic->surface.getBasePtr(sx, sy);
			if (pic->transparent && c == pic->key)
				continue;
			byte r0, g0, b0, r1, g1, b1;
			_screen.format.colorToRGB(*d, r0, g0, b0);
			_screen.format.colorToRGB(c, r1, g1, b1);
			*d = _screen.format.RGBToColor(r0 + (r1 - r0) * alpha / 255, g0 + (g1 - g0) * alpha / 255,
			                               b0 + (b1 - b0) * alpha / 255);
		}
	}
}

void GilbertEngine::frameRect(const Common::Rect &r, uint32 rgb) {
	const uint32 c = _screen.format.RGBToColor(rgb >> 16, (rgb >> 8) & 0xFF, rgb & 0xFF);
	Common::Rect area = r;
	area.clip(Common::Rect(640, 480));
	if (!area.isEmpty())
		_screen.frameRect(area, c);
}

void GilbertEngine::drawSurface(const Graphics::ManagedSurface &src, const Common::Rect &srcRect, int x, int y, uint32 key) {
	Common::Rect dst(x, y, x + srcRect.width(), y + srcRect.height());
	dst.clip(_clip);
	if (dst.isEmpty())
		return;
	const Common::Rect s(srcRect.left + dst.left - x, srcRect.top + dst.top - y,
	                     srcRect.left + dst.right - x, srcRect.top + dst.bottom - y);
	_screen.transBlitFrom(src, s, Common::Point(dst.left, dst.top), key);
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
	// Without the installer's registry value the original cannot save (-1). ScummVM plays an
	// installed game, so it defaults to 0; which values the installer writes is Q-0204.
	_settings.installationType = ConfMan.hasKey("installation_type") ? ConfMan.getInt("installation_type") : 0;
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
	const bool ok = loadSlotIni(ini, _targetName);
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
			_leftHeld = true;
			if (_mode == kModeCua)
				_cua->mouseDown();
			break;
		case Common::EVENT_RBUTTONDOWN:
			_mouse = event.mouse;
			if (_mode == kModeCua)
				_cua->mouseDown();
			break;
		case Common::EVENT_LBUTTONUP:
		case Common::EVENT_RBUTTONUP:
			// Releasing any button clears the button state (boot.md "Mouse").
			_mouse = event.mouse;
			_leftPress = false;
			_leftHeld = false;
			_cua->mouseUp();
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
