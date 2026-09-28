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

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/endian.h"
#include "common/system.h"

#include "graphics/cursorman.h"

#include "peintre/detection.h"
#include "peintre/display.h"
#include "peintre/gfx.h"
#include "peintre/peintre.h"
#include "peintre/sound.h"
#include "peintre/world.h"

namespace Peintre {

// ---------------------------------------------------------------------------------------
// Tables of scene.md

struct SceneInfo {
	const char *bundle;       ///< nullptr for scene 6 (chambreb / chambrev)
	const char *entryMovie;
	const char *returnMovie;
	int zones[6];             ///< zones that complete the scene, -1 terminated; empty = never
};

static const SceneInfo kScenes[kNumScenes] = {
	{ "musee", nullptr, nullptr, { -1 } },
	{ "auberge", nullptr, nullptr, { -1 } },
	{ "hopiext", nullptr, nullptr, { -1 } },
	{ "maisonet", "maisa", "maisr", { 1, -1 } },
	{ "mangeurs", "mangeurs", "mangeurr", { 2, 3, -1 } },
	{ "cafe", "cafe", "cafer", { 4, 5, 6, -1 } },
	{ nullptr, "chamba", "chambr", { 7, 8, 9, 10, 11, -1 } },
	{ "maisonj", "maisonj", "maisonjr", { 12, -1 } },
	{ "hopiint", "hopi", "hopir", { 13, 14, 15, -1 } },
	{ "pont", "pont", "pontr", { 18, -1 } },
	{ "terrasse", "terrasse", "terr", { 16, 17, -1 } },
	{ "jardin", "jardin", nullptr, { -1 } },
	{ "champ", "champ", "champr", { 22, 24, -1 } },
	{ "eglise", "eglise", "eglr", { 23, -1 } }
};

struct StartPos {
	int32 x, y, z, pitch, yaw;
};

// "Start positions", case A: the scene's own start (roll 0). Scene 2 has none.
static const StartPos kOwnStart[kNumScenes] = {
	{ -39, -209, 361, 4066, 20 },
	{ 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0 },
	{ 1149, 141, -1491, 0, 275 },
	{ -106, 6, 100, 3946, 145 },
	{ 393, -175, -136, 3976, 92 },
	{ -628, -336, -268, 3946, 150 },
	{ -3350, 489, -5996, 60, 3944 },
	{ -67, -78, -345, 4036, 17 },
	{ 7805, 1167, 2183, 30, 3265 },
	{ -123, -102, -846, 4066, 31 },
	{ -46, -335, 1263, 0, 4052 },
	{ 2140, 253, 2270, 4006, 214 },
	{ -7189, 802, 102, 210, 3756 }
};

// The museum spot in front of each scene's painting; the flight's targets.
static const StartPos kMuseumSpot[kNumScenes] = {
	{ -39, -209, 361, 4066, 20 },
	{ 3230, -297, 5166, 4036, 3073 },
	{ 538, -297, 8256, 4036, 290 },
	{ -3125, -297, 5252, 4066, 1008 },
	{ -4849, -297, 5195, 4036, 3101 },
	{ -562, -296, 6923, 4036, 2355 },
	{ -909, -297, 7462, 4066, 3062 },
	{ -466, -297, 8184, 4066, 3788 },
	{ 538, -297, 8256, 4036, 290 },
	{ 703, -297, 6902, 4036, 1782 },
	{ 933, -297, 7535, 0, 988 },
	{ 3230, -297, 5166, 4036, 3073 },
	{ 4403, -297, 5867, 4006, 339 },
	{ 4524, -297, 4758, 4006, 1791 }
};

static const StartPos kMuseumAfterEnd = { 1232, -208, 4207, 0, 3410 };

// Case B: walking from one scene to another.
struct Doorway {
	int from, to;
	StartPos pos;
};

static const Doorway kDoorways[] = {
	{ 1, 11, { -6620, 182, -578, 4066, 1128 } },
	{ 2, 8, { -1318, -11, 8435, 4036, 1476 } },
	{ 2, 9, { 4933, -22, 13060, 0, 2581 } },
	{ 2, 7, { 3320, 358, 12464, 0, 2264 } },
	{ 3, 4, { -384, 35, 1373, 3946, 1701 } },
	{ 4, 3, { 7310, 183, 5921, 0, 2873 } },
	{ 5, 10, { -430, 63, 3711, 4066, 1519 } },
	{ 6, 7, { -6199, 358, 737, 0, 1657 } },
	{ 7, 6, { 100, -336, 439, 3946, 3446 } },
	{ 7, 9, { 11579, -56, -2069, 30, 3253 } },
	{ 7, 2, { 17495, 6733, 14500, 4036, 2018 } },
	{ 7, 10, { 677, -63, -2216, 4066, 68 } },
	{ 8, 2, { 9533, 6735, 11297, 4066, 1727 } },
	{ 9, 7, { -831, 489, -9000, 4066, 72 } },
	{ 9, 2, { 17028, 6815, -389, 4036, 4086 } },
	{ 10, 7, { -7605, 358, -1698, 0, 1054 } },
	{ 10, 5, { 1617, -90, -116, 3976, 3788 } },
	{ 11, 12, { 2425, 160, -4360, 4006, 3989 } },
	{ 11, 13, { -19321, 841, 23485, 0, 1651 } },
	{ 11, 1, { 0, 0, 0, 0, 0 } },
	{ 12, 13, { -2500, -634, 384, 60, 3279 } },
	{ 12, 11, { -8760, 228, -928, 3946, 973 } },
	{ 13, 12, { -13857, -522, 9973, 4006, 1241 } },
	{ 13, 11, { 6743, 235, -1283, 4006, 3036 } }
};

// Cursor files (interaction.md "Cursors"), indices 35..65; 0..34 are op_0..op_34.
static const char *const kCursorFiles[] = {
	"def_0", "def_1", "fleche", "main", "curza", "curferme", "sablier",
	"Ct0", "Ct1", "Ct2", "Ct3", "Ct4", "Ct5", "Ct6", "Ct7", "Ct8", "Ct9", "Ct10", "Ct11",
	"Ct12", "Ct13", "Ct14", "Ct15",
	"curdoigt", "acces", "deja_vu", "buche", "fagot", "cle", "manivel", "retour"
};

// Bar item x centres (table 0x4acfa8).
static const int kBarItemX[6] = { 100, 170, 250, 310, 380, 450 };

// The 3D block lives at 0x4aba40 in the original (save.md).
static const uint32 kBlockBase = 0x4aba40;
static const uint32 kChambrebFlag = 0x4abd14;
static const uint32 kAfterEnd = 0x4aba5c;

static const int32 kRadius = 250;
static const int32 kEyeAboveFloor = 700;

// ---------------------------------------------------------------------------------------

World::World(PeintreEngine *vm) : _vm(vm) {
	loadCursors();
	// Field of view (enhancement): 67 degrees is the original's f = 480.
	const int fov = ConfMan.getInt("fov");
	_focal = fov == 67 ? 0.0f : (float)(320.0 / tan(fov * M_PI / 360.0));
	// Turn speed (enhancement): 100% is the original's steps.
	_turn = ConfMan.getInt("turn_speed") / 100.0f;
	_systemCursor = ConfMan.getBool("high_fps");
}

World::~World() {
	unload();
	for (Graphics::Surface &s : _cursors)
		s.free();
	_bar.free();
}

bool World::loadCursors() {
	for (int i = 0; i < kNumCursors; i++) {
		const Common::String name = i < 35 ? Common::String::format("op_%d", i) : Common::String(kCursorFiles[i - 35]);
		if (!loadTga(name, _cursors[i])) {
			// A missing file gives a 32x32 block of 0xFFFF (interaction.md).
			_cursors[i].create(32, 32, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));
			_cursors[i].fillRect(Common::Rect(32, 32), 0xFFFF);
		}
	}
	return loadTga("Invent", _bar);
}

uint32 &World::var(uint32 address) {
	static uint32 dummy;
	const uint32 off = address - kBlockBase;
	if (address < kBlockBase || off + 4 > k3DBlockSize) {
		warning("World::var: 0x%x is outside the saved block", address);
		return dummy;
	}
	return *(uint32 *)(_vm->state().block3D + off);
}

byte &World::varByte(uint32 address) {
	static byte dummy;
	const uint32 off = address - kBlockBase;
	if (address < kBlockBase || off >= k3DBlockSize)
		return dummy;
	return _vm->state().block3D[off];
}

bool World::sceneComplete(int scene) {
	if (scene < 0 || scene >= kNumScenes || kScenes[scene].zones[0] < 0)
		return false;
	for (int i = 0; kScenes[scene].zones[i] >= 0; i++)
		if (!zoneSolved(kScenes[scene].zones[i]))
			return false;
	return true;
}

void World::unload() {
	_vm->display()->forgetTextures();
	_script.reset();
	stopAllSounds();
	for (Texture3D *t : _textures)
		delete t;
	_textures.clear();
	_textureNames.clear();
	_boxSets.clear();
	_boxLoaded.clear();
	_extraBox = 0;
	objects.clear();
	anims.clear();
	_scene3D = Scene3D();
	_carrying = false;
	_carried = -1;
}

int World::findObject(const Common::String &name) const {
	return _scene3D.findNode(name);
}

void World::setHidden(int node, bool hidden) {
	if (node < 0 || (uint)node >= _scene3D.nodes.size())
		return;
	if (hidden)
		_scene3D.nodes[node].flags |= 1;
	else
		_scene3D.nodes[node].flags &= ~1;
}

bool World::isHidden(int node) const {
	return node >= 0 && (uint)node < _scene3D.nodes.size() && (_scene3D.nodes[node].flags & 1);
}

void World::resolveObjects() {
	for (SceneObject &o : objects) {
		o.node = findObject(o.name);
		if (o.startHidden)
			setHidden(o.node, true);
	}
}

int World::objectIndex(int node) const {
	if (node < 0)
		return -1;
	for (uint i = 0; i < objects.size(); i++)
		if (objects[i].node == node)
			return i;
	return -1;
}

bool World::loadTexture(const Common::String &name, const Common::String &file) {
	Common::Array<byte> data;
	if (!_bfg.readEntry(file + ".3DM", data))
		return false;
	Texture3D *t = new Texture3D();
	if (!t->load(data)) {
		delete t;
		return false;
	}
	_textures.push_back(t);
	_textureNames.push_back(name);
	return true;
}

void World::retexture(int node, const Common::String &from, const Common::String &to) {
	if (node < 0 || (uint)node >= _scene3D.nodes.size())
		return;
	const Texture3D *tex = nullptr;
	for (uint i = 0; i < _textureNames.size(); i++)
		if (_textureNames[i].equalsIgnoreCase(to))
			tex = _textures[i];
	if (!tex)
		return;
	for (FaceGroup &g : _scene3D.nodes[node].faceGroups) {
		if (g.textureName.equalsIgnoreCase(from)) {
			g.textureName = to;
			g.texture = tex;
		}
	}
}

bool World::loadBoxSet(uint slot, const Common::String &name) {
	Common::Array<byte> data;
	Boxes3D b;
	if (!_bfg.readEntry(name, data) || !b.load(data)) {
		warning("World: cannot load box set %s", name.c_str());
		return false;
	}
	if (_boxSets.size() <= slot) {
		_boxSets.resize(slot + 1);
		_boxLoaded.resize(slot + 1);
	}
	_boxSets[slot] = b;
	_boxLoaded[slot] = true;
	return true;
}

void World::setBoxSet(uint slot) {
	_extraBox = slot;
}

void World::loadAnims() {
	for (AnimRecord &a : anims) {
		Common::Array<byte> data;
		if (!_bfg.readEntry(a.anim, data) || !a.data.load(data)) {
			warning("LoadAnims::%s manque", a.anim.c_str());
			continue;
		}
		// The length is the first word of the track data (0x437d90).
		a.length = a.data.tracks.empty() ? 0 : a.data.tracks[0].unk0;
		a.nodeIndex = findObject(a.node);
	}
}

/** The first key index in 1..n-1 with time >= t, by the original's bisection (animation.md). */
template<class Key>
static uint findKey(const Common::Array<Key> &keys, int32 frame) {
	uint lo = 0, hi = keys.size() - 1;
	while (lo + 1 < hi) {
		const uint mid = (lo + hi) / 2;
		if ((int32)keys[mid].time < frame)
			lo = mid;
		else
			hi = mid;
	}
	return hi;
}

void World::pose(uint record, int32 frame) {
	// animation.md: track i drives node i of the scene's node table; each track with at
	// least two keys is sampled at `frame` (ticks) and replaces the local pose.
	if (record >= anims.size())
		return;
	const AnimRecord &a = anims[record];
	const uint count = MIN<uint>(a.data.tracks.size(), _scene3D.nodes.size());
	for (uint i = 0; i < count; i++) {
		const AnimTrack &t = a.data.tracks[i];
		Node &nd = _scene3D.nodes[i];
		if (t.rot.size() >= 2) {
			const uint k = findKey(t.rot, frame);
			int32 q[4];
			if ((int32)t.rot[k].time <= frame) {
				memcpy(q, t.rot[k].q, sizeof(q));
			} else {
				const AnimTrack::RotKey &r0 = t.rot[k - 1], &r1 = t.rot[k];
				const int32 f = (int32)((int64)(frame - (int32)r0.time) * 256 / ((int32)r1.time - (int32)r0.time));
				const int32 c = (int32)(((int64)r0.q[0] * r1.q[0] + (int64)r0.q[1] * r1.q[1] +
										 (int64)r0.q[2] * r1.q[2] + (int64)r0.q[3] * r1.q[3]) >> 15);
				if (32768 - c < 21 || 32768 + c <= 20) {
					// Nearly equal keys: linear (the opposite case never occurs in the corpus).
					for (int j = 0; j < 4; j++)
						q[j] = (r0.q[j] * (256 - f) + r1.q[j] * f) / 256;
				} else {
					// Slerp without the shorter-arc flip.
					const double theta = acos(CLIP(c / 32768.0, -1.0, 1.0));
					const double s0 = sin((256 - f) * theta / 256), s1 = sin(f * theta / 256), s = sin(theta);
					for (int j = 0; j < 4; j++)
						q[j] = (int32)((r0.q[j] * s0 + r1.q[j] * s1) / s);
				}
			}
			// Quat_ToMatrix, (x, y, z, w) Q15, row-major.
			const double x = q[0] / 32768.0, y = q[1] / 32768.0, z = q[2] / 32768.0, w = q[3] / 32768.0;
			const double m[9] = {
				1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y),
				2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
				2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)
			};
			for (int j = 0; j < 9; j++)
				nd.rotation[j] = (int32)(m[j] * 32768.0);
		}
		if (t.pos.size() >= 2) {
			const uint k = findKey(t.pos, frame);
			if ((int32)t.pos[k].time <= frame) {
				nd.position = t.pos[k].pos;
			} else {
				const AnimTrack::PosKey &p0 = t.pos[k - 1], &p1 = t.pos[k];
				const int32 f = (int32)((int64)(frame - (int32)p0.time) * 256 / ((int32)p1.time - (int32)p0.time));
				nd.position.x = (p1.pos.x * f + p0.pos.x * (256 - f)) >> 8;
				nd.position.y = (p1.pos.y * f + p0.pos.y * (256 - f)) >> 8;
				nd.position.z = (p1.pos.z * f + p0.pos.z * (256 - f)) >> 8;
			}
		}
	}
}

void World::advanceAnims() {
	for (uint i = 0; i < anims.size(); i++) {
		if (!anims[i].playing)
			continue;
		anims[i].frame += _elapsed;
		pose(i, anims[i].frame);
	}
}

// ---------------------------------------------------------------------------------------
// Loading

bool World::load(int scene, int prevScene, bool keepCamera) {
	unload();
	if (scene < 0 || scene >= kNumScenes)
		return false;
	_scene = scene;
	_prevScene = prevScene;
	Common::String bundle = kScenes[scene].bundle ? kScenes[scene].bundle :
		(var(kChambrebFlag) ? "chambreb" : "chambrev");

	if (!_bfg.open(Common::Path("Scenes_3D/" + bundle + ".BFG"))) {
		warning("World: cannot open %s.BFG", bundle.c_str());
		return false;
	}
	Common::Array<byte> data;
	if (!_bfg.readEntry(bundle + ".3DC", data) || !_scene3D.load(data)) {
		warning("World: cannot load %s.3DC", bundle.c_str());
		return false;
	}
	// Textures by material name (E-0014), then bound to face groups by material.
	for (const Material &m : _scene3D.materials) {
		bool have = m.texture.empty();
		for (const Common::String &n : _textureNames)
			if (n.equalsIgnoreCase(m.texture))
				have = true;
		if (!have)
			loadTexture(m.texture, m.texture);
	}
	for (Node &nd : _scene3D.nodes) {
		for (FaceGroup &g : nd.faceGroups) {
			if (g.materialIndex < 0)
				continue;
			g.textureName = _scene3D.materials[g.materialIndex].texture;
			for (uint i = 0; i < _textureNames.size(); i++)
				if (_textureNames[i].equalsIgnoreCase(g.textureName))
					g.texture = _textures[i];
		}
	}
	loadBoxSet(0, "BOX.3DI");

	if (!keepCamera) {
		StartPos s = kOwnStart[scene];
		if (scene == kSceneMusee) {
			s = kMuseumSpot[prevScene >= 0 && prevScene < kNumScenes ? prevScene : 0];
			if (var(kAfterEnd))
				s = kMuseumAfterEnd;
		} else if (prevScene != 0 && prevScene != scene) {
			for (const Doorway &d : kDoorways)
				if (d.from == prevScene && d.to == scene)
					s = d.pos;
		}
		_cam.x = s.x;
		_cam.y = s.y;
		_cam.z = s.z;
		_cam.pitch = s.pitch;
		_cam.yaw = s.yaw;
		_cam.roll = 0;
	}
	_v = _vy = _w = _p = _s = 0;
	_cursor = kCursorArrow;
	_barState = 0;
	_barY = 480;
	_reload = false;
	_exit = kExitNone;
	_cut = true;

	_script.reset(ConfMan.getBool("dev_noscript") ? nullptr : createSceneScript(scene, bundle));
	if (_script)
		_script->init(*this);
	for (uint i = 0; i < _scene3D.nodes.size(); i++)
		if (_scene3D.nodes[i].flags & 5)
			debugC(2, kDebugLoad, "World: node %s hidden (flags 0x%x)", _scene3D.nodes[i].name.c_str(), _scene3D.nodes[i].flags);
	debugC(1, kDebugLoad, "World: scene %d (%s) from %d, %d nodes, %d textures", scene,
		   bundle.c_str(), prevScene, _scene3D.nodes.size(), _textures.size());
	return true;
}

// ---------------------------------------------------------------------------------------
// Requests (scene.md "Moving between the museum and the scenes", "3D <-> 2D")

void World::requestScene(int target) {
	_prevScene = _scene;
	_scene = target;
	_reload = true;
}

void World::requestMuseum() {
	const int left = _scene;
	_prevScene = _scene;
	_scene = kSceneMusee;
	if (sceneComplete(left) && kScenes[left].returnMovie) {
		stopAllSounds();
		int last = -1;
		for (int i = 0; kScenes[left].zones[i] >= 0; i++)
			last = kScenes[left].zones[i];
		if (last >= 0)
			var(0x4abb70 + 4 * last) = 1;
		requestMovie(kScenes[left].returnMovie);
	} else {
		_reload = true;
	}
}

void World::storeView(bool pitch) {
	byte *b = _vm->state().block3D;
	WRITE_LE_UINT32(b + 0x30, _cam.x);
	WRITE_LE_UINT32(b + 0x34, _cam.y);
	WRITE_LE_UINT32(b + 0x38, _cam.z);
	if (pitch)
		WRITE_LE_UINT32(b + 0x24, _cam.pitch);
	WRITE_LE_UINT32(b + 0x28, _cam.yaw);
	WRITE_LE_UINT32(b + 0x2C, _cam.roll);
	b[0x3C] = _scene;
	b[0x3D] = _prevScene;
}

void World::requestZone(int zone) {
	_zone = zone;
	_exit = kExitZone;
	// The camera and the scene go into the block (0x42f755, save.md "3D block").
	storeView(true);
	_vm->state().block3D[0x3E] = zone;
}

void World::autosave() {
	// 0x42f873 copies the pitch elsewhere (0x5b7fb0), not into the block.
	storeView(false);
	_vm->writeResume(0);
}

void World::requestMovie(const Common::String &name) {
	_movie = name;
	_exit = kExitMovie;
}

void World::startFlight(int target) {
	// Mode 4: 20 steps to the painting's spot, the short way round for angles. The flight
	// marks the robot as no longer talking (0x4aba48, musee.md).
	var(0x4aba48) = 1;
	_prevScene = kSceneMusee;
	_scene = target;
	const StartPos &t = kMuseumSpot[target];
	const int32 cur[5] = { _cam.x, _cam.y, _cam.z, _cam.pitch, _cam.yaw };
	const int32 dst[5] = { t.x, t.y, t.z, t.pitch, t.yaw };
	for (int i = 0; i < 5; i++) {
		int32 d = dst[i] - cur[i];
		if (i >= 3) {
			d &= 0xFFF;
			if (d > 0x800)
				d -= 0x1000;
		}
		d /= 20;
		if (d == 0)
			d = 1;
		_flightDelta[i] = d;
	}
	_flying = true;
	_flightStep = 0;
	_flightTarget = target;
}

void World::flightStep() {
	_cam.x += _flightDelta[0];
	_cam.y += _flightDelta[1];
	_cam.z += _flightDelta[2];
	_cam.pitch = (_cam.pitch + _flightDelta[3]) & 0xFFF;
	_cam.yaw = (_cam.yaw + _flightDelta[4]) & 0xFFF;
	_flightStep += _elapsed >= 3 ? _elapsed / 2 : 1;
	if (_flightStep > 20)
		finishFlight();
}

void World::finishFlight() {
	_flying = false;
	stopAllSounds();
	_vm->sound()->stopStream();
	if (!sceneComplete(_flightTarget) && kScenes[_flightTarget].entryMovie)
		requestMovie(kScenes[_flightTarget].entryMovie);
	else
		_reload = true;
}

void World::afterMovie() {
	_exit = kExitNone;
	_movie.clear();
	load(_scene, _prevScene, false);
}

void World::afterZone(int code) {
	_exit = kExitNone;
	const int zone = _zone;
	_zone = -1;
	// A zone left with more sunflowers than on entry is solved (0x42f2c2).
	if (_vm->zoneLeaveCount > sunflowers() && zone >= 0 && zone < 25)
		var(0x4abb0c + 4 * zone) = 1;
	sunflowers() = _vm->zoneLeaveCount;
	switch (zone) {
	case 0:
		var(0x4aba48) = 1;
		var(0x4aba4c) = 1;
		break;
	case 7:
		var(0x4abd54) = 1;
		var(0x4abd58) = 1;
		break;
	case 8:
		var(0x4abd50) = 1;
		break;
	case 11:
		var(0x4abd4c) = 1;
		if (!var(0x4abd5c))
			localVar(0x4e30f4) = 1;
		break;
	case 21:
		var(kAfterEnd) = 1;
		requestMovie("cinefin");
		return;
	default:
		break;
	}
	if (code == -3) {
		_prevScene = _scene;
		_scene = kSceneMusee;
		load(_scene, _prevScene, false);
		return;
	}
	// Back at the saved spot, pitch 0 (case C).
	const byte *b = _vm->state().block3D;
	load(b[0x3C], b[0x3D], true);
	_cam.x = READ_LE_INT32(b + 0x30);
	_cam.y = READ_LE_INT32(b + 0x34);
	_cam.z = READ_LE_INT32(b + 0x38);
	_cam.pitch = 0;
	_cam.yaw = READ_LE_INT32(b + 0x28) & 0xFFF;
	_cam.roll = READ_LE_INT32(b + 0x2C) & 0xFFF;
}

// ---------------------------------------------------------------------------------------
// Sounds (static sounds are freed with the scene)

void World::playSound(const Common::String &name, bool loop) {
	_vm->sound()->playStatic(name, loop);
	_sounds.push_back(name);
}

void World::stopSound(const Common::String &name) {
	_vm->sound()->stopStatic(name);
}

void World::stopAllSounds() {
	for (const Common::String &s : _sounds)
		_vm->sound()->stopStatic(s);
	_sounds.clear();
}

void World::setAmbience(const Common::String &name) {
	_ambience = name;
	playSound(name, true);
}

// ---------------------------------------------------------------------------------------
// Mouse, cursors, bar

void World::mouse() {
	_mousePos = _vm->mouse();
	if (_carrying && _vm->mouseCaptured()) {
		// The game picks at the carried image's centre: that goes on the crosshair.
		const Graphics::Surface &c = _cursors[_cursor];
		_mousePos.x -= c.w / 2;
		_mousePos.y -= c.h / 2;
	}
	_click = _vm->buttonDown(); // the button's level, no edge (interaction.md)
}

int World::pick() {
	Common::Point p = _mousePos;
	if (_carrying) {
		const Graphics::Surface &c = _cursors[_cursor];
		p.x += c.w / 2;
		p.y += c.h / 2;
	}
	return _renderer.pick(p.x, p.y);
}

void World::applyHover(int objIndex) {
	if (_cursor == kCursorHand || _cursor == kCursorZone || _cursor == kCursorFinger ||
		_cursor == kCursorAccess || _cursor == kCursorDejaVu)
		_cursor = kCursorArrow;
	if (objIndex < 0 || _cursor != kCursorArrow)
		return;
	switch (objects[objIndex].cursorType) {
	case 2: _cursor = kCursorHand; break;
	case 3: _cursor = kCursorZone; break;
	case 4: _cursor = kCursorFinger; break;
	case 6: _cursor = kCursorAccess; break;
	case 0x3C: _cursor = kCursorDejaVu; break;
	default: break;
	}
}

int32 World::distance(int node) const {
	int32 x, y, z;
	if (!_renderer.nodeViewPosition(node, x, y, z))
		return 0x7FFFFFFF;
	return (int32)sqrt((double)x * x + (double)z * z);
}

void World::openBar() {
	if (_barState == 0 || _barState == 3) {
		_barState = 2;
		_barSoundPlayed = false;
	}
}

void World::takeItem(int node, int object) {
	setHidden(node, true);
	_cursor = object;
	openBar();
}

void World::carry(int node) {
	setHidden(node, true);
	_carrying = true;
	_carried = node;
}

void World::dropCarried() {
	_carrying = false;
	_carried = -1;
	_cursor = kCursorArrow;
}

void World::drawImage(const Graphics::Surface &img, int x, int y, bool keyed) {
	Display *display = _vm->display();
	if (display->hardware()) {
		display->drawImage(img, x, y, keyed);
		return;
	}
	Graphics::Surface &dst = _vm->screen();
	if (keyed) {
		blitKeyed(dst, img, x, y);
		return;
	}
	Common::Rect r(x, y, x + img.w, y + img.h);
	r.clip(Common::Rect(dst.w, dst.h));
	if (r.isEmpty())
		return;
	dst.copyRectToSurface(img, r.left, r.top, Common::Rect(r.left - x, r.top - y, r.right - x, r.bottom - y));
}

void World::barLogic() {
	// interaction.md "Inventory bar": move, clicks, items (drawn by drawBar).
	const int barH = _bar.h;
	_barButton = -1;
	if (_barState == 2) {
		if (!_barSoundPlayed) {
			_vm->sound()->playStatic("bar_obj");
			_barSoundPlayed = true;
		}
		_barY -= 8;
		if (_barY <= 480 - barH) {
			_barY = 480 - barH;
			_barState = 1;
		}
	} else if (_barState == 3) {
		_barY += 8;
		if (_barY >= 480) {
			_barY = 480;
			_barState = 0;
			autosave();
		}
	}
	// Clicks while shown.
	if (_barState == 1 && _click) {
		const Common::Point m = _mousePos;
		int held = 0;
		for (uint i = 0; i < kNumObjects; i++)
			held += _vm->state().held(i) ? 1 : 0;
		if (_cursor == kCursorArrow) {
			if (m.x >= 12 && m.x <= 44 && m.y >= _barY + 15 && m.y <= _barY + 47) {
				_barButton = 0;
				if (_barFirst > 0)
					_barFirst--;
			} else if (m.x >= 500 && m.x <= 532 && m.y >= _barY + 15 && m.y <= _barY + 47) {
				_barButton = 1;
				_barFirst++;
				if (held < _barFirst + 6)
					_barFirst--;
			}
		} else if (_cursor < kNumObjects && m.y >= _barY - 30 && m.y <= _barY + 70) {
			if (_cursor == 0)
				var(0x4abbd8) = 1;
			_vm->sound()->playStatic("cf_clic3");
			_vm->state().setHeld(_cursor, 1);
			_cursor = kCursorArrow;
			held++;
			if (held > 6)
				_barFirst = held - 7;
			_barState = 3;
		}
	}
}

void World::drawBar(int barY) {
	if (barY >= 480)
		return;
	drawImage(_bar, 0, barY, false);
	// The pressed arrow shows for the tick of its click.
	if (_barButton == 0)
		drawImage(_cursors[kCursorDef0], 10, barY + 13, true);
	else if (_barButton == 1)
		drawImage(_cursors[kCursorDef1], 503, barY + 13, true);
	// Held objects and the sunflower counter.
	int skipped = 0, drawn = 0;
	for (uint i = 0; i < kNumObjects && drawn < 6; i++) {
		if (!_vm->state().held(i))
			continue;
		if (skipped < _barFirst) {
			skipped++;
			continue;
		}
		const Graphics::Surface &img = _cursors[i];
		drawImage(img, kBarItemX[drawn] - img.w / 2, barY + 30 - img.h / 2, true);
		drawn++;
	}
	const int n = MIN<int>(sunflowers(), 15);
	const Graphics::Surface &ct = _cursors[kCursorCounter + n];
	drawImage(ct, 556, barY + 31 - ct.h / 2, true);
}

Common::Rect World::viewRect() const {
	// The original's 640x480; a wide display adds columns on both sides (enhancement).
	const Display *display = _vm->display();
	return Common::Rect(display->left(), 0, display->left() + display->width(), 480);
}

void World::frameLogic(bool hourglass) {
	// The part of the original's frame that the game reads back: what is under the mouse
	// and where the nodes are (render.md "Picking"), for this tick's camera.
	_renderer.setFocal(_focal);
	_renderer.draw(nullptr, viewRect(), _scene3D, viewOf(_cam));
	barLogic();
	// The return icon (interaction.md "The return icon and the ways out").
	const Graphics::Surface &ret = _cursors[kCursorRetour];
	_returnShown = !hourglass && _scene != kSceneMusee && _barState == 0 &&
		_mousePos.x < ret.w + 10 && _mousePos.y > 474 - ret.h;
	if (_returnShown) {
		_cursor = kCursorFinger;
		if (_click)
			requestMuseum();
	}
	if (_carrying && _carried >= 0) {
		const Common::String &n = _scene3D.nodes[_carried].name;
		if (n.equalsIgnoreCase("fagot"))
			_cursor = kCursorFagot;
		else if (n.equalsIgnoreCase("buche"))
			_cursor = kCursorBuche;
		else if (n.equalsIgnoreCase("poignee04"))
			_cursor = kCursorManivel;
		else if (n.equalsIgnoreCase("clef"))
			_cursor = kCursorCle;
	}
	_hourglass = hourglass;
}

// ---------------------------------------------------------------------------------------
// Drawing between ticks (enhancement: the original draws once per 66 ms tick)

namespace {

/** A rotation matrix (row-major, unit rows) as a unit quaternion (w, x, y, z). */
void toQuat(const float *m, float q[4]) {
	const float t = m[0] + m[4] + m[8];
	if (t > 0) {
		const float s = sqrtf(t + 1.0f) * 2;
		q[0] = 0.25f * s;
		q[1] = (m[7] - m[5]) / s;
		q[2] = (m[2] - m[6]) / s;
		q[3] = (m[3] - m[1]) / s;
	} else if (m[0] > m[4] && m[0] > m[8]) {
		const float s = sqrtf(1.0f + m[0] - m[4] - m[8]) * 2;
		q[0] = (m[7] - m[5]) / s;
		q[1] = 0.25f * s;
		q[2] = (m[1] + m[3]) / s;
		q[3] = (m[2] + m[6]) / s;
	} else if (m[4] > m[8]) {
		const float s = sqrtf(1.0f + m[4] - m[0] - m[8]) * 2;
		q[0] = (m[2] - m[6]) / s;
		q[1] = (m[1] + m[3]) / s;
		q[2] = 0.25f * s;
		q[3] = (m[5] + m[7]) / s;
	} else {
		const float s = sqrtf(1.0f + m[8] - m[0] - m[4]) * 2;
		q[0] = (m[3] - m[1]) / s;
		q[1] = (m[2] + m[6]) / s;
		q[2] = (m[5] + m[7]) / s;
		q[3] = 0.25f * s;
	}
}

void fromQuat(const float q[4], float *m) {
	const float w = q[0], x = q[1], y = q[2], z = q[3];
	m[0] = 1 - 2 * (y * y + z * z);
	m[1] = 2 * (x * y - w * z);
	m[2] = 2 * (x * z + w * y);
	m[3] = 2 * (x * y + w * z);
	m[4] = 1 - 2 * (x * x + z * z);
	m[5] = 2 * (y * z - w * x);
	m[6] = 2 * (x * z - w * y);
	m[7] = 2 * (y * z + w * x);
	m[8] = 1 - 2 * (x * x + y * y);
}

/** Unit rows: a rotation (a scaled node keeps its new pose). */
bool isRotation(const float *m) {
	for (int r = 0; r < 3; r++) {
		const float l = m[3 * r] * m[3 * r] + m[3 * r + 1] * m[3 * r + 1] + m[3 * r + 2] * m[3 * r + 2];
		if (l < 0.96f || l > 1.04f)
			return false;
	}
	return true;
}

/** a + (b - a) * t for angles in 1/4096 turn, the short way round. */
float lerpAngle(float a, float b, float t) {
	float d = fmodf(b - a, 4096.0f);
	if (d > 2048)
		d -= 4096;
	else if (d < -2048)
		d += 4096;
	return a + d * t;
}

} // End of anonymous namespace

void World::endTick() {
	// What this tick drew, and the tick before, for drawing in between.
	const uint n = _scene3D.nodes.size();
	_posePrev = _poseCur;
	_poseCur.resize(12 * n);
	for (uint i = 0; i < n; i++) {
		const Node &nd = _scene3D.nodes[i];
		float *p = &_poseCur[12 * i];
		for (int j = 0; j < 9; j++)
			p[j] = nd.rotation[j] / 32768.0f;
		p[9] = (float)nd.position.x;
		p[10] = (float)nd.position.y;
		p[11] = (float)nd.position.z;
	}
	memcpy(_camPrev, _camCur, sizeof(_camCur));
	_camCur[0] = (float)_cam.x;
	_camCur[1] = (float)_cam.y;
	_camCur[2] = (float)_cam.z;
	_camCur[3] = (float)_cam.pitch + _lookRest[1];
	_camCur[4] = (float)_cam.yaw + _lookRest[0];
	_camCur[5] = (float)_cam.roll;
	_barYPrev = _barYCur;
	_barYCur = _barY;
	if (_cut || _posePrev.size() != _poseCur.size()) {
		_posePrev = _poseCur;
		memcpy(_camPrev, _camCur, sizeof(_camCur));
		_barYPrev = _barYCur;
		_cut = false;
	}
	updateHotspots();
}

void World::look() {
	// Modern controls: the mouse turns the view at once, between ticks too. The camera takes
	// whole steps; the fraction stays in the drawn angles.
	float yaw, pitch;
	_vm->takeLook(yaw, pitch);
	if (!_vm->mouseCaptured() || (yaw == 0 && pitch == 0))
		return;
	float cur = fmodf(_camCur[3], 4096.0f);
	if (cur < 0)
		cur += 4096;
	if (cur >= 2048)
		cur -= 4096;
	// Up or down to about 53 degrees (the original's Page Up/Down reach about 44 from level).
	const float next = CLIP(cur + pitch, MIN(cur, -600.0f), MAX(cur, 600.0f));
	pitch = next - cur;
	_camPrev[3] += pitch;
	_camCur[3] += pitch;
	_camPrev[4] += yaw;
	_camCur[4] += yaw;
	_lookRest[0] += yaw;
	_lookRest[1] += pitch;
	const int32 dy = (int32)_lookRest[0], dp = (int32)_lookRest[1];
	_cam.yaw = (_cam.yaw + dy) & 0xFFF;
	_cam.pitch = (_cam.pitch + dp) & 0xFFF;
	_lookRest[0] -= dy;
	_lookRest[1] -= dp;
	_vm->warpMouse(320, 240);
}

void World::render(float alpha) {
	Display *display = _vm->display();
	alpha = CLIP(alpha, 0.0f, 1.0f);
	// Modern controls: mouse look while the bar is closed; a free cursor over the bar.
	if (_vm->modernControls()) {
		_vm->captureMouse(_barState == 0 && !_hourglass);
		look();
	}
	const bool blend = !_cut && _poseCur.size() == 12 * _scene3D.nodes.size();

	// The camera between the last two ticks; not across a jump.
	float cam[6] = { (float)_cam.x, (float)_cam.y, (float)_cam.z, (float)_cam.pitch, (float)_cam.yaw, (float)_cam.roll };
	if (blend) {
		const float d[3] = { _camCur[0] - _camPrev[0], _camCur[1] - _camPrev[1], _camCur[2] - _camPrev[2] };
		const bool jump = d[0] * d[0] + d[1] * d[1] + d[2] * d[2] > 3000.0f * 3000.0f;
		const float t = jump ? 1.0f : alpha;
		for (int i = 0; i < 3; i++)
			cam[i] = _camPrev[i] + d[i] * t;
		for (int i = 3; i < 6; i++)
			cam[i] = lerpAngle(_camPrev[i], _camCur[i], t);
	}
	const View view = viewOf(cam[0], cam[1], cam[2], cam[3], cam[4], cam[5]);

	// Node poses between the last two ticks: rotations by quaternions, positions linearly;
	// a node that jumped or turned far (a cut in its track) takes the new pose.
	const float *poses = nullptr;
	if (blend) {
		_poseDraw.resize(_poseCur.size());
		for (uint i = 0; i < _poseCur.size(); i += 12) {
			const float *a = &_posePrev[i], *b = &_poseCur[i];
			float *o = &_poseDraw[i];
			memcpy(o, b, 12 * sizeof(float));
			if (!memcmp(a, b, 12 * sizeof(float)))
				continue;
			const float dp[3] = { b[9] - a[9], b[10] - a[10], b[11] - a[11] };
			if (dp[0] * dp[0] + dp[1] * dp[1] + dp[2] * dp[2] > 2000.0f * 2000.0f)
				continue;
			for (int j = 0; j < 3; j++)
				o[9 + j] = a[9 + j] + dp[j] * alpha;
			if (!memcmp(a, b, 9 * sizeof(float)) || !isRotation(a) || !isRotation(b))
				continue;
			float qa[4], qb[4];
			toQuat(a, qa);
			toQuat(b, qb);
			float dot = qa[0] * qb[0] + qa[1] * qb[1] + qa[2] * qb[2] + qa[3] * qb[3];
			if (dot < 0) {
				for (float &c : qb)
					c = -c;
				dot = -dot;
			}
			if (dot < 0.95f)
				continue; // more than about 36 degrees in one tick
			// Close rotations: a normalised lerp is a slerp to within a hair.
			float q[4], len = 0;
			for (int j = 0; j < 4; j++) {
				q[j] = qa[j] + (qb[j] - qa[j]) * alpha;
				len += q[j] * q[j];
			}
			len = sqrtf(len);
			for (float &c : q)
				c /= len;
			fromQuat(q, o);
		}
		poses = _poseDraw.data();
	}

	_view.setFocal(_focal);
	_view.setClip(_renderer.nearZ(), _renderer.farZ());
	if (display->hardware()) {
		_view.draw(nullptr, viewRect(), _scene3D, view, poses, &_tris);
		display->begin3D(_focal > 0 ? _focal : 480.0f, _renderer.nearZ(), _renderer.farZ());
		display->drawTriangles(_tris);
	} else {
		_view.draw(&_vm->screen(), Common::Rect(0, 0, 640, 480), _scene3D, view, poses);
	}

	// The 2D over it: the bar (sliding between ticks too), the return icon, the hourglass.
	drawBar(blend ? (int)(_barYPrev + (_barYCur - _barYPrev) * alpha + 0.5f) : _barY);
	if (_returnShown) {
		const Graphics::Surface &ret = _cursors[kCursorRetour];
		drawImage(ret, 10, 474 - ret.h, true);
	}
	if (_hourglass) {
		const Graphics::Surface &h = _cursors[kCursorHourglass];
		drawImage(h, 320 - h.w / 2, 240 - h.h / 2, true);
	}
	updateCursor();
	if (display->hardware())
		display->end3D();
	else
		_vm->present();
	_vm->drawHotspots();
}

void World::updateHotspots() {
	// ScummVM's hotspot overlay: each object of the scene's table that the game can pick
	// now, marked at the first of its centre and its vertices that picks it.
	if (!_vm->hotspotsShown())
		return;
	Common::Array<Graphics::HotspotInfo> list;
	if (!_flying) {
		for (const SceneObject &o : objects) {
			Common::Point p;
			if (o.node < 0 || !_renderer.nodeScreenPoint(o.node, p))
				continue;
			const Graphics::HotspotType type = o.cursorType == 6 ? Graphics::kHotspotExit : Graphics::kHotspotObject;
			list.push_back(Graphics::HotspotInfo(_vm->display()->toWindow(p), Common::U32String(o.name), type));
		}
	}
	bool same = list.size() == _hotspotList.size();
	for (uint i = 0; same && i < list.size(); i++)
		same = list[i].position == _hotspotList[i].position && list[i].name == _hotspotList[i].name;
	if (!same) {
		_hotspotList = list;
		_hotspotsChanged = true;
	}
}

void World::updateCursor() {
	// The original draws the cursor into each 66 ms frame, top-left at the tick's mouse.
	// With the high_fps option it goes through ScummVM's cursor manager instead, following
	// the mouse at the display's rate.
	if (_hourglass || !_systemCursor) {
		CursorMan.showMouse(false);
		if (!_hourglass)
			drawImage(_cursors[_cursor], _mousePos.x, _mousePos.y, true);
		return;
	}
	const int scale = _vm->display()->pixelScale();
	const bool centred = _carrying && _vm->mouseCaptured();
	if (_cursor != _shownCursor || scale != _shownScale || centred != _shownCentred) {
		_shownCursor = _cursor;
		_shownScale = scale;
		_shownCentred = centred;
		const Graphics::Surface &c = _cursors[_cursor];
		const int hx = centred ? c.w / 2 * scale : 0, hy = centred ? c.h / 2 * scale : 0;
		if (scale <= 1) {
			CursorMan.replaceCursor(c, hx, hy, kTgaKey);
		} else {
			Graphics::Surface *big = c.scale(c.w * scale, c.h * scale); // nearest: the key stays exact
			CursorMan.replaceCursor(*big, hx, hy, kTgaKey);
			big->free();
			delete big;
		}
	}
	CursorMan.showMouse(true);
}

// ---------------------------------------------------------------------------------------
// The tick

void World::move() {
	// movement.md "Walking and turning", once per handled tick.
	auto damp = [](int32 &x) {
		if (x > 10 || x < -10)
			x -= x / 2;
		else
			x = 0;
	};
	damp(_v);
	damp(_w);
	damp(_s);
	if (_vy != 0)
		_vy -= _vy / 2;
	if (_vy < 6)
		_vy += 5;
	if (_vm->keyHeld(Common::KEYCODE_UP) && _v < 2500)
		_v += 60;
	if (_vm->keyHeld(Common::KEYCODE_DOWN) && _v > -2500)
		_v -= 60;
	// The turn speed option scales the turning and looking steps (100%: 40, 300, 30).
	const int32 turnStep = (int32)(40 * _turn), turnMax = (int32)(300 * _turn), lookStep = (int32)(30 * _turn);
	if (_vm->keyHeld(Common::KEYCODE_LEFT) && _w > -turnMax)
		_w -= turnStep;
	if (_vm->keyHeld(Common::KEYCODE_RIGHT) && _w < turnMax)
		_w += turnStep;
	if (_vm->keyHeld(Common::KEYCODE_PAGEUP) && _p < 500) {
		_p += lookStep;
		_cam.pitch += lookStep;
	}
	if (_vm->keyHeld(Common::KEYCODE_PAGEDOWN) && _p > -500) {
		_p -= lookStep;
		_cam.pitch -= lookStep;
	}
	// Strafing (modern controls, not in the original): walking's steps along the right axis.
	if (_vm->keyHeld((Common::KeyCode)kKeyStrafeLeft) && _s > -2500)
		_s -= 60;
	if (_vm->keyHeld((Common::KeyCode)kKeyStrafeRight) && _s < 2500)
		_s += 60;
	_cam.pitch &= 0xFFF;
	_cam.yaw = (_cam.yaw + _w) & 0xFFF;
	_cam.roll &= 0xFFF;
	int32 m[9];
	cameraMatrix(_cam.pitch, _cam.yaw, _cam.roll, m);
	_cam.x += (int32)(((int64)m[2] * _v) / 32768);
	_cam.z += (int32)(((int64)m[8] * _v) / 32768);
	if (_s) {
		_cam.x += (int32)(((int64)m[0] * _s) / 32768);
		_cam.z += (int32)(((int64)m[6] * _s) / 32768);
	}
	_cam.y += _vy;
}

namespace {

struct Vec3d {
	double x, y, z;
};

Vec3d closestOnSegment(const Vec3d &p, const Vec3d &a, const Vec3d &b) {
	const Vec3d ab = { b.x - a.x, b.y - a.y, b.z - a.z };
	const double len2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
	double t = len2 > 0 ? ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y + (p.z - a.z) * ab.z) / len2 : 0;
	t = CLIP(t, 0.0, 1.0);
	const Vec3d r = { a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t };
	return r;
}

/** A .3DI face resolved: its vertices and normal (movement.md "Collision"). */
bool faceData(const Boxes3D &bs, const Boxes3D::Face &f, Vec3d v[3], Vec3i &n) {
	const int ni = (f.ref[3] - (int32)bs.itemBase) / 12;
	if (f.ref[3] < 0 || ni < 0 || (uint)ni >= bs.items.size())
		return false;
	n = bs.items[ni];
	for (int k = 0; k < 3; k++) {
		const int vi = (f.ref[k] - (int32)bs.vertexBase) / 12;
		if (f.ref[k] < 0 || vi < 0 || (uint)vi >= bs.vertices.size())
			return false;
		v[k].x = bs.vertices[vi].x;
		v[k].y = bs.vertices[vi].y;
		v[k].z = bs.vertices[vi].z;
	}
	return true;
}

} // End of anonymous namespace

void World::collide() {
	// movement.md "Collision": a sphere of radius 250 against BOX.3DI and the current extra
	// set; one push-out pass per frame.
	Common::Array<const Boxes3D *> sets;
	if (!_boxLoaded.empty() && _boxLoaded[0])
		sets.push_back(&_boxSets[0]);
	if (_extraBox > 0 && (uint)_extraBox < _boxSets.size() && _boxLoaded[_extraBox])
		sets.push_back(&_boxSets[_extraBox]);

	const Vec3d c = { (double)_cam.x, (double)_cam.y, (double)_cam.z };
	double F[3] = { 0, 0, 0 }, N[3] = { 0, 0, 0 }, E[3] = { 0, 0, 0 };
	int nf = 0, ne = 0;
	for (const Boxes3D *bs : sets) {
		for (const Boxes3D::Face &f : bs->faces) {
			const byte *r = f.raw;
			if (c.x + kRadius < READ_LE_INT32(r + 0x48) || c.x - kRadius > READ_LE_INT32(r + 0x54) ||
				c.y + kRadius < READ_LE_INT32(r + 0x4C) || c.y - kRadius > READ_LE_INT32(r + 0x58) ||
				c.z + kRadius < READ_LE_INT32(r + 0x50) || c.z - kRadius > READ_LE_INT32(r + 0x5C))
				continue;
			Vec3d v[3];
			Vec3i n;
			if (!faceData(*bs, f, v, n))
				continue;
			const double D = READ_LE_INT32(r + 0x10);
			const double d = (n.x * c.x + n.y * c.y + n.z * c.z) / 32768.0 - D;
			if (!(d > -kRadius && d < kRadius) || d < 0)
				continue; // no contact, or behind the face (ignored)
			double e[3];
			bool far = false;
			for (int k = 0; k < 3; k++) {
				e[k] = (READ_LE_INT32(r + 0x14 + 12 * k) * c.x + READ_LE_INT32(r + 0x18 + 12 * k) * c.y +
						READ_LE_INT32(r + 0x1C + 12 * k) * c.z) / 32768.0 - READ_LE_INT32(r + 0x38 + 4 * k);
				if (e[k] < -kRadius)
					far = true;
			}
			if (far)
				continue;
			const bool inside = e[0] >= 0 && e[1] >= 0 && e[2] >= 0;
			Vec3d p = c;
			if (inside) {
				p.x = c.x - n.x / 32768.0 * d;
				p.y = c.y - n.y / 32768.0 * d;
				p.z = c.z - n.z / 32768.0 * d;
			} else {
				double best = 1e30;
				for (int k = 0; k < 3; k++) {
					const Vec3d q = closestOnSegment(c, v[k], v[(k + 1) % 3]);
					const double dd = (q.x - c.x) * (q.x - c.x) + (q.y - c.y) * (q.y - c.y) + (q.z - c.z) * (q.z - c.z);
					if (dd < best) {
						best = dd;
						p = q;
					}
				}
			}
			const double dist2 = (p.x - c.x) * (p.x - c.x) + (p.y - c.y) * (p.y - c.y) + (p.z - c.z) * (p.z - c.z);
			if (dist2 >= (double)kRadius * kRadius)
				continue;
			if (inside) {
				F[0] += p.x;
				F[1] += p.y;
				F[2] += p.z;
				N[0] += n.x;
				N[1] += n.y;
				N[2] += n.z;
				nf++;
			} else if (nf == 0) {
				E[0] += p.x;
				E[1] += p.y;
				E[2] += p.z;
				ne++;
			}
		}
	}
	if (nf > 0) {
		double o[3];
		for (int k = 0; k < 3; k++)
			o[k] = trunc(trunc(trunc(N[k] / nf) * kRadius / 32768.0) + F[k] / nf);
		_cam.x = (int32)o[0];
		_cam.y = (int32)o[1];
		_cam.z = (int32)o[2];
	} else if (ne > 0) {
		const double e[3] = { E[0] / ne, E[1] / ne, E[2] / ne };
		const double d[3] = { trunc(c.x - e[0]), trunc(c.y - e[1]), trunc(c.z - e[2]) };
		const double len = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
		if (len > 0) {
			_cam.x = (int32)trunc(trunc(d[0] * kRadius / len) + e[0]);
			_cam.y = (int32)trunc(trunc(d[1] * kRadius / len) + e[1]);
			_cam.z = (int32)trunc(trunc(d[2] * kRadius / len) + e[2]);
		}
	}

	// movement.md "Floor": the eye 700 above the nearest floor below.
	const double cx = _cam.x, cy = _cam.y, cz = _cam.z;
	bool found = false;
	double floorY = 0;
	for (const Boxes3D *bs : sets) {
		for (const Boxes3D::Face &f : bs->faces) {
			const byte *r = f.raw;
			if (cx + kRadius < READ_LE_INT32(r + 0x48) || cx - kRadius > READ_LE_INT32(r + 0x54) ||
				cz + kRadius < READ_LE_INT32(r + 0x50) || cz - kRadius > READ_LE_INT32(r + 0x5C))
				continue;
			Vec3d v[3];
			Vec3i n;
			if (!faceData(*bs, f, v, n) || n.y == 0)
				continue;
			double s[3];
			for (int k = 0; k < 3; k++) {
				const Vec3d &a = v[k], &b = v[(k + 1) % 3];
				s[k] = (b.x - a.x) * (cz - a.z) - (b.z - a.z) * (cx - a.x);
			}
			if (!((s[0] >= 0 && s[1] >= 0 && s[2] >= 0) || (s[0] <= 0 && s[1] <= 0 && s[2] <= 0)))
				continue;
			const double D = READ_LE_INT32(r + 0x10);
			if ((n.x * cx + n.y * cy + n.z * cz) / 32768.0 - D < 0)
				continue;
			const double yf = (D * 32768.0 - n.x * cx - n.z * cz) / n.y;
			if (yf > cy && (!found || yf < floorY)) {
				floorY = yf;
				found = true;
			}
		}
	}
	if (found)
		_cam.y = (int32)(floorY - kEyeAboveFloor);
}

WorldExit World::tick() {
	// The engine runs every 66 ms tick (a late one is caught up rather than dropped), so
	// one tick has passed since the last (movement.md "The tick": elapsed 1..10).
	_elapsed = 1;

	if (_reload) {
		_reload = false;
		load(_scene, _prevScene, false);
	}
	if (_flying) {
		mouse();
		flightStep();
		frameLogic(true);
		return _exit;
	}
	// One frame (movement.md, 0x4223e8).
	move();
	collide();
	mouse();
	frameLogic(false);
	if (_vm->keyFired(Common::KEYCODE_BACKSPACE) && _scene != kSceneMusee)
		requestMuseum();
	if (_vm->keyFired(Common::KEYCODE_SPACE)) {
		if (_barState == 0)
			openBar();
		else if (_barState == 1)
			_barState = 3;
	}
	if (_vm->keyFired(Common::KEYCODE_ESCAPE) && _exit == kExitNone) {
		storeView(true); // 0x42edef
		_exit = kExitOptions;
	}
	if (_script)
		_script->frame(*this);
	else
		advanceAnims();
	const WorldExit e = _exit;
	if (e == kExitOptions)
		_exit = kExitNone;
	return e;
}

} // End of namespace Peintre
