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

#include "common/algorithm.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/tokenizer.h"

#include "common/serializer.h"
#include "common/system.h"
#include "common/events.h"

#include "grumpa/events.h"
#include "grumpa/grumpa.h"

namespace Grumpa {

// The event VM, written from engines/grumpa/docs/spec/events.md.

enum {
	kFirstSceneId = 600,   // ids 600..979 belong to the current scene (E-0202)
	kGame = 1,             // opcode 60: the main menu, 61: the saved games (E-0901, E-1806)
	kPlayerHolder = 3,     // actor 3 holds the player's character (E-0810)
	kSceneManager = 185,   // fades, views and scene changes (E-0206)
	kProxy = 186,          // forwards to its target (E-0206)
	kSceneLinks = 601,     // CFXToScene, the scene's exits and entries (walk.cpp)
	kFirstItem = 100,      // Items.abi: type 5, ids 100..179
	kLastItem = 179
};

enum {
	kOpPlay = 0, kOpStop = 1, kOpShow = 2, kOpHide = 3, kOpActivate = 11, kOpDeactivate = 12,
	kOpLatch = 13, kOpProxOn = 14, kOpProxOff = 15, kOpSetFlag = 16, kOpClick = 18,
	kOpMouse = 22, kOpEnter = 23, kOpLeave = 25, kOpView = 26, kOpGotoView = 30,
	kOpGotoScene = 31, kOpFadeOut = 32, kOpFadeIn = 33, kOpLifeAdd = 50, kOpLifeSub = 51,
	kOpUnlatch = 52, kOpSetFlag2 = 56, kOpCountAdd = 57, kOpCountSub = 58,
	kOpCountMax = 59, kOpCountReset = 62, kOpProxyTarget = 63, kOpTimerStart = 64,
	kOpMenu = 60, kOpSaves = 61, kOpTimerLimit = 65, kOpTimerOn = 66, kOpTimerStop = 67,
	kOpReset = 86, kOpFilm = 96, kOpOn = 500,
	kOpOff = 501
};

// ---- lookup ---------------------------------------------------------------------------

SceneLogic *EventVM::logicActor(int id) {
	if (id < kFirstSceneId) {
		Common::HashMap<uint32, SceneLogic>::iterator it = _globals.find(id);
		return it != _globals.end() ? &it->_value : nullptr;
	}
	if (_scene)
		for (uint i = 0; i < _scene->logic.size(); i++)
			if ((int)_scene->logic[i].id == id)
				return &_scene->logic[i];
	return nullptr;
}

SceneSprite *EventVM::sprite(int id) {
	if (_scene)
		for (uint i = 0; i < _scene->sprites.size(); i++)
			if ((int)_scene->sprites[i].id == id)
				return &_scene->sprites[i];
	return nullptr;
}

SceneTrigger *EventVM::trigger(int id) {
	if (_scene)
		for (uint i = 0; i < _scene->triggers.size(); i++)
			if ((int)_scene->triggers[i].id == id)
				return &_scene->triggers[i];
	return nullptr;
}

SceneMesh *EventVM::mesh(int id) {
	if (_scene)
		for (uint i = 0; i < _scene->meshes.size(); i++)
			if ((int)_scene->meshes[i].id == id)
				return &_scene->meshes[i];
	return nullptr;
}

const SpriteHooks *EventVM::hooks(uint32 id) const {
	if (_scene)
		for (uint i = 0; i < _scene->spriteHooks.size(); i++)
			if (_scene->spriteHooks[i].id == id)
				return &_scene->spriteHooks[i];
	return nullptr;
}

int EventVM::spriteFrame(uint32 id) const {
	Common::HashMap<uint32, Run>::const_iterator it = _run.find(id);
	return it != _run.end() ? it->_value.frame : 0;
}

// Every actor id the VM knows, ascending (the order of a broadcast and of the update).
void EventVM::collectIds(Common::Array<int> &ids) const {
	ids.clear();
	ids = _globalIds;
	ids.push_back(kSceneManager);
	ids.push_back(kProxy);
	if (_scene) {
		for (uint i = 0; i < _scene->sprites.size(); i++)
			ids.push_back(_scene->sprites[i].id);
		for (uint i = 0; i < _scene->triggers.size(); i++)
			ids.push_back(_scene->triggers[i].id);
		for (uint i = 0; i < _scene->meshes.size(); i++)
			ids.push_back(_scene->meshes[i].id);
		for (uint i = 0; i < _scene->logic.size(); i++)
			ids.push_back(_scene->logic[i].id);
	}
	Common::sort(ids.begin(), ids.end());
}

// ---- conditions and the queue (E-0200, E-0201) ----------------------------------------

// A state slot of an actor the VM owns; false for an actor it does not know (characters,
// items, sounds: their classes are not in the engine yet), which the original skips.
bool EventVM::stateOf(int id, int slot, int32 &value) const {
	if (id == Score::kId)
		return _score.stateOf(slot, value);
	if (id == kMouseId) {  // its State mirrors the cursor kind (E-1800)
		value = slot == 0 ? _engine->cursorState() : 0;
		return true;
	}
	if (Inventory::owns(id))
		return _engine->_inventory.stateOf(id, slot, value);
	if (id == kSceneLinks) {  // CFXToScene: State, and "Scene ID" set by the entry (E-1682)
		value = slot == 1 ? _sceneNum : 0;
		return true;
	}
	if (_engine->_characters.stateOf(id, slot, value))
		return true;
	const SceneLogic *a = const_cast<EventVM *>(this)->logicActor(id);
	if (a) {
		value = slot < (int)a->state.size() ? a->state[slot] : 0;
		return true;
	}
	if (const_cast<EventVM *>(this)->sprite(id) || const_cast<EventVM *>(this)->trigger(id) ||
		const_cast<EventVM *>(this)->mesh(id)) {
		value = 0;  // one "State" slot, never changed by these classes
		return true;
	}
	return false;
}

bool EventVM::conditionsHold(const Common::Array<SceneCond> &conds) {
	if (conds.empty())
		return true;
	bool result = false, prevResult = false;
	int prevLink = 0;
	int lastId = -1;
	for (uint i = 0; i < conds.size(); i++) {
		const SceneCond &c = conds[i];
		int32 v;
		if (!stateOf(c.id, c.slot, v))
			continue;  // an absent actor leaves the result as it was
		lastId = c.id;
		bool r;
		switch (c.mode) {
		case 1: r = v > c.value; break;
		case 2: r = v < c.value; break;
		case 3: r = v != c.value; break;
		default: r = v == c.value; break;
		}
		if (r && i > 0 && prevLink == 0)
			r = prevResult;    // ANDed with the group so far
		if (r && c.link == 1) {
			result = true;     // a whole group holds
			break;
		}
		result = r;
		prevLink = c.link;
		prevResult = r;
	}
	if (result && lastId >= kFirstItem && lastId <= kLastItem)
		_proxyTarget = lastId;  // 186 now acts on the item the guard tested
	return result;
}

void EventVM::push(const SceneCommand &cmd) {
	if (!conditionsHold(cmd.conds)) {
		debug(3, "Grumpa: VM drop (%d,%d,%d,%d) - conditions", cmd.when, cmd.targetId, cmd.opcode, cmd.arg1);
		return;
	}
	if (cmd.when == -1 || cmd.when == _sceneNum)
		_immediate.push_back(cmd);
	else
		_deferred.push_back(cmd);
}

void EventVM::runList(const CommandList &list, int selfId) {
	for (uint i = 0; i < list.size(); i++) {
		if (selfId >= 0 && list[i].targetId == selfId && list[i].opcode == kOpPlay)
			continue;  // triggers and scripts never re-run themselves
		push(list[i]);
	}
}

void EventVM::runImmediate() {
	CommandList now;
	SWAP(now, _immediate);
	for (uint i = 0; i < now.size(); i++)
		deliver(now[i].targetId, now[i].opcode, now[i].arg1, now[i].arg2);
}

void EventVM::deliver(int id, int op, int arg1, int arg2) {
	if (id != -1) {
		deliverOne(id, op, arg1, arg2);
		return;
	}
	Common::Array<int> ids;
	collectIds(ids);
	for (uint i = 0; i < ids.size(); i++)
		deliverOne(ids[i], op, arg1, arg2);
	// The classes outside the VM take a broadcast themselves.
	_engine->_characters.command(-1, op, arg1, arg2);
	for (int i = kFirstItem; i <= kLastItem; i++)
		_engine->_inventory.command(i, op, arg1, arg2);
	CommandList out;
	_engine->_voices->command(-1, op, arg1, out);
	for (uint i = 0; i < out.size(); i++)
		push(out[i]);
}

// ---- DoCommand per class ----------------------------------------------------------------

void EventVM::deliverOne(int id, int op, int arg1, int arg2) {
	debug(2, "Grumpa: VM -> %d op %d (%d, %d)", id, op, arg1, arg2);
	debugC(1, kDebugCoverage, "cov opcode %d", op);
	if (id == kGame) {           // the main menu or the saved games, taken by the main loop
		if (op == kOpMenu || op == kOpSaves)
			_request = op;
		return;
	}
	if (id == kSceneManager) {   // the fade (E-0700); a view or scene waits for its end
		switch (op) {
		case kOpGotoView:
			fadeStart(255, -255 / 20);
			_fade.view = arg1;
			break;
		case kOpGotoScene:
			if (arg2 != -1) {
				fadeStart(255, -255 / 20);
			} else {             // black at once
				fadeStart(0, -1);
				fadeUpdate();
			}
			_fade.scene = arg1;
			break;
		case kOpFadeOut:
			if (arg1 > 0)
				fadeStart(255, -255 / arg1);
			break;
		case kOpFadeIn:
			if (arg1 > 0) {
				fadeStart(0, 255 / arg1);
			} else {
				fadeStart(255, 1);
				fadeUpdate();
			}
			break;
		default:
			break;
		}
		return;
	}
	if (id == Score::kId) {
		_score.command(op, arg1, arg2, _engine->_characters);
		return;
	}
	if (id == Ambience::kId) {
		_ambience.command(op, arg1);
		return;
	}
	if (id == Characters::kPlayerActor || id == Characters::kFollowerActor) {
		// Actors 3 and 4 pass most commands on to the character they hold (E-0811, E-1222).
		const int held = _engine->_characters.held(id);
		if (!Characters::passedOn(op))
			_engine->_characters.command(id, op, arg1, arg2);
		else if (held >= 0)
			deliverOne(held, op, arg1, arg2);
		return;
	}
	if ((op == kOpLifeAdd || op == kOpLifeSub) && _engine->_characters.find(id)) {
		_score.command(op, arg1, id, _engine->_characters);  // a character's life (E-0704)
		return;
	}
	if (id == kProxy) {
		if (op == kOpProxyTarget)
			_proxyTarget = arg1;
		else if (_proxyTarget != -1 && _proxyTarget != kProxy)
			deliverOne(_proxyTarget, op, arg1, arg2);
		return;
	}
	if (SceneLogic *a = logicActor(id)) {
		logicCommand(*a, op, arg1);
		return;
	}
	if (SceneSprite *sp = sprite(id)) {
		Run &r = _run[sp->id];
		if (op == kOpUnlatch) {
			r.latch = false;
			return;
		}
		if (r.latch)
			return;
		switch (op) {
		case kOpPlay: spritePlay(*sp, r); break;
		case kOpStop: r.playing = false; break;
		case kOpShow: sp->visible = true; break;
		case kOpHide: sp->visible = false; break;
		case kOpActivate: sp->active = true; break;
		case kOpDeactivate: sp->active = false; break;
		case kOpLatch: sp->active = sp->visible = r.playing = false; r.latch = true; break;
		case kOpEnter: {
			const SpriteHooks *h = hooks(sp->id);
			if (h && h->autoplay)
				spritePlay(*sp, r);
			break;
		}
		case kOpOn: sp->active = sp->visible = true; spritePlay(*sp, r); break;
		case kOpOff: sp->active = sp->visible = r.playing = false; break;
		default: break;
		}
		return;
	}
	if (SceneTrigger *tr = trigger(id)) {
		if (op == kOpUnlatch) {
			tr->spent = false;
			return;
		}
		if (tr->spent)
			return;
		switch (op) {
		case kOpPlay: tr->active = true; triggerFire(*tr); break;
		case kOpStop: tr->active = false; break;
		case kOpShow: tr->visible = true; break;
		case kOpHide: tr->visible = false; break;
		case kOpActivate:
		case kOpOn: tr->active = tr->visible = true; break;
		case kOpDeactivate:
		case kOpOff: tr->active = tr->visible = false; break;
		case kOpLatch: tr->active = tr->visible = false; tr->spent = true; break;
		case kOpProxOn: tr->proximityOn = true; break;
		case kOpProxOff: tr->proximityOn = false; break;
		case kOpClick: {
			Common::Point p((int16)(arg1 & 0xffff), (int16)((uint32)arg1 >> 16));
			if (triggerClickable(*tr) && tr->poly.size() >= 3 && pointInPolygon(tr->poly, p) &&
				(!walker() || triggerGate(*tr)) && (!tr->hasConds || conditionsHold(tr->conds))) {
				debug(1, "Grumpa: trigger %u fired (%u commands)", tr->id, (uint)tr->cmds.size());
				triggerFire(*tr);
			}
			break;
		}
		default: break;
		}
		return;
	}
	if (SceneMesh *m = mesh(id)) {
		Run &r = _run[m->id];
		if (op == kOpUnlatch) {
			r.latch = false;
			return;
		}
		if (r.latch)
			return;
		switch (op) {
		case kOpPlay: meshPlay(*m, r); break;
		case kOpStop: r.playing = false; break;
		case kOpShow: m->visible = true; break;
		case kOpHide: m->visible = false; break;
		case kOpActivate: m->active = true; break;
		case kOpDeactivate: m->active = false; break;
		case kOpLatch: m->active = m->visible = false; r.latch = true; break;
		case kOpProxOn: m->contactOn = true; break;
		case kOpProxOff: m->contactOn = false; break;
		case kOpEnter:
			if (m->autoplay)
				meshPlay(*m, r);
			break;
		case kOpOn: m->active = m->visible = true; meshPlay(*m, r); break;
		case kOpOff: m->active = m->visible = r.playing = false; break;
		default: break;  // 86/92 reload what is already loaded
		}
		return;
	}
	if (Inventory::owns(id)) {          // items 100..179 and the panel 90 (inventory.cpp)
		_engine->_inventory.command(id, op, arg1, arg2);
		return;
	}
	if (_engine->_characters.command(id, op, arg1, arg2))  // ids 10..88 (character.cpp)
		return;
	CommandList out;
	if (_engine->_voices->command(id, op, arg1, out)) {   // sound actors (dialogue.cpp)
		for (uint i = 0; i < out.size(); i++)
			push(out[i]);
		return;
	}
	debug(3, "Grumpa: VM no actor %d for op %d", id, op);
}

bool EventVM::triggerClickable(const SceneTrigger &tr) const {
	if (tr.spent || !tr.active)
		return false;
	if (tr.view != -1 && _scene && tr.view != _scene->view)
		return false;
	// With no player in the scene a click stands in for walking there (Q-0202): it passes the
	// proximity gate and fires walk-in triggers too.
	if (walker() && !tr.click)
		return false;
	return !tr.proximity || tr.proximityOn;
}

Character *EventVM::walker() const {
	Character *p = _engine->_characters.player();
	return p && _engine->_characters.present(*p) ? p : nullptr;
}

// One source of the proximity gate (E-0705): a present character, of the id required (-1
// any), whose sphere overlaps the trigger's; with `once` it passes once per stay inside.
bool EventVM::gateSource(const SceneTrigger &tr, const Character *c, int32 who, bool &inside) {
	if (!c || !_engine->_characters.present(*c) || (who != -1 && who != (int)c->id))
		return false;
	if (!Characters::touches(*c, tr.centre, tr.radius)) {
		inside = false;
		return false;
	}
	if (tr.once && inside)
		return false;
	inside = true;
	return true;
}

// The proximity gate (E-0207, E-0705): bit 1 the player's character, bit 2 the companion's
// (actor 4's), bit 4 the characters of the fighters 91..94 (no id test), each with its own latch.
bool EventVM::triggerGate(SceneTrigger &tr) {
	if (!tr.proximity)
		return true;
	if (!tr.proximityOn)
		return false;
	Characters &chars = _engine->_characters;
	bool pass = false;
	if (tr.gate & 1)
		pass = gateSource(tr, chars.player(), tr.who, tr.inside);
	if (tr.gate & 2)
		pass = gateSource(tr, chars.follower(), tr.whoCompanion, tr.insideCompanion) || pass;
	if (tr.gate & 4)
		for (int f = 0; f < 4; f++)
			pass = gateSource(tr, chars.fighterCharacter(Characters::kFirstFighter + f), -1, tr.insideFighter[f]) || pass;
	return pass;
}

// The triggers' update (E-0207): a walk-in trigger fires on each update its gate passes; a
// click trigger's latch clears while the player is outside.
void EventVM::triggerUpdate() {
	if (!_scene || !walker())
		return;
	for (uint i = 0; i < _scene->triggers.size(); i++) {
		SceneTrigger &tr = _scene->triggers[i];
		if (!tr.proximity || tr.spent || !tr.active || (tr.view != -1 && tr.view != _scene->view))
			continue;
		if (tr.click) {
			if (!Characters::touches(*walker(), tr.centre, tr.radius))
				tr.inside = false;
			const Character *f = _engine->_characters.follower();
			if (!f || !Characters::touches(*f, tr.centre, tr.radius))
				tr.insideCompanion = false;
			for (int k = 0; k < 4; k++) {
				const Character *e = _engine->_characters.fighterCharacter(Characters::kFirstFighter + k);
				if (!e || !Characters::touches(*e, tr.centre, tr.radius))
					tr.insideFighter[k] = false;
			}
		} else if (triggerGate(tr) && (!tr.hasConds || conditionsHold(tr.conds))) {
			debug(1, "Grumpa: walk-in trigger %u fired", tr.id);
			triggerFire(tr);
		}
	}
}

void EventVM::triggerFire(SceneTrigger &tr) {
	_fired = true;
	runList(tr.cmds, tr.id);
}

// A spawner's round on scene entry (characters.md Spawners, E-0408): every character it lists
// is put away, then min..max different random points each place a random character of their
// list that the round has not placed yet. The spawner is rebuilt with the scene, so what it
// placed is forgotten on the next visit, which spawns afresh.
void EventVM::spawnRound(const SceneLogic &a, int scene) {
	if (a.f1 == 0 || a.spawns.empty())
		return;
	Characters &chars = _engine->_characters;
	for (uint i = 0; i < a.spawns.size(); i++)
		for (uint j = 0; j < a.spawns[i].ids.size(); j++)
			chars.command(a.spawns[i].ids[j], 501, 0, 0);
	const uint n = a.spawns.size();
	const int32 want = a.f0 + (int32)_rnd.getRandomNumber(MAX<int32>(a.f1 - a.f0, 0));
	const uint count = want > 0 ? MIN<uint>((uint)want, n) : 0;
	Common::Array<bool> used(n);
	Common::Array<int32> placed;
	for (uint k = 0; k < count; k++) {
		// A point not used this round, re-drawn on a repeat. The original counts comparisons
		// with the used list (more than 10 ends the round); this counts re-draws.
		int p = -1;
		for (int tries = 0; tries <= 10 && p < 0; tries++) {
			const uint q = _rnd.getRandomNumber(n - 1);
			if (!used[q])
				p = q;
		}
		if (p < 0)
			break;
		used[p] = true;
		const SceneLogic::SpawnPoint &sp = a.spawns[p];
		for (int tries = 0; tries <= 60 && !sp.ids.empty(); tries++) {
			const int32 id = sp.ids[_rnd.getRandomNumber(sp.ids.size() - 1)];
			if (Common::find(placed.begin(), placed.end(), id) == placed.end()) {
				if (chars.spawn(id, sp.pos, sp.rot, scene))
					placed.push_back(id);
				break;
			}
		}
	}
}

// Scripts, counters, timers and flags (E-0204).
void EventVM::logicCommand(SceneLogic &a, int op, int arg1) {
	if (op == kOpUnlatch) {
		a.latch = false;
		return;
	}
	if (a.latch)
		return;
	if (op == kOpLatch) {
		a.latch = true;
		return;
	}
	if (a.state.empty())
		a.state.push_back(0);
	switch (a.type) {
	case 0x07:  // cut scene: 0 play, 23 play if autoplay; 1 stop (the film is over by then)
		if (op == kOpPlay || (op == kOpEnter && a.f0 == 1))
			playFilm(a);
		break;
	case 0x1d:  // spawner: a round on entry (E-0408)
		if (op == kOpEnter)
			spawnRound(a, arg1);
		break;
	case 0x21:  // script
		if (op == kOpPlay)
			runList(a.cmds, a.id);
		else if (op == kOpEnter && (a.f0 != 1 || conditionsHold(a.conds)))
			a.count = 1;  // pending: runs on its update
		break;
	case 0x22:
	case 0x25:  // counter: f0 max, f1 fire
		if (op == kOpCountAdd && a.count < a.f0) {
			a.count += arg1 < 1 ? 1 : arg1;
			if (a.count >= a.f0) {
				a.state[0] = 1;
				if (a.f1 != 0)
					runList(a.cmds, -1);
			}
		} else if (op == kOpCountSub) {
			if (a.state[0] == 1)
				a.state[0] = 0;
			a.count = MAX<int32>(0, a.count - (arg1 < 1 ? 1 : arg1));
		} else if (op == kOpCountMax) {
			a.f0 = arg1;
		} else if (op == kOpCountReset) {
			a.count = 0;
			a.state[0] = 0;
		}
		break;
	case 0x23:
	case 0x26:  // timer: f0 limit (ms), count elapsed
		if (op == kOpTimerStart) {
			a.count = 0;
			a.f0 = arg1;
			a.active = true;
		} else if (op == kOpTimerLimit) {
			a.f0 = arg1;
		} else if (op == kOpTimerOn) {
			a.active = true;
		} else if (op == kOpTimerStop) {
			a.count = 0;
			a.active = false;
		}
		break;
	case 0x24:
	case 0x27:  // flag: f0 fire
		if (op == kOpSetFlag || op == kOpSetFlag2) {
			a.state[0] = arg1;
			if (a.state[0] == 1 && a.f0 == 1)
				runList(a.cmds, -1);
		}
		break;
	default:
		break;
	}
}

// A cut scene (E-1806): broadcast 96 (voice lines stop), full brightness at once, the film with
// the game stopped (Space skips), then its list, actor 3's stop and a full redraw.
void EventVM::playFilm(SceneLogic &a) {
	deliver(-1, kOpFilm, 0, 0);
	runImmediate();
	_fade.level = 255;  // what this does to a pending view or scene is Q-1803
	_fade.running = false;
	_engine->playMovie(a.film, true);
	for (uint i = 0; i < a.cmds.size(); i++)  // pushed now, delivered on the next update
		if (conditionsHold(a.cmds[i].conds))
			_afterFilm.push_back(a.cmds[i]);
	deliver(kPlayerHolder, kOpStop, 0, 0);
	// (87 to the view list, a full redraw: the main loop redraws every frame anyway)
	_engine->_leftHeld = _engine->_leftWas = false;  // the held character stops
}

void EventVM::logicUpdate(SceneLogic &a) {
	if ((a.type == 0x21) && a.count == 1) {
		runList(a.cmds, a.id);
		a.count = 0;
	} else if ((a.type == 0x23 || a.type == 0x26) && a.active) {
		a.count += kUpdateMs;
		if (a.count > a.f0) {
			a.count = 0;
			a.active = false;
			runList(a.cmds, -1);
		}
	}
}

// ---- sprites (E-0208) --------------------------------------------------------------------

void EventVM::spritePlay(const SceneSprite &sp, Run &r) {
	if (r.playing)
		return;
	if (sp.anim & 6)
		r.frame = 0;
	if (sp.anim & 8)
		r.frame = MAX(sp.frameCount, 1) - 1;
	r.playing = r.running = true;
}

void EventVM::spriteAdvance(const SceneSprite &sp, Run &r) {
	int n = MAX(sp.frameCount, 1);
	bool loop = sp.anim & 1;
	const SpriteHooks *h = hooks(sp.id);
	if (sp.anim & 4) {          // forward
		if (++r.frame < n)
			return;
		r.running = false;
		if (!loop || !r.playing) {
			r.frame = n - 1;
			if (!loop) {
				r.playing = false;
				if (h)
					runList(h->onEnd, -1);
			}
			return;
		}
		r.frame = 0;
		r.running = true;
	} else if (sp.anim & 8) {   // backward
		if (--r.frame >= 0)
			return;
		r.running = false;
		if (!loop || !r.playing) {
			r.frame = 0;
			if (!loop) {
				r.playing = false;
				if (h)
					runList(h->onEnd, -1);
			}
			return;
		}
		r.frame = n - 1;
		r.running = true;
	} else if (sp.anim & 2) {   // ping-pong
		if (r.dir == 0) {
			if (++r.frame >= n) {
				r.dir = 1;
				r.frame = MAX(n - 2, 0);
			}
			return;
		}
		if (--r.frame >= 0)
			return;
		r.running = false;
		r.dir = 0;
		r.frame = 0;
		if (!loop) {
			r.playing = false;
			if (h)
				runList(h->onEnd, -1);
		} else if (r.playing) {
			r.frame = MIN(1, n - 1);
			r.running = true;
		}
	} else if (sp.anim & 0x10) {  // forward on one play, back on the next
		if (r.dir == 0) {
			if (++r.frame < n)
				return;
			r.frame = n - 1;
			r.dir = 1;
			r.running = r.playing = false;
			if (h)
				runList(h->onForward, -1);
		} else {
			if (--r.frame >= 0)
				return;
			r.frame = 0;
			r.dir = 0;
			r.running = r.playing = false;
			if (h)
				runList(h->onBackward, -1);
		}
	}
}

// ---- 0x1a mesh animation (E-0601, docs/spec/scene.md) --------------------------------------

// The delay timer's ticks: milliseconds at 50 updates a second; random in [min, max).
void EventVM::meshTimerLoad(SceneMesh &m) {
	if (!m.timerRandom) {
		m.timerTicks = m.timerFixed * 50 / 1000;
		return;
	}
	int32 lo = m.timerMin * 50 / 1000, hi = m.timerMax * 50 / 1000;
	m.timerTicks = hi > lo ? lo + (int32)_rnd.getRandomNumber(hi - lo - 1) : lo;
}

void EventVM::meshPlay(SceneMesh &m, Run &r) {
	if (m.anim & 6)
		r.frame = 0;
	if (m.anim & 8)
		r.frame = MAX(m.mesh.frames, 1);  // one update not drawn, then F-1
	r.playing = true;
	if (m.timerOn) {
		meshTimerLoad(m);
		m.timerCounting = true;
	} else {
		r.running = true;
	}
}

void EventVM::meshUpdate(SceneMesh &m, Run &r) {
	if (!m.active)
		return;
	meshAnimate(m, r);
	meshContacts(m);
}

// A contact sphere's centre (E-1681, E-1683): its vertex counts the renderer's buffer, one
// vertex per uv index across the sections in order, at the position that uv index's corner
// names, in the mesh's current frame.
bool EventVM::contactCentre(const SceneMesh &m, uint i, Vec3 &out) const {
	const uint32 frame = (uint32)MAX(spriteFrame(m.id), 0);
	uint32 v = m.contacts[i].vertex;
	for (uint s = 0; s < m.mesh.sections.size(); v -= m.mesh.sections[s++].vertOfUv.size()) {
		const MeshSection &sec = m.mesh.sections[s];
		if (v < sec.vertOfUv.size()) {
			const uint32 at = frame * sec.nv + sec.vertOfUv[v];
			if (at >= sec.verts.size())
				return false;
			out = sec.verts[at];
			return true;
		}
	}
	return false;
}

// The contact test (E-1681, E-1684), while the tests are on: for each sphere, the kinds its
// flags name, in the order player (3rd list), follower (4th), the mesh `contactMesh`'s spheres
// (6th), the fighters 91..94's characters (8th). The first kind that runs its list ends the
// update. With `once`, a kind's latch makes a touch pass on to the next kind and a miss
// clears it; the latches are per mesh, so another sphere's miss re-arms them (as the
// original does: a touch on one sphere of several runs the list every other update).
// Not modelled: the character filters (+0x220, +0x224, -1 in every file); bit 8's latch
// clears when no sphere of the other mesh touches, where the original clears it when the one
// that hit (+0x254) misses and keeps scanning on a latched hit (the same for the corpus's
// three bit-8 meshes); a sphere whose vertex or frame is out of range is skipped, where the
// original tests its last centre (no file has one).
void EventVM::meshContacts(SceneMesh &m) {
	if (m.contacts.empty() || !m.contactOn || !(m.contactFlags & 0x1b))
		return;
	const SpriteHooks *h = hooks(m.id);
	Characters &chars = _engine->_characters;
	// A kind's test for one sphere: true when it ran its list.
	auto kind = [&](bool touching, bool &latch, const CommandList *list) -> bool {
		if (!touching) {
			latch = false;
			return false;
		}
		if (m.contactOnce && latch)
			return false;
		latch = true;
		if (h && list)
			runList(*list, m.id);
		return true;
	};
	const Character *pl = (m.contactFlags & 1) ? walker() : nullptr;
	const Character *fo = nullptr;
	if (m.contactFlags & 2) {
		fo = chars.follower();
		if (!fo)
			fo = chars.fighterCharacter(Characters::kCompanion);
		if (fo && !chars.present(*fo))
			fo = nullptr;
	}
	SceneMesh *other = nullptr;
	if (m.contactFlags & 8) {
		other = mesh(m.contactMesh);
		if (other && (!other->active || !other->contactOn))
			other = nullptr;
	}
	for (uint i = 0; i < m.contacts.size(); i++) {
		Vec3 c;
		if (!contactCentre(m, i, c))
			continue;
		const float r = m.contacts[i].radius;
		if ((m.contactFlags & 1) && kind(pl && Characters::touches(*pl, c, r), m.contactLatch, h ? &h->onContact : nullptr))
			return;
		if ((m.contactFlags & 2) && kind(fo && Characters::touches(*fo, c, r), m.followerLatch, h ? &h->onFollower : nullptr))
			return;
		if (m.contactFlags & 8) {
			bool touching = false;
			for (uint k = 0; other && k < other->contacts.size() && !touching; k++) {
				Vec3 oc;
				if (contactCentre(*other, k, oc)) {
					const Vec3 d = c - oc;
					const float rr = r + other->contacts[k].radius;
					touching = d.dot(d) < rr * rr;
				}
			}
			if (kind(touching, m.meshLatch, h ? &h->onMesh : nullptr))
				return;
		}
		if (m.contactFlags & 0x10)
			for (int f = 0; f < 4; f++) {
				const Character *e = chars.fighterCharacter(Characters::kFirstFighter + f);
				if (kind(e && chars.present(*e) && Characters::touches(*e, c, r), m.fighterLatch[f], h ? &h->onFighter : nullptr))
					return;
			}
	}
}

void EventVM::meshAnimate(SceneMesh &m, Run &r) {
	if (r.running) {
		// one frame every 50 / fps updates, as for sprites (E-0701)
		if (m.fps > 0 && ++r.counter >= 50 / m.fps) {
			r.counter = 0;
			meshAdvance(m, r);
		}
	} else if (m.timerOn) {
		if (!r.playing) {
			m.timerCounting = false;
			meshTimerLoad(m);
			return;
		}
		if (!m.timerCounting)
			return;
		if (--m.timerTicks < 1) {
			m.timerCounting = false;
			meshTimerLoad(m);
			r.running = true;
		}
	}
}

void EventVM::meshAdvance(SceneMesh &m, Run &r) {
	const int n = MAX(m.mesh.frames, 1);
	const bool loop = m.anim & 1;
	const SpriteHooks *h = hooks(m.id);
	bool restart = false;
	if (m.anim & 4) {           // forward
		if (++r.frame < n)
			return;
		r.running = false;
		r.frame = n - 1;
		if (!loop) {
			if (h)
				runList(h->onEnd, -1);
		} else if (r.playing) {
			r.frame = 0;
			restart = true;
		}
	} else if (m.anim & 8) {    // backward
		if (--r.frame >= 0)
			return;
		r.running = false;
		r.frame = 0;
		if (!loop) {
			if (h)
				runList(h->onEnd, -1);
		} else if (r.playing) {
			r.frame = n - 1;
			restart = true;
		}
	} else if (m.anim & 2) {    // ping-pong
		if (r.dir == 0) {
			if (++r.frame >= n) {
				r.dir = 1;
				r.frame = n - 2;
			}
			return;
		}
		if (--r.frame >= 0)
			return;
		r.running = false;
		r.dir = 0;
		r.frame = MIN(1, n - 1);
		if (!loop) {
			if (h)
				runList(h->onEnd, -1);
		} else if (r.playing) {
			restart = true;
		}
	} else if (m.anim & 0x10) { // forward on one play, back on the next; playing stays set
		if (r.dir == 0) {
			if (++r.frame < n)
				return;
			r.running = false;
			r.frame = n - 1;
			r.dir = 1;
			if (h)
				runList(h->onForward, -1);
		} else {
			if (--r.frame >= 0)
				return;
			r.dir = 0;
			r.running = false;
			r.frame = 0;
			if (h)
				runList(h->onBackward, -1);
		}
	}
	if (restart) {
		if (m.timerOn)
			meshPlay(m, r);
		else
			r.running = true;
	}
}

// ---- the loop, scenes (E-0202) -----------------------------------------------------------

void EventVM::update() {
	_immediate.push_back(_afterFilm);  // a film's list waits for the update after it (E-1806)
	_afterFilm.clear();
	runImmediate();
	CommandList ended;
	_engine->_voices->update(ended);   // sounds that stopped run their lists (dialogue.cpp)
	for (uint i = 0; i < ended.size(); i++)
		push(ended[i]);
	Common::Array<int> ids;
	collectIds(ids);
	for (uint i = 0; i < ids.size(); i++) {
		int id = ids[i];
		if (SceneLogic *a = logicActor(id)) {
			logicUpdate(*a);
		} else if (SceneSprite *sp = sprite(id)) {
			Run &r = _run[sp->id];
			// one frame every max(1, 50 / fps) updates (E-0701)
			if (sp->active && r.running && sp->fps > 0 && ++r.counter >= MAX(1, 50 / sp->fps)) {
				r.counter = 0;
				spriteAdvance(*sp, r);
			}
		} else if (SceneMesh *m = mesh(id)) {
			meshUpdate(*m, _run[m->id]);
		}
	}
	// The mouse (actor 2) updates first: its picture, and its State for conditions (E-1800).
	_engine->updateHoverCursor(g_system->getEventManager()->getMousePos(), true);
	_engine->updatePlayer();
	_engine->_characters.update();
	_engine->updateExits();
	triggerUpdate();
	_engine->_inventory.update(_engine->playerCharacter(), g_system->getEventManager()->getMousePos());
	_score.update();
	_ambience.update();
	fadeUpdate();
}

// ---- actor 185, the fade (E-0700) -------------------------------------------------------------

void EventVM::fadeStart(int level, int step) {
	_fade.level = level;
	_fade.step = step;
	_fade.hold = 4;
	_fade.running = true;
}

void EventVM::fadeUpdate() {
	if (!_fade.running)
		return;
	if (_fade.hold > 0) {
		_fade.hold--;
		return;
	}
	_fade.level += _fade.step;
	if (_fade.level > 0 && _fade.level < 255)
		return;
	_fade.level = CLIP(_fade.level, 0, 255);
	_fade.running = false;
	debug(2, "Grumpa: fade ended at %d", _fade.level);
	if (_fade.view != -1 && _scene) {
		int v = _fade.view;
		_fade.view = -1;
		_engine->setView(v);
		deliver(-1, kOpView, v, 0);
		fadeStart(0, 255 / 20);
	}
	if (_fade.scene != -1) {
		_engine->_nextScene = _fade.scene;   // the main loop enters it (E-0202)
		_fade.scene = -1;
	}
}

bool EventVM::click(const Common::Point &p) {
	_fired = false;
	deliver(-1, kOpClick, (int)(((uint32)(uint16)p.y << 16) | (uint16)p.x), 0);
	return _fired;
}

void EventVM::leaveScene() {
	if (!_scene)
		return;
	deliver(-1, kOpLeave, _sceneNum, 0);
	runImmediate();
	keep();
	_engine->_voices->enterScene(nullptr);  // stop its sounds while their data still exists
	_scene = nullptr;
	_run.clear();
}

void EventVM::keep() {
	StatusMap &kept = _kept[_sceneNum];
	kept.clear();
	for (uint i = 0; i < _scene->sprites.size(); i++) {
		const SceneSprite &sp = _scene->sprites[i];
		const Run &r = _run[sp.id];
		Status s = { sp.active, sp.visible, r.latch, 0, Common::Array<int32>(), r.frame, r.dir, r.playing, r.running };
		kept[sp.id] = s;
	}
	for (uint i = 0; i < _scene->triggers.size(); i++) {
		const SceneTrigger &tr = _scene->triggers[i];
		Status s = { tr.active, tr.visible, tr.spent, 0, Common::Array<int32>(), 0, 0, false, false };
		kept[tr.id] = s;
	}
	for (uint i = 0; i < _scene->meshes.size(); i++) {
		const SceneMesh &m = _scene->meshes[i];
		const Run &r = _run[m.id];
		Status s = { m.active, m.visible, r.latch, 0, Common::Array<int32>(), r.frame, r.dir, r.playing, r.running };
		kept[m.id] = s;
	}
	// Sounds keep their playing flag, not their play position: a line playing when the scene is left
	// or saved plays again from its start (E-1760).
	for (uint i = 0; i < _scene->sounds.size(); i++) {
		Status s = { false, false, false, 0, Common::Array<int32>(), 0, 0, false, false };
		if (_engine->_voices->status(_scene->sounds[i].id, s.active, s.visible, s.latch, s.playing))
			kept[_scene->sounds[i].id] = s;
	}
	for (uint i = 0; i < _scene->logic.size(); i++) {
		const SceneLogic &a = _scene->logic[i];
		// a script, film or spawner keeps its latch, the others their state; a flag's latch is
		// not kept (E-0203, E-0408)
		Status s = { a.active, a.visible, (a.type == 0x21 || a.type == 0x07 || a.type == 0x1d) && a.latch, a.count, a.state, 0, 0, false, false };
		kept[a.id] = s;
	}
}

void EventVM::enterScene(int num, SceneData *scene) {
	debugC(1, kDebugCoverage, "cov scene %d", num);
	_scene = scene;
	_sceneNum = num;
	_run.clear();
	_ambience.enterScene();
	_engine->_voices->enterScene(&scene->sounds);
	for (uint i = 0; i < scene->meshes.size(); i++) {  // as read (E-0601)
		const SceneMesh &m = scene->meshes[i];
		Run &r = _run[m.id];
		r.frame = m.frame;
		r.playing = m.playing;
	}
	if (_kept.contains(num)) {
		const StatusMap &kept = _kept[num];
		for (StatusMap::const_iterator it = kept.begin(); it != kept.end(); ++it) {
			const Status &s = it->_value;
			if (SceneSprite *sp = sprite(it->_key)) {
				sp->active = s.active;
				sp->visible = s.visible;
				Run &r = _run[sp->id];
				r.latch = s.latch;
				r.frame = s.frame;
				r.dir = s.dir;
				r.playing = s.playing;
				r.running = s.running;
			} else if (SceneTrigger *tr = trigger(it->_key)) {
				tr->active = s.active;
				tr->visible = s.visible;
				tr->spent = s.latch;
			} else if (SceneMesh *m = mesh(it->_key)) {
				m->active = s.active;
				m->visible = s.visible;
				m->autoplay = false;  // the status keeps it as 0 (E-0602)
				Run &r = _run[m->id];
				r.latch = s.latch;
				r.frame = s.frame;
				r.dir = s.dir;
				r.playing = s.playing;
				r.running = s.running;
			} else if (_engine->_voices->restore(it->_key, s.active, s.visible, s.latch, s.playing)) {
				// a sound (E-1760)
			} else if (SceneLogic *a = logicActor(it->_key)) {
				a->active = s.active;
				a->visible = s.visible;
				a->latch = s.latch;
				a->count = s.count;
				a->state = s.state;
			}
		}
	}
	// The new floor first, so the commands left for it reach it (E-1540).
	_engine->_characters.loadFloor(num);
	// The commands other scenes left for this one, in the order they were pushed.
	CommandList waiting;
	for (uint i = 0; i < _deferred.size(); ) {
		if (_deferred[i].when == num) {
			waiting.push_back(_deferred[i]);
			_deferred.remove_at(i);
		} else {
			i++;
		}
	}
	for (uint i = 0; i < waiting.size(); i++)
		deliver(waiting[i].targetId, waiting[i].opcode, waiting[i].arg1, waiting[i].arg2);
	debug(1, "Grumpa: VM entered scene %d (%u deferred commands delivered, %u still waiting)",
		  num, (uint)waiting.size(), (uint)_deferred.size());
	deliver(-1, kOpEnter, num, 0);
	runImmediate();
	deliver(-1, kOpReset, 0, 0);
	runImmediate();
	SceneCommand fadeIn;               // the new scene fades in over 24 updates
	fadeIn.targetId = kSceneManager;
	fadeIn.opcode = kOpFadeIn;
	fadeIn.arg1 = 24;
	push(fadeIn);
}

// ---- global.atx (E-0205) -------------------------------------------------------------------

bool EventVM::loadGlobals() {
	Common::File f;
	if (!f.open(Common::Path("Actors/global.atx")))
		return false;
	int type = -1;
	Common::Array<int32> tok;
	while (!f.eos()) {
		Common::String line = f.readLine();
		int slash = line.findFirstOf('/');
		if (slash != -1)
			line = Common::String(line.c_str(), slash);
		line.trim();
		if (line.empty())
			continue;
		if (line[0] == '<') {
			type = atoi(line.c_str() + 1);
			tok.clear();
		} else if (line == "}") {
			if (type < 37 || type > 39 || tok.size() < 4)
				continue;
			// id, active, visible, n, n values, the class fields, the commands
			SceneLogic a;
			uint p = 0;
			a.id = tok[p++];
			a.type = 0x25 + (type - 37);
			a.active = tok[p++] != 0;
			a.visible = tok[p++] != 0;
			int n = tok[p++];
			for (int i = 0; i < n && p < tok.size(); i++)
				a.state.push_back(tok[p++]);
			if (a.state.empty())
				a.state.push_back(0);
			int nf = type == 37 ? 2 : type == 38 ? 3 : 1;
			int32 fields[3] = {0, 0, 0};
			for (int i = 0; i < nf && p < tok.size(); i++)
				fields[i] = tok[p++];
			a.f0 = fields[0];
			a.f1 = fields[1];
			int nc = p < tok.size() ? tok[p++] : 0;
			for (int i = 0; i < nc && p + 6 <= tok.size(); i++) {
				SceneCommand c;
				c.when = tok[p++];
				c.targetId = tok[p++];
				c.opcode = tok[p++];
				c.arg1 = tok[p++];
				c.arg2 = tok[p++];
				int k = tok[p++];
				for (int j = 0; j < k && p + 5 <= tok.size(); j++) {
					SceneCond cd;
					cd.id = tok[p++];
					cd.slot = tok[p++];
					cd.value = tok[p++];
					cd.mode = tok[p++];
					cd.link = tok[p++];
					c.conds.push_back(cd);
				}
				a.cmds.push_back(c);
			}
			_globals[a.id] = a;
			_globalIds.push_back(a.id);
			type = -1;
		} else if (line != "{" && type >= 37 && type <= 39) {
			Common::StringTokenizer t(line, " \t");
			while (!t.empty()) {
				Common::String s = t.nextToken();
				if (!s.empty())
					tok.push_back(atoi(s.c_str()));
			}
		}
	}
	return true;
}

void EventVM::newGame() {
	_scene = nullptr;
	_sceneNum = -1;
	_immediate.clear();
	_deferred.clear();
	_kept.clear();
	_run.clear();
	_globals.clear();
	_globalIds.clear();
	_proxyTarget = -1;
	_fade = Fade();
	if (!_score.load())
		warning("Grumpa: UI/008_Score/008_Score.atx not found");
	if (!_ambience.load())
		warning("Grumpa: Actors/global2.atx not found");
	if (!loadGlobals())
		warning("Grumpa: Actors/global.atx not found");
	_engine->_characters.load();
	debug(1, "Grumpa: VM new game, %u global actors", (uint)_globals.size());
}

// Saves (saveload.cpp): the globals, the deferred list, every scene's kept status (the
// current scene's taken now), actor 186's target.
static void syncCommand(Common::Serializer &s, SceneCommand &c) {
	s.syncAsSint32LE(c.when);
	s.syncAsSint32LE(c.targetId);
	s.syncAsSint32LE(c.opcode);
	s.syncAsSint32LE(c.arg1);
	s.syncAsSint32LE(c.arg2);
	uint32 n = c.conds.size();
	s.syncAsUint32LE(n);
	c.conds.resize(n);
	for (uint i = 0; i < n; i++) {
		s.syncAsSint32LE(c.conds[i].id);
		s.syncAsSint32LE(c.conds[i].slot);
		s.syncAsSint32LE(c.conds[i].value);
		s.syncAsSint32LE(c.conds[i].mode);
		s.syncAsSint32LE(c.conds[i].link);
	}
}

static void syncInts(Common::Serializer &s, Common::Array<int32> &a) {
	uint32 n = a.size();
	s.syncAsUint32LE(n);
	a.resize(n);
	for (uint i = 0; i < n; i++)
		s.syncAsSint32LE(a[i]);
}

void EventVM::syncState(Common::Serializer &s) {
	if (s.isSaving() && _scene)
		keep();
	// globals: ids come from global.atx, so only their changing fields are kept
	uint32 n = _globals.size();
	s.syncAsUint32LE(n);
	for (uint i = 0; i < n; i++) {
		uint32 id = 0;
		if (s.isSaving())
			id = _globalIds[i];
		s.syncAsUint32LE(id);
		SceneLogic &a = _globals[id];
		a.id = id;
		byte f = (a.active ? 1 : 0) | (a.visible ? 2 : 0) | (a.latch ? 4 : 0);
		s.syncAsByte(f);
		a.active = f & 1;
		a.visible = f & 2;
		a.latch = f & 4;
		s.syncAsSint32LE(a.f0);
		s.syncAsSint32LE(a.count);
		syncInts(s, a.state);
	}
	n = _deferred.size();
	s.syncAsUint32LE(n);
	_deferred.resize(n);
	for (uint i = 0; i < n; i++)
		syncCommand(s, _deferred[i]);
	n = _kept.size();
	s.syncAsUint32LE(n);
	Common::HashMap<int, StatusMap>::iterator sc = _kept.begin();
	if (s.isLoading())
		_kept.clear();
	for (uint i = 0; i < n; i++, s.isSaving() ? (void)++sc : (void)0) {
		int32 num = s.isSaving() ? sc->_key : 0;
		s.syncAsSint32LE(num);
		StatusMap &m = _kept[num];
		uint32 k = m.size();
		s.syncAsUint32LE(k);
		StatusMap::iterator it = m.begin();
		for (uint j = 0; j < k; j++) {
			uint32 id = s.isSaving() ? it->_key : 0;
			s.syncAsUint32LE(id);
			Status &st = m[id];
			byte f = (st.active ? 1 : 0) | (st.visible ? 2 : 0) | (st.latch ? 4 : 0) |
					 (st.playing ? 8 : 0) | (st.running ? 16 : 0);
			s.syncAsByte(f);
			st.active = f & 1;
			st.visible = f & 2;
			st.latch = f & 4;
			st.playing = f & 8;
			st.running = f & 16;
			s.syncAsSint32LE(st.count);
			s.syncAsSint32LE(st.frame);
			s.syncAsSint32LE(st.dir);
			syncInts(s, st.state);
			if (s.isSaving())
				++it;
		}
	}
	s.syncAsSint32LE(_proxyTarget);
	if (s.getVersion() >= 3) {
		_score.syncState(s);
		_ambience.syncState(s);
	}
	if (s.isLoading()) {
		_scene = nullptr;   // the scene the save names is entered afresh, from its kept status
		_run.clear();
		_immediate.clear();
	}
}

} // End of namespace Grumpa
