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
#include "common/hashmap.h"
#include "common/hash-str.h"
#include "common/str.h"
#include "common/ustr.h"
#include "common/rect.h"
#include "graphics/managed_surface.h"

namespace Graphics { class Font; }

#include "grumpa/detection.h"
#include "grumpa/character.h"
#include "grumpa/dialogue.h"
#include "grumpa/inventory.h"
#include "grumpa/mesh.h"

namespace Grumpa {

class EventVM;

/** Load Bitmaps/<name> as an actor texture, ARGB8888 (scene.cpp, E-0303). */
bool loadActorTexture(const Common::String &name, Graphics::Surface &out, bool &alpha);

// The original runs one 800x600, 16-bit (RGB555) surface on DirectDraw7 (games/grumpa docs,
// E-0010). We keep the same page and blit it to the backend each frame.
enum {
	kScreenWidth = 800,
	kScreenHeight = 600
};

// Keymapper actions (metaengine.cpp).
enum GrumpaAction {
	kActionNone,
	kActionDismount, // Backspace: leave a mount, else let the companion go (E-1503)
	kActionStance,   // Ctrl held: the combat stance (E-1400)
	kActionJump      // Space held: jump (E-0812)
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
	/** The whole frame: the scene, the score, the inventory panel, then actor 185's fade. */
	void drawFrame(uint32 now);
	/** The items lying in the scene (inventory.md, E-0900), at layer 3 with the meshes. */
	void drawItems(const Camera &cam, Common::Array<uint16> &depth);
	/** The player's character, for the items' 160-unit reach (nullptr: none). */
	const Character *playerCharacter();
	/** A left click in the scene at `p`: run the first unspent trigger whose polygon contains
	 *  it (docs/spec/events.md). Returns true if a trigger fired (the scene changed). */
	bool handleSceneClick(const Common::Point &p);
	/** Load the game's default cursor as the system cursor (E-0005). */
	void setGameCursor();
	/** Draw `c`'s worn attachments on frame `fr` of its clip mesh `body` (E-1700). */
	void drawAttachments(Character &c, const Mesh &body, int fr, const Camera &cam,
						 Common::Array<uint16> &depth);
	/** Draw the water ripple or the shadow under `c` (E-1660, E-1612). */
	void drawUnder(const Character &c, const Camera &cam, Common::Array<uint16> &depth);
	/** Set the system cursor to the named cursor in UI/002_Cursor (cached). */
	void setCursorImage(const Common::String &name, bool tick = false);
	/** The language's cabinet folder name: "Swedish", "Danish", "Finnish" or "Norwegian". */
	const char *languageFolder() const { return _langFolder; }
	/** Update the cursor for the point `p` (E-1721): the held item, the hand, the pointer or the walk arrow. */
	void updateHoverCursor(const Common::Point &p, bool tick = false);
	/** Actor 2's State (slot 0, E-1800): -1 none yet, 0 an item held, 1 the pointer or a walk
	 *  arrow, 2 the hand. */
	int cursorState() const { return _cursorState; }
	/** Whether `p` is over a clickable trigger or an item (the hotspot cursor). */
	bool overHotspot(const Common::Point &p);
	/** Actor 3, the player controller (walk.cpp, docs/spec/walking.md): steers the player's
	 *  character by the mouse, the views by its floor type, the exits. One 20 ms update. */
	void updatePlayer();
	void updateExits();
	/** A world point on the screen through the current view (walk.cpp); the view must exist. */
	void screenPoint(const Vec3 &v, float &x, float &y) const;
	/** The main menu (UI/001_Menu): parchment background + item labels in Grumpa.TTF. */
	bool drawMenu(int selected);
	/** Play Movies/<name>(.mpg) full screen. The boot intro skips on Escape or a click; a
	 *  cut scene (E-1806) only on Space and ends on black. */
	bool playMovie(const Common::String &name, bool cutScene = false);
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
	Characters &characters() { return _characters; }
	Voices &voices() { return *_voices; }
	EventVM &events() { return *_events; }
	SceneData &sceneData() { return _sceneData; }

private:
	const ADGameDescription *_gameDesc;
	const char *_langFolder = "Swedish";  // the language's cabinet folders: Local_<x>, Sounds_<x>, Movies_<x>
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
	EventVM *_events = nullptr;  // the event VM (events.cpp, docs/spec/events.md)
	Characters _characters;      // Actors/Characters.abi (character.cpp)
	Voices *_voices = nullptr;   // the scene's sound actors (dialogue.cpp)
	uint32 _lastUpdate = 0;      // the VM runs one update per 20 ms of this clock
	friend class EventVM;
	friend class Console;
	bool _showHotspots = false;  // H toggles the trigger-polygon overlay
	Common::String _cursorName;  // the current cursor image file, to avoid redundant reloads
	struct CursorAnim {          // a cursor picture's animation (E-1773)
		int frames = 0, frame = 0, dir = 1, tick = 0;
	};
	Common::HashMap<Common::String, CursorAnim> _cursorAnims;
	int _arrowKind = 9;          // the walk arrow's cursor kind, 9..24 (E-1720)
	int _cursorState = -1;       // actor 2's State: the cursor kind, the arrows as 1 (E-1800)
	// Actor 3 (walk.cpp): the left button as it holds it, the last update's, its clock, the
	// scene it last saw, the floor type its view follows; the exits' latch, first-update flag
	// and remembered scene (CFXToScene +0x15c, +0x160, +0x168).
	bool _leftHeld = false, _leftWas = false;
	bool _backspace = false;     // pressed since actor 3's last tick (E-1503)
	// Its combat (combat.md, E-1400): the right button, the keys it polls, the stance, the
	// swing's hit timer and the clip starts it has seen.
	bool _rightHeld = false, _rightWas = false, _ctrlHeld = false, _spaceHeld = false;
	bool _stance = false;
	int _hitTimer = 0, _playerStarts = 0;
	float _playerClock = 0.0f;
	int _playerScene = -1, _playerView = -1;
	bool _exitLatch = true, _exitFirst = true;
	int _exitScene = 0;
	Mesh _glowMesh;              // Meshes/effect_item.ANB, the glow of a placed item
	Graphics::Surface _glowSkin;
	bool _glowAlpha = false, _glowLoaded = false;
	// The characters' water ripple and shadow, shared by all (E-1612).
	Mesh _rippleMesh, _shadowMesh;
	Graphics::Surface _rippleSkin, _shadowSkin;
	bool _rippleAlpha = false, _shadowAlpha = false, _underLoaded = false;
	int _rippleCount = 0, _rippleFrame = 0;
};

} // End of namespace Grumpa

#endif // GRUMPA_GRUMPA_H
