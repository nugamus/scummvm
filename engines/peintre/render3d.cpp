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

#include "common/scummsys.h"
#include "math/utils.h"

#include "peintre/render3d.h"

namespace Peintre {

// The sine and cosine tables of movement.md: trunc(32768 * sin/cos(2 pi i / 4096)).
int32 sinQ15(int32 angle) {
	return (int32)(32768.0 * sin(2.0 * M_PI * (angle & 0xFFF) / 4096.0));
}

int32 cosQ15(int32 angle) {
	return (int32)(32768.0 * cos(2.0 * M_PI * (angle & 0xFFF) / 4096.0));
}

void cameraMatrix(int32 pitch, int32 yaw, int32 roll, int32 m[9]) {
	// movement.md "Rotation matrix": a = pitch, b = yaw, c = roll; each product >> 15.
	const int32 Sa = sinQ15(pitch), Ca = cosQ15(pitch);
	const int32 Sb = sinQ15(yaw), Cb = cosQ15(yaw);
	const int32 Sc = sinQ15(roll), Cc = cosQ15(roll);
	auto mul = [](int32 p, int32 q) { return (int32)(((int64)p * q) >> 15); };
	m[0] = mul(mul(Sc, Sa), Sb) + mul(Cc, Cb);
	m[1] = mul(mul(Cc, Sa), Sb) - mul(Cb, Sc);
	m[2] = mul(Sb, Ca);
	m[3] = mul(Sc, Ca);
	m[4] = mul(Cc, Ca);
	m[5] = -Sa;
	m[6] = mul(mul(Cb, Sc), Sa) - mul(Cc, Sb);
	m[7] = mul(mul(Cc, Cb), Sa) + mul(Sb, Sc);
	m[8] = mul(Cb, Ca);
}

Renderer3D::Renderer3D() {
	_zbuf.resize(640 * 480);
	_nodeBuf.resize(640 * 480);
}

void Renderer3D::computeWorld(const Scene3D &scene) {
	const uint n = scene.nodes.size();
	_rot.resize(9 * n);
	_pos.resize(3 * n);
	_viewPos.resize(3 * n);
	// Nodes in table order: the root first; a child's parent comes before it in a
	// depth-first order, so resolve recursively.
	Common::Array<bool> done(n, false);
	for (uint i = 0; i < n; i++) {
		Common::Array<int> chain;
		for (int k = i; k >= 0 && !done[k]; k = scene.nodes[k].parent)
			chain.push_back(k);
		for (int c = chain.size() - 1; c >= 0; c--) {
			const int k = chain[c];
			const Node &nd = scene.nodes[k];
			float l[9];
			for (int j = 0; j < 9; j++)
				l[j] = nd.rotation[j] / 32768.0f;
			float *r = &_rot[9 * k], *p = &_pos[3 * k];
			const float lp[3] = { (float)nd.position.x, (float)nd.position.y, (float)nd.position.z };
			if (nd.parent < 0) {
				memcpy(r, l, sizeof(l));
				memcpy(p, lp, sizeof(lp));
			} else {
				const float *pr = &_rot[9 * nd.parent], *pp = &_pos[3 * nd.parent];
				for (int a = 0; a < 3; a++) {
					for (int b = 0; b < 3; b++)
						r[3 * a + b] = pr[3 * a] * l[b] + pr[3 * a + 1] * l[3 + b] + pr[3 * a + 2] * l[6 + b];
					p[a] = pp[a] + pr[3 * a] * lp[0] + pr[3 * a + 1] * lp[1] + pr[3 * a + 2] * lp[2];
				}
			}
			done[k] = true;
		}
	}
}

void Renderer3D::draw(Graphics::Surface &dst, const Common::Rect &viewport, const Scene3D &scene,
					  const Common::Array<const Texture3D *> &textures, const Camera &cam) {
	_viewport = viewport;
	// Focal length: 480 for a 640-wide view, scaled by width (scene.md "Camera and view").
	_focal = 480.0f * viewport.width() / 640.0f;
	for (int y = viewport.top; y < viewport.bottom; y++) {
		for (int x = viewport.left; x < viewport.right; x++) {
			_zbuf[y * 640 + x] = 0.0f;
			_nodeBuf[y * 640 + x] = -1;
		}
	}
	int32 m[9];
	cameraMatrix(cam.pitch, cam.yaw, cam.roll, m);
	for (int j = 0; j < 9; j++)
		_view[j] = m[j] / 32768.0f;
	_camPos[0] = cam.x;
	_camPos[1] = cam.y;
	_camPos[2] = cam.z;
	computeWorld(scene);

	Common::Array<Vtx> verts;
	for (uint ni = 0; ni < scene.nodes.size(); ni++) {
		const Node &nd = scene.nodes[ni];
		const float *r = &_rot[9 * ni], *p = &_pos[3 * ni];
		// The camera-relative position of the node (for distance tests).
		const float d0 = p[0] - _camPos[0], d1 = p[1] - _camPos[1], d2 = p[2] - _camPos[2];
		_viewPos[3 * ni] = d0;
		_viewPos[3 * ni + 1] = d1;
		_viewPos[3 * ni + 2] = d2;
		// Hidden: this node or an ancestor has flag bit 0 or bit 2 (E-0014).
		bool hidden = false;
		for (int k = ni; k >= 0; k = scene.nodes[k].parent)
			if (scene.nodes[k].flags & 5)
				hidden = true;
		if (hidden)
			continue;
		verts.resize(nd.vertices.size());
		for (uint vi = 0; vi < nd.vertices.size(); vi++) {
			const Vec3i &v = nd.vertices[vi];
			const float w[3] = {
				r[0] * v.x + r[1] * v.y + r[2] * v.z + p[0] - _camPos[0],
				r[3] * v.x + r[4] * v.y + r[5] * v.z + p[1] - _camPos[1],
				r[6] * v.x + r[7] * v.y + r[8] * v.z + p[2] - _camPos[2]
			};
			// Camera axes are the columns of M (movement.md): right, down, forward.
			Vtx &o = verts[vi];
			o.x = w[0] * _view[0] + w[1] * _view[3] + w[2] * _view[6];
			o.y = w[0] * _view[1] + w[1] * _view[4] + w[2] * _view[7];
			o.z = w[0] * _view[2] + w[1] * _view[5] + w[2] * _view[8];
		}
		for (const FaceGroup &g : nd.faceGroups) {
			const Texture3D *tex = g.materialIndex >= 0 && (uint)g.materialIndex < textures.size() ? textures[g.materialIndex] : nullptr;
			for (const Poly &poly : g.polys) {
				Vtx tri[3];
				bool ok = true;
				for (int c = 0; c < 3; c++) {
					if (poly.vertex[c] < 0) {
						ok = false;
						break;
					}
					tri[c] = verts[poly.vertex[c]];
					if (poly.uv[c] >= 0) {
						tri[c].u = nd.uvs[2 * poly.uv[c]] / 65536.0f;
						tri[c].v = nd.uvs[2 * poly.uv[c] + 1] / 65536.0f;
					} else {
						tri[c].u = tri[c].v = 0;
					}
					tri[c].shade = 8.0f; // provisional until render.md gives the rule
				}
				if (ok)
					clipAndDraw(dst, tri, tex, ni);
			}
		}
	}
}

void Renderer3D::clipAndDraw(Graphics::Surface &dst, Vtx *v, const Texture3D *tex, int node) {
	// Clip against the near plane z = _near; a triangle becomes 0, 1 or 2 triangles.
	Vtx in[4];
	int n = 0;
	for (int i = 0; i < 3; i++) {
		const Vtx &a = v[i], &b = v[(i + 1) % 3];
		const bool ain = a.z >= _near, bin = b.z >= _near;
		if (ain)
			in[n++] = a;
		if (ain != bin) {
			const float t = (_near - a.z) / (b.z - a.z);
			Vtx c;
			c.x = a.x + (b.x - a.x) * t;
			c.y = a.y + (b.y - a.y) * t;
			c.z = _near;
			c.u = a.u + (b.u - a.u) * t;
			c.v = a.v + (b.v - a.v) * t;
			c.shade = a.shade + (b.shade - a.shade) * t;
			if (n < 4)
				in[n++] = c;
		}
	}
	if (n < 3)
		return;
	const float cx = (_viewport.left + _viewport.right) / 2.0f, cy = (_viewport.top + _viewport.bottom) / 2.0f;
	for (int i = 0; i < n; i++) {
		in[i].sx = cx + _focal * in[i].x / in[i].z;
		in[i].sy = cy + _focal * in[i].y / in[i].z;
	}
	drawTriangle(dst, in, tex, node);
	if (n == 4) {
		Vtx t2[3] = { in[0], in[2], in[3] };
		drawTriangle(dst, t2, tex, node);
	}
}

void Renderer3D::drawTriangle(Graphics::Surface &dst, const Vtx *v, const Texture3D *tex, int node) {
	const float area = (v[1].sx - v[0].sx) * (v[2].sy - v[0].sy) - (v[2].sx - v[0].sx) * (v[1].sy - v[0].sy);
	if (fabs(area) < 1e-6f)
		return;
	int minX = (int)floor(MIN(v[0].sx, MIN(v[1].sx, v[2].sx)));
	int maxX = (int)ceil(MAX(v[0].sx, MAX(v[1].sx, v[2].sx)));
	int minY = (int)floor(MIN(v[0].sy, MIN(v[1].sy, v[2].sy)));
	int maxY = (int)ceil(MAX(v[0].sy, MAX(v[1].sy, v[2].sy)));
	minX = MAX<int>(minX, _viewport.left);
	maxX = MIN<int>(maxX, _viewport.right - 1);
	minY = MAX<int>(minY, _viewport.top);
	maxY = MIN<int>(maxY, _viewport.bottom - 1);
	if (minX > maxX || minY > maxY)
		return;
	// Perspective-correct attributes: interpolate a/z and 1/z.
	float iz[3], uz[3], vz[3], sz[3];
	for (int i = 0; i < 3; i++) {
		iz[i] = 1.0f / v[i].z;
		uz[i] = v[i].u * iz[i];
		vz[i] = v[i].v * iz[i];
		sz[i] = v[i].shade * iz[i];
	}
	const float inv = 1.0f / area;
	for (int y = minY; y <= maxY; y++) {
		const float py = y + 0.5f;
		uint16 *row = (uint16 *)dst.getBasePtr(0, y);
		for (int x = minX; x <= maxX; x++) {
			const float px = x + 0.5f;
			float w0 = ((v[1].sx - px) * (v[2].sy - py) - (v[2].sx - px) * (v[1].sy - py)) * inv;
			float w1 = ((v[2].sx - px) * (v[0].sy - py) - (v[0].sx - px) * (v[2].sy - py)) * inv;
			float w2 = 1.0f - w0 - w1;
			if (w0 < 0 || w1 < 0 || w2 < 0)
				continue;
			const float z = w0 * iz[0] + w1 * iz[1] + w2 * iz[2];
			float &zb = _zbuf[y * 640 + x];
			if (z <= zb)
				continue;
			zb = z;
			_nodeBuf[y * 640 + x] = node;
			if (!tex) {
				row[x] = 0x8410;
				continue;
			}
			const float rz = 1.0f / z;
			const int u = (int)((w0 * uz[0] + w1 * uz[1] + w2 * uz[2]) * rz) & 255;
			const int t = (int)((w0 * vz[0] + w1 * vz[1] + w2 * vz[2]) * rz) & 255;
			const int level = CLIP<int>((int)((w0 * sz[0] + w1 * sz[1] + w2 * sz[2]) * rz), 0, 31);
			row[x] = tex->shades[level][tex->texels[t * 256 + u]];
		}
	}
}

int Renderer3D::pick(int x, int y) const {
	if (!_viewport.contains(x, y))
		return -1;
	return _nodeBuf[y * 640 + x];
}

bool Renderer3D::nodeViewPosition(int node, int32 &x, int32 &y, int32 &z) const {
	if (node < 0 || (uint)(3 * node + 2) >= _viewPos.size())
		return false;
	x = (int32)_viewPos[3 * node];
	y = (int32)_viewPos[3 * node + 1];
	z = (int32)_viewPos[3 * node + 2];
	return true;
}

} // End of namespace Peintre
