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


#include "common/events.h"
#include "common/file.h"
#include "common/hashmap.h"
#include "common/system.h"

#include "grumpa/events.h"
#include "grumpa/grumpa.h"
#include "grumpa/walk.h"

namespace Grumpa {

void Floor::reset() {
	verts.clear();
	faces.clear();
	types.clear();
	next.clear();
	exits.clear();
	entries.clear();
	for (int i = 0; i < 29; i++)
		closed[i] = true;  // the constructor's default (E-0803)
}

// Scene_<num>.scn (E-0500): `u32 type, u32 id` then the body, for type 8 (`u16 nv, u16 nf`,
// nv x 32-byte vertices of which the floor reads x, y, z, nf x 3 u16 corners, nf x u16 floor
// types) and type 0x14 (`u32 n, n x u32` slots, `u32 n, n x {f32 x, y, z, r; u32 scene}`
// exits, `u32 m, m x {f32 x, y, z, rx, ry, rz; u32 scene}` entries); the views follow.
bool Floor::load(int num) {
	reset();
	Common::File f;
	if (!f.open(Common::Path(Common::String::format("Scenes/Scene_%03d.scn", num))))
		return false;
	bool bad = false;
	while (!f.eos() && !f.err() && !bad) {
		uint32 type = f.readUint32LE();
		f.readUint32LE();  // id
		if (f.eos())
			break;
		if (type == 8) {
			uint nv = f.readUint16LE(), nf = f.readUint16LE();
			if (nv == 0 && nf > 0) {
				bad = true;
				break;
			}
			for (uint i = 0; i < nv; i++) {
				float x = f.readFloatLE(), y = f.readFloatLE(), z = f.readFloatLE();
				f.skip(20);
				verts.push_back(Vec3(x, y, z));
			}
			for (uint i = 0; i < 3 * nf; i++) {
				uint16 v = f.readUint16LE();
				faces.push_back(v < nv ? v : 0);
			}
			for (uint i = 0; i < nf; i++)
				types.push_back(f.readUint16LE());
		} else if (type == 0x14) {
			f.skip(4 * f.readUint32LE());
			uint32 n = f.readUint32LE();
			if (n >= 1000)
				bad = true, n = 0;
			for (; n > 0 && !f.eos(); n--) {
				Exit e;
				e.c.x = f.readFloatLE(); e.c.y = f.readFloatLE(); e.c.z = f.readFloatLE();
				e.r = f.readFloatLE();
				e.scene = (int32)f.readUint32LE();
				exits.push_back(e);
			}
			n = f.readUint32LE();
			if (n >= 1000)
				bad = true, n = 0;
			for (; n > 0 && !f.eos(); n--) {
				Entry e;
				e.pos.x = f.readFloatLE(); e.pos.y = f.readFloatLE(); e.pos.z = f.readFloatLE();
				e.rot.x = f.readFloatLE(); e.rot.y = f.readFloatLE(); e.rot.z = f.readFloatLE();
				e.scene = (int32)f.readUint32LE();
				entries.push_back(e);
			}
		} else {
			break;  // the views (scene.cpp reads them)
		}
	}
	if (bad || f.err() || types.size() * 3 != faces.size()) {
		reset();
		return false;
	}
	// The neighbours (0x432c60, E-0500): faces sharing an edge.
	next.resize(faces.size());
	Common::HashMap<uint32, int> open;  // an edge seen once -> its face * 3 + slot
	for (uint i = 0; i < faces.size(); i++) {
		next[i] = -1;
		uint a = faces[i], b = faces[i - i % 3 + (i % 3 + 1) % 3];
		uint32 key = MIN(a, b) << 16 | MAX(a, b);
		if (open.contains(key)) {
			int j = open[key];
			next[i] = j / 3;
			next[j] = i / 3;
			open.erase(key);
		} else {
			open[key] = i;
		}
	}
	return true;
}

// ponytail: the flags start closed on every entry; whether a scene's status keeps them is not
// read. Opcode 23 empties the platform list, which entry then refills with the scene's flagged
// meshes (E-1600): the engine takes those meshes directly (Characters::update).
void Floor::command(int op, int arg) {
	if (op != 5 && op != 6)
		return;
	for (int t = 1; t < 29; t++)
		if (arg == 0 || t == arg - 1)
			closed[t] = op == 5;
}

// The face test (E-0800): the x/z box, then three x/z edge functions, all strictly positive.
static bool inTriangle(const Vec3 &a, const Vec3 &b, const Vec3 &c, float x, float z) {
	if (x < MIN(a.x, MIN(b.x, c.x)) || x > MAX(a.x, MAX(b.x, c.x)) ||
		z < MIN(a.z, MIN(b.z, c.z)) || z > MAX(a.z, MAX(b.z, c.z)))
		return false;
	const Vec3 *e[3][2] = { { &b, &a }, { &c, &b }, { &a, &c } };
	for (int i = 0; i < 3; i++) {
		const Vec3 &p = *e[i][0], &q = *e[i][1];
		if ((q.x - p.x) * (z - p.z) - (q.z - p.z) * (x - p.x) <= 0.0f)
			return false;
	}
	return true;
}

bool Floor::inFace(int f, float x, float z) const {
	return inTriangle(verts[faces[3 * f]], verts[faces[3 * f + 1]], verts[faces[3 * f + 2]], x, z);
}

int Floor::faceAt(float x, float z, int hint) const {
	const int nf = (int)types.size();
	if (hint >= 0 && hint < nf) {
		if (inFace(hint, x, z))
			return hint;
		for (int i = 0; i < 3; i++)
			if (next[3 * hint + i] >= 0 && inFace(next[3 * hint + i], x, z))
				return next[3 * hint + i];
	}
	for (int f = 0; f < nf; f++)
		if (inFace(f, x, z))
			return f;
	return -1;
}

// The height (E-0801): each edge's line, the point projected on it in x/z (not clamped), the
// height there weighted by the inverse x/z distance.
float Floor::height(int f, float x, float z) const {
	if (f < 0 || f >= (int)types.size())
		return 0.0f;
	double sw = 0, sy = 0;
	for (int i = 0; i < 3; i++) {
		const Vec3 &a = verts[faces[3 * f + i]], &b = verts[faces[3 * f + (i + 1) % 3]];
		double ex = b.x - a.x, ez = b.z - a.z, len = ex * ex + ez * ez;
		double t = len > 0 ? ((x - a.x) * ex + (z - a.z) * ez) / len : 0;
		double px = a.x + t * ex, pz = a.z + t * ez;
		double d = sqrt((x - px) * (x - px) + (z - pz) * (z - pz));
		double w = fabs(d) < 1e-7 ? 1e15 : 1.0 / d;
		sw += w;
		sy += w * (a.y + t * (b.y - a.y));
	}
	return (float)(sy / sw);
}

// From face f, every edge nearer than `radius` to (x, z): a boundary edge is a wall contact
// (the nearest kept), an inner edge leads on to the face behind it (0x433950, E-0802).
void Floor::contacts(int f, float x, float z, float radius, Common::Array<bool> &seen,
					 float &best, float &bx, float &bz) const {
	seen[f] = true;
	for (int i = 0; i < 3; i++) {
		const Vec3 &a = verts[faces[3 * f + i]], &b = verts[faces[3 * f + (i + 1) % 3]];
		float ex = b.x - a.x, ez = b.z - a.z, len = ex * ex + ez * ez;
		float t = len > 0 ? CLIP(((x - a.x) * ex + (z - a.z) * ez) / len, 0.0f, 1.0f) : 0.0f;
		float cx = a.x + t * ex - x, cz = a.z + t * ez - z;
		float d = sqrtf(cx * cx + cz * cz);
		if (d >= radius)
			continue;
		int n = next[3 * f + i];
		if (n < 0) {
			if (d < best) {
				best = d;
				bx = cx;
				bz = cz;
			}
		} else if (!seen[n]) {
			contacts(n, x, z, radius, seen, best, bx, bz);
		}
	}
}

// CFXFloor::Move (E-0802). The platforms first (E-1600): the first face of theirs, at frame 0,
// under the target takes the move, with the height of its first corner in the current frame
// and no wall slide. The original indexes one buffer of every section (frame * all vertices +
// the uv index); per section is the same for the flagged meshes, one section each (E-1600).
// The frame clamp and the bounds checks are ours: the original reads unchecked.
void Floor::move(Vec3 &pos, Vec3 delta, float radius, int &face, int &platform,
				 const Common::Array<Platform> &platforms) const {
	const float x = pos.x + delta.x, z = pos.z + delta.z;
	for (uint i = 0; i < platforms.size(); i++) {
		const Mesh &m = *platforms[i].mesh;
		const int frame = CLIP(platforms[i].frame, 0, MAX(m.frames - 1, 0));
		int base = 0;
		for (uint s = 0; s < m.sections.size(); s++) {
			const MeshSection &sec = m.sections[s];
			for (uint f = 0; f < sec.faces.size(); f++) {
				const uint16 *v = sec.faces[f].v;
				if (MAX(v[0], MAX(v[1], v[2])) >= sec.nv || !inTriangle(sec.verts[v[0]], sec.verts[v[1]], sec.verts[v[2]], x, z))
					continue;
				face = base + (int)f;
				platform = (int)i;
				pos = Vec3(x, pos.y + delta.y, z);
				const uint h = frame * sec.nv + v[0];
				if (h < sec.verts.size())
					pos.y = 0.4f * pos.y + 0.6f * sec.verts[h].y;
				return;
			}
			base += sec.faces.size();
		}
	}
	if (platform >= 0)
		platform = face = -1;
	if (face < 0 || face >= (int)types.size())
		face = faceAt(pos.x, pos.z);
	Common::Array<bool> seen;
	for (int k = 0; k < 100; k++) {
		int f = faceAt(pos.x + delta.x, pos.z + delta.z, face);
		if (f < 0) {
			pos = Vec3(pos.x + delta.x, pos.y + delta.y, pos.z + delta.z);
			face = -1;
			return;
		}
		face = f;
		seen.clear();
		seen.resize(types.size());
		float best = 999999.0f, bx = 0, bz = 0;
		contacts(f, pos.x + delta.x, pos.z + delta.z, radius, seen, best, bx, bz);
		if (best >= 999999.0f || best <= 0.0f)
			break;
		delta.x += bx / best * (best - radius);  // the target pushed out to `radius`
		delta.z += bz / best * (best - radius);
	}
	pos = Vec3(pos.x + delta.x, pos.y + delta.y, pos.z + delta.z);
	pos.y = 0.4f * pos.y + 0.6f * height(face, pos.x, pos.z);
}

// `v` on the screen, through the view's camera.
void GrumpaEngine::screenPoint(const Vec3 &v, float &x, float &y) const {
	const Camera &cam = _sceneData.views[_sceneData.view].cam;
	const float in[4] = { v.x, v.y, v.z, 1.0f };
	float e[4], o[4];
	for (int j = 0; j < 4; j++)
		e[j] = in[0] * cam.view[j] + in[1] * cam.view[4 + j] + in[2] * cam.view[8 + j] + in[3] * cam.view[12 + j];
	for (int j = 0; j < 4; j++)
		o[j] = e[0] * cam.proj[j] + e[1] * cam.proj[4 + j] + e[2] * cam.proj[8 + j] + e[3] * cam.proj[12 + j];
	const float w = o[3] > 1e-6f ? o[3] : 1e-6f;
	x = (o[0] / w + 1) * kScreenWidth / 2;
	y = (1 - o[1] / w) * kScreenHeight / 2;
}

// Actor 3, the player controller (E-0811, E-0812), with the scene links' exits (E-0804).
// Holding the left button walks the player's character, Shift runs; the character turns
// towards the cursor, its screen angle taken from the view's yaw.
void GrumpaEngine::updatePlayer() {
	if (_playerScene != _sceneNum) {
		_playerScene = _sceneNum;
		_playerView = -1;
		_exitLatch = _exitFirst = true;  // the constructor's, each scene (E-0818)
		_exitScene = 0;
		_hitTimer = 0;
	}
	Character *p = _characters.player();
	if (!p || !_characters.present(*p) || _sceneData.views.empty()) {
		_leftWas = _leftHeld;
		_backspace = false;
		return;
	}
	const Camera &cam = _sceneData.views[_sceneData.view].cam;
	// The character's screen point and the turn to the cursor (E-0812, E-0817): the screen
	// angle of the cursor (pi +/- acos of its normalized y) less the yaw off the view's.
	// ponytail: the screen point is the position projected (Q-0805: where the original's is
	// written is not read).
	const Common::Point m = g_system->getEventManager()->getMousePos();
	float sx, sy;
	screenPoint(p->pos, sx, sy);
	const float dx = m.x - sx, dy = m.y - sy;
	const float viewYaw = atan2f(cam.view[2], cam.view[10]);  // the camera's forward, in x/z
	const float turn = viewYaw + atan2f(dx, -dy) - p->yaw;
	// The walk arrow (E-0817, E-1720): kind 9 + trunc(angle * 2.6 - 0.3925), the angle pi
	// +/- acos of the cursor's normalized screen y, negated when the cursor is to the right.
	const float len = sqrtf(dx * dx + dy * dy);
	if (len > 0.0f) {
		const float a = acosf(CLIP(dy / len, -1.0f, 1.0f));
		_arrowKind = CLIP(9 + (int)(((float)M_PI + (dx > 0 ? -a : a)) * 2.6f - 0.3925f), 9, 24);
	}

	// The buttons (DoCommand 0x12..0x15, 0x4473c0): in the stance a left press attacks
	// from the idle, the right button blocks (E-1400); else a press over a hotspot, the panel
	// or a held item does not walk (the cursor kind is not an arrow, 9..25); a release stops.
	if (_stance && (_leftHeld != _leftWas || _rightHeld != _rightWas)) {
		debug(2, "Grumpa: stance buttons left %d right %d, clip %d", _leftHeld, _rightHeld, p->clip);
		if (_leftHeld) {
			if (p->clip == 0 || p->clip == 0x20)
				_characters.request(*p, 0x12 + _characters.rollDie(2), turn);
		} else {
			if (p->clip == 2 || p->clip == 5)
				_characters.request(*p, 2, turn);
			if (_rightHeld)
				_characters.request(*p, 0x15, turn);
		}
	} else if (_leftHeld && !_leftWas) {
		if (overHotspot(m) || _inventory.shown() || _inventory.held() >= 0)
			_leftHeld = false;
		else
			_characters.request(*p, 0, turn);
	} else if (!_leftHeld && _leftWas) {
		_characters.request(*p, 2, turn);
	}
	_leftWas = _leftHeld;
	_rightWas = _rightHeld;


	// The rest on actor 3's own animation clock (0.46 an update).
	_playerClock += 0.46f;
	if (_playerClock <= 1.0f)
		return;
	_playerClock -= 1.0f;
	// Floor types 0..4 are view numbers, except in a boat (E-1660): the first selects the view,
	// a change fades to it.
	const int type = p->floorType;
	if (p->mode != Characters::kBoat && type >= 0 && type <= 4 && type < (int)_sceneData.views.size() && type != _playerView) {
		if (_playerView == -1)
			setView(type);
		else
			_events->deliver(185, 30, type, 0);
		_playerView = type;
	}
	// Original bug: Backspace is polled each tick with no latch, so one press both leaves a
	// mount and, Grumpa being the player on the next tick, lets the companion go (E-1503).
	// A press acts once.
	if (_backspace) {
		_backspace = false;
		_characters.backspace();
		return;                       // the player may have changed: steer from the next tick
	}
	// The swing's hit timer (E-1400): set when an attack starts, cleared by a hit, the hit test
	// once it runs out.
	if (p->starts != _playerStarts) {
		if (p->clip >= 0x12 && p->clip <= 0x14)
			_hitTimer = (int)(_characters.clipFrames(*p, p->clip) * 0.6f);
		else if (p->clip == 0x17)
			_hitTimer = 0;
		_playerStarts = p->starts;
	}
	if (_hitTimer > 0 && --_hitTimer == 0)
		_characters.playerStrike(*p);
	// Ctrl: the stance, for a character with attacks. Space: jump. ponytail: the global that
	// also stops the jump (0x4c04b4) is not read.
	_stance = _ctrlHeld && _characters.clipFrames(*p, 0x12) > 0;
	if (_spaceHeld && !_stance && type != 13)
		_characters.request(*p, 3, turn);
	if (_leftHeld && _shiftHeld)
		_characters.request(*p, 1, turn);
	if (dx * dx + dy * dy > 20.0f * 20.0f && (p->clip < 0xf || p->clip > 0x11))
		_characters.request(*p, -1, turn);  // turn only, not while jumping
}

void GrumpaEngine::updateExits() {
	Character *p = _characters.player();
	if (_playerScene != _sceneNum || !p || !_characters.present(*p))
		return;
	// The exits (CFXToScene's update, E-0804, E-0818): touching an exit's sphere changes the
	// scene with the fade unless latched; the latch, set on entry, clears once the player is
	// out of the exit last touched (or touches none on the first update). Actor 601 updates
	// after the characters (ascending ids, E-0202), so a player placed at an entry is settled
	// on the floor before his first test.
	const Floor &floor = _characters.floor();
	bool hit = false;
	for (uint i = 0; i < floor.exits.size(); i++) {
		const Floor::Exit &ex = floor.exits[i];
		if (Characters::touches(*p, ex.c, ex.r)) {
			hit = true;
			_exitScene = ex.scene;
			if (!_exitLatch) {
				debug(1, "Grumpa: exit to scene %d", ex.scene);
				_events->deliver(185, 31, ex.scene, 0);
			}
			_exitLatch = true;
		} else if (ex.scene == _exitScene) {
			_exitLatch = false;
		}
	}
	if (_exitFirst && !hit)
		_exitFirst = _exitLatch = false;
}

} // End of namespace Grumpa
