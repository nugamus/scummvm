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

#include "grumpa/character.h"
#include "grumpa/events.h"

namespace Grumpa {

// Character roles (docs/spec/characters.md, Roles and Mounts): actor 3 holds the player's
// character, actor 4 (CFXFollower) the companion; rider forms split into Grumpa and the mount.

// The holders' roles (state slot 4) as actors 3 and 4 set them when they load (E-1530).
void Characters::setRoles() {
	for (uint i = 0; i < _chars.size(); i++) {
		Character &c = _chars[i];
		if ((int)c.id == _player)
			c.setRole(1);
		else if ((int)c.id == _follow.id)
			c.setRole(2);
		else if (c.role() <= 2)
			c.setRole(0);
	}
}

// Actor 4 on scene entry (E-1222): the companion comes along, 20 behind the player unless 0x23
// kept it where it was, back to its idle, at home when active.
void Characters::enterFollower(int scene) {
	Character *f = find(_follow.id), *p = player();
	if (f && p) {
		if (_follow.place) {
			f->pos = Vec3(p->pos.x - 20.0f * sinf(p->yaw), p->pos.y, p->pos.z - 20.0f * cosf(p->yaw));
			f->yaw = p->yaw;
		}
		_follow.place = true;
		request(*f, 5, 0.0f);
		f->turnSteps = 0;
		if (f->active)
			f->home = scene;
		f->face = -1;
		f->floorType = -1;
		_follow.freeze = 0;
	}
}

// Actor 4's rule (E-1221), every second update: run to a player more than 170 away, walk to
// one more than 100 away, else stop; closer than 70, step 4 away and walk round, the turn held
// for 20 runs.
void Characters::follow() {
	Character *f = find(_follow.id), *p = player();
	if (!f || ++_follow.divider < 2)
		return;
	_follow.divider = 0;
	if (!p)
		return;
	const float hx = f->pos.x - p->pos.x, hz = f->pos.z - p->pos.z, h = sqrtf(hx * hx + hz * hz);
	float heading = h > 0.0f ? acosf(CLIP(hz / h, -1.0f, 1.0f)) : 0.0f;
	if (hx < 0.0f)
		heading = -heading;
	float turn = (float)M_PI + heading - f->yaw;
	if (_follow.freeze > 0) {
		turn = 0.0f;
		_follow.freeze--;
	}
	const Vec3 d = f->pos - p->pos;
	const float dist = sqrtf(d.dot(d));
	if (dist > 170.0f) {
		request(*f, 1, turn);
		_follow.state = 1;
	} else if (dist > 100.0f) {
		request(*f, 0, turn);
		_follow.state = 1;
	} else {
		if (_follow.state != 2)
			request(*f, 2, turn);
		_follow.state = 2;
	}
	if (dist < 70.0f && dist > 0.0f) {
		nudge(*f, d.x / dist, d.z / dist);
		request(*f, 0, turn + (float)M_PI * 0.5f);
		_follow.state = 0;
		_follow.freeze = 20;
	}
}

// The follower's 4-unit step along (ux, uz), placed without the floor test (the setter at
// 0x4250a0 forgets the face). Original bug: the step runs while a script has stopped the
// companion, so scene 211's opening pushes it off the walk mesh, where every later move is
// undone and it stays stuck (E-1533); a step off the mesh is not taken.
void Characters::nudge(Character &c, float ux, float uz) {
	const float x = c.pos.x + 4.0f * ux, z = c.pos.z + 4.0f * uz;
	if (!_floor.empty() && _floor.faceAt(x, z, c.face) < 0)
		return;
	c.pos.x = x;
	c.pos.z = z;
	c.face = -1;
	c.floorType = -1;
}

// A character with no role standing in the player's or the companion's sphere steps 4 away
// from it; the stop's zero turn replaces the walk's (E-1224).
void Characters::shuffleAside(Character &c) {
	const int holders[2] = { _player, _follow.id };
	for (int i = 0; i < 2; i++) {
		const Character *o = find(holders[i]);
		if (!o || o == &c || !touches(c, Vec3(o->pos.x, o->pos.y + o->sphere, o->pos.z), o->radius))
			continue;
		const Vec3 d = c.pos - o->pos;
		const float len = sqrtf(d.dot(d));
		if (len <= 0.0f)
			continue;
		c.pos.x += 4.0f * d.x / len;
		c.pos.z += 4.0f * d.z / len;
		c.face = c.floorType = -1;
		request(c, 0, (float)M_PI * 0.5f);
		request(c, 2, 0.0f);
	}
}

// Character op 0x2c (E-0816): the player's character.
void Characters::makePlayer(Character &c) {
	Character *old = player();
	if (old && old != &c) {
		request(*old, 2, 0.0f);      // op 1
		old->setRole(0);
	}
	_player = c.id;
	c.active = c.visible = true;
	c.home = _scene;
	c.setRole(1);
	if (_events)
		_events->deliver(kScore, 85, c.id, 0);  // the form icon and Life follow (E-0704)
}

// Op 0x37 to actor 3 or 4 (E-0811, E-1222): the held character stops, loses its role and is
// let go.
void Characters::release(int actor) {
	Character *c = find(held(actor));
	if (c) {
		request(*c, 2, 0.0f);
		c->setRole(0);
		if (actor == kFollowerActor)
			pushList(c->releaseList);  // +0x640 (E-1610)
	}
	if (actor == kPlayerActor) {
		_player = -1;
	} else {
		_follow.id = -1;
		if (_fighters[kFighters - 1].held >= 0)  // a fighting companion stays off (E-1222)
			_fighters[kFighters - 1].backToFollower = false;
	}
}

// Op 0x46 (E-1501): a rider form leaves Grumpa and the mount side by side and Grumpa is the
// player again.
void Characters::split(Character &form) {
	Character *rider = find(form.parts[0]), *mount = find(form.parts[1]);
	if (form.kind != 2 || !rider || !mount)
		return;
	form.active = form.visible = false;
	apply(*mount, 0x47, form.id);
	apply(*rider, 0x47, form.id);
	// The two half-widths: the mount's least vertex x and Grumpa's greatest, this frame.
	float lo = 3.4e38f, hi = 1.2e-38f;  // the original's starting values (E-1501)
	const Mesh *mm = mesh(*mount), *rm = mesh(*rider);
	for (uint i = 0; mm && i < mm->sections.size(); i++) {
		const MeshSection &sec = mm->sections[i];
		for (uint j = 0; j < sec.nv && mount->frame * sec.nv + j < sec.verts.size(); j++)
			lo = MIN(lo, sec.verts[mount->frame * sec.nv + j].x);
	}
	for (uint i = 0; rm && i < rm->sections.size(); i++) {
		const MeshSection &sec = rm->sections[i];
		for (uint j = 0; j < sec.nv && rider->frame * sec.nv + j < sec.verts.size(); j++)
			hi = MAX(hi, sec.verts[rider->frame * sec.nv + j].x);
	}
	if (mm && clip(*rider, 0)) {
		const float d = fabsf(lo) + fabsf(hi);
		rider->pending = Vec3(d * cosf(rider->yaw), 0.0f, -d * sinf(rider->yaw));
	}
	makePlayer(*rider);
	pushList(form.splitList);  // +0x650, last (E-1610)
}

// Backspace on actor 3's tick (E-1503).
void Characters::backspace() {
	Character *p = player();
	if (!p)
		return;
	if (p->kind == 2)
		apply(*p, 0x46, 0);
	else
		release(kFollowerActor);
}

} // End of namespace Grumpa
