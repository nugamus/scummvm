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

#include "grumpa/grumpa.h"

namespace Grumpa {

// The items lying in the scene, drawn as 3D actors: engines/grumpa/docs/spec/inventory.md
// (Items in the world, E-0900).

// Frame `frame` of `src` placed by Direct3D's yaw-pitch-roll (roll about Z, then pitch about
// X, then yaw about Y, row vectors) and moved to `pos`.
static void placeMesh(const Mesh &src, int frame, const float rot[3], const float pos[3], Mesh &out) {
	const float cp = cosf(rot[0]), sp = sinf(rot[0]), cy = cosf(rot[1]), sy = sinf(rot[1]),
				cr = cosf(rot[2]), sr = sinf(rot[2]);
	auto turn = [&](const Vec3 &v) {
		float x = v.x * cr - v.y * sr, y = v.x * sr + v.y * cr, z = v.z;  // roll
		float y2 = y * cp - z * sp, z2 = y * sp + z * cp;                   // pitch
		return Vec3(x * cy + z2 * sy, y2, -x * sy + z2 * cy);              // yaw
	};
	out = Mesh();
	out.frames = 1;
	for (uint s = 0; s < src.sections.size(); s++) {
		const MeshSection &in = src.sections[s];
		if ((frame + 1) * in.nv > in.verts.size())
			continue;
		MeshSection sec;
		sec.nv = in.nv;
		sec.faces = in.faces;
		sec.u = in.u;
		sec.v = in.v;
		for (uint v = 0; v < in.nv; v++) {
			Vec3 p = turn(in.verts[frame * in.nv + v]);
			sec.verts.push_back(Vec3(p.x + pos[0], p.y + pos[1], p.z + pos[2]));
			sec.normals.push_back(turn(in.normals[frame * in.nv + v]));
		}
		out.sections.push_back(sec);
	}
}

// The screen bounding rectangle of the 8 corners of a placed mesh's bounding box, as the
// original keeps it for clicks (E-0900).
static Common::Rect screenRect(const Mesh &m, const Camera &cam) {
	Vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
	for (uint s = 0; s < m.sections.size(); s++)
		for (uint v = 0; v < m.sections[s].verts.size(); v++) {
			const Vec3 &p = m.sections[s].verts[v];
			lo = Vec3(MIN(lo.x, p.x), MIN(lo.y, p.y), MIN(lo.z, p.z));
			hi = Vec3(MAX(hi.x, p.x), MAX(hi.y, p.y), MAX(hi.z, p.z));
		}
	Common::Rect r;
	bool any = false;
	for (int c = 0; c < 8; c++) {
		float in[4] = { c & 1 ? hi.x : lo.x, c & 2 ? hi.y : lo.y, c & 4 ? hi.z : lo.z, 1.0f }, e[4], o[4];
		for (int j = 0; j < 4; j++)
			e[j] = in[0] * cam.view[j] + in[1] * cam.view[4 + j] + in[2] * cam.view[8 + j] + in[3] * cam.view[12 + j];
		for (int j = 0; j < 4; j++)
			o[j] = e[0] * cam.proj[j] + e[1] * cam.proj[4 + j] + e[2] * cam.proj[8 + j] + e[3] * cam.proj[12 + j];
		if (o[3] <= 1e-6f)
			continue;
		int x = (int)((o[0] / o[3] + 1) * kScreenWidth / 2), y = (int)((1 - o[1] / o[3]) * kScreenHeight / 2);
		if (!any)
			r = Common::Rect(x, y, x, y);
		else
			r.extend(Common::Rect(x, y, x, y));
		any = true;
	}
	return r;
}

const Character *GrumpaEngine::playerCharacter() {
	// ponytail: the player holder (actor 3) is not modelled; Grumpa (10) stands in when he is
	// in the scene (Q-0403).
	Character *g = _characters.find(10);
	return g && _characters.present(*g) ? g : nullptr;
}

void GrumpaEngine::drawItems(const Camera &cam, Common::Array<uint16> &depth) {
	Common::Array<Inventory::Item> &items = _inventory.items();
	for (uint i = 0; i < items.size(); i++) {
		Inventory::Item &it = items[i];
		if (!it.visible || it.scene != _sceneNum || it.state != Inventory::kInScene)
			continue;
		if (!it.loaded) {
			it.loaded = true;
			Common::String base = it.mesh;
			if (base.size() > 4 && base[base.size() - 4] == '.')
				base = Common::String(base.c_str(), base.size() - 4);
			if (!loadMesh(base, it.model))
				warning("Grumpa: item mesh %s not found", it.mesh.c_str());
			loadActorTexture(it.texture, it.skin, it.alpha);
		}
		if (it.model.empty())
			continue;
		Mesh placed;
		placeMesh(it.model, 0, it.rot, it.pos, placed);
		renderMesh(_screen, placed, cam, _sceneData.lights, depth, it.skin.getPixels() ? &it.skin : nullptr, it.alpha);
		it.rect = screenRect(placed, cam);
		if (it.glow < 0)
			continue;
		if (!_glowLoaded) {
			_glowLoaded = true;
			loadMesh("effect_item", _glowMesh);
			loadActorTexture("effect_item.tga", _glowSkin, _glowAlpha);
		}
		if (_glowMesh.empty())
			continue;
		placeMesh(_glowMesh, it.glow % MAX(_glowMesh.frames, 1), it.rot, it.pos, placed);
		Common::Array<uint16> noWrite = depth;  // drawn without z writes
		renderMesh(_screen, placed, cam, _sceneData.lights, noWrite,
				   _glowSkin.getPixels() ? &_glowSkin : nullptr, _glowAlpha);
	}
}

} // End of namespace Grumpa
