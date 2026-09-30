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
#include "common/stream.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"
#include "image/jpeg.h"

#include "grumpa/grumpa.h"

static inline float READ_LE_FLOAT(const byte *p) {
	uint32 v = READ_LE_UINT32(p);
	float f;
	memcpy(&f, &v, 4);
	return f;
}

namespace Grumpa {

// A scene view is a pre-rendered 800x600 colour background (<name>_IS.jpg) plus a 16-bit
// depth buffer (<name>_IZ.fxi) (games/grumpa docs, E-0010, E-0011). This draws the
// background; the depth is decoded for later actor compositing.

bool GrumpaEngine::loadBackground(const Common::String &view) {
	Common::File f;
	Common::String name = "Bitmaps/" + view + "_IS.jpg";
	if (!f.open(Common::Path(name))) {
		debug(1, "Grumpa: background %s not found", name.c_str());
		return false;
	}
	Image::JPEGDecoder jpeg;
	jpeg.setOutputPixelFormat(_screen.format);
	if (!jpeg.loadStream(f)) {
		warning("Grumpa: could not decode %s", name.c_str());
		return false;
	}
	const Graphics::Surface *s = jpeg.getSurface();
	_screen.blitFrom(*s, Common::Point((kScreenWidth - s->w) / 2, (kScreenHeight - s->h) / 2));
	debug(1, "Grumpa: background %s (%dx%d)", name.c_str(), s->w, s->h);
	return true;
}

// The .fxi codec (E-0009): 18-byte header (u8 ver, u8 flag, u16 w, u16 h, u32 x3), then a
// 16-bit surface. flag 0: raw w*h pixels. flag != 0: one control byte per 8x8 block; the
// low nibble codes the high byte of the block's pixels, the high nibble the low byte; each
// plane is mode 0 solid / 1 1bpp+2 vals / 2 2bpp+4 vals / 3 raw 64.
static uint decodePlane(const byte *d, uint off, int mode, byte plane[64]) {
	switch (mode) {
	case 0:
		for (int i = 0; i < 64; i++)
			plane[i] = d[off];
		return off + 1;
	case 1: {
		const byte *mask = d + off, v0 = d[off + 8], v1 = d[off + 9];
		for (int row = 0; row < 8; row++)
			for (int col = 0; col < 8; col++)
				plane[row * 8 + col] = (mask[row] & (1 << col)) ? v1 : v0;
		return off + 10;
	}
	case 2: {
		const byte *mask = d + off, *vals = d + off + 16;
		for (int row = 0; row < 8; row++) {
			byte b0 = mask[row * 2], b1 = mask[row * 2 + 1];
			for (int col = 0; col < 8; col++) {
				byte src = col < 4 ? b0 : b1;
				plane[row * 8 + col] = vals[(src >> ((col & 3) * 2)) & 3];
			}
		}
		return off + 20;
	}
	default:
		for (int i = 0; i < 64; i++)
			plane[i] = d[off + i];
		return off + 64;
	}
}

bool GrumpaEngine::loadDepth(const Common::String &view, Common::Array<uint16> &depth, int &w, int &h) {
	Common::File f;
	Common::String name = "Bitmaps/" + view + "_IZ.fxi";
	if (!f.open(Common::Path(name)))
		return false;
	Common::Array<byte> buf(f.size());
	f.read(buf.begin(), buf.size());
	const byte *d = buf.begin();
	w = READ_LE_UINT16(d + 2);
	h = READ_LE_UINT16(d + 4);
	depth.resize(w * h);
	uint off = 18;
	byte flag = d[1];
	if (flag == 0) {
		for (int i = 0; i < w * h; i++)
			depth[i] = READ_LE_UINT16(d + off + i * 2);
		return true;
	}
	int bw = w / 8;
	uint ctrl = off;
	off += (w / 8) * (h / 8);
	for (int by = 0; by < h / 8; by++) {
		for (int bx = 0; bx < bw; bx++) {
			byte c = d[ctrl++];
			byte hi[64], lo[64];
			off = decodePlane(d, off, c & 0xF, hi);
			off = decodePlane(d, off, c >> 4, lo);
			for (int row = 0; row < 8; row++)
				for (int col = 0; col < 8; col++)
					depth[(by * 8 + row) * w + bx * 8 + col] = (hi[row * 8 + col] << 8) | lo[row * 8 + col];
		}
	}
	return true;
}

// Load an actor mesh: Meshes/<name>.anb, frame-0 geometry (E-0014). We keep only the static
// pose (vertices, faces, UVs); the animation frames after it are ignored for now.
bool GrumpaEngine::loadMesh(const Common::String &name, Mesh &mesh) {
	Common::File f;
	if (!f.open(Common::Path("Meshes/" + name + ".anb")))
		return false;
	Common::Array<byte> buf(f.size());
	f.read(buf.begin(), buf.size());
	const byte *d = buf.begin();
	uint size = buf.size();
	if (size < 8)
		return false;
	mesh.frames = READ_LE_UINT32(d);
	uint nsec = READ_LE_UINT32(d + 4);
	uint off = 8;
	for (uint si = 0; si < nsec; si++) {
		if (off + 12 > size)
			return false;
		uint a = READ_LE_UINT32(d + off), b = READ_LE_UINT32(d + off + 4), c = READ_LE_UINT32(d + off + 8);
		off += 12;
		MeshSection sec;
		for (uint i = 0; i < a; i++, off += 24) {
			sec.verts.push_back(Vec3(READ_LE_FLOAT(d + off), READ_LE_FLOAT(d + off + 4), READ_LE_FLOAT(d + off + 8)));
			sec.normals.push_back(Vec3(READ_LE_FLOAT(d + off + 20), READ_LE_FLOAT(d + off + 16), READ_LE_FLOAT(d + off + 12)));
		}
		Common::Array<Face> faces;
		faces.resize(c);
		for (uint i = 0; i < c; i++, off += 6)
			for (int k = 0; k < 3; k++)
				faces[i].v[k] = READ_LE_UINT16(d + off + k * 2);
		for (uint i = 0; i < b; i++, off += 8) {
			sec.u.push_back(READ_LE_FLOAT(d + off));
			sec.v.push_back(READ_LE_FLOAT(d + off + 4));
		}
		for (uint i = 0; i < c; i++, off += 6)
			for (int k = 0; k < 3; k++)
				faces[i].uv[k] = READ_LE_UINT16(d + off + k * 2);
		sec.faces = faces;
		mesh.sections.push_back(sec);
	}
	return true;
}

} // End of namespace Grumpa
