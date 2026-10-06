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

#ifndef PEINTRE_WORLD_H
#define PEINTRE_WORLD_H

#include "common/array.h"
#include "common/hashmap.h"
#include "common/ptr.h"
#include "common/str.h"

#include "graphics/hotspot_renderer.h"
#include "graphics/surface.h"

#include "peintre/bfg.h"
#include "peintre/obj3d.h"
#include "peintre/render3d.h"

namespace Peintre {

class PeintreEngine;
class World;
struct SceneSession;

/** Scenes by number (scene.md "The scene table"). */
enum {
	kSceneMusee = 0,
	kSceneAuberge = 1,
	kSceneHopiext = 2,
	kSceneMaisonet = 3,
	kSceneMangeurs = 4,
	kSceneCafe = 5,
	kSceneChambre = 6,
	kSceneMaisonj = 7,
	kSceneHopiint = 8,
	kScenePont = 9,
	kSceneTerrasse = 10,
	kSceneJardin = 11,
	kSceneChamp = 12,
	kSceneEglise = 13,
	kNumScenes = 14
};

/** Cursor indices (interaction.md "Cursors"). */
enum {
	kCursorDef0 = 35,
	kCursorDef1 = 36,
	kCursorArrow = 37,
	kCursorHand = 38,
	kCursorZone = 39,
	kCursorFerme = 40,
	kCursorHourglass = 41,
	kCursorCounter = 42,   ///< Ct0..Ct15: 42..57
	kCursorFinger = 58,
	kCursorAccess = 59,
	kCursorDejaVu = 60,
	kCursorBuche = 61,
	kCursorFagot = 62,
	kCursorCle = 63,
	kCursorManivel = 64,
	kCursorRetour = 65,
	kNumCursors = 66
};

/** A scene object-table entry (scene.md "What the init callbacks add"). */
struct SceneObject {
	Common::String name;
	byte cursorType;   ///< 2 hand, 3 zone, 4 finger, 6 access, 0x3C deja vu, else arrow
	bool startHidden;
	int node;          ///< resolved at init, -1 if absent
};

/** An animation record (scene.md, `LoadAnims<scene>`). */
struct AnimRecord {
	Common::String anim;   ///< "<name>.3da" in the bundle
	Common::String node;
	Anim3D data;
	int nodeIndex = -1;
	int32 length = 0;      ///< the first word of the track data
	int32 frame = 0;
	bool playing = false;
	int32 halfTicks = 0;   ///< halfStep's odd tick carried over
};

/** A scene's own code (games/mission-sunlight/docs/<scene>.md). */
class SceneScript {
public:
	virtual ~SceneScript() {}
	/** The init callback, run once after loading. */
	virtual void init(World &w) {}
	/** The frame callback, run every tick after drawing. */
	virtual void frame(World &w) {}
	/** Whether the scene lets the player reach an object from here (distance limits). */
	virtual bool reachable(World &w, int object) { return true; }
};

/** What the 3D loop asks the engine to do next. */
enum WorldExit {
	kExitNone,
	kExitZone,      ///< enter the 2D zone zoneRequest()
	kExitMovie,     ///< play movieRequest(), then call afterMovie()
	kExitOptions,   ///< the option menu
	kExitQuit
};

class World {
public:
	World(PeintreEngine *vm);
	~World();

	/** Loads the scene of state (scene, prevScene); keepCamera for case C of scene.md. */
	bool load(int scene, int prevScene, bool keepCamera);
	void unload();
	/** One 3D tick (movement.md "The tick"). */
	WorldExit tick();
	/** After each tick: keeps its camera and poses for drawing between ticks. */
	void endTick();
	/**
	 * Draws the scene `alpha` (0..1) of the way from the tick before the last to the last
	 * (enhancement: the original draws once per tick), with its 2D and the cursor.
	 */
	void render(float alpha);
	/** The next frames show the current state as it is (after a load or a jump). */
	void cut() { _cut = true; }
	/** ScummVM's hotspot overlay: the markers, and whether they changed since last drawn. */
	void hotspots(Common::Array<Graphics::HotspotInfo> &out) const { out = _hotspotList; }
	bool hotspotsChanged() const { return _hotspotsChanged; }
	void hotspotsDrawn() { _hotspotsChanged = false; }
	/** After the requested movie: load the pending scene. */
	void afterMovie();
	/** After a zone or the option menu: back to the scene at the saved spot. */
	void afterZone(int code);
	/** Starting the 3D after a zone left with `code` (a load, or a resume in 2D). */
	void resumeFromZone(int zone, int code) {
		_zone = zone;
		afterZone(code);
	}
	/** Back from the option menu opened in 3D (0x42f515 sets 0x4e3120). */
	void afterOptions() {
		localVar(0x4e3120) = 1;
		// 0x42f515 resets only the viewport (0x435170), whose clip defaults are 128 and
		// 65,000; the next scene load sets 64 and 80,000 again (scene.md "Camera and view").
		_renderer.setClip(128, 65000);
	}

	PeintreEngine *vm() { return _vm; }
	int scene() const { return _scene; }
	int prevScene() const { return _prevScene; }
	Scene3D &scene3D() { return _scene3D; }
	Camera &camera() { return _cam; }
	int zoneRequest() const { return _zone; }
	const Common::String &movieRequest() const { return _movie; }

	// --- Scene code API -------------------------------------------------------------
	/** A u32 of the saved 3D block, by its original address (0x4aba40..0x4abdab). */
	uint32 &var(uint32 address);
	byte &varByte(uint32 address);
	/**
	 * A u32 of the original outside the saved block (e.g. 0x4e3120, 0x4e30f4): kept for
	 * the whole run, not saved, 0 until set.
	 */
	uint32 &localVar(uint32 address) { return _locals[address]; }
	/** The scene's tables as the last visit this run left them, or null (SceneSession). */
	const SceneSession *lastVisit() const;
	bool zoneSolved(int zone) { return var(0x4abb0c + 4 * zone) != 0; }
	byte &sunflowers() { return varByte(0x4abbd4); }
	bool sceneComplete(int scene);

	/** The name table lookup (0x41edc9): node index or -1. */
	int findObject(const Common::String &name) const;
	void setHidden(int node, bool hidden);
	bool isHidden(int node) const;

	Common::Array<SceneObject> objects;
	/** Resolves `objects` and hides the start-hidden ones. */
	void resolveObjects();
	/** Index in `objects` of a node, -1. */
	int objectIndex(int node) const;

	Common::Array<AnimRecord> anims;
	/** Loads the tracks of `anims` (missing: "LoadAnims::%s manque"). */
	void loadAnims();
	/** Poses a record's node at a frame (0x438290). */
	void pose(uint record, int32 frame);
	/** Advances the playing records by the elapsed ticks and poses them. */
	void advanceAnims();
	uint32 elapsed() const { return _elapsed; }

	/** Extra box sets (`LoadBox<Scene>`): slot 0 is BOX.3DI. */
	bool loadBoxSet(uint slot, const Common::String &name);
	/** Unregisters the current extra set and registers `slot` (0 = BOX.3DI alone). */
	void setBoxSet(uint slot);

	/** Loads <file>.3DM of the bundle as texture `name` (0x4395d0). */
	bool loadTexture(const Common::String &name, const Common::String &file);
	/** Retextures the face groups of `node` using texture `from` to `to` (0x435dc0). */
	void retexture(int node, const Common::String &from, const Common::String &to);

	// Mouse and cursor (interaction.md).
	int pick();
	bool click() const { return _click; }
	void clearClick() { _click = false; }
	byte &cursor() { return _cursor; }
	/** The standard hover: hover shapes back to the arrow, then the object's shape. */
	void applyHover(int objIndex);
	/** A node's (x, z) distance from the camera (0x436200). */
	int32 distance(int node) const;
	/** Picking up item `object` (0..34): hide the node, cursor = item, open the bar. */
	void takeItem(int node, int object);
	void openBar();

	// Carrying a 3D object (interaction.md "Carrying a 3D object").
	bool carrying() const { return _carrying; }
	void carry(int node);
	void dropCarried();
	int carriedNode() const { return _carried; }

	// Requests.
	void requestScene(int target);
	void requestMuseum();
	void requestZone(int zone);
	/** The autosave on closing the bar (0x42f873): camera and scene into the block, then the resume file. */
	void autosave();
	void requestMovie(const Common::String &name);
	void startFlight(int target);

	// Sound.
	void playSound(const Common::String &name, bool loop = false);
	void stopSound(const Common::String &name);
	void stopAllSounds();
	void setAmbience(const Common::String &name);

private:
	/** Camera and scene into the 3D block (0x42f755, 0x42edef, 0x42f873); the autosave leaves the pitch. */
	void storeView(bool pitch);
	void move();
	void collide();
	/** The fixed focal length for the fov option scaled to a picture `width` wide, 0 for the original's. */
	float focal(int width = 640) const { return _focal > 0 ? _focal * MIN<int>(width, 640) / 640 : 0.0f; }
	/** Whether the last move went through a wall from its front (the bug fix in tick()). */
	bool crossedWall(const Vec3i &from) const;
	void mouse();
	void frameLogic(bool hourglass);
	void barLogic();
	void drawBar(int barY);
	void updateCursor();
	void updateHotspots();
	/** Whether a click on the object now does what its hover cursor promises (the overlay). */
	bool objectClickable(int object);
	/**
	 * A point of the node where a click picks it this tick, in the coordinates of `frame`
	 * (the node itself, or its parent whose corners a flag 0x10 node uses).
	 */
	bool findAnchor(int node, int &frame, float local[3]) const;
	/** Where the anchors show in the drawn view (every frame), for the overlay. */
	void placeHotspots();
	/** The outline of the clickable objects over the software frame. */
	void outlineSoftware(Graphics::Surface &dst, const Common::Rect &view);
	void look();
	Common::Rect viewRect() const;
	/** The software renderer's picture: the player's view size, scaled to fill the view. */
	Common::Rect renderRect() const;
	bool loadCursors();
	void drawImage(const Graphics::Surface &img, int x, int y, bool keyed);
	void flightStep();
	void finishFlight();

	PeintreEngine *_vm;
	Renderer3D _renderer;   ///< the tick's frame: picking and node positions
	Renderer3D _view;       ///< the frames drawn
	Common::Array<Tri3D> _tris;
	float _focal = 0.0f;    ///< 0: the original's
	Bfg _bfg;
	Scene3D _scene3D;
	Common::Array<Texture3D *> _textures;          ///< owned
	Common::Array<Common::String> _textureNames;
	Common::Array<const Texture3D *> _materialTex;  ///< per material, may be null
	Common::Array<Boxes3D> _boxSets;                ///< slot -> set
	Common::Array<bool> _boxLoaded;
	int _extraBox = 0;
	SceneScript *_script = nullptr;  ///< owned by the engine's SceneSession
	Common::String _bundle;          ///< the loaded scene's bundle, its SceneSession key
	Graphics::Surface _cursors[kNumCursors];
	Graphics::Surface _bar;
	Graphics::Surface _small; ///< the 3D at a smaller view size, before scaling

	int _scene = -1, _prevScene = 0;
	Camera _cam;
	int32 _v = 0, _vy = 0, _w = 0, _p = 0;
	int32 _s = 0;           ///< strafing (modern controls)
	float _turn = 1.0f;     ///< the turn speed option
	float _lookRest[2] = {}; ///< mouse look below one step: yaw, pitch
	Common::Point _mousePos;
	bool _click = false;
	byte _cursor = kCursorArrow;
	bool _carrying = false;
	int _carried = -1;

	// Inventory bar (interaction.md "Inventory bar").
	int _barState = 0;   ///< 0 hidden, 1 shown, 2 opening, 3 closing
	int _barY = 480;
	int _barFirst = 0;
	bool _barSound = true;     ///< 0x599138: bar_obj plays on the next opening
	int _barButton = -1;      ///< the bar arrow clicked this tick, drawn pressed
	bool _returnShown = false;
	bool _hourglass = false;
	int _shownCursor = -1, _shownScale = 0;
	bool _shownCentred = false;
	bool _systemCursor = false; ///< the high_fps option: the cursor manager's cursor
	Common::Array<Graphics::HotspotInfo> _hotspotList;
	// The overlay's marker for each object: a point of its node a click reaches, kept while
	// it still does, so the marker stays on the object as the view turns.
	struct Anchor {
		int node = -1;
		int frame = -1;   ///< the node whose coordinates `local` is in
		float local[3] = { 0, 0, 0 };
		bool valid = false;
	};
	Common::Array<Anchor> _anchors;   ///< per object
	Common::Array<bool> _outlined;    ///< per node: a clickable object's, outlined
	float _markerScale = 1.0f;        ///< the drawn view's pixels to the frame's (a small view size)
	bool _hotspotsChanged = false;

	// The last two ticks, for drawing in between.
	bool _cut = true;
	Common::Array<float> _posePrev, _poseCur, _poseDraw;
	float _camPrev[6] = {}, _camCur[6] = {};
	int _barYPrev = 480, _barYCur = 480;

	// Requests and modes.
	bool _reload = false;
	int _zone = -1;
	Common::String _movie;
	bool _flying = false;
	int _flightStep = 0;
	int32 _flightDelta[5];
	int _flightTarget = 0;
	uint32 _elapsed = 1;
	WorldExit _exit = kExitNone;
	Common::String _ambience;
	Common::Array<Common::String> _sounds;
	Common::HashMap<uint32, uint32> _locals;
};

/** The scene code for a scene number and bundle (scenes/). */
SceneScript *createSceneScript(int scene, const Common::String &bundle);

} // End of namespace Peintre

#endif // PEINTRE_WORLD_H
