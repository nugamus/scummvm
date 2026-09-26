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

#include "peintre/detection.h"
#include "peintre/obj3d.h"

namespace Peintre {

// Sizes from the loader's relocation code (obj3d.ksy, E-0014..E-0016).
static const uint32 kVertexSize = 40;
static const uint32 kUvSize = 8;
static const uint32 kNormalSize = 16;
static const uint32 kMaterialSize = 44;

namespace {

/** Bounds-checked reads in an entry body (the entry minus its object header). */
class Body {
public:
	Body(const Common::Array<byte> &entry) : _p(entry.data() + kObjectHeaderSize),
		_size(entry.size() > kObjectHeaderSize ? entry.size() - kObjectHeaderSize : 0), _ok(true) {}

	int32 s32(uint32 off) {
		if (off > _size || _size - off < 4) {
			_ok = false;
			return 0;
		}
		return (int32)READ_LE_UINT32(_p + off);
	}
	Vec3i vec(uint32 off) {
		Vec3i v = { s32(off), s32(off + 4), s32(off + 8) };
		return v;
	}
	Common::String str(uint32 off, uint32 len) {
		if (off > _size || _size - off < len) {
			_ok = false;
			return Common::String();
		}
		const char *s = (const char *)_p + off;
		uint32 n = 0;
		while (n < len && s[n])
			n++;
		return Common::String(s, n);
	}
	bool ok() const { return _ok; }
	uint32 size() const { return _size; }
	const byte *data() const { return _p; }

private:
	const byte *_p;
	uint32 _size;
	bool _ok;
};

int32 indexOf(uint32 ptr, uint32 base, uint32 stride, int32 count) {
	if (ptr < base || (ptr - base) % stride || (int32)((ptr - base) / stride) >= count)
		return -1;
	return (ptr - base) / stride;
}

} // End of anonymous namespace

bool Scene3D::load(const Common::Array<byte> &entry) {
	nodes.clear();
	materials.clear();
	Body b(entry);
	const int32 n = b.s32(0);
	if (n <= 0 || (uint32)n > b.size() / 4)
		return false;
	// The node table: body offsets of every node, the root first.
	Common::Array<uint32> offsets;
	for (int32 i = 0; i < n; i++)
		offsets.push_back(b.s32(4 + 4 * i));
	const int32 nmat = b.s32(4 + 4 * n);
	const uint32 matOff = 8 + 4 * n;
	for (int32 i = 0; i < nmat && b.ok(); i++) {
		Material m;
		m.name = b.str(matOff + kMaterialSize * i, 16);
		m.texture = b.str(matOff + kMaterialSize * i + 16, 16);
		materials.push_back(m);
	}
	// Tree pointers are 1-based offsets from the root node, 0 = none.
	const uint32 root = offsets[0];
	auto tree = [root](int32 v) -> uint32 { return root + v - 1; };

	nodes.resize(n);
	for (int32 i = 0; i < n && b.ok(); i++) {
		Node &nd = nodes[i];
		const uint32 o = offsets[i];
		nd.name = b.str(o, 12);
		nd.flags = b.s32(o + 0x0C);
		nd.parent = -1;
		nd.position = b.vec(o + 0x1C);
		for (int k = 0; k < 9; k++)
			nd.rotation[k] = b.s32(o + 0x28 + 4 * k);
		const int32 nv = b.s32(o + 0x7C), pv = b.s32(o + 0x80);
		const int32 nuv = b.s32(o + 0x84), puv = b.s32(o + 0x88);
		const int32 nvn = b.s32(o + 0x8C), pvn = b.s32(o + 0x90);
		const int32 nfn = b.s32(o + 0x94), pfn = b.s32(o + 0x98);
		for (int32 k = 0; k < nv && b.ok(); k++) {
			nd.vertexFlags.push_back(b.s32(tree(pv) + kVertexSize * k));
			nd.vertices.push_back(b.vec(tree(pv) + kVertexSize * k + 4));
		}
		for (int32 k = 0; k < nuv && b.ok(); k++) {
			nd.uvs.push_back(b.s32(tree(puv) + kUvSize * k));
			nd.uvs.push_back(b.s32(tree(puv) + kUvSize * k + 4));
		}
		for (int32 k = 0; k < nvn && b.ok(); k++)
			nd.vertexNormals.push_back(b.vec(tree(pvn) + kNormalSize * k));
		for (int32 k = 0; k < nfn && b.ok(); k++)
			nd.faceNormals.push_back(b.vec(tree(pfn) + kNormalSize * k));

		// Face groups: a linked list from +0xA4; polys are 0x44 bytes with UVs, else 0x38.
		for (int32 g = b.s32(o + 0xA4); g && b.ok(); ) {
			const uint32 go = tree(g);
			FaceGroup fg;
			fg.type = b.s32(go + 4);
			fg.material = b.str(go + 0x0C, 16);
			fg.materialIndex = -1;
			for (uint m = 0; m < materials.size(); m++) {
				if (materials[m].name == fg.material) {
					fg.materialIndex = m;
					break;
				}
			}
			const int32 count = b.s32(go + 0x1C);
			const uint32 polys = tree(b.s32(go + 0x20));
			const uint32 stride = b.s32(go + 0x2C);
			const bool textured = stride == 0x44;
			for (int32 k = 0; k < count && b.ok(); k++) {
				const uint32 po = polys + stride * k;
				Poly p;
				for (int c = 0; c < 3; c++) {
					p.vertex[c] = indexOf(tree(b.s32(po + 8 + 12 * c)), tree(pv), kVertexSize, nv);
					const int32 vn = b.s32(po + 12 + 12 * c);
					p.normal[c] = vn ? indexOf(tree(vn), tree(pvn), kNormalSize, nvn) : -1;
					const int32 uv = textured ? b.s32(po + 0x34 + 4 * c) : 0;
					p.uv[c] = uv ? indexOf(tree(uv), tree(puv), kUvSize, nuv) : -1;
				}
				const int32 fn = b.s32(po + 0x2C);
				p.faceNormal = fn ? indexOf(tree(fn), tree(pfn), kNormalSize, nfn) : -1;
				p.unk30 = b.s32(po + 0x30);
				fg.polys.push_back(p);
			}
			nd.faceGroups.push_back(fg);
			g = b.s32(go);
		}
		// Children: the first child at +0x14, then its siblings through +0x18.
		for (int32 c = b.s32(o + 0x14); c && b.ok(); c = b.s32(tree(c) + 0x18)) {
			for (int32 k = 0; k < n; k++) {
				if (offsets[k] == tree(c)) {
					nd.children.push_back(k);
					break;
				}
			}
		}
	}
	for (int32 i = 0; i < n; i++)
		for (int c : nodes[i].children)
			nodes[c].parent = i;
	if (!b.ok())
		warning("Scene3D: truncated or corrupt scene");
	debugC(1, kDebugLoad, "Scene3D: %d nodes, %d materials", n, nmat);
	return b.ok();
}

int Scene3D::findNode(const Common::String &name) const {
	for (uint i = 0; i < nodes.size(); i++)
		if (nodes[i].name.equalsIgnoreCase(name))
			return i;
	return -1;
}

bool Texture3D::load(const Common::Array<byte> &entry) {
	Body b(entry);
	if (b.size() < 32 * 256 * 4)
		return false;
	// RGB565 in the high half of each u32, level 0 brightest (E-0015).
	for (int l = 0; l < 32; l++)
		for (int c = 0; c < 256; c++)
			shades[l][c] = READ_LE_UINT32(b.data() + 4 * (l * 256 + c)) >> 16;
	texels.resize(256 * 256);
	const uint32 have = MIN<uint32>(b.size() - 0x8000, 256 * 256);
	memcpy(texels.data(), b.data() + 0x8000, have);
	if (have < 256 * 256)
		memset(texels.data() + have, 0, 256 * 256 - have); // three short textures (Q-0003)
	return true;
}

bool Anim3D::load(const Common::Array<byte> &entry) {
	tracks.clear();
	Body b(entry);
	const int32 n = b.s32(0);
	for (int32 i = 0; i < n && b.ok(); i++) {
		const uint32 t = b.s32(4 + 4 * i);
		AnimTrack tr;
		tr.unk0 = b.s32(t);
		const int32 nrot = b.s32(t + 4), npos = b.s32(t + 8);
		const uint32 prot = b.s32(t + 12), ppos = b.s32(t + 16);
		for (int32 k = 0; k < nrot && b.ok(); k++) {
			AnimTrack::RotKey key;
			key.time = b.s32(prot + 20 * k);
			for (int c = 0; c < 4; c++)
				key.q[c] = b.s32(prot + 20 * k + 4 + 4 * c);
			tr.rot.push_back(key);
		}
		for (int32 k = 0; k < npos && b.ok(); k++) {
			AnimTrack::PosKey key;
			key.time = b.s32(ppos + 16 * k);
			key.pos = b.vec(ppos + 16 * k + 4);
			tr.pos.push_back(key);
		}
		tracks.push_back(tr);
	}
	return b.ok();
}

bool Boxes3D::load(const Common::Array<byte> &entry) {
	vertices.clear();
	faces.clear();
	items.clear();
	Body b(entry);
	// Pointers are 1-based offsets from the body.
	const int32 nv = b.s32(0), pv = b.s32(4), nf = b.s32(8), pf = b.s32(12);
	const int32 ni = b.s32(16), pi = b.s32(20);
	vertexBase = pv - 1;
	itemBase = pi - 1;
	for (int32 k = 0; k < nv && b.ok(); k++)
		vertices.push_back(b.vec(pv - 1 + 12 * k));
	static const int kWords[5] = { 0, 1, 2, 3, 17 };
	for (int32 k = 0; k < nf && b.ok(); k++) {
		const uint32 o = pf - 1 + 0x60 * k;
		Face f;
		for (int w = 0; w < 5; w++) {
			const int32 v = b.s32(o + 4 * kWords[w]);
			f.ref[w] = v ? v - 1 : -1;
		}
		if (b.ok())
			memcpy(f.raw, b.data() + o, 0x60);
		faces.push_back(f);
	}
	for (int32 k = 0; k < ni && b.ok(); k++)
		items.push_back(b.vec(pi - 1 + 12 * k));
	return b.ok();
}

} // End of namespace Peintre
