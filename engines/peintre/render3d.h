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

/**
 * A software renderer drawing the 3D scene into the 640x480 RGB565 frame with the
 * original's rules (render.md): unlit shade row 16, single-sided counter-clockwise
 * faces, per-pixel nearest 1/z, group types 3 (opaque), -6 (texel 0 keyed), -4 (50%
 * blend) and 1 (flat colour).
 */
class Renderer3D {
public:
	Renderer3D();

	/** Near and far planes: 64 / 80,000 after a scene load, 128 / 65,000 after the option menu. */
	void setClip(float nearZ, float farZ) {
		_near = nearZ;
		_far = farZ;
	}
	/** Draws the scene in the viewport of `dst`; outside it `dst` is untouched. */
	void draw(Graphics::Surface &dst, const Common::Rect &viewport, const Scene3D &scene, const Camera &cam);
	/** The node under (x, y) after the last draw, -1 for none (render.md "Picking"). */
	int pick(int x, int y) const;
	/** A node's position in camera space after the last draw (its +0x4c). */
	bool nodeViewPosition(int node, int32 &x, int32 &y, int32 &z) const;

private:
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
	void clipAndDraw(Graphics::Surface &dst, Vtx *v, const Fill &fill);
	void drawTriangle(Graphics::Surface &dst, const Vtx *v, const Fill &fill);

	Common::Rect _viewport;
	float _focal = 480.0f;
	float _near = 64.0f, _far = 80000.0f;
	Common::Array<float> _zbuf;       ///< 1/z per pixel of the frame
	Common::Array<float> _pickZ;      ///< 1/z of every covered pixel, key pixels included
	Common::Array<int16> _pickNode;
	Common::Array<float> _rot;        ///< per node: camera-space rotation (9)
	Common::Array<float> _pos;        ///< per node: camera-space position (3)
	Common::Array<float> _viewPos;    ///< per node: world position minus the eye (3)
	Common::Array<bool> _visible;     ///< per node: not in a hidden subtree
	Common::Array<Common::Array<Vtx> > _verts; ///< per node: camera-space vertices
	bool _blendPass = false;
};

} // End of namespace Peintre

#endif // PEINTRE_RENDER3D_H
