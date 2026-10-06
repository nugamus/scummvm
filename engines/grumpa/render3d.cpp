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

// The 3D actors' renderer (docs/spec/scene.md, engine's own code). The original draws them
// with Direct3D 7's fixed-function pipeline into its 800x600 RGB555 page, whose 16-bit
// z-buffer holds the view's .fxi depth: each vertex goes through the view's view and
// projection matrices, is lit by the scene's point lights (Gouraud) and the triangle is
// textured (modulate, bilinear), depth-tested less-or-equal and written to the z-buffer.

#include "common/array.h"
#include "common/scummsys.h"
#include "graphics/managed_surface.h"

#include "grumpa/mesh.h"

namespace Grumpa {

static const float kAmbient = 30.0f / 255.0f;  // D3DRENDERSTATE_AMBIENT 0x1e1e1e (boot)

static Vec3 normalize(const Vec3 &v) {
	float len = sqrtf(v.dot(v));
	return len > 1e-6f ? Vec3(v.x / len, v.y / len, v.z / len) : v;
}

// Row vector times a 4x4 Direct3D matrix.
static void xform(const float m[16], float x, float y, float z, float w, float out[4]) {
	for (int j = 0; j < 4; j++)
		out[j] = x * m[j] + y * m[4 + j] + z * m[8 + j] + w * m[12 + j];
}

Camera lookAtCamera(const Vec3 &eye, const Vec3 &target, float fovY, float zn, float zf) {
	Camera c;
	Vec3 zaxis = normalize(target - eye);
	Vec3 up = fabsf(zaxis.y) > 0.99f ? Vec3(0, 0, 1) : Vec3(0, 1, 0);
	Vec3 xaxis = normalize(up.cross(zaxis));
	Vec3 yaxis = zaxis.cross(xaxis);
	const float v[16] = {
		xaxis.x, yaxis.x, zaxis.x, 0,
		xaxis.y, yaxis.y, zaxis.y, 0,
		xaxis.z, yaxis.z, zaxis.z, 0,
		-xaxis.dot(eye), -yaxis.dot(eye), -zaxis.dot(eye), 1 };
	float ys = 1.0f / tanf(fovY / 2), q = zf / (zf - zn);
	const float p[16] = {
		ys * 3 / 4, 0, 0, 0,
		0, ys, 0, 0,
		0, 0, q, 1,
		0, 0, -q * zn, 0 };
	memcpy(c.view, v, sizeof(v));
	memcpy(c.proj, p, sizeof(p));
	return c;
}

namespace {

// A triangle corner in clip space, with its lit colour and texture coordinates.
struct ClipVertex {
	float c[4];
	float r, g, b;
	float u, v;
};

struct ScreenVertex {
	float sx, sy, z;    // screen position, z/w in [0,1]
	float iw;           // 1/w, for perspective-correct interpolation
	float r, g, b;      // lit colour
	float u, v;
};

// Direct3D 7 fixed-function lighting of one vertex (material: diffuse and ambient white,
// no specular/emissive, the CFXTexture default; E-0303).
static void light(const Vec3 &p, const Vec3 &n, const Common::Array<SceneLight> &lights,
				  float &r, float &g, float &b) {
	r = g = b = kAmbient;
	for (uint i = 0; i < lights.size(); i++) {
		const SceneLight &l = lights[i];
		if (!l.on)
			continue;
		Vec3 d = Vec3(l.d[13], l.d[14], l.d[15]) - p;
		float dist = sqrtf(d.dot(d));
		if (dist > l.d[19] || dist < 1e-6f)
			continue;
		float ndl = n.dot(d) / dist;
		if (ndl <= 0)
			continue;
		float att = l.d[21] + l.d[22] * dist + l.d[23] * dist * dist;
		att = att > 0 ? 1.0f / att : 1.0f;
		r += l.d[1] * ndl * att;
		g += l.d[2] * ndl * att;
		b += l.d[3] * ndl * att;
	}
	r = MIN(r, 1.0f); g = MIN(g, 1.0f); b = MIN(b, 1.0f);
}

// Bilinear texture sample with wrapping (D3DTFG/D3DTFN_LINEAR, D3DTADDRESS_WRAP).
static void sample(const Graphics::Surface &t, float u, float v, float &r, float &g, float &b, float &a) {
	float fx = u * t.w - 0.5f, fy = v * t.h - 0.5f;
	int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
	float ax = fx - x0, ay = fy - y0;
	r = g = b = a = 0;
	for (int k = 0; k < 4; k++) {
		int x = x0 + (k & 1), y = y0 + (k >> 1);
		x %= t.w; if (x < 0) x += t.w;
		y %= t.h; if (y < 0) y += t.h;
		float wgt = ((k & 1) ? ax : 1 - ax) * ((k >> 1) ? ay : 1 - ay);
		byte pa, pr, pg, pb;
		t.format.colorToARGB(t.getPixel(x, y), pa, pr, pg, pb);
		r += wgt * pr; g += wgt * pg; b += wgt * pb; a += wgt * pa;
	}
}

// Rasterise one screen triangle: clockwise only (default D3DCULL_CCW, y down), z/w tested
// less-or-equal against and written to `depth`, pixels past the far plane (z/w > 1) dropped.
static void drawTriangle(Graphics::ManagedSurface &screen, Common::Array<uint16> &depth,
						 const ScreenVertex &a, const ScreenVertex &b, const ScreenVertex &c,
						 const Graphics::Surface *tex, bool alpha, bool zWrite, int zBias) {
	const int W = screen.w, H = screen.h;
	float area = (b.sx - a.sx) * (c.sy - a.sy) - (b.sy - a.sy) * (c.sx - a.sx);
	if (area <= 0)
		return;
	int minx = MAX((int)ceilf(MIN(a.sx, MIN(b.sx, c.sx)) - 0.5f), 0);
	int maxx = MIN((int)floorf(MAX(a.sx, MAX(b.sx, c.sx)) - 0.5f), W - 1);
	int miny = MAX((int)ceilf(MIN(a.sy, MIN(b.sy, c.sy)) - 0.5f), 0);
	int maxy = MIN((int)floorf(MAX(a.sy, MAX(b.sy, c.sy)) - 0.5f), H - 1);
	for (int y = miny; y <= maxy; y++) {
		for (int x = minx; x <= maxx; x++) {
			float px = x + 0.5f, py = y + 0.5f;
			float w0 = ((b.sx - px) * (c.sy - py) - (b.sy - py) * (c.sx - px)) / area;
			float w1 = ((c.sx - px) * (a.sy - py) - (c.sy - py) * (a.sx - px)) / area;
			float w2 = 1.0f - w0 - w1;
			if (w0 < 0 || w1 < 0 || w2 < 0)
				continue;
			// z/w is affine in screen space: interpolate linearly (the z-buffer value).
			float z = w0 * a.z + w1 * b.z + w2 * c.z;
			if (z > 1.0f)
				continue;
			uint16 z16 = (uint16)CLIP(z * 65535.0f - zBias, 0.0f, 65535.0f);
			uint16 &zb = depth[y * W + x];
			if (z16 > zb)
				continue;
			// Perspective-correct colour and texture coordinates.
			float pw0 = w0 * a.iw, pw1 = w1 * b.iw, pw2 = w2 * c.iw;
			float inv = 1.0f / (pw0 + pw1 + pw2);
			pw0 *= inv; pw1 *= inv; pw2 *= inv;
			float r = pw0 * a.r + pw1 * b.r + pw2 * c.r;
			float g = pw0 * a.g + pw1 * b.g + pw2 * c.g;
			float bl = pw0 * a.b + pw1 * b.b + pw2 * c.b;
			float ta = 255;
			if (tex) {
				float tr, tg, tb;
				sample(*tex, pw0 * a.u + pw1 * b.u + pw2 * c.u, pw0 * a.v + pw1 * b.v + pw2 * c.v,
					   tr, tg, tb, ta);
				r *= tr; g *= tg; bl *= tb;
			} else {
				r *= 255; g *= 255; bl *= 255;
			}
			if (alpha) {
				// SRCALPHA / INVSRCALPHA blend with the page.
				byte dr, dg, db;
				screen.format.colorToRGB(screen.getPixel(x, y), dr, dg, db);
				float k = ta / 255.0f;
				r = r * k + dr * (1 - k); g = g * k + dg * (1 - k); bl = bl * k + db * (1 - k);
			}
			if (zWrite)
				zb = z16;
			screen.setPixel(x, y, screen.format.RGBToColor((byte)r, (byte)g, (byte)bl));
		}
	}
}

static ClipVertex lerp(const ClipVertex &p, const ClipVertex &q, float t) {
	ClipVertex o;
	for (int k = 0; k < 4; k++)
		o.c[k] = p.c[k] + (q.c[k] - p.c[k]) * t;
	o.r = p.r + (q.r - p.r) * t; o.g = p.g + (q.g - p.g) * t; o.b = p.b + (q.b - p.b) * t;
	o.u = p.u + (q.u - p.u) * t; o.v = p.v + (q.v - p.v) * t;
	return o;
}

} // anonymous namespace


void renderMesh(Graphics::ManagedSurface &screen, const Mesh &mesh, const Camera &cam,
				const Common::Array<SceneLight> &lights, Common::Array<uint16> &depth,
				const Graphics::Surface *tex, bool alpha, int frame, bool lit, bool zWrite, int zBias) {
	const int W = screen.w, H = screen.h;
	if (frame < 0 || frame >= MAX(mesh.frames, 1))
		return;
	if ((int)depth.size() != W * H) {
		depth.resize(W * H);
		for (uint i = 0; i < depth.size(); i++)
			depth[i] = 0xFFFF;
	}
	if (!tex || !tex->getPixels() || tex->w <= 0 || tex->h <= 0)
		tex = nullptr;

	for (uint s = 0; s < mesh.sections.size(); s++) {
		const MeshSection &sec = mesh.sections[s];
		const uint base = frame * sec.nv;
		if (base + sec.nv > sec.verts.size())
			continue;
		Common::Array<ClipVertex> cv;
		cv.resize(sec.nv);
		for (uint i = 0; i < sec.nv; i++) {
			const Vec3 &v = sec.verts[base + i];
			float e[4];
			xform(cam.view, v.x, v.y, v.z, 1, e);
			xform(cam.proj, e[0], e[1], e[2], e[3], cv[i].c);
			if (lit)
				light(v, sec.normals[base + i], lights, cv[i].r, cv[i].g, cv[i].b);
			else
				cv[i].r = cv[i].g = cv[i].b = 1.0f;
		}
		for (uint fi = 0; fi < sec.faces.size(); fi++) {
			const Face &face = sec.faces[fi];
			// Clip the triangle to the near plane z >= 0 (Sutherland-Hodgman), then fan it.
			ClipVertex in[3], poly[4];
			for (int k = 0; k < 3; k++) {
				in[k] = cv[face.v[k]];
				in[k].u = tex ? sec.u[face.uv[k]] : 0;
				in[k].v = tex ? sec.v[face.uv[k]] : 0;
			}
			int n = 0;
			for (int k = 0; k < 3; k++) {
				const ClipVertex &p = in[k], &q = in[(k + 1) % 3];
				bool pin = p.c[2] >= 0, qin = q.c[2] >= 0;
				if (pin)
					poly[n++] = p;
				if (pin != qin)
					poly[n++] = lerp(p, q, p.c[2] / (p.c[2] - q.c[2]));
			}
			if (n < 3)
				continue;
			ScreenVertex sv[4];
			bool ok = true;
			for (int k = 0; k < n; k++) {
				const ClipVertex &p = poly[k];
				if (p.c[3] <= 1e-6f) {
					ok = false;
					break;
				}
				ScreenVertex &o = sv[k];
				o.iw = 1.0f / p.c[3];
				o.sx = (p.c[0] * o.iw + 1) * W / 2;
				o.sy = (1 - p.c[1] * o.iw) * H / 2;
				o.z = p.c[2] * o.iw;
				o.r = p.r; o.g = p.g; o.b = p.b;
				o.u = p.u; o.v = p.v;
			}
			if (!ok)
				continue;
			for (int k = 1; k + 1 < n; k++)
				drawTriangle(screen, depth, sv[0], sv[k], sv[k + 1], tex, alpha, zWrite, zBias);
		}
	}
}

} // End of namespace Grumpa
