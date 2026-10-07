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

#ifndef CRYOMNI3D_CHINA_ENGINE_H
#define CRYOMNI3D_CHINA_ENGINE_H

#include "common/hashmap.h"
#include "common/hash-str.h"
#include "common/path.h"
#include "common/str.h"

#include "graphics/managed_surface.h"
#include "graphics/pixelformat.h"

#include "cryomni3d/cryomni3d.h"
#include "cryomni3d/omni3d.h"

namespace CryOmni3D {
namespace China {

/** A China sprite (`.SPR`): one 16-bit picture with a key colour and a screen position. */
struct Sprite {
	Graphics::ManagedSurface surface;
	uint32 keyColor = 0;
	Common::Point pos;
};

enum MenuChoice {
	kMenuNone = 0,
	kMenuNewGame = 1,
	kMenuLoad = 2,
	kMenuResume = 3,
	kMenuSave = 4,
	kMenuVisit = 5,
	kMenuQuit = 6
};

class CryOmni3DEngine_China : public CryOmni3DEngine {
public:
	CryOmni3DEngine_China(OSystem *syst, const CryOmni3DGameDescription *gamedesc);
	~CryOmni3DEngine_China() override;

	bool displayToolbar(const Graphics::Surface *original) override { return false; }
	bool hasPlaceDocumentation() override { return false; }
	bool displayPlaceDocumentation() override { return false; }
	uint displayOptions() override { return 0; }
	void makeTranslucent(Graphics::Surface &dst, const Graphics::Surface &src) const override {}
	void setupPalette(const byte *colors, uint start, uint num) override {}
	bool shouldSkipVideo() override;
	void initializePath(const Common::FSNode &gamePath) override;

protected:
	Common::Error run() override;

private:
	// Files: every name is relative to the game's DATA folder (`WARP/PNE140.HNM`).
	bool loadStill(const Common::Path &path, Graphics::ManagedSurface &dst);
	bool loadSprite(const Common::Path &path, Sprite &sprite);
	void loadLabels();
	Common::String label(const char *key) const;
	/** Converts one of the game's X1R5G5B5 colours to the screen format. */
	uint32 gameColor(uint16 color) const;
	/** Converts one of the code's R5G6B5 text colours (0x7020, 0x9a73) to the screen format. */
	uint32 textColor(uint16 color) const;

	// Start-up (spec/china-boot.md)
	void playIntroduction();

	// Main menu
	MenuChoice mainMenu();
	void drawMenu(int hovered);
	Common::Rect menuButtonRect(uint button);
	bool menuButtonEnabled(uint button) const;

	// Play
	void newGame();
	void playLoop();
	bool loadWarp(const Common::String &name);
	void scrollByCursor();
	void drawFrame();
	void waitFrame();

	Graphics::PixelFormat _format;
	Graphics::ManagedSurface _screen;

	Common::HashMap<Common::String, Common::String, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _labels;

	Graphics::ManagedSurface _menuBackground;
	Sprite _menuNormal[6];
	Sprite _menuHover[6];

	Omni3DManager _omni3D;
	Graphics::ManagedSurface _warpImage;
	bool _hasWarp;
	double _alphaSpeed, _betaSpeed;
	uint _panoramaSpeed;

	Sprite _cursor;

	bool _gameRunning;
	uint32 _nextFrame;
};

} // End of namespace China
} // End of namespace CryOmni3D

#endif
