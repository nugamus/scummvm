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
#include "common/system.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"
#include "graphics/cursorman.h"
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

// Load the game's default pointer (UI/002_Cursor/default_0000.jpg, a white glove on black,
// E-0005) as the system cursor, with near-black pixels made transparent (the JPEG has no
// alpha, so the original keys out black).
void GrumpaEngine::setGameCursor() {
	Common::File f;
	if (!f.open(Common::Path("UI/002_Cursor/default_0000.jpg")))
		return;
	Image::JPEGDecoder jpeg;
	Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 0, 8, 16, 24);
	jpeg.setOutputPixelFormat(rgba);
	if (!jpeg.loadStream(f))
		return;
	const Graphics::Surface *src = jpeg.getSurface();
	Graphics::Surface cur;
	cur.create(src->w, src->h, rgba);
	for (int y = 0; y < src->h; y++) {
		for (int x = 0; x < src->w; x++) {
			byte r, g, b, a;
			rgba.colorToARGB(src->getPixel(x, y), a, r, g, b);
			a = (MAX(r, MAX(g, b)) < 24) ? 0 : 255;  // key out near-black
			cur.setPixel(x, y, rgba.ARGBToColor(a, r, g, b));
		}
	}
	CursorMan.replaceCursor(cur, 2, 2, 0, nullptr);
	CursorMan.showMouse(true);
	cur.free();
}

// ---- .abi scene-graph reader (docs/spec/scene.md; mirrors tools/parsers/abi.py) ----------
//
// A Scene_<NNN>.abi is a flat stream of records `u32 type, u32 id, Serialize(mode 1)` read to
// EOF (E-0100, E-0104). We walk every record by its byte-length grammar and collect the
// type-0x11 views (each carrying a camera block). The grammar below is abi.py's, one skip
// function per type. On any over-read the cursor goes bad and the walk stops.

namespace {

struct AbiCur {
	const byte *d;
	uint32 n, o;
	bool ok;
	AbiCur(const byte *d_, uint32 n_) : d(d_), n(n_), o(0), ok(true) {}
	uint32 left() const { return o <= n ? n - o : 0; }
	void skip(uint32 k) { if (!ok || o + k > n) { ok = false; return; } o += k; }
	uint32 u32() { if (!ok || o + 4 > n) { ok = false; return 0; } uint32 v = READ_LE_UINT32(d + o); o += 4; return v; }
	int32 i32() { return (int32)u32(); }
};

static int32 acount(AbiCur &c) {               // bounded count (abi.py _count)
	int32 v = c.i32();
	if (v < 0 || v > 0x100000) { c.ok = false; return 0; }
	return v;
}
static void ec(AbiCur &c) { c.skip(20); }
static void cc(AbiCur &c) { c.skip(20); int32 k = acount(c); for (int32 i = 0; i < k && c.ok; i++) ec(c); }
static void ecVec(AbiCur &c) { int32 k = acount(c); for (int32 i = 0; i < k && c.ok; i++) ec(c); }
static void ccVec(AbiCur &c) { int32 k = acount(c); for (int32 i = 0; i < k && c.ok; i++) cc(c); }
static void pstr(AbiCur &c) { int32 k = c.i32(); if (k > 0) c.skip(k); }
static void sub56(AbiCur &c) { c.skip(56); }
static void sub24(AbiCur &c) { c.skip(24); }
static void pairVec(AbiCur &c) { int32 k = acount(c); c.skip(8 * (uint32)k); }

// Skip a record body by type; returns false for an unmodelled type (ends the walk).
static bool skipBody(AbiCur &c, uint32 t) {
	switch (t) {
	case 0x05: c.skip(16); pstr(c); pstr(c); pstr(c); pstr(c); c.skip(40); pairVec(c); return true;
	case 0x07: c.skip(12); ecVec(c); c.skip(4); pstr(c); c.skip(4); c.skip(76); ccVec(c); return true;
	case 0x0d: {
		c.skip(12); ecVec(c); c.skip(32); c.skip(4);
		uint32 gate = c.u32();
		ccVec(c); sub56(c);
		if (gate == 1) c.skip(8);
		ccVec(c); ccVec(c); pstr(c);
		return true;
	}
	case 0x11: c.skip(12); ecVec(c); c.skip(8); ccVec(c); ccVec(c); c.skip(4); c.skip(0x68); return true;
	case 0x18:
	case 0x2a: c.skip(12); ecVec(c); c.skip(40); sub56(c); pstr(c); ccVec(c); return true;
	case 0x19: {
		c.skip(12); ecVec(c); c.skip(32); c.skip(76); sub56(c); ecVec(c); ccVec(c);
		int32 k = c.i32();
		if (k > 0) c.skip(8 * (uint32)k);
		c.skip(16); c.skip(4);
		uint32 mode = c.u32();
		if (mode == 2) {
			int32 nb = acount(c);
			for (int32 i = 0; i < nb && c.ok; i++) {
				c.skip(8);
				if (c.u32() != 0) pstr(c);
			}
		}
		return true;
	}
	case 0x1a: {
		c.skip(12); ecVec(c); c.skip(72); sub56(c); sub24(c); sub24(c); pairVec(c);
		for (int i = 0; i < 8 && c.ok; i++) ccVec(c);
		pstr(c); pstr(c);
		return true;
	}
	case 0x1d: {
		c.skip(12); ecVec(c); c.skip(8);
		int32 k = acount(c);
		for (int32 i = 0; i < k && c.ok; i++) { sub24(c); c.skip(4 * (uint32)acount(c)); }
		return true;
	}
	case 0x1e: c.skip(12); ecVec(c); c.skip(24); return true;
	case 0x20: c.skip(12); ecVec(c); c.skip(80); return true;
	case 0x21: c.skip(12); ecVec(c); c.skip(4); ecVec(c); ccVec(c); return true;
	case 0x22:
	case 0x25: c.skip(12); ecVec(c); c.skip(8); ccVec(c); return true;
	case 0x23:
	case 0x26: c.skip(12); ecVec(c); c.skip(12); ccVec(c); return true;
	case 0x24:
	case 0x27: c.skip(12); ecVec(c); c.skip(4); ccVec(c); return true;
	default: return false;
	}
}

} // anonymous namespace

Camera cameraFromBlock(const float cam[26]) {
	Camera c;
	c.eye = Vec3(cam[13], cam[14], cam[15]);
	// The view has no stored orientation; the camera looks at the scene target (~origin),
	// confirmed by projecting the 0x1a meshes onto the matching background (E-0115). forward =
	// normalize(target - eye); use a world up of +Y, except when looking near-vertical (a
	// top-down view), where +Y is parallel to forward, so use +Z instead.
	float len = sqrtf(c.eye.dot(c.eye));
	c.forward = len > 1e-3f ? Vec3(-c.eye.x / len, -c.eye.y / len, -c.eye.z / len)
							: Vec3(0.0f, 0.0f, -1.0f);
	c.up = (c.forward.y > 0.99f || c.forward.y < -0.99f) ? Vec3(0.0f, 0.0f, 1.0f)
														 : Vec3(0.0f, 1.0f, 0.0f);
	c.projX = cam[2] != 0.0f ? cam[2] : 1.0f;
	c.projY = cam[3] != 0.0f ? cam[3] : 1.0f;
	c.farZ = cam[19];
	return c;
}

bool GrumpaEngine::loadScene(int num, SceneData &scene) {
	Common::String name = Common::String::format("Scenes/Scene_%03d.abi", num);
	Common::File f;
	if (!f.open(Common::Path(name))) {
		debug(1, "Grumpa: scene %s not found", name.c_str());
		return false;
	}
	Common::Array<byte> buf(f.size());
	f.read(buf.begin(), buf.size());
	AbiCur c(buf.begin(), buf.size());
	if (c.left() >= 4 && READ_LE_UINT32(buf.begin()) == 0x03)
		return false;  // a CFXCharacter database, not a scene (Q-0006)

	while (c.left() >= 8 && c.ok) {
		uint32 t = c.u32();
		uint32 id = c.u32();
		if (t == 0x11) {
			c.skip(12); ecVec(c); c.skip(8); ccVec(c); ccVec(c);
			SceneView v;
			v.id = id;
			v.camId = c.u32();
			for (int i = 0; i < 26; i++)
				v.cam[i] = READ_LE_FLOAT(c.d + c.o + i * 4);
			c.skip(0x68);
			if (c.ok)
				scene.views.push_back(v);
		} else if (t == 0x0d) {
			// Sprite prop (E-0107): header flags, anim block, gate, then position when gated.
			SceneSprite sp;
			sp.id = id;
			c.skip(4);                      // +0x108 (parent/link)
			sp.active = c.u32() != 0;       // +0x10c
			sp.visible = c.u32() != 0;      // +0x110
			ecVec(c);
			c.skip(8);               // +0x114,+0x314
			sp.frames = (int)c.u32();  // +0x1e0
			sp.fps = (int)c.u32();     // +0x1e4
			c.skip(16);              // +0x1c8,+0x1f8,+0x208,+0x48c (rest of the 8-u32 block)
			c.skip(4);               // +0x1d4
			uint32 gate = c.u32();   // +0x20c
			ccVec(c);
			sub56(c);
			if (gate == 1) {
				sp.x = (int)c.u32();  // +0x190
				sp.y = (int)c.u32();  // +0x194
			}
			ccVec(c); ccVec(c);
			// trailing pstr: the JPG frame-base name
			int32 n = c.i32();
			if (n > 0) {
				sp.name = Common::String((const char *)(c.d + c.o), n);
				c.skip(n);
			}
			if (c.ok)
				scene.sprites.push_back(sp);
		} else if (t == 0x19) {
			// Trigger (E-0108/E-0109): capture its command list (CC) and clickable polygon.
			SceneTrigger tr;
			tr.id = id;
			c.skip(12); ecVec(c); c.skip(32); c.skip(76); sub56(c); ecVec(c);
			int32 ncc = acount(c);
			for (int32 j = 0; j < ncc && c.ok; j++) {
				int32 base[5];
				for (int b = 0; b < 5; b++) base[b] = (int32)c.u32();
				int32 nsub = acount(c);           // nested EC guard conditions
				c.skip(20 * (uint32)nsub);
				SceneCommand cmd;
				cmd.when = base[0];               // base = (when, targetId, opcode, arg1, arg2)
				cmd.targetId = base[1];
				cmd.opcode = base[2];
				cmd.arg1 = base[3];
				cmd.arg2 = base[4];
				cmd.hasCond = nsub > 0;
				tr.cmds.push_back(cmd);
			}
			int32 k = c.i32();                     // polygon point count
			for (int32 j = 0; j < k && c.ok; j++) {
				float px = READ_LE_FLOAT(c.d + c.o); c.skip(4);
				float py = READ_LE_FLOAT(c.d + c.o); c.skip(4);
				tr.poly.push_back(Common::Point((int16)px, (int16)py));
			}
			c.skip(16); c.skip(4);
			uint32 mode = c.u32();
			if (mode == 2) {
				int32 nb = acount(c);
				for (int32 j = 0; j < nb && c.ok; j++) {
					c.skip(8);
					if (c.u32() != 0) { int32 n = c.i32(); if (n > 0) c.skip(n); }
				}
			}
			if (c.ok)
				scene.triggers.push_back(tr);
		} else if (t == 0x1a) {
			// 3D animated-mesh actor (E-0114): flags, transform/command blocks, then the
			// .anb mesh and .tga texture names.
			SceneMesh m;
			m.id = id;
			c.skip(4);
			m.active = c.u32() != 0;        // +0x10c
			m.visible = c.u32() != 0;       // +0x110
			ecVec(c); c.skip(72); sub56(c); sub24(c); sub24(c); pairVec(c);
			for (int v = 0; v < 8 && c.ok; v++) ccVec(c);
			int32 na = c.i32();
			if (na > 0) { m.anb = Common::String((const char *)(c.d + c.o), na); c.skip(na); }
			int32 nt = c.i32();
			if (nt > 0) { m.tga = Common::String((const char *)(c.d + c.o), nt); c.skip(nt); }
			if (c.ok)
				scene.meshes.push_back(m);
		} else if (!skipBody(c, t)) {
			warning("Grumpa: scene %s unmodelled type %#x at %#x", name.c_str(), t, c.o - 8);
			return false;
		}
	}
	debug(1, "Grumpa: scene %s -> %u views, %u sprites, %u triggers", name.c_str(),
		  (uint)scene.views.size(), (uint)scene.sprites.size(), (uint)scene.triggers.size());
	return c.ok;
}

// The file name of a sprite's frame: a name ending in "_<4 digits>.<ext>" animates by
// substituting the frame number; any other name is static (drawn as-is). E-0107.
static Common::String spriteFrameName(const Common::String &base, int frame) {
	int dot = -1;
	for (int i = (int)base.size() - 1; i >= 0; i--)
		if (base[i] == '.') { dot = i; break; }
	if (dot >= 5 && base[dot - 5] == '_') {
		bool digits = true;
		for (int i = dot - 4; i < dot; i++)
			if (base[i] < '0' || base[i] > '9') { digits = false; break; }
		if (digits)
			return Common::String(base.c_str(), dot - 4)
				+ Common::String::format("%04d", frame) + (base.c_str() + dot);
	}
	return base;
}

// Draw a sprite prop's frame `frame` at (x, y) with the blue colour key (E-0107): pixels near
// pure blue (high B, low R/G) are transparent. flag0==0 sprites (smoke etc.) use a different
// blend in the original (Q-0009); for now they key the same way.
void GrumpaEngine::drawSprite(const SceneSprite &sprite, int frame) {
	if (sprite.name.empty())
		return;
	Common::String file = spriteFrameName(sprite.name, frame);
	Common::File f;
	if (!f.open(Common::Path("Bitmaps/" + file))) {
		debug(2, "Grumpa: sprite %s not found", file.c_str());
		return;
	}
	Image::JPEGDecoder jpeg;
	jpeg.setOutputPixelFormat(_screen.format);
	if (!jpeg.loadStream(f))
		return;
	const Graphics::Surface *s = jpeg.getSurface();
	for (int yy = 0; yy < s->h; yy++) {
		int dy = sprite.y + yy;
		if (dy < 0 || dy >= kScreenHeight)
			continue;
		for (int xx = 0; xx < s->w; xx++) {
			int dx = sprite.x + xx;
			if (dx < 0 || dx >= kScreenWidth)
				continue;
			byte r, g, b;
			_screen.format.colorToRGB(s->getPixel(xx, yy), r, g, b);
			if (b > 200 && r < 96 && g < 96)  // blue colour key
				continue;
			_screen.setPixel(dx, dy, s->getPixel(xx, yy));
		}
	}
}

// Ray-cast point-in-polygon for a trigger's clickable region (E-0108).
static bool pointInPoly(const Common::Array<Common::Point> &poly, const Common::Point &p) {
	bool in = false;
	for (uint i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
		if ((poly[i].y > p.y) != (poly[j].y > p.y) &&
			p.x < (int)((int64)(poly[j].x - poly[i].x) * (p.y - poly[i].y) /
						(poly[j].y - poly[i].y) + poly[i].x))
			in = !in;
	}
	return in;
}

// Apply one command to its target actor (by id), per the opcode vocabulary (E-0111). Only
// sprites and triggers are modelled so far; commands to other actor ids are ignored.
static void applyCommand(SceneData &scene, const SceneCommand &cmd) {
	for (uint i = 0; i < scene.sprites.size(); i++) {
		SceneSprite &s = scene.sprites[i];
		if ((int)s.id != cmd.targetId)
			continue;
		debug(2, "Grumpa:   cmd target=%d op=%d -> sprite %s (vis %d act %d)",
			  cmd.targetId, cmd.opcode, s.name.c_str(), s.visible, s.active);
		switch (cmd.opcode) {
		case 0:   s.active = true; break;               // play
		case 1:   s.active = false; break;              // stop
		case 2:   s.visible = true; break;              // show
		case 3:   s.visible = false; break;             // hide
		case 11:  s.active = true; break;               // activate
		case 12:  s.active = false; break;              // deactivate
		case 13:  s.active = s.visible = false; break;  // disable
		case 500: s.active = s.visible = true; break;   // full on
		case 501: s.active = s.visible = false; break;  // full off
		default: break;
		}
		return;
	}
	for (uint i = 0; i < scene.triggers.size(); i++) {
		SceneTrigger &tr = scene.triggers[i];
		if ((int)tr.id != cmd.targetId)
			continue;
		if (cmd.opcode == 13)       // disable (one-shot)
			tr.spent = true;
		else if (cmd.opcode == 11 || cmd.opcode == 500 || cmd.opcode == 52)
			tr.spent = false;       // (re-)enable
		return;
	}
}

bool GrumpaEngine::handleSceneClick(const Common::Point &p) {
	for (uint i = 0; i < _sceneData.triggers.size(); i++) {
		SceneTrigger &tr = _sceneData.triggers[i];
		if (tr.spent || tr.poly.size() < 3 || !pointInPoly(tr.poly, p))
			continue;
		debug(1, "Grumpa: trigger %u fired (%u commands)", tr.id, (uint)tr.cmds.size());
		// Apply only the immediate, unconditional commands for now; timed (when>=0) and
		// condition-guarded commands need the tick queue and variable actors (Q-0010).
		for (uint j = 0; j < tr.cmds.size(); j++) {
			const SceneCommand &cmd = tr.cmds[j];
			if (cmd.when != -1 || cmd.hasCond)
				continue;
			// Navigation: a command to the scene manager (id 185) opcode 31 goes to the scene
			// in arg1 (E-0116); the main loop performs the load.
			if (cmd.targetId == 185 && cmd.opcode == 31) {
				_nextScene = cmd.arg1;
				continue;
			}
			applyCommand(_sceneData, cmd);
		}
		return true;
	}
	return false;
}

// Enter scene <num>: read its graph (views + sprites) and decode its background view
// "<num>_1" into _sceneBg (cached, so animation only re-decodes the small sprites).
bool GrumpaEngine::enterScene(int num) {
	_sceneData = SceneData();
	loadScene(num, _sceneData);
	Common::String bg = Common::String::format("%d_1", num);
	_sceneBg.create(kScreenWidth, kScreenHeight, _screen.format);
	_sceneBg.clear();
	Common::File f;
	if (f.open(Common::Path("Bitmaps/" + bg + "_IS.jpg"))) {
		Image::JPEGDecoder jpeg;
		jpeg.setOutputPixelFormat(_screen.format);
		if (jpeg.loadStream(f)) {
			const Graphics::Surface *s = jpeg.getSurface();
			_sceneBg.blitFrom(*s, Common::Point((kScreenWidth - s->w) / 2, (kScreenHeight - s->h) / 2));
		}
	}
	// The scene depth buffer (for occluding the 3D mesh actors), and the mesh geometry.
	_sceneDepth.clear();
	_depthW = _depthH = 0;
	loadDepth(bg, _sceneDepth, _depthW, _depthH);
	for (uint i = 0; i < _sceneData.meshes.size(); i++) {
		Common::String base = _sceneData.meshes[i].anb;
		if (base.size() > 4 && base[base.size() - 4] == '.')  // strip ".anb"/".ANB"
			base = Common::String(base.c_str(), base.size() - 4);
		loadMesh(base, _sceneData.meshes[i].mesh);
	}
	_sceneTick0 = g_system->getMillis();
	debug(1, "Grumpa: entered scene %d (%u sprites, %u meshes)", num,
		  (uint)_sceneData.sprites.size(), (uint)_sceneData.meshes.size());
	return true;
}

// Redraw the current scene: the cached background, then each sprite's current animation frame
// (frame = elapsed * fps / 1000, wrapped to the frame count). E-0107.
void GrumpaEngine::renderSceneFrame(uint32 now) {
	_screen.blitFrom(_sceneBg);
	uint32 elapsed = now - _sceneTick0;
	for (uint i = 0; i < _sceneData.sprites.size(); i++) {
		const SceneSprite &sp = _sceneData.sprites[i];
		if (!sp.visible)  // hidden until a command shows it (E-0111)
			continue;
		int frame = 0;
		if (sp.active && sp.frames > 1 && sp.fps > 0)  // only animate active sprites
			frame = (int)((elapsed * (uint32)sp.fps / 1000) % (uint32)sp.frames);
		drawSprite(sp, frame);
	}
	// Hotspot overlay (H): outline each unspent trigger's clickable polygon (E-0108), so the
	// exits and interactions are visible. A development/accessibility view.
	if (_showHotspots) {
		uint32 col = _screen.format.RGBToColor(0, 255, 0);
		for (uint i = 0; i < _sceneData.triggers.size(); i++) {
			const SceneTrigger &tr = _sceneData.triggers[i];
			if (tr.spent || tr.poly.size() < 2)
				continue;
			for (uint j = 0; j < tr.poly.size(); j++)
				_screen.drawLine(tr.poly[j].x, tr.poly[j].y,
								 tr.poly[(j + 1) % tr.poly.size()].x, tr.poly[(j + 1) % tr.poly.size()].y, col);
		}
	}
	// 3D animated-mesh actors (type 0x1a, E-0114) are loaded (meshes cached) but not yet drawn
	// in the live scene: each scene has several views at different eye positions, and the
	// view that matches the shown background ("<n>_1") is not yet mapped (Scene_061 has 4
	// views near/far the geometry; picking views[0] renders them off-screen). Once the
	// view->background mapping is known, draw the active meshes through that view's camera with
	// depth occlusion (verified via the dev harness). See docs/spec/scene.md.
}

} // End of namespace Grumpa
