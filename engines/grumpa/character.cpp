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

#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/system.h"

#include "grumpa/character.h"
#include "grumpa/dialogue.h"
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
	void cmds(CommandList &out) {    // a CC vector kept: when, target, opcode, arg1, arg2, conditions
		uint32 n = count();
		for (uint32 i = 0; i < n && ok; i++) {
			SceneCommand cmd;
			cmd.when = (int32)u32(); cmd.targetId = (int32)u32(); cmd.opcode = (int32)u32();
			cmd.arg1 = (int32)u32(); cmd.arg2 = (int32)u32();
			conds(cmd.conds);
			out.push_back(cmd);
		}
	}
	void conds(Common::Array<SceneCond> &out) {  // an EC vector: id, slot, value, mode, link
		for (uint32 k = count(); k > 0 && ok; k--) {
			SceneCond e;
			e.id = (int32)u32(); e.slot = (int32)u32(); e.value = (int32)u32();
			e.mode = (int32)u32(); e.link = (int32)u32();
			out.push_back(e);
		}
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
	freeSurfaces();
	_chars.clear();
	_walls.clear();
	_floorScene = -1;
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
		c.startLife = c.slot(1);
		c.home = (int32)r.u32();
		c.pos.x = r.f32(); c.pos.y = r.f32(); c.pos.z = r.f32();
		r.skip(4);
		c.yaw = r.f32();
		r.skip(4);
		c.radius = r.f32();          // [0x28c]
		c.reach = r.f32();           // [0x5fc]
		c.sphere = r.f32();          // [0x290]
		c.mode = (int)r.u32();       // [0x48c]
		r.skip(4);                   // [0x490]
		uint32 n = r.count();        // click rules: EC vector + CC vector each (E-1610)
		for (uint32 i = 0; i < n && r.ok; i++) {
			Character::Rule rule;
			r.conds(rule.conds);
			r.cmds(rule.cmds);
			c.rules.push_back(rule);
		}
		r.cmds(c.releaseList);       // +0x640
		r.cmds(c.splitList);         // +0x650
		n = r.count();               // messages: text + CC vector (none in the data)
		for (uint32 i = 0; i < n && r.ok; i++) {
			r.str();
			r.ccVec();
		}
		r.cmds(c.deathList);         // +0x630 (E-1433)
		n = r.count();               // reactions: character id + CC vector (E-1460)
		for (uint32 i = 0; i < n && r.ok; i++) {
			Character::Reaction re;
			re.other = (int32)r.u32();
			r.cmds(re.cmds);
			c.reactions.push_back(re);
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
			a.face = (int)r.u32();       // [0x36c]
			a.attack = (int32)r.u32();   // [0x37c]
			a.defence = (int32)r.u32();  // [0x38c]
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
	endFights();
	_player = kGrumpa;
	_follow = Follower();
	setRoles();
	debug(1, "Grumpa: %u characters loaded%s", (uint)_chars.size(), r.ok ? "" : " (read error)");
	return r.ok;
}

Characters::~Characters() {
	freeSurfaces();
}

void Characters::freeSurfaces() {
	for (uint i = 0; i < _chars.size(); i++) {
		_chars[i].skin.free();
		for (uint j = 0; j < _chars[i].attachments.size(); j++)
			_chars[i].attachments[j].skin.free();
	}
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

int Characters::clipFrames(Character &c, int slot) {
	Character::Clip *k = clip(c, slot);
	return k ? k->mesh.frames : 0;
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

// The request table (E-0813, combat.md). A "cut" ends the clip playing, so the queue takes
// over on the next animation tick; a "now" starts a clip at once. ponytail: no turn lock
// +0x4dc (Q-0805).
void Characters::request(Character &c, int req, float turn) {
	if (req != 5 && c.deathTimer > 0)
		return;  // dying (E-1404)
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
	static const int jumpIdle[] = { 0xf, 0, -1 }, jumpWalk[] = { 0x10, 0, -1 };
	static const int jumpUp[] = { 0x21, 0xf, 0, -1 }, die[] = { 8, 0xc, -1 };
	int slotThen[] = { req, 0, -1 };
	const int *push = nullptr;
	bool cut = false;
	int now = -1;
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
	case 3:
		if (!clip(c, 0x10))
			return;
		if (cur == 0 || cur == 0xb)
			push = jumpIdle;
		else if (cur == 1 || cur == 3)
			push = jumpWalk;
		else if (cur == 2 || cur == 5)
			push = stopIdle, now = cur == 2 ? 0x10 : 0x11;
		else if (cur == 0x20)
			push = jumpUp;
		break;
	case 4:
		push = die;
		break;
	case 5:
		startClip(c, 0);
		c.clock = 0.6f;
		c.queue.clear();
		return;
	default:
		push = (req >= 0x12 && req <= 0x15) || req == 0x17 || (req >= 0x1f && req <= 0x28) ? slotThen : stopIdle;
		break;
	}
	if (!push)
		return;  // walk, run or jump from a clip with no row: the queue stays (E-1404)
	c.queue.clear();
	for (; push && *push >= 0; push++)
		c.queue.push_back(*push);
	if (cut)
		c.frame = cutFrame;
	if (now >= 0 && clip(c, now))
		startClip(c, now);
}

// A clip starts (the update's clip-start block, E-1403): counted, and N2D2N's +5 defence
// lasts while it plays.
void Characters::startClip(Character &c, int slot) {
	const int prev = c.clip;
	c.clip = slot;
	c.frame = 0;
	c.starts++;
	if (slot == 0)
		c.lift = 0.0f;
	if (c.state.size() > 3) {
		if (slot == 0x15)
			c.state[3] += 5;
		if (prev == 0x15)
			c.state[3] -= 5;
	}
}

void Characters::wear(int id, int k, bool on) {
	Character *c = find(id);
	if (!c)
		return;
	auto shield = [](int j) { return j == 0 || j == 5; };
	auto bonus = [c, &shield](int j, int sign) {
		const Character::Attachment &a = c->attachments[j];
		const uint slot = shield(j) ? 3 : 2;
		if (slot < c->state.size())
			c->state[slot] += sign * (shield(j) ? a.defence : a.attack);
	};
	for (uint j = 0; j < c->attachments.size() && j < 32; j++) {
		if (shield(j) == shield(k) && (c->worn & (1u << j))) {
			c->worn &= ~(1u << j);
			bonus(j, -1);
		}
	}
	if (on && k >= 0 && k < (int)c->attachments.size() && k < 32) {
		c->worn |= 1u << k;
		bonus(k, 1);
	}
}

// The current scene's walls go into its status (E-1540).
void Characters::keepWalls() {
	if (_floorScene < 0 || _floor.empty())
		return;
	Walls *w = walls(_floorScene);
	if (!w) {
		_walls.push_back(Walls());
		w = &_walls.back();
		w->scene = _floorScene;
	}
	for (uint i = 0; i < ARRAYSIZE(_floor.closed); i++)
		w->closed[i] = _floor.closed[i] ? 1 : 0;
}

bool Characters::syncState(Common::Serializer &s) {
	for (uint i = 0; i < _chars.size(); i++) {
		Character &c = _chars[i];
		s.syncAsFloatLE(c.pos.x);
		s.syncAsFloatLE(c.pos.y);
		s.syncAsFloatLE(c.pos.z);
		s.syncAsFloatLE(c.yaw);
		s.syncAsSint32LE(c.home);
		s.syncAsByte(c.active);
		s.syncAsByte(c.visible);
		// Version 6: the rest of the original's status (E-1300): the state slots, the
		// texture, the worn attachments and the latch. The clip is not kept (E-1300): the
		// entry after a load starts the idle (E-0830).
		uint32 n = c.state.size();
		s.syncAsUint32LE(n, 6);
		if (s.isLoading() && s.getVersion() >= 6) {
			if (n > 1000)
				return false;  // a corrupt save
			c.state.resize(n);
		}
		for (uint j = 0; j < n && s.getVersion() >= 6; j++)
			s.syncAsSint32LE(c.state[j]);
		s.syncAsSint32LE(c.texture, 6);
		s.syncAsUint32LE(c.worn, 6);
		s.syncAsByte(c.latched, 6);
	}
	// Actors 3 and 4 keep the character they hold (E-1530, E-1220).
	s.syncAsSint32LE(_player, 5);
	s.syncAsSint32LE(_follow.id, 5);
	// Version 7: each visited scene's walls (E-1540).
	if (s.isSaving())
		keepWalls();
	uint32 nw = _walls.size();
	s.syncAsUint32LE(nw, 7);
	if (s.isLoading()) {
		if (nw > 1000)
			return false;  // a corrupt save
		_walls.resize(s.getVersion() >= 7 ? nw : 0);
		_floorScene = -1;  // the loaded scene's floor comes from its status
	}
	for (uint i = 0; i < _walls.size() && s.getVersion() >= 7; i++) {
		s.syncAsSint32LE(_walls[i].scene);
		s.syncBytes(_walls[i].closed, sizeof(_walls[i].closed));
	}
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
		endFights();
		// Original bug: fights are not saved (E-1300), so a companion saved while fighting
		// (role 7, held by fighter 95 rather than actor 4) would be held by no one after a
		// load; it goes back to the follower (op 0x2d, E-1223).
		for (uint i = 0; i < _chars.size() && _follow.id == -1; i++)
			if (_chars[i].role() == 7)
				apply(_chars[i], 0x2d, 0);
	}
	return !s.err();
}

// The body sphere (E-1405): centred `[0x290]` above the position, of radius `[0x28c]`.
bool Characters::touches(const Character &c, const Vec3 &centre, float r) {
	const Vec3 d = Vec3(c.pos.x, c.pos.y + c.sphere, c.pos.z) - centre;
	return d.dot(d) < (c.radius + r) * (c.radius + r);
}

// The character update (E-0603, E-0813, E-0814, E-0802/E-0803): on each animation tick (0.46
// an update) the frame steps and, at the clip's end, the queue's next slot starts (else the
// clip loops); the yaw takes one step of its turn; the clip's root motion for the frame, turned
// by the yaw, goes through the walk mesh and the platforms; eleven idle loops in a row play
// the fidget.
void Characters::update() {
	_ruleFired = false;  // each character's vtable[4] clears it before its update (E-1610)
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
					startClip(c, next);
					k = clip(c, next);
					break;
				}
			}
			// The idle fidget (E-1740): the 11th idle start in a row queues S01, then S02 looping.
			c.idleStarts = c.clip == 0 ? c.idleStarts + 1 : 0;
			if (c.idleStarts > 10) {
				c.idleStarts = 0;
				if (clip(c, 0x1f)) {
					c.queue.clear();
					c.queue.push_back(0x1f);
					c.queue.push_back(0x20);
				}
			}
		} else {
			c.frame++;
		}
		combatTick(c);
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
			c.lift += m.y;  // drawn only: a jump's arc, never the position (E-1406)
		}
		if (_floor.empty())
			continue;
		Vec3 old = c.pos;
		const int oldFace = c.face, oldPlatform = c.platform;
		_floor.move(c.pos, delta, c.radius, c.face, c.platform, platforms);
		// Off the mesh, a step up of more than 20 (80 on a platform, type 15), a closed wall
		// type, or a boat off the water: back (E-0803, E-1600). Then the water (E-1660): a boat
		// floats at -0.5 on type 13, so does a dragonfly; a jump over 12/13 keeps its height.
		// ponytail: the water ripple and the shadow under a character are not drawn (Q-1660).
		int type = c.platform >= 0 ? 15 : c.face >= 0 ? _floor.types[c.face] : -1;
		bool back = c.face < 0 || c.pos.y > old.y + (c.platform >= 0 ? 80.0f : 20.0f);
		if (!back && c.mode == kBoat) {
			c.pos.y = -0.5f;
			old.y = 0.0f;
		}
		back = back || (type > 18 && type <= 28 && _floor.closed[type]) || (c.mode == kBoat && type != 13);
		if (back) {
			c.pos = old;
			c.face = oldFace;
			c.platform = oldPlatform;
		} else {
			c.floorType = type;
			if (type == 13 && c.mode == kDragonfly) {
				c.pos.y = -0.5f;
				old.y = 0.0f;
			}
			if ((type == 12 || type == 13) && ((c.clip >= 0xf && c.clip <= 0x11) || (type == 12 && c.mode == kDragonfly)))
				c.pos.y = old.y;
		}
		pumpSpeech(c);  // the update's last call
	}
	for (int f = 0; f < kFighters; f++) {  // actors 91..95 update after the characters
		Fighter &fi = _fighters[f];
		if (fi.held < 0 || !find(fi.held))
			continue;
		fi.clock += 0.46f;
		if (fi.clock > 1.0f) {
			fi.clock -= 1.0f;
			fighterRule(f);
		}
	}
}

// Actor 3 on scene entry (E-0804): the player is placed at the entry from the scene left, or
// stays where it is (an entry for scene -2), or takes the first entry; not on the first entry
// of a new game or a load. Then, on every entry, the request 5 back to the idle (E-0830).
// The new scene's walk mesh, before anything is sent to it (E-1540): the old scene's walls go
// into its status, the new floor starts closed and takes its status's walls, if any.
void Characters::loadFloor(int scene) {
	keepWalls();
	_floorScene = scene;
	_floor.load(scene);
	if (const Walls *w = walls(scene)) {
		for (uint i = 0; i < ARRAYSIZE(_floor.closed); i++)
			_floor.closed[i] = w->closed[i] != 0;
	}
}

Characters::Walls *Characters::walls(int scene) {
	for (uint i = 0; i < _walls.size(); i++)
		if (_walls[i].scene == scene)
			return &_walls[i];
	return nullptr;
}

void Characters::enter(int scene) {
	const int prev = _scene;
	if (_floorScene != scene)
		loadFloor(scene);
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
		if (op == 0x17)
			fightersCommand(op, arg1);
		for (uint i = 0; i < _chars.size(); i++)  // the characters (ids 10..88) before 91..95
			apply(_chars[i], op, arg1);
		if (op == 0x19)
			fightersCommand(op, arg1);
		return true;
	}
	if (id == kFloor) {
		_floor.command(op, arg1);
		return true;
	}
	if (fighterForward(id, op, arg1))
		return true;
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
	case 0x12:                       // the left button (the VM's click broadcast)
		clickRules(c, Common::Point((int16)(arg1 & 0xffff), (int16)((uint32)arg1 >> 16)));
		break;
	case 0x19:                       // leaving the scene while dying: the death list now (E-1610)
		if (c.deathTimer > 0) {
			pushList(c.deathList);
			c.active = c.visible = false;
			c.home = -1;
			c.deathTimer = c.hideTimer = -1;
		}
		break;
	case 0x17:
		_scene = arg1;
		c.screen = Common::Rect();  // drawn afresh in the new scene
		flushSpeech(c);
		if (c.home == arg1 && !_floor.empty()) {  // at home here: placed afresh (E-1460)
			for (uint i = 0; i < c.reactions.size(); i++)
				c.reactions[i].fired = false;
			c.face = c.floorType = c.platform = -1;
			request(c, 5, 0.0f);
		}
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
	case 0x48:                       // say slot arg1: below 60 (but 32) at once, else queued
		if (arg1 < 0 || arg1 > 99 || Common::find(c.speech.begin(), c.speech.end(), arg1) != c.speech.end())
			break;
		if (arg1 < 60 && arg1 != 32) {
			playVoice(c, arg1);
			break;
		}
		c.speech.push_back(arg1);
		if (c.speech.size() == 1 && voiceLoaded(c, arg1) && c.state.size() > 5 && c.state[5] == 0) {
			playVoice(c, arg1);
			c.state[5] = 1;
		}
		break;
	case 0x60:                       // stop every slot; the queue stays
		for (int i = 0; i < 100; i++)
			g_system->getMixer()->stopHandle(c.voices[i]);
		break;
	case 0x2e: case 0x2f: case 0x30: case 0x4b: case 0x54: {  // fight (E-1430)
		const int f = op == 0x4b ? 3 : op == 0x54 ? 4 : op - 0x2e;
		engage(f, c, op == 0x54 ? arg1 : _player, f + 3);
		break;
	}
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

// Spawning (characters.md Spawners, E-0408): no Life or alive check, so a character killed
// on an earlier visit comes back whole.
// The two updates the original runs after placing it are left to the next tick, which settles
// idle characters on the floor too; clearing the floor face and the death timers stands in
// for them. +0x478 = 1 is not kept (Q-0408).
bool Characters::spawn(int id, const Vec3 &pos, const Vec3 &rot, int scene) {
	Character *c = find(id);
	if (!c) {
		warning("Grumpa: spawn of unknown character %d", id);
		return false;
	}
	c->pos = pos;
	c->yaw = rot.y;
	c->active = c->visible = true;
	apply(*c, 2, 0);
	c->home = scene;
	if (c->state.size() > 1)
		c->state[1] = c->startLife;
	c->deathTimer = c->hideTimer = -1;
	c->face = c->floorType = c->platform = -1;
	request(*c, 5, 0.0f);
	c->setRole(0);
	c->idleStarts = 9;
	c->lift = 0.0f;
	debug(1, "Grumpa: spawned character %d at (%.0f, %.0f, %.0f)", id, pos.x, pos.y, pos.z);
	return true;
}

// A load: besides the entry handling, the speech of the session left is silenced (the queue
// is not saved).
void Characters::forgetScene(bool firstEntry) {
	_scene = -1;
	_firstEntry = firstEntry;
	for (uint i = 0; i < _chars.size(); i++) {
		Character &c = _chars[i];
		for (int n = 0; n < 100; n++)
			g_system->getMixer()->stopHandle(c.voices[n]);
		c.speech.clear();
		if (c.state.size() > 5)
			c.state[5] = 0;
	}
}

// The sound slots (E-1640): slot n is the character's .wav whose name starts with the number
// n, loaded once; a missing file leaves it empty.
bool Characters::voiceLoaded(Character &c, int n) {
	if (c.voiceFiles.empty()) {
		c.voiceFiles.resize(100);
		for (uint i = 0; i < c.sounds.size(); i++) {
			int k = atoi(c.sounds[i].c_str());
			Common::SeekableReadStream *f = k >= 0 && k < 100 ? openSound(c.sounds[i]) : nullptr;
			if (f)
				c.voiceFiles[k] = c.sounds[i];
			delete f;
		}
	}
	return n >= 0 && n < 100 && !c.voiceFiles[n].empty();
}

// A slot already playing carries on.
void Characters::playVoice(Character &c, int n) {
	Audio::Mixer *mixer = g_system->getMixer();
	if (!voiceLoaded(c, n) || mixer->isSoundHandleActive(c.voices[n]))
		return;
	Common::SeekableReadStream *f = openSound(c.voiceFiles[n]);
	Audio::SeekableAudioStream *s = f ? Audio::makeWAVStream(f, DisposeAfterUse::YES) : nullptr;
	if (!s)
		return;
	// Slots below 60 but 32 are the clips' effects; the queued ones are speech.
	const bool effect = n < 60 && n != 32;
	mixer->playStream(effect ? Audio::Mixer::kSFXSoundType : Audio::Mixer::kSpeechSoundType, &c.voices[n], s);
	debug(1, "Grumpa: character %u says %s", c.id, c.voiceFiles[n].c_str());
}

// Away from the current scene: the line speaking stops and the queue empties (E-1620).
void Characters::flushSpeech(Character &c) {
	if (c.home == _scene || c.speech.empty())
		return;
	if (voiceLoaded(c, c.speech[0]))
		g_system->getMixer()->stopHandle(c.voices[c.speech[0]]);
	c.speech.clear();
}

void Characters::flushSpeech(int id) {
	if (Character *c = find(id))
		flushSpeech(*c);
}

// On an animation step at home (E-1620, E-1640): a front line that has ended leaves the queue
// and the next one speaks. An empty slot at the front is never popped.
void Characters::pumpSpeech(Character &c) {
	if (c.speech.empty() || !voiceLoaded(c, c.speech[0]) ||
		g_system->getMixer()->isSoundHandleActive(c.voices[c.speech[0]]))
		return;
	c.speech.remove_at(0);
	if (c.state.size() > 5)
		c.state[5] = 0;
	if (!c.speech.empty() && voiceLoaded(c, c.speech[0])) {
		playVoice(c, c.speech[0]);
		if (c.state.size() > 5)
			c.state[5] = 1;
	}
}

// The click rules (E-1610): a click on a character at home here, while the player's reaction
// sphere touches its own, pushes the first rule whose conditions hold; one rule an update.
// ponytail: the hover test that arms the mouse glitter is not run.
void Characters::clickRules(Character &c, const Common::Point &p) {
	const Character *pl = find(held(kPlayerActor));
	if (_ruleFired || c.rules.empty() || !c.active || c.home != _scene || !c.screen.contains(p) || !pl || !_vm)
		return;
	const Vec3 d = Vec3(c.pos.x, c.pos.y + c.sphere, c.pos.z) - Vec3(pl->pos.x, pl->pos.y + pl->sphere, pl->pos.z);
	if (d.dot(d) >= (c.reach + pl->reach) * (c.reach + pl->reach))
		return;
	for (uint i = 0; i < c.rules.size(); i++) {
		if (!_vm->events().conditionsHold(c.rules[i].conds))
			continue;
		debug(1, "Grumpa: character %u click rule %u", c.id, i);
		pushList(c.rules[i].cmds);
		_ruleFired = true;
		return;
	}
}

void Characters::pushList(const CommandList &list) {
	for (uint i = 0; i < list.size() && _vm; i++)
		_vm->events().push(list[i]);
}

} // End of namespace Grumpa
