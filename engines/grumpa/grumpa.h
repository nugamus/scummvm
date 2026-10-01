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

#ifndef GRUMPA_GRUMPA_H
#define GRUMPA_GRUMPA_H

#include "common/error.h"
#include "engines/advancedDetector.h"
#include "engines/engine.h"
#include "common/array.h"
#include "common/str.h"
#include "common/ustr.h"
#include "common/rect.h"
#include "graphics/managed_surface.h"

namespace Graphics { class Font; }

#include "grumpa/detection.h"
#include "grumpa/mesh.h"

namespace Grumpa {

// The original runs one 800x600, 16-bit (RGB555) surface on DirectDraw7 (games/grumpa docs,
// E-0010). We keep the same page and blit it to the backend each frame.
enum {
	kScreenWidth = 800,
	kScreenHeight = 600
};

class GrumpaEngine : public Engine {
public:
	GrumpaEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~GrumpaEngine() override;

	Common::Error run() override;

	/** Draw the pre-rendered colour view <view>_IS.jpg onto the page (E-0011). */
	bool loadBackground(const Common::String &view);
	/** Decode the view's depth buffer <view>_IZ.fxi into `depth` (E-0009). */
	bool loadDepth(const Common::String &view, Common::Array<uint16> &depth, int &w, int &h);
	/** Load an actor mesh from Meshes/<name>.anb (frame 0 geometry, E-0014). */
	bool loadMesh(const Common::String &name, Mesh &mesh);
	/** Read Scenes/Scene_<num>.abi and collect its type-0x11 views (docs/spec/scene.md). */
	bool loadScene(int num, Common::Array<SceneView> &views);
	/** Load the game's hand cursor as the system cursor (E-0005). */
	void setGameCursor();
	/** The main menu (UI/001_Menu): parchment background + item labels in Grumpa.TTF. */
	bool drawMenu(int selected);
	/** Play an MPEG-1 film from a Movies_<lang> folder; Esc/click skips. */
	bool playMovie(const Common::String &name);
	/** Index of the menu item at screen point p (-1 if none); valid after drawMenu. */
	int menuItemAt(const Common::Point &p) const;
	/** A scrolling text screen (Credits, Help) over the parchment. */
	void showTextScreen(const Common::String &textFile);
	bool loadTextFile(const Common::String &rel, Common::Array<Common::U32String> &lines);
	const Graphics::Font *menuFont(int size);
	static Common::U32String fromCp1252(const Common::String &s);
	bool loadMenuText(Common::Array<Common::U32String> &items);

private:
	const ADGameDescription *_gameDesc;
	Graphics::ManagedSurface _screen;
	Graphics::Font *_menuFont = nullptr;
	int _menuFontSize = 0;
	Common::Array<Common::Rect> _menuRects;
};

} // End of namespace Grumpa

#endif // GRUMPA_GRUMPA_H
