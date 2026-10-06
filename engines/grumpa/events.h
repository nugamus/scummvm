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

#ifndef GRUMPA_EVENTS_H
#define GRUMPA_EVENTS_H

#include "common/hashmap.h"
#include "common/random.h"
#include "common/serializer.h"
#include "common/rect.h"

#include "grumpa/mesh.h"
#include "grumpa/score.h"

namespace Grumpa {

class GrumpaEngine;
struct Character;

/** Ray-cast point-in-polygon for a trigger's clickable region (E-0108; scene.cpp). */
bool pointInPolygon(const Common::Array<Common::Point> &poly, const Common::Point &p);

/**
 * The event VM (docs/spec/events.md): the immediate and deferred command lists, conditions,
 * the 50 Hz update, scene entry/exit, and the classes it owns: sprites (0x0d), triggers
 * (0x19), meshes' shared opcodes (0x1a), scripts, counters, timers and flags (0x21..0x27,
 * scene and global.atx), the fade/scene manager 185, the proxy 186, the score 8 and the
 * ambience 180.
 */
class EventVM {
public:
	enum { kUpdateMs = 20 };  // 50 updates a second (E-0202)

	explicit EventVM(GrumpaEngine *engine) : _engine(engine), _rnd("grumpa") {}

	/** A new game: the global actors from Actors/global.atx, no deferred commands, no kept
	 *  scene status. */
	void newGame();
	/** Leave the current scene: broadcast 25, run the immediate list, keep its status. */
	void leaveScene();
	/** Enter scene `num` (its SceneData already loaded): restore its kept status, deliver its
	 *  deferred commands, broadcast 23 and 86 (E-0202). */
	void enterScene(int num, SceneData *scene);
	/** One 20 ms update: run the immediate list, then every actor's update. */
	void update();
	/** A left click at `p` (opcode 18 to the triggers). Returns true if a trigger fired. */
	bool click(const Common::Point &p);
	/** Whether a click at `p` would reach a trigger (for the hover cursor and the overlay). */
	bool triggerClickable(const SceneTrigger &tr) const;

	/** Push a command through its conditions (E-0200). */
	void push(const SceneCommand &cmd);
	/** Run the immediate list once (commands it pushes wait for the next run). */
	void runImmediate();
	/** Deliver one command to an actor (`id` -1: every actor). */
	void deliver(int id, int op, int arg1, int arg2);

	/** Save/load the VM (saveload.cpp): globals, deferred commands, every scene's kept status. */
	void syncState(Common::Serializer &s);

	/** The frame a sprite (E-0208) or a mesh actor (E-0601) shows. */
	int spriteFrame(uint32 id) const;

	/** Actor 185's level (E-0700): the screen's colours are scaled by it / 255. */
	int fadeLevel() const { return _fade.level; }
	/** Escape is taken only while no fade runs and the screen is not black (E-0700). */
	bool fadeIdle() const { return !_fade.running && _fade.level != 0; }
	/** Play the ambience's pending sound now (the boot, before the main menu). */
	void startAmbience() { _ambience.enterScene(); }
	/** The score display (actor 8), drawn over the scene. */
	const Score &score() const { return _score; }

private:
	struct Run {      // sprite animation state and the sprite / mesh latch (E-0208)
		int frame = 0, dir = 0, counter = 0;
		bool playing = false, running = false, latch = false;
	};
	struct Status {   // what a scene keeps of an actor between visits (E-0203)
		bool active, visible, latch;
		int32 count;
		Common::Array<int32> state;
		int frame, dir;
		bool playing, running;
	};
	typedef Common::HashMap<uint32, Status> StatusMap;
	struct Fade {     // actor 185 (E-0700)
		int level = 255, step = 0, hold = 0;
		bool running = false;
		int view = -1, scene = -1;  // shown / entered when the fade ends
	};

	void fadeStart(int level, int step);
	void fadeUpdate();

	bool conditionsHold(const Common::Array<SceneCond> &conds);
	bool stateOf(int id, int slot, int32 &value) const;
	void runList(const CommandList &list, int selfId);
	void collectIds(Common::Array<int> &ids) const;
	void keep();
	void deliverOne(int id, int op, int arg1, int arg2);
	void spritePlay(const SceneSprite &sp, Run &r);
	void spriteAdvance(const SceneSprite &sp, Run &r);
	void meshPlay(SceneMesh &m, Run &r);
	void meshAdvance(SceneMesh &m, Run &r);
	void meshUpdate(SceneMesh &m, Run &r);
	void meshTimerLoad(SceneMesh &m);
	const SpriteHooks *hooks(uint32 id) const;
	void triggerFire(SceneTrigger &tr);
	Character *walker() const;
	bool triggerGate(SceneTrigger &tr);
	void triggerUpdate();
	void logicCommand(SceneLogic &a, int op, int arg1);
	void logicUpdate(SceneLogic &a);
	SceneLogic *logicActor(int id);
	SceneSprite *sprite(int id);
	SceneTrigger *trigger(int id);
	SceneMesh *mesh(int id);
	bool loadGlobals();

	GrumpaEngine *_engine;
	SceneData *_scene = nullptr;
	int _sceneNum = -1;
	CommandList _immediate;
	CommandList _deferred;
	Common::HashMap<uint32, SceneLogic> _globals;  // ids < 600 from global.atx
	Common::Array<int> _globalIds;                 // their ids, in file order (ascending)
	Common::HashMap<uint32, Run> _run;             // the scene's sprites and meshes
	Common::HashMap<int, StatusMap> _kept;         // scene number -> its actors' status
	int _proxyTarget = -1;                         // actor 186 (E-0206)
	bool _fired = false;                           // a trigger fired during click()
	Common::RandomSource _rnd;                     // mesh delay timers
	Fade _fade;
	Score _score;
	Ambience _ambience;
};

} // End of namespace Grumpa

#endif // GRUMPA_EVENTS_H
