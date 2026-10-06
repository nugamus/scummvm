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


#ifndef GRUMPA_WALK_H
#define GRUMPA_WALK_H

#include "common/array.h"

#include "grumpa/mesh.h"

namespace Grumpa {

// A scene's walk mesh and links, the first two records of its Scene_<NNN>.scn
// (docs/spec/walking.md): CFXFloor (type 8, id 600) and CFXToScene (type 0x14, id 601).
struct Floor {
	struct Exit {
		Vec3 c;
		float r;
		int scene;
	};
	struct Entry {
		Vec3 pos, rot;
		int scene;
	};
	// An active 0x1a mesh actor with +0x1d0 = 1 (E-1600): walked on at its frame-0 x/z, at the
	// height of its current frame.
	struct Platform {
		const Mesh *mesh;
		int frame;
	};

	Common::Array<Vec3> verts;
	Common::Array<uint16> faces;  // 3 vertex indices a face
	Common::Array<uint16> types;  // the floor type of each face
	Common::Array<int32> next;    // 3 a face: the face across edge v[i]->v[i+1], -1 = boundary
	bool closed[29];              // +0x3048: floor types > 18 that block while set (E-0803)
	Common::Array<Exit> exits;
	Common::Array<Entry> entries;

	Floor() { reset(); }
	void reset();
	/** Read Scenes/Scene_<num>.scn's floor and links; false (and empty) if absent. */
	bool load(int num);
	bool empty() const { return faces.empty(); }
	/** The face under (x, z): `hint`, then its neighbours, then every face; -1 if none. */
	int faceAt(float x, float z, int hint = -1) const;
	/** The floor height at (x, z) on face `f` (E-0801). */
	float height(int f, float x, float z) const;
	/** The floor's opcodes (E-0803): 5 closes, 6 opens floor type `arg - 1` (0: all). */
	void command(int op, int arg);
	/** CFXFloor::Move (E-0802, E-1600): move `pos` by `delta`, onto one of `platforms` or kept
	 *  `radius` off the walls; the floor type rules are the caller's. `face` is the face under
	 *  the result (-1: off the mesh), `platform` the index of the platform it is on (-1: none). */
	void move(Vec3 &pos, Vec3 delta, float radius, int &face, int &platform,
			  const Common::Array<Platform> &platforms) const;

private:
	bool inFace(int f, float x, float z) const;
	void contacts(int f, float x, float z, float radius, Common::Array<bool> &seen,
				  float &best, float &bx, float &bz) const;
};

} // End of namespace Grumpa

#endif // GRUMPA_WALK_H
