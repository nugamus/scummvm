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

#include "common/debug.h"

#include "grumpa/character.h"
#include "grumpa/events.h"
#include "grumpa/grumpa.h"
#include "grumpa/score.h"

// Combat (docs/spec/combat.md): hits, death, reactions and the fighters, actors 91..95.

namespace Grumpa {

static float horizontalLength(const Vec3 &d) {
	return sqrtf(d.x * d.x + d.z * d.z);
}

// The player's swing at 60 % of the attack clip (E-1401): each enemy within 140 units and
// within about 37 degrees of the player's forward takes a hit with the player's strength.
void Characters::playerStrike(const Character &p) {
	for (int f = 0; f < 4; f++) {
		Character *t = fighterCharacter(kFirstFighter + f);
		if (!t)
			continue;
		const Vec3 d = p.pos - t->pos;
		const float h = horizontalLength(d);
		debug(1, "Grumpa: the player swings at %u: distance %.1f, facing %.2f", t->id, sqrtf(d.dot(d)),
			  h > 0.0f ? (d.x * sinf(p.yaw) + d.z * cosf(p.yaw)) / h : 0.0f);
		if (sqrtf(d.dot(d)) >= 140.0f || h <= 0.0f)
			continue;
		if ((d.x * sinf(p.yaw) + d.z * cosf(p.yaw)) / h < -0.8f)
			hit(*t, p.id, p.slot(2));
	}
}

// 0x425730 (E-1432): the damage is the attack less the victim's defence, taken off its
// Life by the score, which plays the hit clip while it lives (E-1402).
void Characters::hit(Character &victim, int attacker, int32 attack) {
	victim.lastAttacker = attacker;
	const int32 n = attack - victim.slot(3);
	debug(1, "Grumpa: character %d hits %u for %d (life %d)", attacker, victim.id, n, victim.slot(1));
	if (victim.slot(1) >= 0 && n > 0 && _vm)
		_vm->events().deliver(Score::kId, 0x33, n, victim.id);
}

Character *Characters::fighterCharacter(int id) {
	const int f = id - kFirstFighter;
	return f >= 0 && f < kFighters && _fighters[f].held >= 0 ? find(_fighters[f].held) : nullptr;
}

// Per animation tick of an active character at home: its reactions, then its death (E-1403):
// request 4, the death list when N2D ends, hidden 20 ticks later.
void Characters::combatTick(Character &c) {
	if (c.state.size() < 2)
		return;  // no Life slot
	react(c);
	if (c.slot(1) < 1 && c.deathTimer == -1) {
		request(c, 4, 0.0f);
		c.turnSteps = 0;
		const int f = clipFrames(c, 8);
		if (f > 0) {
			c.deathTimer = f;
			c.hideTimer = f + 20;
		}
		return;
	}
	if (c.deathTimer > 0 && --c.deathTimer == 0) {
		debug(1, "Grumpa: character %u dies", c.id);
		for (uint i = 0; i < c.deathList.size() && _vm; i++)
			_vm->events().push(c.deathList[i]);
		const int role = c.role();
		if (role >= 3 && role <= 7)
			releaseFighter(role - 3);
	}
	if (c.hideTimer > 0 && --c.hideTimer == 0) {
		// These five leave their body where they fell.
		// ponytail: roles 3..5 also message actors 0xb1..0xb3 here (E-1224), not modelled.
		const bool body = c.id == 0x13 || c.id == 0x38 || c.id == 0x40 || c.id == 0x45 || c.id == 0x50;
		c.active = false;
		c.visible = body;
		c.home = body ? _scene : -1;
		c.deathTimer = c.hideTimer = -1;
	}
}

// The reaction list (E-1460): the first entry whose character comes within the two reaction
// spheres fires its commands, once until that character has been away.
void Characters::react(Character &c) {
	if (c.role() == 7 || c.slot(1) < 1 || c.deathTimer > 0 || c.hideTimer > 0)
		return;
	for (uint i = 0; i < c.reactions.size(); i++) {
		Character::Reaction &re = c.reactions[i];
		const Character *o = find(re.other);
		bool touching = false;  // ponytail: items as the other (State 4 here) are in no record
		if (o && o->visible && o->home == _scene) {
			const Vec3 d = Vec3(c.pos.x, c.pos.y + c.sphere, c.pos.z) - Vec3(o->pos.x, o->pos.y + o->sphere, o->pos.z);
			touching = d.dot(d) < (c.reach + o->reach) * (c.reach + o->reach);
		}
		if (!touching) {
			re.fired = false;
		} else if (!re.fired) {
			re.fired = true;
			debug(1, "Grumpa: character %u reacts to %d", c.id, re.other);
			for (uint k = 0; k < re.cmds.size() && _vm; k++)
				_vm->events().push(re.cmds[k]);
			return;
		}
	}
}

// Character opcodes 0x2e..0x30, 0x4b, 0x54 (E-1430): the character fights, held by fighter
// `f`, against `target`; the follower's character joins as the companion (fighter 95) when it
// can attack. ponytail: actor 301 is not told (Q-1420).
void Characters::engage(int f, Character &c, int target, int role) {
	if (c.slot(1) <= 0)
		return;
	Fighter &fi = _fighters[f];
	if (fi.held != (int)c.id) {
		Character *o = fi.held >= 0 ? find(fi.held) : nullptr;
		if (o) {
			apply(*o, 1, 0);
			o->setRole(0);
		}
		_fightCount++;
		fi.held = c.id;
		fi.target = target;
		fi.hitTimer = 0;
		fi.starts = c.starts;
	}
	debug(1, "Grumpa: character %u fights as %d against %d", c.id, kFirstFighter + f, target);
	c.lastAttacker = target;
	c.home = _scene;
	c.active = c.visible = true;
	c.setRole(role);
	Character *comp = find(_follow.id);
	if (comp && comp != &c && _fighters[kFighters - 1].held < 0 && clip(*comp, 0x12)) {
		_follow.id = -1;             // actor 4 lets go
		apply(*comp, 0x54, c.id);
	}
}

// The release (E-1433): when only the companion is left the fight is over and it goes back.
void Characters::releaseFighter(int f) {
	_fighters[f].held = _fighters[f].target = -1;
	_fightCount = MAX(_fightCount - 1, 0);
	if (_fightCount == 1 && _fighters[kFighters - 1].held >= 0) {
		if (_fighters[kFighters - 1].backToFollower)
			handBack();
		else if (Character *comp = find(_fighters[kFighters - 1].held))
			comp->setRole(0);
		_fighters[f].backToFollower = true;
		_fightCount = 0;
	}
}

// Fighter 95's character becomes the follower's again (op 0x2d, E-1223).
void Characters::handBack() {
	if (Character *comp = find(_fighters[kFighters - 1].held))
		apply(*comp, 0x2d, 0);
}

// The scene-change broadcasts (E-1433): leaving (0x19) ends every fight, entering (0x17)
// drops the enemies of 91..93.
void Characters::fightersCommand(int op, int arg1) {
	for (int f = 0; f < kFighters; f++) {
		Fighter &fi = _fighters[f];
		Character *c = fi.held >= 0 ? find(fi.held) : nullptr;
		if (op == 0x17)
			fi.backToFollower = true;
		if (!c || (op == 0x17 && f > 2))
			continue;
		if (f == kFighters - 1) {    // the companion goes back, or loses its role
			if (fi.backToFollower)
				handBack();
			else
				c->setRole(0);
			fi.held = fi.target = -1;
			continue;
		}
		c->deathTimer = c->hideTimer = -1;
		request(*c, 5, 0.0f);
		if (f <= 2) {
			c->active = false;
			if (op == 0x19)
				c->visible = false;
			c->home = -1;
		}
		if (op == 0x19)
			c->setRole(0);
		fi.held = fi.target = -1;
	}
	if (op == 0x19)
		_fightCount = 0;
}

// A new game or a load: no fight goes on and no one is dying (the original saves neither,
// E-1300); the reactions fire afresh.
void Characters::endFights() {
	for (int f = 0; f < kFighters; f++)
		_fighters[f] = Fighter();
	_fightCount = 0;
	for (uint i = 0; i < _chars.size(); i++) {
		Character &c = _chars[i];
		c.deathTimer = c.hideTimer = c.lastAttacker = -1;
		for (uint k = 0; k < c.reactions.size(); k++)
			c.reactions[k].fired = false;
	}
}

// A command to fighter `id` goes to its character (E-1433).
bool Characters::fighterForward(int id, int op, int arg1) {
	if (id < kFirstFighter || id >= kFirstFighter + kFighters)
		return false;
	Character *c = fighterCharacter(id);
	const bool passed = (op >= 0 && op <= 3) || (op >= 0xb && op <= 0xd) || (op >= 0x32 && op <= 0x36) || op == 500 || op == 501;
	if (!c || !passed)
		return true;
	if ((op == 0x32 || op == 0x33) && _vm)
		_vm->events().deliver(c->id, op, arg1, 0);  // Life +/-: the score (E-0704)
	else
		apply(*c, op, arg1);
	return true;
}

// CFXFighter's rule (E-1431), every 0.46-clock tick: approach the target, taunt, attack, keep
// apart, land a swing at 60 % of the attack clip.
void Characters::fighterRule(int f) {
	Fighter &fi = _fighters[f];
	Character *c = find(fi.held);
	if (!c || fi.target < 0 || c->slot(1) <= 0)
		return;
	// Original bug: fighter 95 keeps the companion after handing it back (only leaving the
	// scene clears it, so it joins no other fight there), and its rule turns from Grumpa back
	// to the dead enemy, its last attacker: the companion swings at nothing until the scene
	// changes (E-1431, E-1433). A companion no longer fighting (role 7) is left to follow.
	if (f == kFighters - 1 && c->role() != 7)
		return;
	Character *t = find(fi.target);
	if (!t || t->slot(1) <= 0 || !t->visible) {
		if (f == 4 && _fightCount > 1) {  // the companion takes the next enemy
			for (int g = 0; g < 4; g++) {
				if (_fighters[g].held >= 0) {
					fi.target = c->lastAttacker = _fighters[g].held;
					return;
				}
			}
		}
		if (fi.target != kGrumpa) {
			fi.target = kGrumpa;
			if (f < 4)
				c->lastAttacker = kGrumpa;
		} else {
			request(*c, 2, 0.0f);
			fi.target = -1;
		}
		return;
	}
	Character *p = player();
	if (!t->active && p && p != t && p->active) {
		fi.target = p->id;
		return;
	}
	fi.target = c->lastAttacker;
	t = find(fi.target);
	if (!t)
		return;
	const Vec3 d = c->pos - t->pos;
	const float h = horizontalLength(d);
	const float ux = h > 0.0f ? d.x / h : 0.0f, uz = h > 0.0f ? d.z / h : 1.0f;
	float heading = acosf(CLIP(uz, -1.0f, 1.0f));
	if (ux < 0.0f)
		heading = -heading;
	const float turn = (float)M_PI + heading - c->yaw;
	const float dist = sqrtf(d.dot(d));
	if (dist > 280.0f) {
		request(*c, 1, turn);
	} else if (dist > 180.0f) {
		request(*c, 0, turn);
	} else if (dist > 140.0f) {
		request(*c, 0x23 + (int)_rnd.getRandomNumber(2), turn);
	} else if (c->starts != fi.starts) {
		if (c->clip >= 0x12 && c->clip <= 0x14)
			fi.hitTimer = (int)(clipFrames(*c, c->clip) * 0.6f);
		else if (c->clip == 0x17)
			fi.hitTimer = 0;
		static const int choice[] = { 2, 0x23, 0x24, 0x12, 0x13, 0x14 };
		request(*c, choice[_rnd.getRandomNumber(5)], turn);
		fi.starts = c->starts;
	}
	if (dist < 70.0f && dist > 0.0f) {
		c->pos.x += 4.0f * d.x / dist;
		c->pos.z += 4.0f * d.z / dist;
	}
	for (int g = 0; g <= kFighters; g++) {  // the other fighters' characters, then the follower's
		const Character *o = g == kFighters ? find(_follow.id) : g != f ? fighterCharacter(kFirstFighter + g) : nullptr;
		if (!o || o == c)
			continue;
		const Vec3 e = c->pos - o->pos;
		const float l = sqrtf(e.dot(e));
		if (l > 0.0f && touches(*c, Vec3(o->pos.x, o->pos.y + o->sphere, o->pos.z), o->sphere)) {
			c->pos.x += 4.0f * e.x / l;
			c->pos.z += 4.0f * e.z / l;
		}
	}
	if (fi.hitTimer > 0 && --fi.hitTimer == 0 && dist < 140.0f && h > 0.0f
		&& ux * sinf(c->yaw) + uz * cosf(c->yaw) < -0.8f)
		hit(*t, c->id, c->slot(2));
}

} // End of namespace Grumpa
