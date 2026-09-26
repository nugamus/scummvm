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
#include "common/ptr.h"
#include "common/str.h"

#include "peintre/bfg.h"
#include "peintre/obj3d.h"
#include "peintre/render3d.h"

namespace Peintre {

class PeintreEngine;
class World;

/** A scene's own code: init once after loading, frame every tick (scene.md). */
class SceneScript {
public:
	virtual ~SceneScript() {}
	virtual void init(World &w) {}
	virtual void frame(World &w) {}
};

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

/** What the 3D loop asks the engine to do next. */
enum WorldExit {
	kExitNone,
	kExitZone,      ///< enter the 2D zone world.zoneRequest()
	kExitMovie,     ///< play movieRequest(), then load the pending scene
	kExitOptions,   ///< the option menu
	kExitQuit
};

class World {
public:
	World(PeintreEngine *vm);
	~World();

	/** Loads `scene` coming from `prevScene` (scene.md "Start positions"). */
	bool load(int scene, int prevScene, bool keepCamera);
	void unload();
	/** One 3D tick (movement.md "The tick"). Returns what the engine must do next. */
	WorldExit tick();

	// Scene code API.
	int scene() const { return _scene; }
	int prevScene() const { return _prevScene; }
	Scene3D &scene3D() { return _scene3D; }
	/** The name table lookup (0x41edc9): the node index, -1 if none. */
	int findObject(const Common::String &name) const;
	void setHidden(int node, bool hidden);
	Camera &camera() { return _cam; }
	const Common::String &zoneMovie() const { return _movie; }
	int zoneRequest() const { return _zone; }
	const Common::String &movieRequest() const { return _movie; }
	/** After a movie or a zone: load the pending scene (or reload the current one). */
	void resume();

private:
	void move();
	void collide();
	void drawFrame();

	PeintreEngine *_vm;
	Renderer3D _renderer;
	Bfg _bfg;
	Scene3D _scene3D;
	Common::Array<Texture3D *> _textures;          ///< loaded textures, owned
	Common::Array<const Texture3D *> _materialTex;  ///< per material, may be null
	Common::Array<Boxes3D> _boxes;                  ///< registered box sets
	Common::ScopedPtr<SceneScript> _script;

	int _scene = -1, _prevScene = 0;
	Camera _cam;
	int32 _v = 0, _vy = 0, _w = 0, _p = 0;          ///< viewer velocities (movement.md)
	int _zone = -1;
	Common::String _movie;
	int _pendingScene = -1;
	uint32 _lastTick = 0;
};

} // End of namespace Peintre

#endif // PEINTRE_WORLD_H
