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
#include "common/file.h"

#include "grumpa/character.h"
#include "grumpa/events.h"
#include "grumpa/grumpa.h"

namespace Grumpa {

namespace {

// A bounds-checked reader over a whole .abi file; any over-read or implausible count marks
// it bad.
struct Reader {
	Common::Array<byte> buf;
	uint32 pos = 0;
	bool ok = true;

	uint32 left() const { return pos <= buf.size() ? buf.size() - pos : 0; }
	void skip(uint32 n) {
		if (!ok || n > left()) { ok = false; return; }
		pos += n;
	}
	uint32 u32() {
		if (!ok || left() < 4) { ok = false; return 0; }
		uint32 v = READ_LE_UINT32(&buf[pos]);
		pos += 4;
		return v;
	}
	float f32() {
		uint32 v = u32();
		float f;
		memcpy(&f, &v, 4);
		return f;
	}
	uint32 count() {
		uint32 n = u32();
		if (n > 0x100000) ok = false;
		return ok ? n : 0;
	}
	Common::String str() {           // pascal string, NUL-terminated inside
		uint32 n = count();
		if (!ok || n > left()) { ok = false; return Common::String(); }
		Common::String s((const char *)&buf[pos], n);
		pos += n;
		uint p = s.findFirstOf('\0');
		if (p != Common::String::npos)
			s = Common::String(s.c_str(), p);
		return s;
	}
	void names(Common::Array<Common::String> &out) {
		uint32 n = count();
		for (uint32 i = 0; i < n && ok; i++)
			out.push_back(str());
	}
	void ccVec() {                   // CC = 5 u32 + u32 m + m * EC (5 u32)
		uint32 n = count();
		for (uint32 i = 0; i < n && ok; i++) {
			skip(20);
			skip(20 * count());
		}
	}
};

} // anonymous namespace

// The CFXCharacter record (docs/formats/README.md, E-0401): the header, the placement, the
// rule / command / message / reaction lists (kept for later, skipped here), the name lists,
// the carried objects, the kind and the pair list.
bool Characters::load() {
	Common::File f;
	if (!f.open(Common::Path("Actors/Characters.abi"))) {
		warning("Grumpa: Actors/Characters.abi not found");
		return false;
	}
	Reader r;
	r.buf.resize(f.size());
	f.read(r.buf.begin(), r.buf.size());
	_chars.clear();
	while (r.left() >= 8 && r.ok) {
		if (r.u32() != 0x03) {
			r.ok = false;
			break;
		}
		Character c;
		c.id = r.u32();
		c.active = r.u32() != 0;
		c.visible = r.u32() != 0;
		for (uint32 k = r.count(); k > 0 && r.ok; k--)   // the state slots (E-0407)
			c.state.push_back((int32)r.u32());
		c.home = (int32)r.u32();
		c.pos.x = r.f32(); c.pos.y = r.f32(); c.pos.z = r.f32();
		r.skip(4);
		c.yaw = r.f32();
		r.skip(4);
		c.radius = r.f32();          // [0x28c]
		r.skip(4);                   // [0x5fc]
		c.sphere = r.f32();          // [0x290]
		r.skip(8);                   // [0x48c], [0x490]
		uint32 n = r.count();        // rules: EC vector + CC vector each
		for (uint32 i = 0; i < n && r.ok; i++) {
			r.skip(20 * r.count());
			r.ccVec();
		}
		r.ccVec();
		r.ccVec();
		n = r.count();               // messages: text + CC vector (none in the data)
		for (uint32 i = 0; i < n && r.ok; i++) {
			r.str();
			r.ccVec();
		}
		r.ccVec();
		n = r.count();               // reactions: character id + CC vector
		for (uint32 i = 0; i < n && r.ok; i++) {
			r.skip(4);
			r.ccVec();
		}
		const uint32 start = r.u32();  // [0x434]: the clip slot playing (E-0603)
		r.names(c.anims);
		r.names(c.sounds);
		c.texture = (int)r.u32();
		r.names(c.textures);
		n = r.count();
		for (uint32 i = 0; i < n && r.ok; i++) {
			Character::Attachment a;
			a.anb = r.str();
			a.tga = r.str();
			r.skip(12);
			c.attachments.push_back(a);
		}
		c.kind = (int)r.u32();
		c.parts[0] = (int)r.u32();
		c.parts[1] = (int)r.u32();
		n = r.count();
		for (uint32 i = 0; i < 2 * n && r.ok; i++)
			c.pairs.push_back(r.u32());
		c.clips.resize(44);          // SetDevice sizes the slot table to 0x2c (E-0815)
		c.clip = MIN<uint32>(start, 43);
		debug(3, "Grumpa: character %u clip %u, first .anb %s", c.id, start, c.anims.empty() ? "-" : c.anims[0].c_str());
		if (r.ok)
			_chars.push_back(c);
	}
	_scene = -1;                     // a new game: no scene entered yet
	_player = kGrumpa;
	_follow = Follower();
	setRoles();
	debug(1, "Grumpa: %u characters loaded%s", (uint)_chars.size(), r.ok ? "" : " (read error)");
	return r.ok;
}

Character *Characters::find(int id) {
	for (uint i = 0; i < _chars.size(); i++)
		if ((int)_chars[i].id == id)
			return &_chars[i];
	return nullptr;
}

bool Characters::stateOf(int id, int slot, int32 &value) {
	Character *c = find(id);
	if (!c)
		return false;
	value = slot >= 0 && slot < (int)c->state.size() ? c->state[slot] : 0;
	return true;
}

// A clip slot (E-0815): the .anb whose name starts with the slot's number, and its .amb, a
// u32 count and per frame 24 bytes: the root motion x, y, z, then a rotation (unused).
Character::Clip *Characters::clip(Character &c, int slot) {
	if (slot < 0 || slot >= (int)c.clips.size())
		return nullptr;
	Character::Clip &k = c.clips[slot];
	if (!k.loaded && _vm) {
		k.loaded = true;
		for (uint i = 0; i < c.anims.size(); i++) {
			if (!Common::isDigit(c.anims[i].firstChar()) || atoi(c.anims[i].c_str()) != slot)
				continue;
			Common::String base = c.anims[i];
			if (base.size() > 4 && base[base.size() - 4] == '.')
				base = Common::String(base.c_str(), base.size() - 4);
			_vm->loadMesh(base, k.mesh);
			Common::File f;
			if (f.open(Common::Path("Meshes/" + base + ".amb"))) {
				for (uint32 n = f.readUint32LE(); n > 0 && n < 10000 && f.pos() + 24 <= f.size(); n--) {
					float x = f.readFloatLE(), y = f.readFloatLE(), z = f.readFloatLE();
					f.skip(12);
					k.motion.push_back(Vec3(x, y, z));
				}
			}
			break;
		}
	}
	return k.mesh.empty() ? nullptr : &k;
}

const Mesh *Characters::mesh(Character &c) {
	Character::Clip *k = clip(c, c.clip);
	return k ? &k->mesh : nullptr;
}

static float wrapAngle(float a) {
	while (a > (float)M_PI)
		a -= 2.0f * (float)M_PI;
	while (a < -(float)M_PI)
		a += 2.0f * (float)M_PI;
	return a;
}

// The request table (E-0813). A "cut" ends the clip playing, so the queue takes over on the
// next animation tick. ponytail: walk, run, stop and reset; jump, death, attacks and the turn
// lock +0x4dc come with combat (Q-0806).
void Characters::request(Character &c, int req, float turn) {
	c.yaw = wrapAngle(c.yaw);
	c.turnSteps = 10;
	c.turnStep = wrapAngle(turn) * 0.1f;
	if (req == -1)
		return;
	const int cur = c.clip;
	Character::Clip *k = clip(c, cur);
	const int cutFrame = k ? k->mesh.frames : 0;
	static const int walkIdle[] = { 1, 2, -1 }, walkOn[] = { 2, -1 }, walkUp[] = { 0x21, 1, 2, -1 };
	static const int runIdle[] = { 1, 4, 5, 6, 2, -1 }, runWalk[] = { 4, 5, 6, 2, -1 };
	static const int runRun[] = { 5, 6, 2, -1 }, runUp[] = { 0x21, 1, 4, 5, 6, 2, -1 };
	static const int stopWalk[] = { 3, 0, -1 }, stopRun[] = { 7, 0, -1 }, stopIdle[] = { 0, -1 };
	const int *push = nullptr;
	bool cut = false;
	switch (req) {
	case 0:
		if (cur == 0 || cur == 0xb || cur == 0xd)
			push = walkIdle, cut = true;
		else if (cur >= 1 && cur <= 3)
			push = walkOn;
		else if (cur == 0x20)
			push = walkUp, cut = true;
		break;
	case 1:
		if (cur == 0 || cur == 0xb || cur == 3)
			push = runIdle;
		else if (cur == 1 || cur == 2)
			push = runWalk;
		else if (cur == 5)
			push = runRun;
		else if (cur == 0x20)
			push = runUp;
		break;
	case 2:
		push = cur == 1 || cur == 2 ? stopWalk : cur == 5 ? stopRun : stopIdle;
		break;
	case 5:
		c.clip = 0;
		c.frame = 0;
		c.clock = 0.6f;
		c.queue.clear();
		return;
	default:
		return;
	}
	c.queue.clear();
	for (; push && *push >= 0; push++)
		c.queue.push_back(*push);
	if (cut)
		c.frame = cutFrame;
}

void Characters::syncState(Common::Serializer &s) {
	for (uint i = 0; i < _chars.size(); i++) {
		Character &c = _chars[i];
		s.syncAsFloatLE(c.pos.x);
		s.syncAsFloatLE(c.pos.y);
		s.syncAsFloatLE(c.pos.z);
		s.syncAsFloatLE(c.yaw);
		s.syncAsSint32LE(c.home);
		s.syncAsByte(c.active);
		s.syncAsByte(c.visible);
	}
	// Actors 3 and 4 keep the character they hold (E-1530, E-1220).
	s.syncAsSint32LE(_player, 5);
	s.syncAsSint32LE(_follow.id, 5);
	if (s.isLoading()) {
		if (s.getVersion() < 5) {
			_player = kGrumpa;
			_follow.id = kKraken;
		}
		for (uint i = 0; i < _chars.size(); i++)
			_chars[i].pending = Vec3();
		_follow.freeze = _follow.divider = 0;
		_follow.state = -1;
		setRoles();
	}
}

bool Characters::touches(const Character &c, const Vec3 &centre, float r) {
	const Vec3 d = Vec3(c.pos.x, c.pos.y + c.sphere, c.pos.z) - centre;
	return d.dot(d) < (c.sphere + r) * (c.sphere + r);
}

// The character update (E-0603, E-0813, E-0814, E-0802/E-0803): on each animation tick (0.46
// an update) the frame steps and, at the clip's end, the queue's next slot starts (else the
// clip loops); the yaw takes one step of its turn; the clip's root motion for the frame, turned
// by the yaw, goes through the walk mesh and the platforms. ponytail: no idle fidget (slots
// 0x1f/0x20 after 11 idle loops), no swimming mode [0x48c].
void Characters::update() {
	// The platform list (E-1600): the scene's 0x1a meshes with +0x1d0 set, when active; the
	// files list them by ascending id, the original's order.
	Common::Array<Floor::Platform> platforms;
	const Common::Array<SceneMesh> &meshes = _vm->sceneData().meshes;
	for (uint i = 0; i < meshes.size(); i++)
		if (meshes[i].platform && meshes[i].active && !meshes[i].mesh.empty()) {
			Floor::Platform pl = { &meshes[i].mesh, _vm->events().spriteFrame(meshes[i].id) };
			platforms.push_back(pl);
		}
	follow();
	for (uint i = 0; i < _chars.size(); i++) {
		Character &c = _chars[i];
		if (!c.active || c.home != _scene)
			continue;
		if (c.role() == 0)
			shuffleAside(c);
		Character::Clip *k = clip(c, c.clip);
		if (!k || k->mesh.frames <= 0)
			continue;
		c.clock += 0.46f;
		if (c.clock <= 1.0f)
			continue;
		c.clock -= 1.0f;
		if (c.frame + 1 >= k->mesh.frames) {
			c.frame = 0;
			while (!c.queue.empty()) {  // slots a character lacks are passed over
				int next = c.queue.front();
				c.queue.remove_at(0);
				if (clip(c, next)) {
					c.clip = next;
					k = clip(c, next);
					break;
				}
			}
		} else {
			c.frame++;
		}
		if (c.turnSteps > 0 && c.clip != 8 && c.clip != 0xc && (c.clip < 0x12 || c.clip > 0x14)) {
			c.yaw += c.turnStep;
			c.turnSteps--;
		}
		Vec3 delta = c.pending;
		c.pending = Vec3();
		if (c.clip != 0 && !k->motion.empty()) {
			const Vec3 &m = k->motion[MIN<int>(c.frame, k->motion.size() - 1)];  // Q-0807
			const float sn = sinf(c.yaw), cs = cosf(c.yaw);
			delta = Vec3(delta.x + m.z * sn + m.x * cs, 0.0f, delta.z + m.z * cs - m.x * sn);
		}
		if (_floor.empty())
			continue;
		const Vec3 old = c.pos;
		const int oldFace = c.face, oldPlatform = c.platform;
		_floor.move(c.pos, delta, c.radius, c.face, c.platform, platforms);
		// Off the mesh, a step up of more than 20 (80 on a platform, type 15) or a closed wall
		// type: back (E-0803, E-1600).
		int type = c.platform >= 0 ? 15 : c.face >= 0 ? _floor.types[c.face] : -1;
		if (c.face < 0 || c.pos.y > old.y + (c.platform >= 0 ? 80.0f : 20.0f) ||
			(type > 18 && type <= 28 && _floor.closed[type])) {
			c.pos = old;
			c.face = oldFace;
			c.platform = oldPlatform;
		} else {
			c.floorType = type;
		}
	}
}

// Actor 3 on scene entry (E-0804): the player is placed at the entry from the scene left, or
// stays where it is (an entry for scene -2), or takes the first entry; not on the first entry
// of a new game or a load. Then, on every entry, the request 5 back to the idle (E-0830).
void Characters::enter(int scene) {
	const int prev = _scene;
	_floor.load(scene);
	Character *p = player();
	if (!p)
		return;
	if (_firstEntry && !_floor.entries.empty()) {  // a save without the characters
		p->pos = _floor.entries[0].pos;
		p->yaw = _floor.entries[0].rot.y;
		p->home = scene;
	}
	_firstEntry = false;
	if (prev != -1) {
		const Floor::Entry *e = nullptr;
		bool stay = false;
		for (uint i = 0; i < _floor.entries.size() && !e; i++)
			if (_floor.entries[i].scene == prev)
				e = &_floor.entries[i];
		for (uint i = 0; i < _floor.entries.size() && !e && !stay; i++)
			stay = _floor.entries[i].scene == -2;
		if (!e && !stay && !_floor.entries.empty())
			e = &_floor.entries[0];
		if (e) {
			p->pos = e->pos;
			p->yaw = e->rot.y;
		}
		// The player goes along (E-0816). The original does it on the first entry too, for the
		// character op 0x2c has made the player (Q-0812); a new game enters scene 1, Grumpa's
		// home, so only a first entry elsewhere differs.
		if (p->active)
			p->home = scene;
	}
	p->face = p->platform = -1;
	p->floorType = -1;
	request(*p, 5, 0.0f);
	p->turnSteps = 0;
	enterFollower(scene);
}

bool Characters::command(int id, int op, int arg1, int arg2) {
	if (id == kPlayerActor || id == kFollowerActor) {  // what they do not pass on (E-0811, E-1222)
		if (op == 0x37)
			release(id);
		else if (op == 0x23 && id == kFollowerActor)
			_follow.place = arg1 == 0;
		return true;
	}
	if (id == -1) {
		if (op == 0x17)
			enter(arg1);
		for (uint i = 0; i < _chars.size(); i++)
			apply(_chars[i], op, arg1);
		return true;
	}
	if (id == kFloor) {
		_floor.command(op, arg1);
		return true;
	}
	Character *c = find(id);
	if (!c)
		return false;
	apply(*c, op, arg1);
	return true;
}

// CFXCharacter::DoCommand, the opcodes characters.md lists (E-0403). Roles, combat and the
// rule lists are Q-0403.
void Characters::apply(Character &c, int op, int arg1) {
	if (op == 0x34) {
		c.latched = false;
		return;
	}
	if (c.latched)
		return;
	switch (op) {
	case 2:
		c.visible = true;
		c.home = _scene;
		break;
	case 3:
		c.visible = false;
		break;
	case 0xb:
		c.active = true;
		break;
	case 0xc:
		c.active = false;
		break;
	case 0xd:
		c.active = c.visible = false;
		c.home = -1;
		c.latched = true;
		break;
	case 0x17:
		_scene = arg1;
		break;
	case 0x35:
		c.texture = arg1;            // clamped as the original does (count < arg -> count - 1)
		if ((int)c.textures.size() < arg1)
			c.texture = (int)c.textures.size() - 1;
		break;
	case 0x36:
		c.home = arg1;
		break;
	case 1:
		request(c, 2, 0.0f);
		break;
	case 500:                        // ponytail: the per-state slot flags it clears are not kept
		c.active = c.visible = true;
		request(c, 5, 0.0f);
		break;
	case 501:
		c.active = c.visible = false;
		request(c, 2, 0.0f);
		break;
	case 0x2c:
		makePlayer(c);
		break;
	case 0x2d:                       // the follower's, if it holds none (E-1223)
		if (_follow.id != -1)
			break;
		_follow.id = c.id;
		c.home = _scene;
		c.active = c.visible = true;
		c.setRole(2);
		break;
	case 0x37:
		release(kFollowerActor);
		break;
	case 0x46:
		split(c);
		break;
	case 0x47: {                     // E-1530
		Character *o = find(arg1);
		if (!o)
			break;
		c.pos = o->pos;
		c.pending = Vec3(30.0f * sinf(c.yaw), 0.0f, 30.0f * cosf(c.yaw));
		c.yaw = o->yaw;
		c.home = _scene;
		c.active = c.visible = true;
		c.setRole(0);
		break;
	}
	case 0x58: case 0x59: case 0x5a: case 0x5b: {  // slot 2 / 3 + / - arg1 (E-0704)
		int slot = op < 0x5a ? 2 : 3;
		if (slot < (int)c.state.size())
			c.state[slot] = (op & 1) ? MAX<int32>(c.state[slot] - arg1, 0) : c.state[slot] + arg1;
		break;
	}
	case 0x5d: case 0x5e: case 0x5f: {             // slot 2 / 3 / 1 = arg1
		int slot = op == 0x5d ? 2 : op == 0x5e ? 3 : 1;
		if (slot < (int)c.state.size())
			c.state[slot] = arg1;
		break;
	}
	default:
		debug(2, "Grumpa: character %u opcode %#x not modelled", c.id, op);
		break;
	}
}

} // End of namespace Grumpa
