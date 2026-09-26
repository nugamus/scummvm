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
 * A software renderer drawing the 3D scene into the 640x480 RGB565 frame, like the
 * original (renderer spec, render.md). Textures are the .3DM shade tables and texels.
 */
class Renderer3D {
public:
	Renderer3D();

	/** Draws the scene in the viewport of `dst`. textures[i] is material i's texture or null. */
	void draw(Graphics::Surface &dst, const Common::Rect &viewport, const Scene3D &scene,
			  const Common::Array<const Texture3D *> &textures, const Camera &cam);
	/** The node under (x, y) after the last draw, -1 for none. */
	int pick(int x, int y) const;
	/** The camera-relative position of a node after the last draw (its +0x4c). */
	bool nodeViewPosition(int node, int32 &x, int32 &y, int32 &z) const;

private:
	struct Vtx {
		float x, y, z;   ///< camera space
		float sx, sy;    ///< screen
		float u, v;
		float shade;     ///< shade level 0..31
	};
	void computeWorld(const Scene3D &scene);
	void drawTriangle(Graphics::Surface &dst, const Vtx *v, const Texture3D *tex, int node);
	void clipAndDraw(Graphics::Surface &dst, Vtx *v, const Texture3D *tex, int node);

	Common::Rect _viewport;
	float _focal = 480.0f;
	float _near = 64.0f;
	Common::Array<float> _zbuf;      ///< 1/z per pixel of the frame
	Common::Array<int16> _nodeBuf;   ///< node index per pixel, for picking
	// Per node: world rotation (row-major) and position, then camera-relative.
	Common::Array<float> _rot;       ///< 9 per node
	Common::Array<float> _pos;       ///< 3 per node
	Common::Array<float> _viewPos;   ///< 3 per node
	float _view[9];
	float _camPos[3];
};

} // End of namespace Peintre

#endif // PEINTRE_RENDER3D_H
