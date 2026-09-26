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

#include "graphics/surface.h"

#include "peintre/bfg.h"
#include "peintre/obj3d.h"
#include "peintre/render3d.h"

namespace Peintre {

class PeintreEngine;
class World;

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
};

/** A scene's own code (games/mission-sunlight/docs/<scene>.md). */
class SceneScript {
public:
	virtual ~SceneScript() {}
	/** The init callback, run once after loading. */
	virtual void init(World &w) {}
	/** The frame callback, run every tick after drawing. */
	virtual void frame(World &w) {}
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
	/** After the requested movie: load the pending scene. */
	void afterMovie();
	/** After a zone or the option menu: back to the scene at the saved spot. */
	void afterZone(int code);
	/** Back from the option menu opened in 3D (0x42f515 sets 0x4e3120). */
	void afterOptions() { localVar(0x4e3120) = 1; }

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
	void requestMovie(const Common::String &name);
	void startFlight(int target);

	// Sound.
	void playSound(const Common::String &name, bool loop = false);
	void stopSound(const Common::String &name);
	void stopAllSounds();
	void setAmbience(const Common::String &name);

private:
	void move();
	void collide();
	void mouse();
	void drawFrame(bool hourglass);
	void drawBar();
	bool loadCursors();
	void drawImage(const Graphics::Surface &img, int x, int y, bool keyed);
	void flightStep();
	void finishFlight();

	PeintreEngine *_vm;
	Renderer3D _renderer;
	Bfg _bfg;
	Scene3D _scene3D;
	Common::Array<Texture3D *> _textures;          ///< owned
	Common::Array<Common::String> _textureNames;
	Common::Array<const Texture3D *> _materialTex;  ///< per material, may be null
	Common::Array<Boxes3D> _boxSets;                ///< slot -> set
	Common::Array<bool> _boxLoaded;
	int _extraBox = 0;
	Common::ScopedPtr<SceneScript> _script;
	Graphics::Surface _cursors[kNumCursors];
	Graphics::Surface _bar;

	int _scene = -1, _prevScene = 0;
	Camera _cam;
	int32 _v = 0, _vy = 0, _w = 0, _p = 0;
	Common::Point _mousePos;
	bool _click = false;
	byte _cursor = kCursorArrow;
	bool _carrying = false;
	int _carried = -1;

	// Inventory bar (interaction.md "Inventory bar").
	int _barState = 0;   ///< 0 hidden, 1 shown, 2 opening, 3 closing
	int _barY = 480;
	int _barFirst = 0;
	bool _barSoundPlayed = false;

	// Requests and modes.
	bool _reload = false;
	int _zone = -1;
	Common::String _movie;
	bool _flying = false;
	int _flightStep = 0;
	int32 _flightDelta[5];
	int _flightTarget = 0;
	uint32 _lastTickCount = 0;
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
