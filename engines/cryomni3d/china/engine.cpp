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
#include "common/file.h"
#include "common/fs.h"
#include "common/savefile.h"
#include "common/system.h"

#include "engines/metaengine.h"
#include "engines/util.h"
#include "graphics/cursorman.h"

#include "cryomni3d/image/hnm.h"

#include "cryomni3d/china/engine.h"

namespace CryOmni3D {
namespace China {

// The view's fields of view and the renderer's row-step shifts (spec/china-warp.md, E-0601, E-0603)
static const double kWarpHFov = 75.137 * M_PI / 180.;
static const double kWarpVFov = 50. * M_PI / 180.;

// Main menu bullets (E-0505): positions come from the SPR files, the last two are moved copies
static const uint kMenuButtons = 8;
static const char *const kMenuLabels[kMenuButtons] = {
	"nouveau_jeu", "charge_jeu", "reprendre", "sauve_jeu", "visite", "consulte_doc", "options", "quitter"
};

CryOmni3DEngine_China::CryOmni3DEngine_China(OSystem *syst, const CryOmni3DGameDescription *gamedesc) :
	CryOmni3DEngine(syst, gamedesc), _format(2, 5, 6, 5, 0, 11, 5, 0, 0), _display(kDisplayNone),
	_fadePending(false), _alphaSpeed(0.), _betaSpeed(0.), _panoramaSpeed(2), _cursorId(-1), _hoveredZone(-1),
	_clickedZone(-1), _pressLatch(false), _pressed(false), _place(nullptr), _entryPending(false),
	_heldObject(kNoObject), _gameRunning(false), _nextFrame(0), _loadedGame(false), _inPlay(false),
	_endOfPlay(false), _spacePressed(false), _spaceUp(false), _spaceArmed(false), _inPlaceCall(false), _pendingLoad(-1), _puzzleMode(false), _rightLatch(false),
	_fightStart(0), _barLoaded(false), _barWarp(false), _skipFade(false), _voiceRate(22050), _rnd("china") {
	memset(_vars, 0, sizeof(_vars));
	memset(_objects, 0, sizeof(_objects));
	for (uint i = 0; i < kSlotCount; i++) {
		_slots[i] = kNoObject;
	}
	// The original's options default to subtitles on (E-0501)
	ConfMan.registerDefault("subtitles", true);
}

CryOmni3DEngine_China::~CryOmni3DEngine_China() {
}

bool CryOmni3DEngine_China::hasFeature(EngineFeature f) const {
	return CryOmni3DEngine::hasFeature(f) || f == kSupportsSavingDuringRuntime ||
	       f == kSupportsLoadingDuringRuntime;
}

// Each video before the menu is skipped by Escape or a left click (E-0504)
bool CryOmni3DEngine_China::shouldSkipVideo() {
	bool skip = getCurrentMouseButton() == 1;
	while (!_keysPressed.empty()) {
		if (_keysPressed.pop().keycode == Common::KEYCODE_ESCAPE) {
			skip = true;
		}
	}
	return skip;
}

void CryOmni3DEngine_China::initializePath(const Common::FSNode &gamePath) {
	// The CD's root holds CHINE/, which holds CHINE.EXE and DATA/; either folder may be chosen.
	const Common::FSNode chine = gamePath.getChild("CHINE");
	const Common::FSNode root = chine.isDirectory() ? chine : gamePath;
	SearchMan.addDirectory(root, 0, 1, false);
	SearchMan.addSubDirectoryMatching(root, "data", 0, 4, false);
}

uint32 CryOmni3DEngine_China::textColor(uint16 color) const {
	const byte r = (color >> 11) & 0x1f, g = (color >> 5) & 0x3f, b = color & 0x1f;
	return _format.RGBToColor((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2));
}

uint32 CryOmni3DEngine_China::gameColor(uint16 color) const {
	const byte r = (color >> 10) & 0x1f, g = (color >> 5) & 0x1f, b = color & 0x1f;
	return _format.RGBToColor((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2));
}

bool CryOmni3DEngine_China::loadStill(const Common::Path &path, Graphics::ManagedSurface &dst) {
	Common::File file;
	if (!file.open(path)) {
		warning("China: cannot open %s", path.toString().c_str());
		return false;
	}
	Image::HNMFileDecoder decoder(_format);
	if (!decoder.loadStream(file) || !decoder.getSurface()) {
		warning("China: cannot decode %s", path.toString().c_str());
		return false;
	}
	dst.copyFrom(*decoder.getSurface());
	return true;
}

// SPR (formats README, E-0102, E-0505): an 18-byte TGA header (12-byte image ID, type 2,
// depth 15 or 16), the ID (key colour u32, x s32, y s32), then X1R5G5B5 pixels.
bool CryOmni3DEngine_China::loadSprite(const Common::Path &path, Sprite &sprite) {
	Common::File file;
	if (!file.open(path)) {
		warning("China: cannot open %s", path.toString().c_str());
		return false;
	}
	byte header[18];
	if (file.read(header, sizeof(header)) != sizeof(header)) {
		return false;
	}
	const uint idLength = header[0];
	const uint16 width = READ_LE_UINT16(header + 12);
	const uint16 height = READ_LE_UINT16(header + 14);
	const byte depth = header[16];
	const bool topFirst = header[17] & 0x20;
	if (header[2] != 2 || (depth != 15 && depth != 16) || idLength < 12) {
		warning("China: %s is not a sprite", path.toString().c_str());
		return false;
	}
	// Every key colour in the corpus fits the low 16 bits (E-0102)
	const uint16 key = (uint16)file.readUint32LE();
	sprite.pos.x = file.readSint32LE();
	sprite.pos.y = file.readSint32LE();
	file.skip(idLength - 12);
	if ((int64)width * height * 2 > file.size() - file.pos()) {
		warning("China: %s is truncated", path.toString().c_str());
		return false;
	}

	sprite.surface.create(width, height, _format);
	for (uint y = 0; y < height; y++) {
		uint16 *row = (uint16 *)sprite.surface.getBasePtr(0, topFirst ? y : height - 1 - y);
		for (uint x = 0; x < width; x++) {
			row[x] = gameColor(file.readUint16LE());
		}
	}
	sprite.keyColor = gameColor(key);
	return !file.err();
}

// LABELS.TXT and MINUTES.TXT (spec/china-boot.md Main menu): `#key#` lines, each followed by `<text>`.
void CryOmni3DEngine_China::loadTextFile(const char *path, TextMap &dst) {
	Common::File file;
	if (!file.open(path)) {
		warning("China: no %s", path);
		return;
	}
	Common::String key;
	while (!file.eos() && !file.err()) {
		Common::String line = file.readLine();
		line.trim();
		if (line.size() > 2 && line.firstChar() == '#' && line.lastChar() == '#') {
			key = Common::String(line.c_str() + 1, line.size() - 2);
			key.trim();
		} else if (!key.empty() && line.hasPrefix("<")) {
			Common::String text(line.c_str() + 1);
			while (!text.contains('>') && !file.eos()) {
				text += "\n" + file.readLine();
			}
			const size_t end = text.findLastOf('>');
			if (end != Common::String::npos) {
				dst[key] = Common::String(text.c_str(), end);
			}
			key.clear();
		}
	}
}

Common::String CryOmni3DEngine_China::label(const char *key) const {
	return _labels.getValOrDefault(key);
}

Common::Error CryOmni3DEngine_China::run() {
	CryOmni3DEngine::run();

	initGraphics(640, 480, &_format);
	if (g_system->getScreenFormat() != _format) {
		return Common::kUnsupportedColorMode;
	}
	_screen.create(640, 480, _format);

	Common::Array<Common::Path> fonts;
	for (uint i = 1; i <= 11; i++) {
		fonts.push_back(Common::Path(Common::String::format("FONTES/FONT%02u.CRF", i)));
	}
	_fontManager.loadFonts(fonts, Common::kWindows1252);
	_fontManager.setSurface(&_screen);
	_fontManager.setTransparentBackground(true);
	// Each glyph advances by its width plus one pixel (E-0800)
	_fontManager.setCharSpacing(1);
	loadTextFile("LOC/LABELS.TXT", _labels);
	loadTextFile("LOC/MINUTES.TXT", _minuteTexts);
	loadDialogues();

	_omni3D.init(kWarpHFov, kWarpVFov);
	// China's renderer changes the per-pixel step down a block by >> 4 and >> 9 (E-0603, Q-0600)
	_omni3D.setRowStepShifts(4, 9);

	// Cursor sprites (E-0901)
	static const char *const cursors[kCursorCount] = {
		"tri270", "tri90", "tri0", "tri180", "tri315", "tri45", "tri135", "tri225", "doigt", "voir",
		"prendre", "ptroug", "doigt", "inter", "util", "interrog", "bouche", "pointact"
	};
	for (uint i = 0; i < kCursorCount; i++) {
		loadSprite(Common::Path(Common::String::format("SPRITES/CURSEURS/%s.SPR", cursors[i])), _cursors[i]);
	}
	setCursorSprite(11);
	CursorMan.showMouse(true);

	const int saveSlot = ConfMan.getInt("save_slot");
	if (saveSlot >= 0 && loadGameState(saveSlot).getCode() == Common::kNoError) {
		playLoop();
	} else {
		playIntroduction();
	}

	while (!shouldAbort()) {
		const MenuChoice choice = mainMenu();
		if (choice == kMenuQuit || choice == kMenuNone) {
			// Quit shows the credits first (E-0506)
			if (choice == kMenuQuit) {
				credits();
			}
			break;
		}
		if (choice == kMenuNewGame) {
			newGame();
		} else if (choice == kMenuVisit) {
			// Visit (E-0506): a new game with MODE_VISITE set, not counted as a game
			newGame();
			setVar(0, 1);
			_gameRunning = false;
		} else if (choice == kMenuLoad) {
			_loadedGame = false;
			if (!loadGameDialog() || !_loadedGame) {
				continue;
			}
		} else if (choice != kMenuResume) {
			warning("China: menu choice %d is not implemented", choice);
			continue;
		}
		playLoop();
	}
	return Common::kNoError;
}

// Before the menu (E-0504): each video is skipped on its own by Escape or a click; between
// INTRO and ITB the emperor's servant speaks (ANJGEN41), without subtitles.
void CryOmni3DEngine_China::playIntroduction() {
	static const char *const videos[] = { "HNM/LOGO.HNS", "HNM/INTRO.HNS", nullptr, "HNM/ITB.HNS" };
	for (uint i = 0; i < ARRAYSIZE(videos) && !shouldAbort(); i++) {
		if (!videos[i]) {
			dialogue("ANJGEN41", "P000ANJ", "P000EMP", true);
			continue;
		}
		CursorMan.showMouse(false);
		playHNM(videos[i], Audio::Mixer::kMusicSoundType);
		CursorMan.showMouse(true);
		clearKeys();
	}
	// The menu's music (E-0504)
	playMusic("Allee");
}

Common::Rect CryOmni3DEngine_China::menuButtonRect(uint button) {
	const Sprite &bullet = _menuNormal[MIN<uint>(button, 5)];
	Common::Point pos = bullet.pos;
	if (button > 5) {
		pos += Common::Point(20 * (button - 5), 25 * (button - 5));
	}
	// The labels are measured in font slot 0 but drawn in slot 1 (E-0801)
	const Common::String text = label(kMenuLabels[button]);
	_fontManager.setCurrentFont(0);
	return Common::Rect(pos.x, pos.y, pos.x + bullet.surface.w + _fontManager.getStrWidth(text),
	                    pos.y + bullet.surface.h + _fontManager.getFontMaxHeight());
}

bool CryOmni3DEngine_China::menuButtonEnabled(uint button) {
	switch (button) {
	case 0:
		return true;
	case 1:
		// Any save (E-0505); ScummVM saves stand in for the original's 12 emblem slots
		return !getSaveFileManager()->listSavefiles(getMetaEngine()->getSavegameFilePattern(_targetName.c_str())).empty();
	case 2:
		return _gameRunning || visitMode();
	case 3:
		return false; // save mode only (E-0505)
	default:
		return true;
	}
}

void CryOmni3DEngine_China::drawMenu(int hovered) {
	_screen.blitFrom(_menuBackground);
	for (uint i = 0; i < kMenuButtons; i++) {
		const Sprite &sprite = (int)i == hovered ? _menuHover[MIN<uint>(i, 5)] : _menuNormal[MIN<uint>(i, 5)];
		const Common::Rect rect = menuButtonRect(i);
		_screen.transBlitFrom(sprite.surface, Common::Point(rect.left, rect.top), sprite.keyColor);
		_fontManager.setCurrentFont(1);
		_fontManager.setForeColor(textColor((int)i == hovered ? 0xffff : (menuButtonEnabled(i) ? 0x7020 : 0x9a73)));
		_fontManager.displayStr(rect.left + 20, rect.top + 1, label(kMenuLabels[i]));
	}
	g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, 640, 480);
}

MenuChoice CryOmni3DEngine_China::mainMenu() {
	if (_menuBackground.empty()) {
		loadStill("INTERF/FONDTITR.HNM", _menuBackground);
		for (uint i = 0; i < 6; i++) {
			loadSprite(Common::Path(Common::String::format("INTERF/ROUG_%u.SPR", i + 1)), _menuNormal[i]);
			loadSprite(Common::Path(Common::String::format("INTERF/BLC_%u.SPR", i + 1)), _menuHover[i]);
		}
	}

	setCursorSprite(11);
	int lastHovered = -2;
	while (!shouldAbort()) {
		pollEvents();
		const Common::Point mouse = getMousePos();
		int hovered = -1;
		for (uint i = 0; i < kMenuButtons; i++) {
			if (menuButtonRect(i).contains(mouse)) {
				hovered = i;
			}
		}
		if (hovered != lastHovered) {
			drawMenu(hovered);
			lastHovered = hovered;
		}
		if (getDragStatus() == kDragStatus_Finished && hovered >= 0 && menuButtonEnabled(hovered)) {
			static const MenuChoice choices[kMenuButtons] = {
				kMenuNewGame, kMenuLoad, kMenuResume, kMenuSave, kMenuVisit, kMenuNone, kMenuNone, kMenuQuit
			};
			if (choices[hovered] != kMenuNone) {
				return choices[hovered];
			}
			warning("China: menu button %d is not implemented", hovered);
		}
		g_system->updateScreen();
		g_system->delayMillis(10);
	}
	return kMenuNone;
}

} // End of namespace China
} // End of namespace CryOmni3D
