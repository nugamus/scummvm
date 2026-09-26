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

#ifndef X3D_COLLISION_H
#define X3D_COLLISION_H

#include "common/array.h"
#include "common/str-array.h"

#include "math/vector3d.h"

#include "x3d/scene.h"

namespace Common {
class Serializer;
}

namespace X3D {

// World-space collision against a scene's objects (docs/engine-spec/movement.md, Collision)
class Collision {
public:
	// Every object of every loaded file, visible or hidden
	void build(const Scene &scene);
	// Re-reads the faces of animated files (their objects move)
	void refresh();
	// Collision on or off for the named object, and optionally everything below it
	void setEnabled(const Common::String &name, bool enabled, bool subtree = false);
	void syncState(Common::Serializer &s); // the enabled flags

	// Sphere resolution: pushes the centre c of a sphere of radius r out of the faces
	Math::Vector3d resolveSphere(Math::Vector3d c, float r) const;

	// Nearest face crossed from its front side by the segment from -> to; t in [0, 1].
	// hitName, if given, receives the hit object's name.
	bool cast(const Math::Vector3d &from, const Math::Vector3d &to, float &t, Common::String *hitName = nullptr) const;

private:
	struct Face {
		Common::Array<Math::Vector3d> vertices;
		Math::Vector3d normal;
	};

	struct Object {
		const Scene::Model *model;
		uint object;
		bool enabled = true;
		Math::Vector3d center;
		float radius;
		Common::Array<Face> faces;
	};

	void fill(Object &obj) const;

	static bool inside(const Face &face, const Math::Vector3d &p);
	static Math::Vector3d nearestOnBoundary(const Face &face, const Math::Vector3d &p);

	Common::Array<Object> _objects; // depth first through each file's hierarchy
};

} // End of namespace X3D

#endif // X3D_COLLISION_H
