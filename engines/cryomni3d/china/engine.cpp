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
#include "common/system.h"

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
	CryOmni3DEngine(syst, gamedesc), _format(2, 5, 6, 5, 0, 11, 5, 0, 0), _hasWarp(false),
	_alphaSpeed(0.), _betaSpeed(0.), _panoramaSpeed(2), _gameRunning(false), _nextFrame(0) {
}

CryOmni3DEngine_China::~CryOmni3DEngine_China() {
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

// LABELS.TXT (spec/china-boot.md Main menu): `#key#` lines, each followed by `<text>`.
void CryOmni3DEngine_China::loadLabels() {
	Common::File file;
	if (!file.open("LOC/LABELS.TXT")) {
		warning("China: no LOC/LABELS.TXT");
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
				_labels[key] = Common::String(text.c_str(), end);
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
	loadLabels();

	_omni3D.init(kWarpHFov, kWarpVFov);
	// China's renderer changes the per-pixel step down a block by >> 4 and >> 9 (E-0603, Q-0600)
	_omni3D.setRowStepShifts(4, 9);

	// Q-0010: the warp cursor is not specced yet; the red pointer stands in.
	if (loadSprite("SPRITES/CURSEURS/PTROUG.SPR", _cursor)) {
		CursorMan.replaceCursor(_cursor.surface.rawSurface(), _cursor.surface.w / 2, _cursor.surface.h / 2,
		                        _cursor.keyColor);
	}
	CursorMan.showMouse(true);

	playIntroduction();

	while (!shouldAbort()) {
		const MenuChoice choice = mainMenu();
		if (choice == kMenuQuit || choice == kMenuNone) {
			break;
		}
		if (choice == kMenuNewGame) {
			newGame();
		} else if (choice != kMenuResume) {
			warning("China: menu choice %d is not implemented", choice);
			continue;
		}
		playLoop();
	}
	return Common::kNoError;
}

// Before the menu (E-0504): each video is skipped on its own by Escape or a click.
// Q-0011: the ANJGEN41 dialogue between INTRO and ITB and the ALLEE music are not played yet.
void CryOmni3DEngine_China::playIntroduction() {
	static const char *const videos[] = { "HNM/LOGO.HNS", "HNM/INTRO.HNS", "HNM/ITB.HNS" };
	for (uint i = 0; i < ARRAYSIZE(videos) && !shouldAbort(); i++) {
		CursorMan.showMouse(false);
		playHNM(videos[i], Audio::Mixer::kMusicSoundType);
		CursorMan.showMouse(true);
		clearKeys();
	}
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

bool CryOmni3DEngine_China::menuButtonEnabled(uint button) const {
	switch (button) {
	case 0:
		return true;
	case 1:
		return false; // Q-0012: saves are not implemented yet
	case 2:
		return _gameRunning;
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

// New game (E-0506, E-0507): Script_Start sets the view to alpha 4.7, beta 0 and enters pne140.
void CryOmni3DEngine_China::newGame() {
	_gameRunning = true;
	loadWarp("pne140");
	_omni3D.setAlpha(4.7);
	_omni3D.setBeta(0.);
}

bool CryOmni3DEngine_China::loadWarp(const Common::String &name) {
	if (!loadStill(Common::Path("WARP/" + name + ".HNM"), _warpImage)) {
		return false;
	}
	_omni3D.setSourceSurface(_warpImage.surfacePtr());
	_alphaSpeed = _betaSpeed = 0.;
	_hasWarp = true;
	debug(1, "China: warp %s", name.c_str());
	return true;
}

// Edge scrolling (E-0509, E-0605): the cursor near an edge pushes the view; velocities decay.
void CryOmni3DEngine_China::scrollByCursor() {
	const Common::Point c = getMousePos();
	double pushX = 0., pushY = 0.;
	if (c.x < 100) {
		pushX = 100 - c.x;
	} else if (c.x > 540) {
		pushX = 540 - c.x;
	}
	if (c.y < 100) {
		pushY = c.y - 100;
	} else if (c.y > 380) {
		pushY = c.y - 380;
	}
	const double k = 5. - _panoramaSpeed;
	_alphaSpeed += pushX / (1250. * k);
	_betaSpeed += pushY / (1500. * k);
	if (_alphaSpeed != 0. || _betaSpeed != 0.) {
		_omni3D.setAlpha(_omni3D.getAlpha() + _alphaSpeed);
		_omni3D.setBeta(_omni3D.getBeta() + _betaSpeed);
		_alphaSpeed *= 0.8;
		_betaSpeed *= 0.8;
	}
}

void CryOmni3DEngine_China::drawFrame() {
	if (_hasWarp) {
		const Graphics::Surface *view = _omni3D.getSurface();
		if (view) {
			g_system->copyRectToScreen(view->getPixels(), view->pitch, 0, 0, view->w, view->h);
		}
	}
	g_system->updateScreen();
}

// The original never paces its frames (E-0804) and turns the view per frame; we run 25 frames/s (Q-0800).
void CryOmni3DEngine_China::waitFrame() {
	const uint32 now = g_system->getMillis();
	if (_nextFrame > now) {
		g_system->delayMillis(_nextFrame - now);
	}
	_nextFrame = MAX(_nextFrame, now) + 40;
}

void CryOmni3DEngine_China::playLoop() {
	clearKeys();
	while (!shouldAbort()) {
		pollEvents();
		bool toMenu = false;
		while (!_keysPressed.empty()) {
			if (_keysPressed.pop().keycode == Common::KEYCODE_ESCAPE) {
				toMenu = true;
			}
		}
		if (toMenu) {
			return;
		}
		scrollByCursor();
		drawFrame();
		waitFrame();
	}
}

} // End of namespace China
} // End of namespace CryOmni3D
