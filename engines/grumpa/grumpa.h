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
#include "grumpa/inventory.h"
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
	/** Decode Bitmaps/<file>, an .fxi 16-bit surface (E-0009). */
	bool loadFxi(const Common::String &file, Common::Array<uint16> &depth, int &w, int &h);
	/** Load an actor mesh from Meshes/<name>.anb (frame 0 geometry, E-0014). */
	bool loadMesh(const Common::String &name, Mesh &mesh);
	/** Read Scenes/Scene_<num>.abi (lights, sprite props, triggers, mesh actors) and the views
	 *  of Scene_<num>.scn (docs/spec/scene.md). */
	bool loadScene(int num, SceneData &scene);
	/** Read the CFXView record of Scenes/Scene_<num>.scn into scene.views. */
	bool loadSceneViews(int num, SceneData &scene);
	/** Draw a sprite prop's frame at its position (colour key or opaque), copying its depth
	 *  frame into `depth` when it has one (docs/spec/scene.md). */
	void drawSprite(const SceneSprite &sprite, int frame = 0, Common::Array<uint16> *depth = nullptr);
	/** Enter scene <num>: load its graph and views and show view 0. */
	bool enterScene(int num);
	/** Show view `k` of the current scene: its background and depth (185 op 30, E-0304). */
	void setView(int k);
	/** Redraw the current scene (background, sprites, 3D actors in render order) at `now`. */
	void renderSceneFrame(uint32 now);
	/** A left click in the scene at `p`: run the first unspent trigger whose polygon contains
	 *  it (docs/spec/events.md). Returns true if a trigger fired (the scene changed). */
	bool handleSceneClick(const Common::Point &p);
	/** Load the game's default cursor as the system cursor (E-0005). */
	void setGameCursor();
	/** Set the system cursor to the named cursor in UI/002_Cursor (cached). */
	void setCursorImage(const Common::String &name);
	/** Update the cursor for the point `p`: a hand over a clickable trigger, else the pointer. */
	void updateHoverCursor(const Common::Point &p);
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


	// Saves (saveload.cpp, docs/spec/save.md) and the inventory (inventory.cpp).
	bool hasFeature(EngineFeature f) const override;
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override;
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override;
	Common::Error saveGameStream(Common::WriteStream *stream, bool isAutosave = false) override;
	Common::Error loadGameStream(Common::SeekableReadStream *stream) override;
	Inventory &inventory() { return _inventory; }

private:
	const ADGameDescription *_gameDesc;
	Graphics::ManagedSurface _screen;
	Graphics::Font *_menuFont = nullptr;
	int _menuFontSize = 0;
	Common::Array<Common::Rect> _menuRects;

	// Current scene (docs/spec/scene.md): its graph, the decoded background, and the tick the
	// scene was entered (for sprite animation timing).
	SceneData _sceneData;
	Graphics::ManagedSurface _sceneBg;
	Common::Array<uint16> _sceneDepth;
	int _depthW = 0, _depthH = 0;
	uint32 _sceneTick0 = 0;
	Inventory _inventory;
	int _sceneNum = -1;     // the scene shown (-1: menu)
	bool _restoring = false;  // _nextScene comes from a loaded save
	bool syncGame(Common::Serializer &s);
	void syncEvents(Common::Serializer &s);
	int _nextScene = -1;   // set by a go-to-scene trigger (E-0116); loaded by the main loop
	bool _showHotspots = false;  // H toggles the trigger-polygon overlay
	Common::String _cursorName;  // the current cursor image, to avoid redundant reloads
};

} // End of namespace Grumpa

#endif // GRUMPA_GRUMPA_H
