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

#ifndef X3D_O3D_H
#define X3D_O3D_H

#include "common/array.h"
#include "common/str.h"

namespace Common {
class SeekableReadStream;
}

namespace X3D {

// .O3D geometry: materials, then objects with vertices, normals and faces

struct O3DMaterial {
	Common::String name;
	uint32 renderClass = 2; // 0 unlit, 2 RGB lit
	byte colors[4][3] = {};
	bool wrap = true;       // tiling: false clamps texture coordinates
	uint32 transparency = 0; // percent
	uint32 mode = 0;         // draw mode: 1 and 3 colour-keyed, 2 additive
	Common::String textureMap; // empty when the material has no map
};

struct O3DFace {
	Common::Array<uint32> indices;
	Common::Array<float> uvs; // u, v per index; empty without UVs
	uint32 material = 0;
	float normal[3] = {}; // object-local plane normal, the front side for collision
};

struct O3DObject {
	Common::String name;
	int cameraType = 0; // 1 $XYZ$, 2 $Z$, 3 $XZ$ in the file's name, set by the scene
	int parent = -1; // index into O3DFile::objects
	Common::Array<float> vertices; // x, y, z per vertex, object-local
	Common::Array<float> normals;  // x, y, z per vertex, object-local
	Common::Array<O3DFace> faces;
	// Live transform: loaded from the file, rewritten by animations
	float pivot[3] = {}, localPosition[3] = {}, localScale[3] = {};
	float matrix[16] = {}; // row-vector form, translation in elements 12..14

	// A welded object transforms vertices weldFirst .. weldFirst + ownCount - 1 of its top
	// object's array (the nearest ancestor with vertices); others own [0, ownCount)
	bool welded = false;
	uint32 weldFirst = 0, ownCount = 0;

	// Object-local to world, row-vector form
	float world[16] = {};
};

struct O3DFile {
	Common::Array<O3DMaterial> materials;
	Common::Array<O3DObject> objects;

	bool load(Common::SeekableReadStream &s);
	void updateWorld(); // rebuilds every object's world matrix from its live transform
	// The vertex array the object's faces index: its own, or the nearest ancestor's that
	// has vertices; -1 when none has
	int vertexOwner(int object) const {
		while (object >= 0 && objects[object].vertices.empty())
			object = objects[object].parent;
		return object;
	}
};

} // End of namespace X3D

#endif // X3D_O3D_H
