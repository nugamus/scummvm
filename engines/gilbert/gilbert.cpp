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
#include "common/fs.h"
#include "common/formats/ini-file.h"
#include "common/savefile.h"
#include "common/memstream.h"
#include "common/system.h"

#include "engines/util.h"

#include "backends/keymapper/keymapper.h"

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

// The clip rectangle of every picture draw (boot.md "Conventions"). Nothing is drawn outside
// it, so the window shows only it: the original's black border is cut off.
static const int kClipLeft = 64, kClipTop = 50, kClipRight = 576, kClipBottom = 430;
static const int kWindowW = kClipRight - kClipLeft, kWindowH = kClipBottom - kClipTop;
// The autosave's slot (the autosave_rooms option).
static const int kAutosaveSlot = 50;
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
	// Started from the CD itself: the game's folder is its Program folder.
	const Common::FSNode gameDir(ConfMan.getPath("path"));
	Common::FSList children;
	if (gameDir.getChildren(children, Common::FSNode::kListDirectoriesOnly)) {
		for (const Common::FSNode &c : children)
			if (c.getName().equalsIgnoreCase("program") && c.getChild("Gilbert.exe").exists()) {
				SearchMan.addDirectory(c, 0, 1);
				SearchMan.addSubDirectoryMatching(c, "data", 0, 4);
			}
	}
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
	for (auto &f : _drawingFonts)
		delete f._value;
}

Common::Error GilbertEngine::run() {
	const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
	loadOptions();
	_scale = _options.highResText ? 2 : 1;
	initGraphics(kWindowW * _scale, kWindowH * _scale, &format);
	_screen.create(640 * _scale, 480 * _scale, format);
	_clip = Common::Rect(kClipLeft, kClipTop, kClipRight, kClipBottom);
	_sound = new Sound(_mixer);
	_menu = new Menu(this);
	_logic = new Logic(this);
	_room = new Room(this);
	_cua = new CloseUp(this);
	_book = new Book(this);
	_dialog = new DialogBox(this);
	loadSettings();
	syncSoundSettings();
	enableKeymaps();
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
		autosave();
		if (_mode != kModeMenu)
			_keys.clear();
		_shortcut = kActionNone;
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
	drawText(text, x, 201, 8, kColourBlack);
	drawText(text, x, 200, 8, kColourTan);
	present();
	const uint32 end = _system->getMillis() + kLoadingWaitMs;
	setBusy(true);
	while (!shouldQuit() && _system->getMillis() < end) {
		pollEvents();
		_system->delayMillis(10);
	}
	setBusy(false);
}

// movie::Play (boot.md "Films").
void GilbertEngine::playFilm(const Common::String &name, bool fromIntro) {
	setBusy(true);
	playFilmBody(name);
	setBusy(false);
	if (fromIntro)
		_menu->musicAfterFilm();
	else
		_room->restartMusic();
}

void GilbertEngine::playFilmBody(const Common::String &name) {
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
	decoder.setSoundType(Audio::Mixer::kSFXSoundType);
	const bool smooth = _options.fullscreenFilms;
	const bool fullscreen = _settings.fullscreenVideo || smooth;
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
				if (fullscreen) {
					// The window, with the film's proportions.
					int w = kWindowW, h = frame->h * kWindowW / MAX<int>(frame->w, 1);
					if (h > kWindowH) {
						h = kWindowH;
						w = frame->w * kWindowH / MAX<int>(frame->h, 1);
					}
					const Common::Rect dst(kClipLeft + (kWindowW - w) / 2, kClipTop + (kWindowH - h) / 2,
					                       kClipLeft + (kWindowW + w) / 2, kClipTop + (kWindowH + h) / 2);
					const Common::Rect d = scaled(dst);
					if (smooth) {
						Graphics::Surface *big = frame->scale(d.width(), d.height(), true);
						_screen.blitFrom(*big, Common::Point(d.left, d.top));
						big->free();
						delete big;
					} else {
						_screen.blitFrom(*frame, Common::Rect(frame->w, frame->h), d);
					}
				} else {
					_screen.blitFrom(*frame, Common::Rect(frame->w, frame->h),
					                 scaled(Common::Rect(127, 80, 127 + frame->w, 80 + frame->h)));
				}
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
}

// Saving slot n (boot.md "Save slots"): file=game<n>.dat and the name in the slot list,
// then GESaveFile into `<target>.game<n>.dat`, the original's file byte for byte.
bool GilbertEngine::saveSlot(int n, const Common::String &name) {
	// SaveSlot does nothing without the installer's registry value (E-0216).
	if (n < 1 || n > 50 || _settings.installationType == -1)
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
	// What the player has seen (the marking options), beside the original's file.
	Common::ScopedPtr<Common::OutSaveFile> seen(_saveFileMan->openForSaving(_targetName + "." + file + ".seen", false));
	if (seen) {
		for (const auto &k : _seen)
			seen->writeString(k._key + "\n");
		seen->finalize();
	}
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
	_seen.clear();
	Common::ScopedPtr<Common::InSaveFile> seen(_saveFileMan->openForLoading(_targetName + "." + file + ".seen"));
	if (seen) {
		while (!seen->eos()) {
			const Common::String line = seen->readLine();
			if (!line.empty())
				markSeen(line);
		}
	} else {
		seedTopics();
	}
	_logic->continueGame();
	_autosaved = _logic->walkmapId();
	return true;
}

// ScummVM's slots 0..49 are the game's slots 1..50.
bool GilbertEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	return !_busy && _mode == kModeRoom && _menu->canSave();
}

bool GilbertEngine::canLoadGameStateCurrently(Common::U32String *msg) {
	return !_busy && (_mode == kModeRoom || _mode == kModeMenu);
}

Common::Error GilbertEngine::saveGameState(int slot, const Common::String &desc, bool isAutosave) {
	// The game's names are Windows-1252, at most 20 characters (boot.md "Typing").
	Common::String name = Common::U32String(desc, Common::kUtf8).encode(Common::kWindows1252);
	if (name.size() > 20)
		name = name.substr(0, 20);
	return saveSlot(slot + 1, name) ? Common::kNoError : Common::kWritingFailed;
}

Common::Error GilbertEngine::loadGameState(int slot) {
	if (!loadSlot(slot + 1))
		return Common::kReadingFailed;
	_menu->gameLoaded(true);
	return Common::kNoError;
}

void GilbertEngine::resetState() {
	// ResetState (rooms.md "State").
	_room->reset();
	_cua->reset();
	_dialog->close();
	_menu->reset();
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
	_seen.clear();
	seedTopics();
	_logic->startNewGame();
	_autosaved = _logic->walkmapId();
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
	const bool held = _runHeld || (_eventMan->getModifierState() & Common::KBD_CTRL) != 0;
	// Always run (option): Ctrl walks.
	return _options.alwaysRun ? !held : held;
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
	_screen.fillRect(Common::Rect(_screen.w, _screen.h), 0);
}

void GilbertEngine::present() {
	present(_brightness);
}

void GilbertEngine::present(int brightness) {
	_brightness = brightness;
	const int w = kWindowW * _scale, h = kWindowH * _scale;
	const int left = kClipLeft * _scale, top = kClipTop * _scale;
	if (brightness >= 256) {
		_system->copyRectToScreen(_screen.getBasePtr(left, top), _screen.pitch, 0, 0, w, h);
	} else {
		Graphics::ManagedSurface dim(w, h, _screen.format);
		for (int y = 0; y < h; y++) {
			const uint16 *s = (const uint16 *)_screen.getBasePtr(left, top + y);
			uint16 *d = (uint16 *)dim.getBasePtr(0, y);
			for (int x = 0; x < w; x++) {
				byte r, g, b;
				_screen.format.colorToRGB(s[x], r, g, b);
				d[x] = _screen.format.RGBToColor(r * brightness / 256, g * brightness / 256, b * brightness / 256);
			}
		}
		_system->copyRectToScreen(dim.getPixels(), dim.pitch, 0, 0, w, h);
	}
	drawHotspots();
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
	if (_scale == 1) {
		if (pic->transparent)
			_screen.transBlitFrom(pic->surface, src, Common::Point(dst.left, dst.top), pic->key);
		else
			_screen.blitFrom(pic->surface, src, Common::Point(dst.left, dst.top));
	} else if (pic->transparent) {
		_screen.transBlitFrom(pic->surface, src, scaled(dst), pic->key);
	} else {
		_screen.blitFrom(pic->surface, src, scaled(dst));
	}
}

void GilbertEngine::blendPattern(Picture *pic, int k, const Common::Rect &dst, int alpha) {
	if (!pic)
		return;
	const Common::Rect pat = pic->pattern(k);
	if (pat.isEmpty() || dst.isEmpty())
		return;
	Common::Rect area = dst;
	area.clip(_clip);
	const Common::Rect big = scaled(dst);
	area = scaled(area);
	for (int y = area.top; y < area.bottom; y++) {
		const int sy = pat.top + (y - big.top) * pat.height() / big.height();
		uint16 *d = (uint16 *)_screen.getBasePtr(area.left, y);
		for (int x = area.left; x < area.right; x++, d++) {
			const int sx = pat.left + (x - big.left) * pat.width() / big.width();
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
	if (area.isEmpty())
		return;
	area = scaled(area);
	for (int i = 0; i < _scale; i++) {
		_screen.frameRect(area, c);
		area.grow(-1);
	}
}

void GilbertEngine::fillAlpha(const Common::Rect &r, uint32 rgb, int alpha) {
	Common::Rect area = r;
	area.clip(Common::Rect(640, 480));
	area = scaled(area);
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

void GilbertEngine::drawText(const Common::U32String &text, int x, int y, int size, uint32 rgb, bool bold) {
	renderText(_screen, text, x * _scale, y * _scale, size, rgb, bold);
}

// Layout always measures with font(); text is drawn with this one. The original's look is
// the layout font itself; the smooth_text and high_res_text options draw it anti-aliased, at
// the page's scale.
const Graphics::Font *GilbertEngine::drawingFont(int size, bool bold) {
	if (!_options.smoothText && _scale == 1)
		return font(size, bold);
	const int key = size * 2 + (bold ? 1 : 0);
	if (!_drawingFonts.contains(key)) {
		Graphics::Font *f = Graphics::loadTTFFontFromArchive(bold ? "LiberationSans-Bold.ttf" : "LiberationSans-Regular.ttf",
		                                                     size * _scale, Graphics::kTTFSizeModeCharacter, 96, 96,
		                                                     Graphics::kTTFRenderModeLight);
		if (!f)
			error("Gilbert: cannot load Liberation Sans from fonts.dat");
		_drawingFonts[key] = f;
	}
	return _drawingFonts[key];
}

// Text at a position in `dst`'s own pixels.
void GilbertEngine::renderText(Graphics::ManagedSurface &dst, const Common::U32String &text, int x, int y, int size, uint32 rgb, bool bold) {
	const uint32 colour = dst.format.RGBToColor(rgb >> 16, (rgb >> 8) & 0xFF, rgb & 0xFF);
	drawingFont(size, bold)->drawString(&dst, text, x, y, dst.w - x, colour);
}

void TextPage::text(const Common::U32String &s, int x, int y, int size, uint32 rgb, bool bold) {
	Item i;
	i.type = Item::kText;
	i.text = s;
	i.x = x;
	i.y = y;
	i.size = size;
	i.rgb = rgb;
	i.bold = bold;
	items.push_back(i);
}

void TextPage::underlineLast(int y, int w) {
	if (!items.empty()) {
		items.back().underline = y;
		items.back().underlineW = w;
	}
}

void TextPage::picture(Picture *pic, int x, int y) {
	Item i;
	i.type = Item::kPicture;
	i.picture = pic;
	i.x = x;
	i.y = y;
	items.push_back(i);
}

void GilbertEngine::drawPage(const TextPage &page, const Common::Rect &src, int x, int y) {
	Common::Rect view(x, y, x + src.width(), y + src.height());
	view.clip(_clip);
	if (view.isEmpty())
		return;
	// Drawing into the view's part of the page clips everything to it.
	const Common::Rect v = scaled(view);
	Graphics::ManagedSurface sub(_screen, v);
	const int dx = x - src.left - view.left, dy = y - src.top - view.top;
	for (const TextPage::Item &i : page.items) {
		const int ix = i.x + dx, iy = i.y + dy;
		if (iy > view.height() || iy + 40 < 0)
			continue;
		switch (i.type) {
		case TextPage::Item::kText:
			renderText(sub, i.text, ix * _scale, iy * _scale, i.size, i.rgb, i.bold);
			if (i.underline >= 0) {
				int w = i.underlineW * _scale;
				if (_scale > 1)
					w = MIN(drawingFont(i.size, i.bold)->getStringWidth(i.text), (src.right - i.x) * _scale);
				const int uy = i.underline + dy;
				sub.fillRect(Common::Rect(ix * _scale, uy * _scale, ix * _scale + w, (uy + 1) * _scale),
				             sub.format.RGBToColor(i.rgb >> 16, (i.rgb >> 8) & 0xFF, i.rgb & 0xFF));
			}
			break;
		case TextPage::Item::kPicture: {
			const Graphics::ManagedSurface &p = i.picture->surface;
			sub.transBlitFrom(p, Common::Rect(p.w, p.h),
			                  Common::Rect(ix * _scale, iy * _scale, (ix + p.w) * _scale, (iy + p.h) * _scale),
			                  _screen.format.RGBToColor(0xFF, 0, 0xFF));
			break;
		}
		}
	}
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
		// The window shows the page from the clip rectangle's corner.
		event.mouse = Common::Point(event.mouse.x / _scale + kClipLeft, event.mouse.y / _scale + kClipTop);
		switch (event.type) {
		case Common::EVENT_MOUSEMOVE:
			_mouse = event.mouse;
			if (_buttonState == -1) {
				const int held = _eventMan->getButtonState();
				if (held & Common::EventManager::LBUTTON)
					setButtonState(1);
				else if (held & Common::EventManager::RBUTTON)
					setButtonState(2);
			}
			break;
		case Common::EVENT_LBUTTONDOWN:
		case Common::EVENT_RBUTTONDOWN:
			_mouse = event.mouse;
			if (_buttonState == -1)
				setButtonState(event.type == Common::EVENT_LBUTTONDOWN ? 1 : 2);
			if (_mode == kModeCua)
				_cua->mouseDown();
			break;
		case Common::EVENT_LBUTTONUP:
		case Common::EVENT_RBUTTONUP:
			// Releasing any button clears the button state (boot.md "Mouse").
			_mouse = event.mouse;
			_buttonState = -1;
			for (int i = 0; i < kScreenCount; i++)
				_seenState[i] = -1;
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
		case Common::EVENT_WHEELUP:
		case Common::EVENT_WHEELDOWN:
			if (_options.wheel)
				_shortcut = event.type == Common::EVENT_WHEELUP ? kActionInventoryUp : kActionInventoryDown;
			break;
		case Common::EVENT_CUSTOM_ENGINE_ACTION_START:
			if (event.customType == kActionSkip) {
				_escHeld = true;
				if (_options.shortcuts && !_busy)
					_shortcut = kActionBack;
			} else if (event.customType == kActionRun) {
				_runHeld = true;
			} else if (event.customType == kActionHotspots) {
				toggleHotspots();
			} else if (_options.shortcuts) {
				_shortcut = event.customType;
			}
			break;
		case Common::EVENT_CUSTOM_ENGINE_ACTION_END:
			if (event.customType == kActionSkip)
				_escHeld = false;
			else if (event.customType == kActionRun)
				_runHeld = false;
			break;
		default:
			break;
		}
	}
}

void GilbertEngine::setButtonState(int state) {
	_buttonState = state;
	if (state == 1)
		_downs++;
}

void GilbertEngine::setMode(int mode) {
	if (mode != _mode) {
		static const struct {
			int mode;
			Screen screen;
		} screens[] = { { kModeMenu, kScreenMenu }, { kModeRoom, kScreenRoom }, { kModeCua, kScreenCua }, { kModeBook, kScreenBook } };
		for (const auto &s : screens)
			if (s.mode == mode)
				syncPress(s.screen);
	}
	_mode = mode;
	enableKeymaps();
}

void GilbertEngine::syncPress(Screen screen) {
	_seenDowns[screen] = _downs;
	_seenState[screen] = _buttonState;
}

bool GilbertEngine::press(Screen screen, bool *changed) {
	// A down and up between two ticks still counts once (_downs), as a held click would.
	const bool pressed = _downs != _seenDowns[screen];
	if (changed)
		*changed = pressed || _buttonState != _seenState[screen];
	_seenDowns[screen] = _downs;
	_seenState[screen] = _buttonState;
	return pressed;
}

Common::Array<Common::KeyState> GilbertEngine::takeKeys() {
	Common::Array<Common::KeyState> keys;
	keys.swap(_keys);
	return keys;
}

void GilbertEngine::warpMouse(int x, int y) {
	_mouse = Common::Point(x, y);
	_system->warpMouse((x - kClipLeft) * _scale, (y - kClipTop) * _scale);
}

void GilbertEngine::loadOptions() {
	_options.alwaysRun = ConfMan.getBool("always_run");
	_options.shortcuts = ConfMan.getBool("keyboard_shortcuts");
	_options.wheel = ConfMan.getBool("wheel_inventory");
	_options.markChoices = ConfMan.getBool("mark_choices");
	_options.newTopics = ConfMan.getBool("mark_new_topics");
	_options.autosave = ConfMan.getBool("autosave_rooms");
	_options.fullscreenFilms = ConfMan.getBool("fullscreen_films");
	_options.smoothText = ConfMan.getBool("smooth_text");
	_options.highResText = ConfMan.getBool("high_res_text");
}

// The game screens' keys are off in the main menu, where the keys type save names.
void GilbertEngine::enableKeymaps() {
	if (Common::Keymap *k = _eventMan->getKeymapper()->getKeymap("gilbert-play"))
		k->setEnabled(_mode != kModeMenu);
}

Common::String GilbertEngine::choiceKey(uint32 dialog, const Common::String &text) {
	Common::String key = Common::String::format("c %u ", dialog) + text;
	for (uint i = 0; i < key.size(); i++)
		if (key[i] == '\n' || key[i] == '\r')
			key.setChar(' ', i);
	return key;
}

Common::String GilbertEngine::topicKey(int book, uint32 topic) {
	return Common::String::format("t %d %u", book, topic);
}

// Topics already in the books when a game starts, or in a save from before the marking, are
// not new.
void GilbertEngine::seedTopics() {
	for (int b = 0; b < Database::kBooks; b++)
		for (const Topic &t : _logic->db().books[b])
			if (t.shown)
				markSeen(topicKey(b, t.id));
}

// The autosave_rooms option: slot 50, each time a room is shown that is not the last
// autosave's.
void GilbertEngine::autosave() {
	if (!_options.autosave || _mode != kModeRoom || !_room->shown() || _dialog->isOpen() || _busy)
		return;
	const uint32 room = _logic->walkmapId();
	if (room == _autosaved || !canSaveGameStateCurrently())
		return;
	_autosaved = room;
	if (!saveSlot(kAutosaveSlot, "Autosave"))
		warning("Gilbert: autosave failed");
}

void GilbertEngine::toggleHotspots() {
	if (ConfMan.hasKey("enable_hotspots") && !ConfMan.getBool("enable_hotspots"))
		return;
	showHotspots(!_showHotspots);
	_shownHotspots.clear();
}

void GilbertEngine::getHotspotPositions(Common::Array<Graphics::HotspotInfo> &hotspots) {
	if (_dialog->isOpen() || _busy)
		return;
	if (_mode == kModeRoom)
		_room->hotspots(hotspots);
	else if (_mode == kModeCua)
		_cua->hotspots(hotspots);
	for (Graphics::HotspotInfo &h : hotspots)
		h.position = Common::Point((h.position.x - kClipLeft) * _scale, (h.position.y - kClipTop) * _scale);
}

// The overlay is drawn again only when the markers change.
void GilbertEngine::drawHotspots() {
	if (!_showHotspots)
		return;
	Common::Array<Graphics::HotspotInfo> list;
	getHotspotPositions(list);
	bool same = list.size() == _shownHotspots.size();
	for (uint i = 0; same && i < list.size(); i++)
		same = list[i].position == _shownHotspots[i].position && list[i].name == _shownHotspots[i].name;
	if (same && !_hotspotForceRedraw)
		return;
	_shownHotspots = list;
	if (list.empty()) {
		_hotspotForceRedraw = false;
		if (_system->isOverlayVisible())
			_system->hideOverlay();
		return;
	}
	Engine::drawHotspots();
}

} // End of namespace Gilbert
