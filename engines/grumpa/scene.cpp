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
#include "image/tga.h"

#include "grumpa/dialogue.h"
#include "grumpa/events.h"
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
	return loadFxi(view + "_IZ.fxi", depth, w, h);
}

bool GrumpaEngine::loadFxi(const Common::String &file, Common::Array<uint16> &depth, int &w, int &h) {
	Common::File f;
	Common::String name = "Bitmaps/" + file;
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

// Load an actor mesh: Meshes/<name>.anb (E-0014, E-0600): frame 0 inside the sections, then
// frames 1..F-1, each every section's vertices in turn; anything after them is never read.
// Each face corner is pointed at the vertex the original's vertex buffer keeps for the corner's
// uv index: the last corner, in face order, naming that uv index.
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
	mesh.frames = MAX<int>(READ_LE_UINT32(d), 1);
	uint nsec = READ_LE_UINT32(d + 4);
	uint off = 8;
	for (uint si = 0; si < nsec; si++) {
		if (off + 12 > size)
			return false;
		uint a = READ_LE_UINT32(d + off), b = READ_LE_UINT32(d + off + 4), c = READ_LE_UINT32(d + off + 8);
		off += 12;
		if (off + a * 24 + c * 12 + b * 8 > size)
			return false;
		MeshSection sec;
		sec.nv = a;
		sec.verts.resize(a * mesh.frames);
		sec.normals.resize(a * mesh.frames);
		for (uint i = 0; i < a; i++, off += 24) {
			sec.verts[i] = Vec3(READ_LE_FLOAT(d + off), READ_LE_FLOAT(d + off + 4), READ_LE_FLOAT(d + off + 8));
			sec.normals[i] = Vec3(READ_LE_FLOAT(d + off + 12), READ_LE_FLOAT(d + off + 16), READ_LE_FLOAT(d + off + 20));
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
		Common::Array<uint16> vertOfUv;
		vertOfUv.resize(b);
		for (uint i = 0; i < c; i++)
			for (int k = 0; k < 3; k++) {
				if (faces[i].uv[k] >= b || faces[i].v[k] >= a)
					return false;
				vertOfUv[faces[i].uv[k]] = faces[i].v[k];
			}
		for (uint i = 0; i < c; i++)
			for (int k = 0; k < 3; k++)
				faces[i].v[k] = vertOfUv[faces[i].uv[k]];
		sec.faces = faces;
		mesh.sections.push_back(sec);
	}
	for (int fr = 1; fr < mesh.frames; fr++) {
		for (uint si = 0; si < mesh.sections.size(); si++) {
			MeshSection &sec = mesh.sections[si];
			if (off + sec.nv * 24 > size) {
				mesh.frames = fr;  // a short file: keep the frames it has
				return true;
			}
			for (uint i = 0; i < sec.nv; i++, off += 24) {
				sec.verts[fr * sec.nv + i] = Vec3(READ_LE_FLOAT(d + off), READ_LE_FLOAT(d + off + 4), READ_LE_FLOAT(d + off + 8));
				sec.normals[fr * sec.nv + i] = Vec3(READ_LE_FLOAT(d + off + 12), READ_LE_FLOAT(d + off + 16), READ_LE_FLOAT(d + off + 20));
			}
		}
	}
	return true;
}

// The cursor's pictures (inventory.md "The cursor's picture", E-1720): UI/002_Cursor/
// <name>_0000.jpg (default, grabing, ...) or, for the walk arrows, arrow<n>_.tga; keyed on
// the colour of pixel (0, 0), drawn with the top-left corner at the mouse point. Cached by
// name so the per-update refresh is cheap. ponytail: frame 0 only (the timing is Q-1710).
void GrumpaEngine::setCursorImage(const Common::String &name) {
	if (name == _cursorName)
		return;
	Common::File f;
	const bool arrow = name.hasSuffix("_");
	if (!f.open(Common::Path("UI/002_Cursor/" + name + (arrow ? ".tga" : "_0000.jpg"))))
		return;
	Image::JPEGDecoder jpeg;
	Image::TGADecoder tga;
	Image::ImageDecoder *dec = arrow ? static_cast<Image::ImageDecoder *>(&tga) : &jpeg;
	if (!dec->loadStream(f) || !dec->getSurface())
		return;
	Graphics::Surface *cur = dec->getSurface()->convertTo(_screen.format, dec->getPalette().data(),
														  dec->getPalette().size());
	CursorMan.replaceCursor(*cur, 0, 0, cur->getPixel(0, 0));
	CursorMan.showMouse(true);
	cur->free();
	delete cur;
	_cursorName = name;
}

void GrumpaEngine::setGameCursor() {
	setCursorImage("default");
}

// The cursor's kind, refreshed every update and on mouse moves (E-1721): the held item's icon
// (0); over a clickable trigger or an item in reach the hand (2); with the panel shown the
// pointer (1); else the walk arrow actor 3 aims (9..24). ponytail: the kind is not kept: the
// original leaves the pointer after the panel closes until a drop or message 0x23 (E-1721),
// here the arrow is back at once; no 8-update hover hold, no hand over filled slots; Ctrl's
// attack cursor (7) comes with combat.
void GrumpaEngine::updateHoverCursor(const Common::Point &p) {
	const int held = _inventory.held();
	if (held >= 0) {
		const Common::String name = Common::String::format("*%d", held);
		if (name != _cursorName && _inventory.showHeldCursor())
			_cursorName = name;
		_cursorState = 0;
		return;
	}
	const bool hand = overHotspot(p);
	if (hand)
		setCursorImage("grabing");
	else if (_inventory.shown())
		setCursorImage("default");
	else
		setCursorImage(Common::String::format("arrow%d_", _arrowKind - 8));
	_cursorState = hand ? 2 : 1;
}

bool GrumpaEngine::overHotspot(const Common::Point &p) {
	for (uint i = 0; i < _sceneData.triggers.size(); i++) {
		const SceneTrigger &tr = _sceneData.triggers[i];
		if (_events->triggerClickable(tr) && tr.poly.size() >= 3 && pointInPolygon(tr.poly, p))
			return true;
	}
	return _inventory.itemAt(p);  // a hovered item takes the hotspot cursor too (E-0900)
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

// The event VM's reads (docs/spec/events.md): an EC vector as conditions or as state slots,
// a CC vector as commands with their conditions (E-0109, E-0201).
static void readConds(AbiCur &c, Common::Array<SceneCond> &out) {
	int32 k = acount(c);
	for (int32 i = 0; i < k && c.ok; i++) {
		SceneCond e;
		e.id = c.i32(); e.slot = c.i32(); e.value = c.i32(); e.mode = c.i32(); e.link = c.i32();
		out.push_back(e);
	}
}
// Every record's head after `type, id`: active (+0x10c), visible (+0x110), then its state
// slots as `n, n x u32` (the loader re-reads the id into +0x108; E-0400, E-0201).
static void readHead(AbiCur &c, bool &active, bool &visible, Common::Array<int32> *state = nullptr) {
	active = c.u32() != 0;
	visible = c.u32() != 0;
	int32 n = acount(c);
	for (int32 i = 0; i < n && c.ok; i++) {
		int32 v = c.i32();
		if (state)
			state->push_back(v);
	}
	if (state && state->empty())
		state->push_back(0);   // the "State" slot every actor has
}
static void readCmds(AbiCur &c, CommandList &out) {
	int32 k = acount(c);
	for (int32 i = 0; i < k && c.ok; i++) {
		SceneCommand cmd;
		cmd.when = c.i32(); cmd.targetId = c.i32(); cmd.opcode = c.i32();
		cmd.arg1 = c.i32(); cmd.arg2 = c.i32();
		readConds(c, cmd.conds);
		out.push_back(cmd);
	}
}
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

// The scene's views live in its .scn (docs/spec/scene.md, E-0301): the last record is the
// CFXView, `u32 9, u32 id, u32 n, n x (pstr colour, pstr depth), n view matrices, n projection
// matrices` (16 floats each), ending the file in every scene. Found by its header from the end.
static bool readSceneViews(const Common::Array<byte> &buf, Common::Array<SceneView> &views) {
	const byte *d = buf.begin();
	const uint32 size = buf.size();
	for (int32 o = (int32)size - 12; o >= 0; o--) {
		if (READ_LE_UINT32(d + o) != 9)
			continue;
		uint32 n = READ_LE_UINT32(d + o + 8);
		if (n < 1 || n > 5)
			continue;
		uint32 p = o + 12;
		Common::Array<SceneView> vs;
		vs.resize(n);
		bool ok = true;
		for (uint32 k = 0; k < n && ok; k++) {
			for (int j = 0; j < 2 && ok; j++) {
				if (p + 4 > size) { ok = false; break; }
				uint32 len = READ_LE_UINT32(d + p);
				if (len == 0 || len > 64 || p + 4 + len > size) { ok = false; break; }
				Common::String str((const char *)d + p + 4, len);
				str = Common::String(str.c_str());  // drop the terminating NUL
				(j ? vs[k].depth : vs[k].colour) = str;
				p += 4 + len;
			}
		}
		if (!ok || p + n * 128 != size)
			continue;
		for (uint32 k = 0; k < n; k++) {
			for (int i = 0; i < 16; i++) {
				vs[k].cam.view[i] = READ_LE_FLOAT(d + p + k * 64 + i * 4);
				vs[k].cam.proj[i] = READ_LE_FLOAT(d + p + (n + k) * 64 + i * 4);
			}
		}
		views = vs;
		return true;
	}
	return false;
}

bool GrumpaEngine::loadSceneViews(int num, SceneData &scene) {
	Common::File f;
	if (!f.open(Common::Path(Common::String::format("Scenes/Scene_%03d.scn", num))))
		return false;
	Common::Array<byte> buf(f.size());
	f.read(buf.begin(), buf.size());
	return readSceneViews(buf, scene.views);
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
			// CFXLight (E-0300): its last 0x68 bytes are a D3DLIGHT7, set and enabled at load.
			c.skip(12); ecVec(c); c.skip(8); ccVec(c); ccVec(c);
			c.skip(4);
			SceneLight l;
			l.id = id;
			for (int i = 0; i < 26; i++)
				l.d[i] = READ_LE_FLOAT(c.d + c.o + i * 4);
			c.skip(0x68);
			if (c.ok)
				scene.lights.push_back(l);
		} else if (t == 0x0d) {
			// Sprite prop (E-0107): header flags, anim block, gate, then position when gated.
			SceneSprite sp;
			sp.id = id;
			readHead(c, sp.active, sp.visible);
			sp.layer = c.i32();       // +0x114
			c.skip(4);                // +0x314
			sp.fps = c.i32();         // +0x1e0
			sp.anim = c.u32();        // +0x1e4
			sp.view = c.i32();        // +0x1c8
			sp.keyed = c.u32() == 1;  // +0x1f8
			sp.keyColor = c.i32();    // +0x208
			c.skip(4);
			SpriteHooks hk;           // the event VM's part (E-0208)
			hk.id = id;
			hk.autoplay = c.u32() == 1;  // +0x1d4
			uint32 gate = c.u32();   // +0x20c
			readCmds(c, hk.onEnd);   // +0x14c
			sub56(c);
			if (gate == 1) {
				sp.x = (int)c.u32();  // +0x190
				sp.y = (int)c.u32();  // +0x194
			}
			readCmds(c, hk.onForward);   // +0x12c
			readCmds(c, hk.onBackward);  // +0x13c
			// trailing pstr: the JPG frame-base name
			int32 n = c.i32();
			if (n > 0) {
				sp.name = Common::String((const char *)(c.d + c.o), n);
				c.skip(n);
			}
			if (c.ok) {
				scene.sprites.push_back(sp);
				scene.spriteHooks.push_back(hk);
			}
		} else if (t == 0x19) {
			// Trigger (E-0108, E-0207): flags, gates, conditions, commands and polygon.
			SceneTrigger tr;
			tr.id = id;
			readHead(c, tr.active, tr.visible);
			tr.once = c.u32() == 1;        // +0x170
			tr.view = c.i32();             // +0x174
			tr.click = c.u32() == 1;       // +0x178
			tr.proximity = c.u32() == 1;   // +0x17c
			tr.hasConds = c.u32() == 1;    // +0x180
			tr.gate = c.u32();             // +0x188
			tr.whoCompanion = c.i32();     // +0x14c
			tr.who = c.i32();              // +0x150
			c.skip(76); sub56(c);
			readConds(c, tr.conds);        // +0x190
			readCmds(c, tr.cmds);          // +0x12c
			int32 k = c.i32();                     // polygon point count
			for (int32 j = 0; j < k && c.ok; j++) {
				float px = READ_LE_FLOAT(c.d + c.o); c.skip(4);
				float py = READ_LE_FLOAT(c.d + c.o); c.skip(4);
				tr.poly.push_back(Common::Point((int16)px, (int16)py));
			}
			for (int j = 0; j < 4 && c.ok; j++) {  // the sphere x, y, z, r (E-0705)
				float v = READ_LE_FLOAT(c.d + c.o);
				c.skip(4);
				(j == 0 ? tr.centre.x : j == 1 ? tr.centre.y : j == 2 ? tr.centre.z : tr.radius) = v;
			}
			c.skip(4);
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
			readHead(c, m.active, m.visible);
			// The animation fields (E-0601): fps, mode, then fields of the bubble tests and
			// the rest (Q-0600), playing, start frame, autoplay.
			int32 af[18];
			for (int k = 0; k < 18; k++)
				af[k] = c.i32();
			m.fps = af[0];
			m.anim = (uint32)af[1];
			m.playing = af[13] == 1;
			m.frame = af[14];
			m.autoplay = af[15] == 1;
			m.platform = af[6] == 1;
			// The delay timer: on, (+0x104), (+0x10c), counting, (+0x114), random, (2),
			// min, max, (+0x12c), fixed, (+0x134), ticks left.
			int32 tm[14];
			for (int k = 0; k < 14; k++)
				tm[k] = c.i32();
			m.timerOn = tm[0] != 0;
			m.timerCounting = tm[3] == 1;
			m.timerRandom = tm[5] == 1;
			m.timerMin = tm[8];
			m.timerMax = tm[9];
			m.timerFixed = tm[11];
			m.timerTicks = tm[13];
			sub24(c); sub24(c); pairVec(c);
			// Lists 1, 2 and 7 end the animation (forward end, backward end, end; E-1807);
			// 3, 4, 6 and 8 belong to the contact tests, 5 is never read (Q-0600).
			SpriteHooks hk;
			hk.id = id;
			readCmds(c, hk.onForward);
			readCmds(c, hk.onBackward);
			for (int v = 2; v < 6 && c.ok; v++) ccVec(c);
			readCmds(c, hk.onEnd);
			ccVec(c);
			int32 na = c.i32();
			if (na > 0) { m.anb = Common::String((const char *)(c.d + c.o), na); c.skip(na); }
			int32 nt = c.i32();
			if (nt > 0) { m.tga = Common::String((const char *)(c.d + c.o), nt); c.skip(nt); }
			if (c.ok) {
				scene.meshes.push_back(m);
				scene.spriteHooks.push_back(hk);
			}
		} else if (t == 0x18 || t == 0x2a) {
			// Sound actor (dialogue.cpp, E-0405).
			SceneSound snd;
			uint32 o = c.o;
			if (!readSceneSound(c.d, c.n, o, id, snd)) {
				c.ok = false;
				break;
			}
			c.o = o;
			scene.sounds.push_back(snd);
		} else if (t >= 0x21 && t <= 0x27) {
			// Logic actors (E-0204): script, counter, timer, flag (0x25..0x27 = 0x22..0x24).
			SceneLogic a;
			a.id = id;
			a.type = t;
			readHead(c, a.active, a.visible, &a.state);
			uint32 k = t >= 0x25 ? t - 3 : t;
			if (k == 0x21) {
				a.f0 = c.i32();          // +0x128 guarded
				readConds(c, a.conds);   // +0x12c
			} else if (k == 0x22) {
				a.f0 = c.i32();          // +0x130 max
				a.f1 = c.i32();          // +0x148 fire
			} else if (k == 0x23) {
				a.f0 = c.i32();          // +0x12c limit (ms)
				c.skip(8);               // +0x130, +0x134
			} else {
				a.f0 = c.i32();          // +0x12c fire
			}
			readCmds(c, a.cmds);
			if (c.ok)
				scene.logic.push_back(a);
		} else if (!skipBody(c, t)) {
			warning("Grumpa: scene %s unmodelled type %#x at %#x", name.c_str(), t, c.o - 8);
			return false;
		}
	}
	loadSceneViews(num, scene);
	debug(1, "Grumpa: scene %s -> %u views, %u lights, %u sprites, %u triggers", name.c_str(),
		  (uint)scene.views.size(), (uint)scene.lights.size(), (uint)scene.sprites.size(),
		  (uint)scene.triggers.size());
	return c.ok;
}

// A sprite's frame files (CFXSprite load, E-0302): a name ending in "0000.<ext>" animates,
// frame i being the name with i in those four digits; its depth frames, when present, are
// "<stem>_Z<i>.fxi" (stem = the name without "0000.<ext>"). Any other name is one static frame,
// with the depth "<name without ext>_Z.fxi".
static bool spriteAnimated(const Common::String &name) {
	return name.size() > 8 && Common::String(name.c_str() + name.size() - 8).hasPrefix("0000");
}

static Common::String spriteFrameName(const Common::String &name, int frame) {
	if (!spriteAnimated(name))
		return name;
	return Common::String(name.c_str(), name.size() - 8)
		+ Common::String::format("%04d", frame) + (name.c_str() + name.size() - 4);
}

static Common::String spriteDepthName(const Common::String &name, int frame) {
	if (!spriteAnimated(name))
		return Common::String(name.c_str(), name.size() - 4) + "_Z.fxi";
	return Common::String(name.c_str(), name.size() - 8) + Common::String::format("_Z%04d.fxi", frame);
}

// Draw a sprite's frame at (x, y) as the original's BltFast (E-0302): with a source colour
// key when `keyed` (the key is the COLORREF keyColor, or frame 0's top-left pixel when it is
// -1, matched exactly in the 16-bit page), else opaque. A depth frame, when the sprite has
// them, is copied into the z-buffer `depth` over the same rectangle, so the 3D actors drawn
// later are hidden behind it.
void GrumpaEngine::drawSprite(const SceneSprite &sprite, int frame, Common::Array<uint16> *depth) {
	if (sprite.name.empty())
		return;
	Common::File f;
	if (!f.open(Common::Path("Bitmaps/" + spriteFrameName(sprite.name, frame))))
		return;
	Image::JPEGDecoder jpeg;
	jpeg.setOutputPixelFormat(_screen.format);
	if (!jpeg.loadStream(f))
		return;
	const Graphics::Surface *s = jpeg.getSurface();
	uint32 key = 0;
	if (sprite.keyed) {
		if (sprite.keyColor != -1)
			key = _screen.format.RGBToColor(sprite.keyColor & 0xFF, (sprite.keyColor >> 8) & 0xFF,
											(sprite.keyColor >> 16) & 0xFF);
		else if (frame == 0 || !spriteAnimated(sprite.name))
			key = s->getPixel(0, 0);
		else {
			Common::File f0;
			Image::JPEGDecoder j0;
			j0.setOutputPixelFormat(_screen.format);
			if (f0.open(Common::Path("Bitmaps/" + spriteFrameName(sprite.name, 0))) && j0.loadStream(f0))
				key = j0.getSurface()->getPixel(0, 0);
		}
	}
	for (int yy = 0; yy < s->h; yy++) {
		int dy = sprite.y + yy;
		if (dy < 0 || dy >= kScreenHeight)
			continue;
		for (int xx = 0; xx < s->w; xx++) {
			int dx = sprite.x + xx;
			if (dx < 0 || dx >= kScreenWidth)
				continue;
			uint32 pix = s->getPixel(xx, yy);
			if (!sprite.keyed || pix != key)
				_screen.setPixel(dx, dy, pix);
		}
	}
	if (!sprite.hasDepth || !depth || depth->size() != (uint)(kScreenWidth * kScreenHeight))
		return;
	Common::Array<uint16> z;
	int zw = 0, zh = 0;
	if (!loadFxi(spriteDepthName(sprite.name, frame), z, zw, zh))
		return;
	for (int yy = 0; yy < zh; yy++) {
		int dy = sprite.y + yy;
		if (dy < 0 || dy >= kScreenHeight)
			continue;
		for (int xx = 0; xx < zw; xx++) {
			int dx = sprite.x + xx;
			if (dx >= 0 && dx < kScreenWidth)
				(*depth)[dy * kScreenWidth + dx] = z[yy * zw + xx];
		}
	}
}

// Ray-cast point-in-polygon for a trigger's clickable region (E-0108).
bool pointInPolygon(const Common::Array<Common::Point> &poly, const Common::Point &p) {
	bool in = false;
	for (uint i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
		if ((poly[i].y > p.y) != (poly[j].y > p.y) &&
			p.x < (int)((int64)(poly[j].x - poly[i].x) * (p.y - poly[i].y) /
						(poly[j].y - poly[i].y) + poly[i].x))
			in = !in;
	}
	return in;
}

// A click in the scene goes to the triggers as opcode 18 (docs/spec/events.md, E-0207).
bool GrumpaEngine::handleSceneClick(const Common::Point &p) {
	return _events->click(p);
}

// An actor's texture (E-0114): a 32-bit .tga keeps its alpha and is blended, any other becomes
// an RGB555 texture (CFXTexture, E-0303); returned as ARGB8888. Names are cp1252
// (e.g. a dead-father texture); the extracted cabinet has '_' for those letters.
bool loadActorTexture(const Common::String &name, Graphics::Surface &out, bool &alpha) {
	Common::File tf;
	Common::String ascii = name;
	for (uint k = 0; k < ascii.size(); k++)
		if ((byte)ascii[k] >= 0x80)
			ascii.setChar('_', k);
	if (name.empty() || !(tf.open(Common::Path("Bitmaps/" + name)) || tf.open(Common::Path("Bitmaps/" + ascii))))
		return false;
	Image::TGADecoder tga;
	if (!tga.loadStream(tf))
		return false;
	const Graphics::Surface *src = tga.getSurface();
	alpha = src->format.bytesPerPixel == 4 && src->format.aBits() > 0;
	Graphics::Surface *conv = src->convertTo(alpha ? Graphics::PixelFormat(4, 8, 8, 8, 8, 16, 8, 0, 24)
												   : Graphics::PixelFormat(2, 5, 5, 5, 0, 10, 5, 0, 0));
	Graphics::Surface *argb = conv->convertTo(Graphics::PixelFormat(4, 8, 8, 8, 8, 16, 8, 0, 24));
	out.free();
	out.copyFrom(*argb);
	argb->free();
	delete argb;
	conv->free();
	delete conv;
	return true;
}

// Enter scene <num>: read its graph and views, show view 0 (the player's scene entry sets
// view 0, E-0304) and load the sprites' frame counts and the mesh actors' geometry/textures.
bool GrumpaEngine::enterScene(int num) {
	_sceneNum = num;
	_inventory.setScene(num);  // op 23 to the items (E-0503)
	_events->leaveScene();     // broadcast 25, keep the old scene's status (E-0202)
	_sceneData = SceneData();
	loadScene(num, _sceneData);
	if (_sceneData.views.empty()) {  // no .scn: the background by name
		SceneView v;
		v.colour = Common::String::format("%d_1_IS.jpg", num);
		v.depth = Common::String::format("%d_1_IZ.fxi", num);
		_sceneData.views.push_back(v);
	}
	setView(0);
	for (uint i = 0; i < _sceneData.sprites.size(); i++) {
		SceneSprite &sp = _sceneData.sprites[i];
		sp.frameCount = 0;
		if (sp.name.empty())
			continue;
		do
			sp.frameCount++;
		while (spriteAnimated(sp.name) && sp.frameCount < 10000 &&
			   Common::File::exists(Common::Path("Bitmaps/" + spriteFrameName(sp.name, sp.frameCount))));
		sp.hasDepth = Common::File::exists(Common::Path("Bitmaps/" + spriteDepthName(sp.name, 0)));
	}
	for (uint i = 0; i < _sceneData.meshes.size(); i++) {
		SceneMesh &m = _sceneData.meshes[i];
		Common::String base = m.anb;
		if (base.size() > 4 && base[base.size() - 4] == '.')  // strip ".anb"/".ANB"
			base = Common::String(base.c_str(), base.size() - 4);
		loadMesh(base, m.mesh);
		loadActorTexture(m.tga, m.texture, m.alpha);
	}
	_sceneTick0 = g_system->getMillis();
	_events->enterScene(num, &_sceneData);  // kept status, deferred commands, 23 and 86
	debug(1, "Grumpa: entered scene %d (%u views, %u lights, %u sprites, %u meshes)", num,
		  (uint)_sceneData.views.size(), (uint)_sceneData.lights.size(),
		  (uint)_sceneData.sprites.size(), (uint)_sceneData.meshes.size());
	return true;
}

void GrumpaEngine::setView(int k) {
	if (k < 0 || k >= (int)_sceneData.views.size())
		return;
	_sceneData.view = k;
	const SceneView &v = _sceneData.views[k];
	_sceneBg.create(kScreenWidth, kScreenHeight, _screen.format);
	_sceneBg.clear();
	Common::File f;
	if (f.open(Common::Path("Bitmaps/" + v.colour))) {
		Image::JPEGDecoder jpeg;
		jpeg.setOutputPixelFormat(_screen.format);
		if (jpeg.loadStream(f)) {
			const Graphics::Surface *s = jpeg.getSurface();
			_sceneBg.blitFrom(*s, Common::Point((kScreenWidth - s->w) / 2, (kScreenHeight - s->h) / 2));
		}
	}
	_sceneDepth.clear();
	_depthW = _depthH = 0;
	loadFxi(v.depth, _sceneDepth, _depthW, _depthH);
	if (_depthW != kScreenWidth || _depthH != kScreenHeight)
		_sceneDepth.clear();  // drawn with a cleared z-buffer
}

// The worn attachments (characters.md Attachments, E-1700): each a static mesh (frame 0) on
// the body's face `face`: at the position of the face's first corner in the clip's frame `fr`,
// pitched (and, for the shields 0 and 5, turned) by that corner's normal, then placed with
// the character. Faces count across the body's sections.
void GrumpaEngine::drawAttachments(Character &c, const Mesh &body, int fr, const Camera &cam,
								   Common::Array<uint16> &depth) {
	for (uint k = 0; k < c.attachments.size() && k < 32; k++) {
		if (!(c.worn & (1u << k)))
			continue;
		Character::Attachment &a = c.attachments[k];
		if (!a.loaded) {
			a.loaded = true;
			Common::String base = a.anb;
			if (base.size() > 4 && base[base.size() - 4] == '.')
				base = Common::String(base.c_str(), base.size() - 4);
			loadMesh(base, a.mesh);
			loadActorTexture(a.tga, a.skin, a.alpha);
		}
		int face = a.face;
		const MeshSection *sec = nullptr;
		for (uint s = 0; s < body.sections.size() && !sec; s++) {
			if (face < (int)body.sections[s].faces.size())
				sec = &body.sections[s];
			else
				face -= body.sections[s].faces.size();
		}
		if (!sec || a.mesh.empty() || face < 0)
			continue;
		const uint vi = fr * sec->nv + sec->faces[face].v[0];
		if (vi >= sec->verts.size() || vi >= sec->normals.size())
			continue;
		const Vec3 &p = sec->verts[vi], &n = sec->normals[vi];
		const float ny = CLIP(n.y, -1.0f, 1.0f);
		const float pitch = n.z < 0 ? -acosf(ny) : acosf(ny);
		// Original: the shields' yaw tests n.y again where n.x looks meant (E-1700); kept.
		const float yaw = (k == 0 || k == 5) ? (n.y < 0 ? -acosf(ny) : acosf(ny)) : 0.0f;
		const float rot[3] = { pitch, yaw, 0.0f }, at[3] = { p.x, p.y, p.z };
		const float turn[3] = { 0.0f, c.yaw, 0.0f }, pos[3] = { c.pos.x, c.pos.y, c.pos.z };
		Mesh local, placed;
		placeMesh(a.mesh, 0, rot, at, local);
		placeMesh(local, 0, turn, pos, placed);
		renderMesh(_screen, placed, cam, _sceneData.lights, depth,
				   a.skin.getPixels() ? &a.skin : nullptr, a.alpha);
	}
}

// Redraw the current scene in the original's render order (E-0305): the view's background
// and depth, then every visible actor by layer: layer-1 sprites, the layer-3 mesh actors
// (lit, textured, depth-tested against the z-buffer), layer-4 sprites.
void GrumpaEngine::renderSceneFrame(uint32 now) {
	_screen.blitFrom(_sceneBg);
	Common::Array<uint16> depth = _sceneDepth;
	for (int layer = 0; layer <= 8; layer++) {
		for (uint i = 0; i < _sceneData.sprites.size(); i++) {
			const SceneSprite &sp = _sceneData.sprites[i];
			if (sp.layer != layer || !sp.visible || (sp.view != -1 && sp.view != _sceneData.view))
				continue;
			drawSprite(sp, _events->spriteFrame(sp.id), &depth);  // animated by the VM (E-0208)
		}
		if (layer != 3)
			continue;
		const Camera &cam = _sceneData.views[_sceneData.view].cam;
		for (uint i = 0; i < _sceneData.meshes.size(); i++) {
			const SceneMesh &m = _sceneData.meshes[i];
			if (m.visible && !m.mesh.empty())
				renderMesh(_screen, m.mesh, cam, _sceneData.lights, depth,
						   m.texture.getPixels() ? &m.texture : nullptr, m.alpha,
						   _events->spriteFrame(m.id));  // animated by the VM (E-0601)
		}
		drawItems(cam, depth);  // the items lying here (E-0900)
		// The characters at home in this scene (characters.md, E-0403): their clip's mesh turned
		// by yaw about +Y as D3DX's RotationY (E-0814) and moved to the position.
		Common::Array<Character> &chars = _characters.list();
		for (uint i = 0; i < chars.size(); i++) {
			Character &c = chars[i];
			if (!_characters.present(c))
				continue;
			const Mesh *clip = _characters.mesh(c);
			if (c.skinIndex != c.texture && c.texture >= 0 && c.texture < (int)c.textures.size()) {
				c.skinIndex = c.texture;
				loadActorTexture(c.textures[c.texture], c.skin, c.alpha);
			}
			if (!clip)
				continue;
			const int fr = CLIP(c.frame, 0, clip->frames - 1);
			drawAttachments(c, *clip, fr, cam, depth);
			// The current frame of the clip (its clock: Characters::update, E-0603), placed.
			Mesh placed;
			placed.frames = 1;
			float cs = cosf(c.yaw), sn = sinf(c.yaw);
			for (uint s = 0; s < clip->sections.size(); s++) {
				const MeshSection &src = clip->sections[s];
				MeshSection sec;
				sec.nv = src.nv;
				sec.faces = src.faces;
				sec.u = src.u;
				sec.v = src.v;
				for (uint v = 0; v < src.nv; v++) {
					const Vec3 p = src.verts[fr * src.nv + v], n = src.normals[fr * src.nv + v];
					sec.verts.push_back(Vec3(p.x * cs + p.z * sn + c.pos.x, p.y + c.pos.y, -p.x * sn + p.z * cs + c.pos.z));
					sec.normals.push_back(Vec3(n.x * cs + n.z * sn, n.y, -n.x * sn + n.z * cs));
				}
				placed.sections.push_back(sec);
			}
			renderMesh(_screen, placed, cam, _sceneData.lights, depth,
					   c.skin.getPixels() ? &c.skin : nullptr, c.alpha);
		}
	}
	// Hotspot overlay (H): outline each unspent trigger's clickable polygon (E-0108), so the
	// exits and interactions are visible. A development/accessibility view.
	if (_showHotspots) {
		uint32 col = _screen.format.RGBToColor(0, 255, 0);
		for (uint i = 0; i < _sceneData.triggers.size(); i++) {
			const SceneTrigger &tr = _sceneData.triggers[i];
			if (!_events->triggerClickable(tr) || tr.poly.size() < 2)
				continue;
			for (uint j = 0; j < tr.poly.size(); j++)
				_screen.drawLine(tr.poly[j].x, tr.poly[j].y,
								 tr.poly[(j + 1) % tr.poly.size()].x, tr.poly[(j + 1) % tr.poly.size()].y, col);
		}
	}
}

} // End of namespace Grumpa
