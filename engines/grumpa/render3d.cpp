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

// A small perspective, z-buffered, flat-shaded triangle rasteriser (engine's own code, from
// graphics first principles). It draws actor meshes (E-0014) into the 800x600 page. The
// original composites against the scene's .fxi depth buffer; wiring the actor's view-space Z
// to those depth values needs the camera's near/far calibration, so for now each mesh uses
// its own z-buffer over the background. Textured shading and scene-depth occlusion follow.

#include "common/array.h"
#include "common/scummsys.h"
#include "graphics/managed_surface.h"

#include "grumpa/mesh.h"

namespace Grumpa {

struct Vertex2D {
	float sx, sy, z;   // screen x/y and view-space depth
	bool behind;       // clipped by the near plane
};

static Vec3 normalize(const Vec3 &v) {
	float len = sqrtf(v.dot(v));
	return len > 1e-6f ? Vec3(v.x / len, v.y / len, v.z / len) : v;
}

// The scene .fxi holds the render device's own 16-bit depth (E-0010). We don't yet know the
// exact view-space-Z -> z16 mapping (Q-0008 device math), so assume a linear ramp over the
// camera's far/range and occlude where the scene is nearer. Returns 0xFFFF (farthest) when
// the actor is behind the camera.
static inline uint16 depth16(float vz, float farZ) {
	if (vz <= 0.0f || farZ <= 0.0f)
		return 0xFFFF;
	float t = vz / farZ;
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;
	return (uint16)(t * 65535.0f);
}

void renderMesh(Graphics::ManagedSurface &screen, const Mesh &mesh, const Camera &cam,
				const Common::Array<uint16> *depth, int dw, int dh) {
	const int W = screen.w, H = screen.h;
	const bool useScene = depth && !depth->empty() && dw == W && dh == H;
	Common::Array<float> zbuf;
	if (!useScene) {
		zbuf.resize(W * H);
		for (uint i = 0; i < zbuf.size(); i++)
			zbuf[i] = 1e30f;
	}

	// Camera basis: right, up, forward (right-handed, looking along +forward).
	Vec3 fwd = normalize(cam.forward);
	Vec3 right = normalize(fwd.cross(cam.up));
	Vec3 up = right.cross(fwd);
	Vec3 light = normalize(Vec3(-0.3f, -0.6f, -0.7f));

	for (uint s = 0; s < mesh.sections.size(); s++) {
		const MeshSection &sec = mesh.sections[s];
		Common::Array<Vertex2D> proj;
		proj.resize(sec.verts.size());
		for (uint i = 0; i < sec.verts.size(); i++) {
			Vec3 rel = sec.verts[i] - cam.eye;
			float vx = rel.dot(right), vy = rel.dot(up), vz = rel.dot(fwd);
			Vertex2D &p = proj[i];
			p.z = vz;
			p.behind = vz < 0.01f;
			if (!p.behind) {
				p.sx = (0.5f + 0.5f * cam.projX * vx / vz) * W;
				p.sy = (0.5f - 0.5f * cam.projY * vy / vz) * H;
			}
		}
		for (uint fi = 0; fi < sec.faces.size(); fi++) {
			const Face &face = sec.faces[fi];
			const Vertex2D &a = proj[face.v[0]], &b = proj[face.v[1]], &c = proj[face.v[2]];
			if (a.behind || b.behind || c.behind)
				continue;
			// Flat shade from the triangle's geometric normal.
			Vec3 n = normalize((sec.verts[face.v[1]] - sec.verts[face.v[0]])
							   .cross(sec.verts[face.v[2]] - sec.verts[face.v[0]]));
			float lit = 0.35f + 0.65f * MAX(0.0f, -n.dot(light));
			byte r = (byte)(200 * lit), g = (byte)(150 * lit), bl = (byte)(110 * lit);
			uint32 col = screen.format.RGBToColor(r, g, bl);

			int minx = (int)floorf(MIN(a.sx, MIN(b.sx, c.sx)));
			int maxx = (int)ceilf(MAX(a.sx, MAX(b.sx, c.sx)));
			int miny = (int)floorf(MIN(a.sy, MIN(b.sy, c.sy)));
			int maxy = (int)ceilf(MAX(a.sy, MAX(b.sy, c.sy)));
			minx = MAX(minx, 0); miny = MAX(miny, 0);
			maxx = MIN(maxx, W - 1); maxy = MIN(maxy, H - 1);
			float area = (b.sx - a.sx) * (c.sy - a.sy) - (b.sy - a.sy) * (c.sx - a.sx);
			if (area == 0.0f)
				continue;
			for (int y = miny; y <= maxy; y++) {
				for (int x = minx; x <= maxx; x++) {
					float px = x + 0.5f, py = y + 0.5f;
					float w0 = ((b.sx - px) * (c.sy - py) - (b.sy - py) * (c.sx - px)) / area;
					float w1 = ((c.sx - px) * (a.sy - py) - (c.sy - py) * (a.sx - px)) / area;
					float w2 = 1.0f - w0 - w1;
					if (w0 < 0 || w1 < 0 || w2 < 0)
						continue;
					float z = w0 * a.z + w1 * b.z + w2 * c.z;
					if (useScene) {
						// Occlude against the pre-rendered scene depth.
						if (depth16(z, cam.farZ) >= (*depth)[y * W + x])
							continue;
						screen.setPixel(x, y, col);
					} else {
						float &zb = zbuf[y * W + x];
						if (z < zb) {
							zb = z;
							screen.setPixel(x, y, col);
						}
					}
				}
			}
		}
	}
}

} // End of namespace Grumpa
