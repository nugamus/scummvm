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

#ifndef PEINTRE_OBJ3D_H
#define PEINTRE_OBJ3D_H

#include "common/array.h"
#include "common/str.h"

#include "peintre/bfg.h"

namespace Peintre {

// The 3D objects inside a BFG (docs/formats/obj3d.ksy, README "objects inside a BFG").

struct Vec3i {
	int32 x, y, z;
};

struct Poly {
	int32 vertex[3];       ///< index into Node::vertices
	int32 normal[3];       ///< index into Node::vertexNormals, -1 if none
	int32 uv[3];           ///< index into Node::uvs, -1 when the group is untextured
	int32 faceNormal;      ///< index into Node::faceNormals, -1 if none
	int32 unk30;           ///< poly +0x30 (s32, meaning open)
};

struct FaceGroup {
	int32 type;            ///< face-group type (3, -6, -4, 1 in the corpus)
	Common::String material;
	int materialIndex;     ///< into Scene3D::materials, -1 when no material matches
	Common::Array<Poly> polys;
};

struct Node {
	Common::String name;
	uint32 flags;
	int parent;            ///< index into Scene3D::nodes, -1 for the root
	Common::Array<int> children;
	Vec3i position;        ///< local, s32
	int32 rotation[9];     ///< local 3x3, Q15 (0x8000 = 1)
	Common::Array<Vec3i> vertices;
	Common::Array<uint32> vertexFlags;
	Common::Array<int32> uvs;          ///< pairs of 16.16 texel coordinates
	Common::Array<Vec3i> vertexNormals; ///< Q15
	Common::Array<Vec3i> faceNormals;   ///< Q15
	Common::Array<FaceGroup> faceGroups;
};

struct Material {
	Common::String name;
	Common::String texture;  ///< "<texture>.3DM" in the same BFG, empty for none
};

/** A .3DC scene: every node, the first one is the root. */
struct Scene3D {
	Common::Array<Node> nodes;
	Common::Array<Material> materials;

	bool load(const Common::Array<byte> &entry);
	int findNode(const Common::String &name) const;
};

/** A .3DM texture: 32 shade levels of 256 RGB565 colours, 256x256 colour indices. */
struct Texture3D {
	uint16 shades[32][256]; ///< level 0 brightest
	Common::Array<byte> texels; ///< 256 * 256, padded or cut to that size

	bool load(const Common::Array<byte> &entry);
};

/** A .3DA animation: one track per animated node. */
struct AnimTrack {
	struct RotKey {
		uint32 time;
		int32 q[4];  ///< Q15 (component order: Q-0004)
	};
	struct PosKey {
		uint32 time;
		Vec3i pos;
	};
	uint32 unk0;
	Common::Array<RotKey> rot;
	Common::Array<PosKey> pos;
};

struct Anim3D {
	Common::Array<AnimTrack> tracks;
	bool load(const Common::Array<byte> &entry);
};

/** A .3DI box set. */
struct Boxes3D {
	struct Face {
		int32 ref[5];        ///< words 0..3 and 17 as body offsets, -1 for 0 (fields open)
		byte raw[0x60];      ///< the whole record
	};
	Common::Array<Vec3i> vertices;
	Common::Array<Face> faces;
	Common::Array<Vec3i> items;  ///< 12-byte records: the face normals (movement.md)
	uint32 vertexBase = 0;       ///< body offset of vertices[0]
	uint32 itemBase = 0;         ///< body offset of items[0]
	bool load(const Common::Array<byte> &entry);
};

} // End of namespace Peintre

#endif // PEINTRE_OBJ3D_H
