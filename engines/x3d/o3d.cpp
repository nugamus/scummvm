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


#include "common/stream.h"
#include "common/textconsole.h"

#include "x3d/o3d.h"

namespace X3D {

static Common::String readName(Common::SeekableReadStream &s) {
	char buf[33];
	s.read(buf, 32);
	buf[32] = 0;
	return Common::String(buf);
}

static void readFloats(Common::SeekableReadStream &s, float *out, uint n) {
	for (uint i = 0; i < n; i++)
		out[i] = s.readFloatLE();
}

// dst = a * b, 4x4 row-major
static void mult(float *dst, const float *a, const float *b) {
	float r[16];
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			r[i * 4 + j] = a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 3] * b[12 + j];
	memcpy(dst, r, sizeof(r));
}

static void translation(float *m, float x, float y, float z) {
	static const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	memcpy(m, identity, sizeof(identity));
	m[12] = x;
	m[13] = y;
	m[14] = z;
}

bool O3DFile::load(Common::SeekableReadStream &s) {
	const Common::String signature = readName(s);
	if (signature != "(c) 1998 4X Tech. 0.95 (O)") {
		// 1.00 adds a LOD block per object; no file in the corpus uses it
		warning("O3D: unsupported signature '%s'", signature.c_str());
		return false;
	}

	materials.resize(s.readUint32LE());
	for (O3DMaterial &m : materials) {
		m.name = readName(s);
		s.readUint32LE(); // flags
		s.read(m.colors, sizeof(m.colors));
		s.skip(5 * 4); // unk
		if (s.readUint32LE()) {
			m.textureMap = readName(s);
			s.readUint32LE();
		}
		if (s.readUint32LE()) { // light map, unused in the corpus
			readName(s);
			s.readUint32LE();
		}
	}

	objects.resize(s.readUint32LE());
	for (uint i = 0; i < objects.size(); i++) {
		O3DObject &o = objects[i];
		o.name = readName(s);
		if (s.readUint32LE()) {
			// The parent is an object loaded earlier in this file
			const Common::String parent = readName(s);
			for (int j = i - 1; j >= 0 && o.parent < 0; j--)
				if (objects[j].name == parent)
					o.parent = j;
		}

		// Welded objects share their top object's vertex array (animation.md, E-0054)
		o.ownCount = s.readUint32LE();
		uint32 count = o.ownCount;
		o.welded = s.readUint32LE() != 0;
		if (o.welded) {
			o.weldFirst = s.readUint32LE();
			count = s.readUint32LE();
		}
		o.vertices.resize(count * 3);
		readFloats(s, o.vertices.data(), count * 3);
		s.skip(count * 3 * 4); // normals

		o.faces.resize(s.readUint32LE());
		for (O3DFace &f : o.faces) {
			f.indices.resize(s.readUint32LE());
			for (uint32 &index : f.indices)
				index = s.readUint32LE();
			if (s.readUint32LE()) {
				f.uvs.resize(f.indices.size() * 2);
				readFloats(s, f.uvs.data(), f.uvs.size());
			}
			f.material = s.readUint32LE();
			readFloats(s, f.normal, 3);
		}

		s.skip(10 * 4); // bounds
		s.skip(s.readUint32LE() * 4); // light indices
		s.readUint32LE();
		readFloats(s, o.pivot, 3);
		readFloats(s, o.localPosition, 3);
		readFloats(s, o.localScale, 3);
		readFloats(s, o.matrix, 16);

		for (const O3DFace &f : o.faces)
			for (uint32 index : f.indices)
				if (f.material >= materials.size() || (count && index >= count))
					error("O3D: face out of range in '%s'", o.name.c_str());
	}

	updateWorld();
	return !s.err() && s.pos() == s.size();
}

void O3DFile::updateWorld() {
	// Parents come before their children in the file
	for (O3DObject &o : objects) {
		// Tr(-pivot) * diag(scale) * M * [Tr(parent pivot)] * Tr(position) * [parent world]
		float m[16], t[16];
		translation(m, -o.pivot[0], -o.pivot[1], -o.pivot[2]);
		for (int k = 0; k < 4; k++)
			for (int j = 0; j < 3; j++)
				m[k * 4 + j] *= o.localScale[j];
		mult(m, m, o.matrix);
		float px = o.localPosition[0], py = o.localPosition[1], pz = o.localPosition[2];
		if (o.parent >= 0) {
			const O3DObject &p = objects[o.parent];
			px += p.pivot[0];
			py += p.pivot[1];
			pz += p.pivot[2];
		}
		translation(t, px, py, pz);
		mult(m, m, t);
		if (o.parent >= 0)
			mult(m, m, objects[o.parent].world);
		memcpy(o.world, m, sizeof(m));
	}
}

} // End of namespace X3D
