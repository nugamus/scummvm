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
#include "common/endian.h"
#include "common/system.h"

#include "peintre/detection.h"
#include "peintre/peintre.h"
#include "peintre/world.h"

namespace Peintre {

// scene.md "The scene table": bundle per scene (scene 6 picks chambreb/chambrev).
static const char *const kBundles[kNumScenes] = {
	"musee", "auberge", "hopiext", "maisonet", "mangeurs", "cafe", nullptr,
	"maisonj", "hopiint", "pont", "terrasse", "jardin", "champ", "eglise"
};

struct StartPos {
	int32 x, y, z, pitch, yaw;
};

// scene.md "Start positions", case A: the scene's own start (roll 0).
static const StartPos kOwnStart[kNumScenes] = {
	{ -39, -209, 361, 4066, 20 },       // 0 museum, game start
	{ 0, 0, 0, 0, 0 },                  // 1 auberge
	{ 0, 0, 0, 0, 0 },                  // 2 hopiext: no row (only entered from scenes)
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

// The museum spot in front of the painting of prevScene (and the flight's targets).
static const StartPos kMuseumSpot[kNumScenes] = {
	{ -39, -209, 361, 4066, 20 },
	{ 3230, -297, 5166, 4036, 3073 },
	{ 538, -297, 8256, 4036, 290 },
	{ -3125, -297, 5252, 4066, 1008 },
	{ -4849, -297, 5195, 4036, 3101 },
	{ -562, -296, 6923, 4036, 2355 },
	{ -909, -297, 7462, 4066, 3062 },
	{ -466, -297, 8184, 4066, 3788 },
	{ 538, -297, 8256, 4036, 290 },     // 8 as 2
	{ 703, -297, 6902, 4036, 1782 },
	{ 933, -297, 7535, 0, 988 },
	{ 3230, -297, 5166, 4036, 3073 },   // 11 as 1
	{ 4403, -297, 5867, 4006, 339 },
	{ 4524, -297, 4758, 4006, 1791 }
};

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

// Viewer body (movement.md "Collision", "Floor").
static const int32 kRadius = 250;
static const int32 kEyeAboveFloor = 700;

// The 3D block's bedroom flag (0x4abd14, block +0x2D4): chambreb instead of chambrev.
static const uint kChambrebFlag = 0x2D4;

World::World(PeintreEngine *vm) : _vm(vm) {
}

World::~World() {
	unload();
}

void World::unload() {
	_script.reset();
	for (Texture3D *t : _textures)
		delete t;
	_textures.clear();
	_materialTex.clear();
	_boxes.clear();
	_scene3D = Scene3D();
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

bool World::load(int scene, int prevScene, bool keepCamera) {
	unload();
	_scene = scene;
	_prevScene = prevScene;
	Common::String bundle;
	if (scene == kSceneChambre)
		bundle = _vm->state().block3D[kChambrebFlag] ? "chambreb" : "chambrev";
	else if (scene >= 0 && scene < kNumScenes)
		bundle = kBundles[scene];
	else
		return false;

	if (!_bfg.open(Common::Path("Scenes_3D/" + bundle + ".BFG"))) {
		warning("World: cannot open %s.BFG", bundle.c_str());
		return false;
	}
	Common::Array<byte> data;
	if (!_bfg.readEntry(bundle + ".3DC", data) || !_scene3D.load(data)) {
		warning("World: cannot load %s.3DC", bundle.c_str());
		return false;
	}
	// Textures by material (E-0014: "<texture>.3DM" of the same bundle), shared by name.
	Common::Array<Common::String> texNames;
	for (const Material &m : _scene3D.materials) {
		const Texture3D *tex = nullptr;
		if (!m.texture.empty()) {
			for (uint i = 0; i < texNames.size(); i++)
				if (texNames[i].equalsIgnoreCase(m.texture))
					tex = _textures[i];
			if (!tex && _bfg.readEntry(m.texture + ".3DM", data)) {
				Texture3D *t = new Texture3D();
				if (t->load(data)) {
					_textures.push_back(t);
					texNames.push_back(m.texture);
					tex = t;
				} else {
					delete t;
				}
			}
		}
		_materialTex.push_back(tex);
	}
	// The collision faces (scene.md "What a scene is made of", step 3).
	Boxes3D boxes;
	if (_bfg.readEntry("BOX.3DI", data) && boxes.load(data))
		_boxes.push_back(boxes);

	// Start position (scene.md "Start positions").
	if (!keepCamera) {
		StartPos s = kOwnStart[scene];
		if (scene == kSceneMusee) {
			s = kMuseumSpot[prevScene >= 0 && prevScene < kNumScenes ? prevScene : 0];
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
	_v = _vy = _w = _p = 0;
	debugC(1, kDebugLoad, "World: scene %d (%s) from %d, %d nodes, %d textures, %d box faces",
		   scene, bundle.c_str(), prevScene, _scene3D.nodes.size(), _textures.size(),
		   _boxes.empty() ? 0 : _boxes[0].faces.size());
	return true;
}

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
	if (_vy != 0)
		_vy -= _vy / 2;
	if (_vy < 6)
		_vy += 5;
	if (_vm->keyHeld(Common::KEYCODE_UP) && _v < 2500)
		_v += 60;
	if (_vm->keyHeld(Common::KEYCODE_DOWN) && _v > -2500)
		_v -= 60;
	if (_vm->keyHeld(Common::KEYCODE_LEFT) && _w > -300)
		_w -= 40;
	if (_vm->keyHeld(Common::KEYCODE_RIGHT) && _w < 300)
		_w += 40;
	if (_vm->keyHeld(Common::KEYCODE_PAGEUP) && _p < 500) {
		_p += 30;
		_cam.pitch += 30;
	}
	if (_vm->keyHeld(Common::KEYCODE_PAGEDOWN) && _p > -500) {
		_p -= 30;
		_cam.pitch -= 30;
	}
	_cam.pitch &= 0xFFF;
	_cam.yaw = (_cam.yaw + _w) & 0xFFF;
	_cam.roll &= 0xFFF;
	int32 m[9];
	cameraMatrix(_cam.pitch, _cam.yaw, _cam.roll, m);
	_cam.x += (int32)(((int64)m[2] * _v) / 32768);
	_cam.z += (int32)(((int64)m[8] * _v) / 32768);
	_cam.y += _vy;
}

namespace {

struct Vec3f {
	double x, y, z;
};

Vec3f closestOnSegment(const Vec3f &p, const Vec3f &a, const Vec3f &b) {
	const Vec3f ab = { b.x - a.x, b.y - a.y, b.z - a.z };
	const double len2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
	double t = len2 > 0 ? ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y + (p.z - a.z) * ab.z) / len2 : 0;
	t = CLIP(t, 0.0, 1.0);
	const Vec3f r = { a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t };
	return r;
}

} // End of anonymous namespace

void World::collide() {
	// movement.md "Collision" and "Floor": a sphere of radius 250 against the registered
	// box sets' faces; one push-out pass, then the eye 700 above the nearest floor below.
	const Vec3f c = { (double)_cam.x, (double)_cam.y, (double)_cam.z };
	double F[3] = { 0, 0, 0 }, N[3] = { 0, 0, 0 }, E[3] = { 0, 0, 0 };
	int nf = 0, ne = 0;
	for (const Boxes3D &bs : _boxes) {
		for (const Boxes3D::Face &f : bs.faces) {
			const byte *r = f.raw;
			const int32 minX = READ_LE_INT32(r + 0x48), minY = READ_LE_INT32(r + 0x4C), minZ = READ_LE_INT32(r + 0x50);
			const int32 maxX = READ_LE_INT32(r + 0x54), maxY = READ_LE_INT32(r + 0x58), maxZ = READ_LE_INT32(r + 0x5C);
			if (c.x + kRadius < minX || c.x - kRadius > maxX || c.y + kRadius < minY || c.y - kRadius > maxY ||
				c.z + kRadius < minZ || c.z - kRadius > maxZ)
				continue;
			const int ni = (f.ref[3] - (int32)bs.itemBase) / 12;
			if (f.ref[3] < 0 || ni < 0 || (uint)ni >= bs.items.size())
				continue;
			const Vec3i &n = bs.items[ni];
			const double D = READ_LE_INT32(r + 0x10);
			const double d = (n.x * c.x + n.y * c.y + n.z * c.z) / 32768.0 - D;
			if (!(d > -kRadius && d < kRadius) || d < 0)
				continue; // behind the plane: ignored
			double e[3];
			bool far = false;
			for (int k = 0; k < 3; k++) {
				const int32 ex = READ_LE_INT32(r + 0x14 + 12 * k), ey = READ_LE_INT32(r + 0x18 + 12 * k), ez = READ_LE_INT32(r + 0x1C + 12 * k);
				e[k] = (ex * c.x + ey * c.y + ez * c.z) / 32768.0 - READ_LE_INT32(r + 0x38 + 4 * k);
				if (e[k] < -kRadius)
					far = true;
			}
			if (far)
				continue;
			Vec3f v[3];
			bool okv = true;
			for (int k = 0; k < 3; k++) {
				const int vi = (f.ref[k] - (int32)bs.vertexBase) / 12;
				if (f.ref[k] < 0 || vi < 0 || (uint)vi >= bs.vertices.size()) {
					okv = false;
					break;
				}
				v[k].x = bs.vertices[vi].x;
				v[k].y = bs.vertices[vi].y;
				v[k].z = bs.vertices[vi].z;
			}
			if (!okv)
				continue;
			const bool inside = e[0] >= 0 && e[1] >= 0 && e[2] >= 0;
			Vec3f p = c;
			if (inside) {
				p.x = c.x - n.x / 32768.0 * d;
				p.y = c.y - n.y / 32768.0 * d;
				p.z = c.z - n.z / 32768.0 * d;
			} else {
				// The nearest point of the triangle's edges.
				double best = 1e30;
				for (int k = 0; k < 3; k++) {
					const Vec3f q = closestOnSegment(c, v[k], v[(k + 1) % 3]);
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
		double n[3], f[3], o[3];
		for (int k = 0; k < 3; k++) {
			n[k] = trunc(N[k] / nf);
			f[k] = F[k] / nf;
			o[k] = trunc(trunc(n[k] * kRadius / 32768.0) + f[k]);
		}
		_cam.x = (int32)o[0];
		_cam.y = (int32)o[1];
		_cam.z = (int32)o[2];
	} else if (ne > 0) {
		double e[3], d[3];
		for (int k = 0; k < 3; k++)
			e[k] = E[k] / ne;
		d[0] = trunc(c.x - e[0]);
		d[1] = trunc(c.y - e[1]);
		d[2] = trunc(c.z - e[2]);
		const double len = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
		if (len > 0) {
			_cam.x = (int32)trunc(trunc(d[0] * kRadius / len) + e[0]);
			_cam.y = (int32)trunc(trunc(d[1] * kRadius / len) + e[1]);
			_cam.z = (int32)trunc(trunc(d[2] * kRadius / len) + e[2]);
		}
	}

	// Floor: the smallest plane height below the camera (larger y) among the faces whose
	// (x, z) projection holds the camera, on their front side.
	const double cx = _cam.x, cy = _cam.y, cz = _cam.z;
	bool found = false;
	double floorY = 0;
	for (const Boxes3D &bs : _boxes) {
		for (const Boxes3D::Face &f : bs.faces) {
			const byte *r = f.raw;
			if (cx + kRadius < READ_LE_INT32(r + 0x48) || cx - kRadius > READ_LE_INT32(r + 0x54) ||
				cz + kRadius < READ_LE_INT32(r + 0x50) || cz - kRadius > READ_LE_INT32(r + 0x5C))
				continue;
			const int ni = (f.ref[3] - (int32)bs.itemBase) / 12;
			if (f.ref[3] < 0 || ni < 0 || (uint)ni >= bs.items.size())
				continue;
			const Vec3i &n = bs.items[ni];
			if (n.y == 0)
				continue;
			Vec3f v[3];
			bool okv = true;
			for (int k = 0; k < 3; k++) {
				const int vi = (f.ref[k] - (int32)bs.vertexBase) / 12;
				if (f.ref[k] < 0 || vi < 0 || (uint)vi >= bs.vertices.size()) {
					okv = false;
					break;
				}
				v[k].x = bs.vertices[vi].x;
				v[k].y = bs.vertices[vi].y;
				v[k].z = bs.vertices[vi].z;
			}
			if (!okv)
				continue;
			// Inside the (x, z) triangle: same sign of the three edge cross products.
			double s[3];
			for (int k = 0; k < 3; k++) {
				const Vec3f &a = v[k], &b = v[(k + 1) % 3];
				s[k] = (b.x - a.x) * (cz - a.z) - (b.z - a.z) * (cx - a.x);
			}
			if (!((s[0] >= 0 && s[1] >= 0 && s[2] >= 0) || (s[0] <= 0 && s[1] <= 0 && s[2] <= 0)))
				continue;
			const double D = READ_LE_INT32(r + 0x10);
			if ((n.x * cx + n.y * cy + n.z * cz) / 32768.0 - D < 0)
				continue; // behind the face
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

void World::drawFrame() {
	Graphics::Surface &dst = _vm->screen();
	const Common::Rect viewport(0, 0, 640, 480);
	_renderer.draw(dst, viewport, _scene3D, _materialTex, _cam);
}

WorldExit World::tick() {
	move();
	collide();
	drawFrame();
	if (_script)
		_script->frame(*this);
	if (_vm->keyFired(Common::KEYCODE_ESCAPE))
		return kExitOptions;
	return kExitNone;
}

void World::resume() {
}

} // End of namespace Peintre
