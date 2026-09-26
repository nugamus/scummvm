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

// The unlit shade row (render.md "Lighting": brightness 15 -> row 31 - 15).
static const int kShadeRow = 16;

Renderer3D::Renderer3D() {
	_zbuf.resize(640 * 480);
	_pickZ.resize(640 * 480);
	_pickNode.resize(640 * 480);
}

void Renderer3D::draw(Graphics::Surface &dst, const Common::Rect &viewport, const Scene3D &scene, const Camera &cam) {
	_viewport = viewport;
	// f = 480 * w / 640 (integer division) on both axes (render.md "Projection").
	_focal = (float)(480 * viewport.width() / 640);
	// Black background, nothing picked (render.md "Visibility").
	for (int y = viewport.top; y < viewport.bottom; y++) {
		memset(dst.getBasePtr(viewport.left, y), 0, viewport.width() * 2);
		for (int x = viewport.left; x < viewport.right; x++) {
			_zbuf[y * 640 + x] = 0.0f;
			_pickZ[y * 640 + x] = 0.0f;
			_pickNode[y * 640 + x] = -1;
		}
	}

	// The view transform: v_cam = R^T (v - eye); the columns of R are right, down, forward.
	int32 m[9];
	cameraMatrix(cam.pitch, cam.yaw, cam.roll, m);
	float view[9];
	for (int a = 0; a < 3; a++)
		for (int b = 0; b < 3; b++)
			view[3 * a + b] = m[3 * b + a] / 32768.0f;
	const float eye[3] = { (float)cam.x, (float)cam.y, (float)cam.z };

	// World matrices parent x local, from the root down (render.md "Per frame"); a hidden
	// node (flag bit 0) hides its subtree.
	const uint n = scene.nodes.size();
	_rot.resize(9 * n);
	_pos.resize(3 * n);
	_viewPos.resize(3 * n);
	_visible.resize(n);
	_verts.resize(n);
	Common::Array<float> wrot(9 * n), wpos(3 * n);
	Common::Array<int> order, stack;
	for (uint i = 0; i < n; i++)
		if (scene.nodes[i].parent < 0)
			stack.push_back(i);
	while (!stack.empty()) {
		const int k = stack.back();
		stack.pop_back();
		order.push_back(k);
		const Node &nd = scene.nodes[k];
		float l[9];
		for (int j = 0; j < 9; j++)
			l[j] = nd.rotation[j] / 32768.0f;
		const float lp[3] = { (float)nd.position.x, (float)nd.position.y, (float)nd.position.z };
		float *r = &wrot[9 * k], *p = &wpos[3 * k];
		if (nd.parent < 0) {
			memcpy(r, l, sizeof(l));
			memcpy(p, lp, sizeof(lp));
			_visible[k] = !(nd.flags & 1);
		} else {
			const float *pr = &wrot[9 * nd.parent], *pp = &wpos[3 * nd.parent];
			for (int a = 0; a < 3; a++) {
				for (int b = 0; b < 3; b++)
					r[3 * a + b] = pr[3 * a] * l[b] + pr[3 * a + 1] * l[3 + b] + pr[3 * a + 2] * l[6 + b];
				p[a] = pp[a] + pr[3 * a] * lp[0] + pr[3 * a + 1] * lp[1] + pr[3 * a + 2] * lp[2];
			}
			_visible[k] = _visible[nd.parent] && !(nd.flags & 1);
		}
		const float d[3] = { p[0] - eye[0], p[1] - eye[1], p[2] - eye[2] };
		for (int a = 0; a < 3; a++) {
			for (int b = 0; b < 3; b++)
				_rot[9 * k + 3 * a + b] = view[3 * a] * r[b] + view[3 * a + 1] * r[3 + b] + view[3 * a + 2] * r[6 + b];
			_pos[3 * k + a] = view[3 * a] * d[0] + view[3 * a + 1] * d[1] + view[3 * a + 2] * d[2];
			_viewPos[3 * k + a] = d[a];
		}
		for (int c = nd.children.size() - 1; c >= 0; c--)
			stack.push_back(nd.children[c]);
	}

	// Every node's vertices in camera space; a flag 0x10 node's corners may use its
	// parent's, transformed with the parent's matrix (render.md, E-0504).
	for (int k : order) {
		const Node &nd = scene.nodes[k];
		const float *r = &_rot[9 * k], *p = &_pos[3 * k];
		Common::Array<Vtx> &vs = _verts[k];
		vs.resize(nd.vertices.size());
		for (uint vi = 0; vi < nd.vertices.size(); vi++) {
			const Vec3i &v = nd.vertices[vi];
			vs[vi].x = r[0] * v.x + r[1] * v.y + r[2] * v.z + p[0];
			vs[vi].y = r[3] * v.x + r[4] * v.y + r[5] * v.z + p[1];
			vs[vi].z = r[6] * v.x + r[7] * v.y + r[8] * v.z + p[2];
		}
	}

	// Opaque and keyed groups, then the -4 blend over them (render.md "Face groups").
	for (int pass = 0; pass < 2; pass++) {
		_blendPass = pass == 1;
		for (int k : order) {
			const Node &nd = scene.nodes[k];
			if (!_visible[k] || (nd.flags & 4))
				continue;
			// The eye in node space for the plane test: e = W^T (-T).
			const float *r = &_rot[9 * k], *p = &_pos[3 * k];
			const float e[3] = {
				-(r[0] * p[0] + r[3] * p[1] + r[6] * p[2]),
				-(r[1] * p[0] + r[4] * p[1] + r[7] * p[2]),
				-(r[2] * p[0] + r[5] * p[1] + r[8] * p[2])
			};
			for (const FaceGroup &g : nd.faceGroups) {
				if ((g.type == -4) != _blendPass)
					continue;
				Fill fill;
				fill.type = g.type;
				fill.tex = g.texture;
				fill.node = k;
				const uint16 base = g.materialIndex >= 0 ? scene.materials[g.materialIndex].colour : 0;
				for (uint pi = 0; pi < g.polys.size(); pi++) {
					const Poly &poly = g.polys[pi];
					// Type 1: the material colour plus 0x1388 per poly in group order.
					fill.colour = (uint16)(base + (pi + 1) * 0x1388);
					Vtx tri[3];
					bool ok = true;
					for (int c = 0; c < 3 && ok; c++) {
						const int owner = poly.vertexNode[c];
						if (owner < 0 || poly.vertex[c] < 0 || !_visible[owner] || (uint)poly.vertex[c] >= _verts[owner].size()) {
							ok = false;
							break;
						}
						tri[c] = _verts[owner][poly.vertex[c]];
						if (tri[c].z >= _far)
							ok = false; // any corner at or beyond far drops the triangle
						if (poly.uv[c] >= 0) {
							tri[c].u = nd.uvs[2 * poly.uv[c]] / 65536.0f;
							tri[c].v = nd.uvs[2 * poly.uv[c] + 1] / 65536.0f;
						} else {
							tri[c].u = tri[c].v = 0;
						}
					}
					if (!ok)
						continue;
					// Back faces (render.md): the plane test, or by the corners for bit 3.
					if (poly.word0 & 8) {
						const float a[3] = { tri[0].x - tri[1].x, tri[0].y - tri[1].y, tri[0].z - tri[1].z };
						const float b[3] = { tri[1].x - tri[2].x, tri[1].y - tri[2].y, tri[1].z - tri[2].z };
						const float cr[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
						if (tri[0].x * cr[0] + tri[0].y * cr[1] + tri[0].z * cr[2] >= 0)
							continue;
					} else if (poly.faceNormal >= 0) {
						const Vec3i &fn = nd.faceNormals[poly.faceNormal];
						if ((fn.x * e[0] + fn.y * e[1] + fn.z * e[2]) / 32768.0f < poly.planeDistance)
							continue;
					}
					clipAndDraw(dst, tri, fill);
				}
			}
		}
	}
}

void Renderer3D::clipAndDraw(Graphics::Surface &dst, Vtx *v, const Fill &fill) {
	// Clip against the near plane; a triangle becomes 0, 1 or 2 triangles.
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
			if (n < 4)
				in[n++] = c;
		}
	}
	if (n < 3)
		return;
	// Projected corners are truncated to whole pixels (render.md "Integer snapping").
	const float cx = (float)(_viewport.width() / 2 + _viewport.left);
	const float cy = (float)(_viewport.height() / 2 + _viewport.top);
	for (int i = 0; i < n; i++) {
		in[i].sx = truncf(_focal * in[i].x / in[i].z + cx);
		in[i].sy = truncf(_focal * in[i].y / in[i].z + cy);
	}
	drawTriangle(dst, in, fill);
	if (n == 4) {
		Vtx t2[3] = { in[0], in[2], in[3] };
		drawTriangle(dst, t2, fill);
	}
}

void Renderer3D::drawTriangle(Graphics::Surface &dst, const Vtx *v, const Fill &fill) {
	// Only counter-clockwise triangles on the y-down screen are drawn (negative area).
	const float area = (v[2].sy - v[1].sy) * (v[1].sx - v[0].sx) - (v[1].sy - v[0].sy) * (v[2].sx - v[1].sx);
	if (area >= 0)
		return;
	int minX = (int)MIN(v[0].sx, MIN(v[1].sx, v[2].sx));
	int maxX = (int)MAX(v[0].sx, MAX(v[1].sx, v[2].sx));
	int minY = (int)MIN(v[0].sy, MIN(v[1].sy, v[2].sy));
	int maxY = (int)MAX(v[0].sy, MAX(v[1].sy, v[2].sy));
	minX = MAX<int>(minX, _viewport.left);
	maxX = MIN<int>(maxX, _viewport.right - 1);
	minY = MAX<int>(minY, _viewport.top);
	maxY = MIN<int>(maxY, _viewport.bottom - 1);
	if (minX > maxX || minY > maxY)
		return;
	float iz[3], uz[3], vz[3];
	for (int i = 0; i < 3; i++) {
		iz[i] = 1.0f / v[i].z;
		uz[i] = v[i].u * iz[i];
		vz[i] = v[i].v * iz[i];
	}
	const float den = (v[1].sx - v[0].sx) * (v[2].sy - v[0].sy) - (v[2].sx - v[0].sx) * (v[1].sy - v[0].sy);
	const float rden = 1.0f / den;
	for (int y = minY; y <= maxY; y++) {
		const float py = y + 0.5f;
		uint16 *row = (uint16 *)dst.getBasePtr(0, y);
		for (int x = minX; x <= maxX; x++) {
			const float px = x + 0.5f;
			const float w0 = ((v[1].sx - px) * (v[2].sy - py) - (v[2].sx - px) * (v[1].sy - py)) * rden;
			const float w1 = ((v[2].sx - px) * (v[0].sy - py) - (v[0].sx - px) * (v[2].sy - py)) * rden;
			const float w2 = 1.0f - w0 - w1;
			if (w0 < 0 || w1 < 0 || w2 < 0)
				continue;
			const float z = w0 * iz[0] + w1 * iz[1] + w2 * iz[2];
			const int at = y * 640 + x;
			// Picking sees every covered pixel, key texels included (render.md "Picking").
			if (z > _pickZ[at]) {
				_pickZ[at] = z;
				_pickNode[at] = fill.node;
			}
			if (z <= _zbuf[at])
				continue;
			uint16 colour;
			if (fill.type == 1 || !fill.tex) {
				colour = fill.colour;
			} else {
				const float rz = 1.0f / z;
				const int u = (int)floorf((w0 * uz[0] + w1 * uz[1] + w2 * uz[2]) * rz);
				const int t = (int)floorf((w0 * vz[0] + w1 * vz[1] + w2 * vz[2]) * rz);
				int idx;
				if (fill.type == -4)
					idx = ((t & 255) << 8) | (u & 255);
				else
					idx = (t * 256 + u) & 0xFFFF; // type 3 runs on into the next row; -6 wraps
				const byte texel = fill.tex->texels[idx];
				if (fill.type == -6 && texel == 0)
					continue;
				colour = fill.tex->shades[kShadeRow][texel];
				if (fill.type == -4) {
					// (src + dst) / 2 per channel; depth not written.
					row[x] = ((colour & 0xF7DE) + (row[x] & 0xF7DE)) >> 1;
					continue;
				}
			}
			_zbuf[at] = z;
			row[x] = colour;
		}
	}
}

int Renderer3D::pick(int x, int y) const {
	// render.md "Picking": edges inclusive.
	if (x < _viewport.left || x > _viewport.right || y < _viewport.top || y > _viewport.bottom)
		return -1;
	x = MIN<int>(x, 639);
	y = MIN<int>(y, 479);
	return _pickNode[y * 640 + x];
}

bool Renderer3D::nodeViewPosition(int node, int32 &x, int32 &y, int32 &z) const {
	if (node < 0 || (uint)(3 * node + 2) >= _pos.size())
		return false;
	// The node's +0x4c: its position in camera space.
	x = (int32)_pos[3 * node];
	y = (int32)_pos[3 * node + 1];
	z = (int32)_pos[3 * node + 2];
	return true;
}

} // End of namespace Peintre
