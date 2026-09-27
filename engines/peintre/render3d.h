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

#ifndef PEINTRE_RENDER3D_H
#define PEINTRE_RENDER3D_H

#include "common/array.h"
#include "common/rect.h"

#include "graphics/surface.h"

#include "peintre/obj3d.h"

namespace Peintre {

/** The viewer: position and angles in 1/4096 turn (movement.md). */
struct Camera {
	int32 x = 0, y = 0, z = 0;
	int32 pitch = 0, yaw = 0, roll = 0;
};

/** The camera matrix M of movement.md "Rotation matrix" (row-major, Q15). */
void cameraMatrix(int32 pitch, int32 yaw, int32 roll, int32 m[9]);
int32 sinQ15(int32 angle);
int32 cosQ15(int32 angle);

/** What the renderer looks from: the eye and the camera matrix R (columns right, down, forward). */
struct View {
	float eye[3];
	float rot[9];   ///< row-major, 1.0 = 0x8000
};

/** The view of a camera, with the original's Q15 matrix. */
View viewOf(const Camera &cam);
/** A view between ticks: position and angles (1/4096 turn) as floats (enhancement). */
View viewOf(float x, float y, float z, float pitch, float yaw, float roll);

/** A camera-space triangle ready for a hardware renderer (render.md, "for GL" notes). */
struct Tri3D {
	float x[3], y[3], z[3];   ///< camera space: x right, y down, z forward
	float u[3], v[3];         ///< texel units (0..256)
	int type;                 ///< face-group type: 3, -6, -4 or 1
	const Texture3D *tex;
	uint16 colour;            ///< type 1: the flat RGB565 colour
};

/**
 * A software renderer drawing the 3D scene into the 640x480 RGB565 frame with the
 * original's rules (render.md): unlit shade row 16, single-sided counter-clockwise
 * faces, per-pixel nearest 1/z, group types 3 (opaque), -6 (texel 0 keyed), -4 (50%
 * blend) and 1 (flat colour).
 */
class Renderer3D {
public:
	Renderer3D();

	/** The focal length in pixels; 0 (the original) is 480 * view width / 640. */
	void setFocal(float focal) { _fixedFocal = focal; }
	float nearZ() const { return _near; }
	float farZ() const { return _far; }
	/** Near and far planes: 64 / 80,000 after a scene load, 128 / 65,000 after the option menu. */
	void setClip(float nearZ, float farZ) {
		_near = nearZ;
		_far = farZ;
	}
	/**
	 * Draws the scene in the viewport of `dst`; outside it `dst` is untouched. Without `dst`
	 * only picking is updated; with `out` the front-facing triangles are collected for a
	 * hardware renderer instead of drawn. `poses` (12 floats per node: the local rotation,
	 * 1.0 = 0x8000, then the position) replaces the nodes' own.
	 */
	void draw(Graphics::Surface *dst, const Common::Rect &viewport, const Scene3D &scene, const View &view,
			  const float *poses = nullptr, Common::Array<Tri3D> *out = nullptr);
	/** The node under (x, y) after the last draw, -1 for none (render.md "Picking"). */
	int pick(int x, int y) const;
	/** The same through a whole-view buffer, built on first use (many points, dev harness). */
	int pickBuffered(int x, int y);
	/** A point where the last draw can pick the node: its centre or else a vertex. */
	bool nodeScreenPoint(int node, Common::Point &p) const;
	/** A node's position in camera space after the last draw (its +0x4c). */
	bool nodeViewPosition(int node, int32 &x, int32 &y, int32 &z) const;

private:
	int bufferIndex(int x, int y) const { return (y - _viewport.top) * _viewport.width() + (x - _viewport.left); }
	struct Vtx {
		float x, y, z;   ///< camera space
		float sx, sy;    ///< screen, truncated
		float u, v;
	};
	struct Fill {
		int type;
		const Texture3D *tex;
		uint16 colour;
		int node;
	};
	struct PickTri {
		Vtx v[3];
		int node;
	};
	void clipAndDraw(Vtx *v, const Fill &fill);
	void addPickTri(const Vtx *v, const Fill &fill);
	bool covers(const Vtx *v, int x, int y, float &z) const;
	void drawTriangle(const Vtx *v, const Fill &fill);

	Graphics::Surface *_dst = nullptr;
	Common::Array<Tri3D> *_out = nullptr;
	Common::Rect _viewport;
	float _focal = 480.0f, _fixedFocal = 0.0f;
	float _near = 64.0f, _far = 80000.0f;
	Common::Array<float> _zbuf;       ///< 1/z per pixel of the viewport
	Common::Array<float> _pickZ;      ///< 1/z of every covered pixel, key pixels included
	Common::Array<int16> _pickNode;
	Common::Array<PickTri> _pickTris; ///< without a destination: the projected triangles
	bool _pickBuilt = false;
	Common::Array<float> _rot;        ///< per node: camera-space rotation (9)
	Common::Array<float> _pos;        ///< per node: camera-space position (3)
	Common::Array<float> _viewPos;    ///< per node: world position minus the eye (3)
	Common::Array<bool> _visible;     ///< per node: not in a hidden subtree
	Common::Array<Common::Array<Vtx> > _verts; ///< per node: camera-space vertices
	bool _blendPass = false;
};

} // End of namespace Peintre

#endif // PEINTRE_RENDER3D_H
