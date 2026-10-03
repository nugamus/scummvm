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

struct ScreenVertex {
	float sx, sy, z;    // screen position, z/w in [0,1]
	float iw;           // 1/w, for perspective-correct interpolation
	float r, g, b;      // lit colour
	bool clipped;       // in front of the near plane or behind the camera
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

} // anonymous namespace

void renderMesh(Graphics::ManagedSurface &screen, const Mesh &mesh, const Camera &cam,
				const Common::Array<SceneLight> &lights, Common::Array<uint16> &depth,
				const Graphics::Surface *tex, bool alpha) {
	const int W = screen.w, H = screen.h;
	if ((int)depth.size() != W * H) {
		depth.resize(W * H);
		for (uint i = 0; i < depth.size(); i++)
			depth[i] = 0xFFFF;
	}
	const bool textured = tex && tex->getPixels() && tex->w > 0 && tex->h > 0;

	for (uint s = 0; s < mesh.sections.size(); s++) {
		const MeshSection &sec = mesh.sections[s];
		Common::Array<ScreenVertex> sv;
		sv.resize(sec.verts.size());
		for (uint i = 0; i < sec.verts.size(); i++) {
			const Vec3 &v = sec.verts[i];
			float e[4], c[4];
			xform(cam.view, v.x, v.y, v.z, 1, e);
			xform(cam.proj, e[0], e[1], e[2], e[3], c);
			ScreenVertex &p = sv[i];
			// ponytail: a triangle crossing the near plane is dropped whole, not clipped;
			// the scenes' actors stay well inside the frustum.
			p.clipped = c[3] <= 1e-6f || c[2] < 0;
			if (p.clipped)
				continue;
			p.iw = 1.0f / c[3];
			p.sx = (c[0] * p.iw + 1) * W / 2;
			p.sy = (1 - c[1] * p.iw) * H / 2;
			p.z = c[2] * p.iw;
			light(v, i < sec.normals.size() ? sec.normals[i] : Vec3(), lights, p.r, p.g, p.b);
		}
		for (uint fi = 0; fi < sec.faces.size(); fi++) {
			const Face &face = sec.faces[fi];
			const ScreenVertex &a = sv[face.v[0]], &b = sv[face.v[1]], &c = sv[face.v[2]];
			if (a.clipped || b.clipped || c.clipped)
				continue;
			// Default D3DCULL_CCW: only clockwise (on screen, y down) triangles are drawn.
			float area = (b.sx - a.sx) * (c.sy - a.sy) - (b.sy - a.sy) * (c.sx - a.sx);
			if (area <= 0)
				continue;
			float au = 0, av = 0, bu = 0, bv = 0, cu = 0, cv = 0;
			if (textured) {
				au = sec.u[face.uv[0]]; av = sec.v[face.uv[0]];
				bu = sec.u[face.uv[1]]; bv = sec.v[face.uv[1]];
				cu = sec.u[face.uv[2]]; cv = sec.v[face.uv[2]];
			}
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
					uint16 z16 = (uint16)CLIP(z * 65535.0f, 0.0f, 65535.0f);
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
					if (textured) {
						float tr, tg, tb;
						sample(*tex, pw0 * au + pw1 * bu + pw2 * cu, pw0 * av + pw1 * bv + pw2 * cv,
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
					zb = z16;
					screen.setPixel(x, y, screen.format.RGBToColor((byte)r, (byte)g, (byte)bl));
				}
			}
		}
	}
}

} // End of namespace Grumpa
