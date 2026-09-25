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

#include "common/serializer.h"

#include "x3d/collision.h"
#include "x3d/scene.h"

namespace X3D {

using Math::Vector3d;

void Collision::build(const Scene &scene) {
	_objects.clear();
	for (const Scene::Model *m : scene.models()) {
		const Common::Array<O3DObject> &objects = m->file.objects;

		// Depth first: an object, then its children in file order
		Common::Array<int> order, stack;
		for (int i = objects.size() - 1; i >= 0; i--)
			if (objects[i].parent < 0)
				stack.push_back(i);
		while (!stack.empty()) {
			const int i = stack.back();
			stack.pop_back();
			order.push_back(i);
			for (int j = objects.size() - 1; j > i; j--)
				if (objects[j].parent == i)
					stack.push_back(j);
		}

		for (int i : order) {
			if (objects[i].faces.empty())
				continue;
			Object obj;
			obj.model = m;
			obj.object = i;
			fill(obj);
			_objects.push_back(obj);
		}
	}
}

void Collision::refresh() {
	for (Object &obj : _objects)
		if (((const Scene::Model *)obj.model)->animated)
			fill(obj);
}

void Collision::syncState(Common::Serializer &s) {
	uint32 n = _objects.size();
	s.syncAsUint32LE(n);
	for (uint32 i = 0; i < n; i++) {
		byte enabled = i < _objects.size() ? _objects[i].enabled : 1;
		s.syncAsByte(enabled);
		if (s.isLoading() && i < _objects.size())
			_objects[i].enabled = enabled;
	}
}

void Collision::setEnabled(const Common::String &name, bool enabled, bool subtree) {
	for (Object &obj : _objects) {
		const Common::Array<O3DObject> &objects = ((const Scene::Model *)obj.model)->file.objects;
		for (int o = obj.object; o >= 0; o = subtree ? objects[o].parent : -1) {
			if (objects[o].name.equalsIgnoreCase(name)) {
				obj.enabled = enabled;
				break;
			}
		}
	}
}

void Collision::fill(Object &obj) const {
	const Scene::Model *m = (const Scene::Model *)obj.model;
	const Common::Array<O3DObject> &objects = m->file.objects;
	const O3DObject &o = objects[obj.object];
	obj.faces.clear();

	// Welded objects use the vertices of their top object (animation.md)
	int owner = obj.object;
	while (owner >= 0 && objects[owner].vertices.empty())
		owner = objects[owner].parent;
	if (owner < 0)
		return;
	const Common::Array<float> &vertices = m->worldVertices[owner];
	const float *w = objects[owner].world;

	Vector3d lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
	for (const O3DFace &f : o.faces) {
		Face face;
		for (uint32 index : f.indices) {
			if (index * 3 + 2 >= vertices.size())
				break;
			const Vector3d v(&vertices[index * 3]);
			face.vertices.push_back(v);
			for (int k = 0; k < 3; k++) {
				lo.getData()[k] = MIN(lo.getData()[k], v.getData()[k]);
				hi.getData()[k] = MAX(hi.getData()[k], v.getData()[k]);
			}
		}
		if (face.vertices.size() < 3)
			continue;
		// The stored normal is object-local; rotate it with the object (row vectors)
		const float *n = f.normal;
		face.normal.set(n[0] * w[0] + n[1] * w[4] + n[2] * w[8],
		                n[0] * w[1] + n[1] * w[5] + n[2] * w[9],
		                n[0] * w[2] + n[1] * w[6] + n[2] * w[10]);
		face.normal.normalize();
		obj.faces.push_back(face);
	}
	obj.center = (lo + hi) * 0.5f;
	obj.radius = (hi - lo).getMagnitude() * 0.5f;
}

bool Collision::inside(const Face &face, const Vector3d &p) {
	// Faces are drawn as triangle fans; p (on the plane) is inside if it is in any of them
	const Common::Array<Vector3d> &v = face.vertices;
	for (uint i = 1; i + 1 < v.size(); i++) {
		const Vector3d *tri[3] = { &v[0], &v[i], &v[i + 1] };
		int positive = 0, negative = 0;
		for (int k = 0; k < 3; k++) {
			const Vector3d edge = *tri[(k + 1) % 3] - *tri[k];
			const float s = Vector3d::dotProduct(Vector3d::crossProduct(edge, p - *tri[k]), face.normal);
			positive += s >= 0;
			negative += s <= 0;
		}
		if (positive == 3 || negative == 3)
			return true;
	}
	return false;
}

Vector3d Collision::nearestOnBoundary(const Face &face, const Vector3d &p) {
	const Common::Array<Vector3d> &v = face.vertices;
	Vector3d best;
	float bestDistance = 1e30f;
	for (uint i = 0; i < v.size(); i++) {
		const Vector3d &a = v[i], &b = v[(i + 1) % v.size()];
		const Vector3d ab = b - a;
		const float length = Vector3d::dotProduct(ab, ab);
		const float t = length > 0 ? CLIP(Vector3d::dotProduct(p - a, ab) / length, 0.0f, 1.0f) : 0.0f;
		const Vector3d q = a + ab * t;
		const float d = (p - q).getMagnitude();
		if (d < bestDistance) {
			bestDistance = d;
			best = q;
		}
	}
	return best;
}

Vector3d Collision::resolveSphere(Vector3d c, float r) const {
	const float cos45 = 0.70710678f;
	for (const Object &o : _objects) {
		if (!o.enabled || o.faces.empty() || (c - o.center).getMagnitude() >= r + o.radius)
			continue;
		for (const Face &f : o.faces) {
			const float d = Vector3d::dotProduct(c - f.vertices[0], f.normal);
			if (d <= 0 || d >= r)
				continue;

			Vector3d q = c - f.normal * d, m = f.normal;
			if (!inside(f, q)) {
				q = nearestOnBoundary(f, c);
				const float distance = (c - q).getMagnitude();
				if (distance >= r || distance == 0)
					continue;
				m = (c - q) * (1.0f / distance);
			}
			// Floors and ceilings never push; the ground step handles them
			if (fabs(m.z()) >= cos45)
				return c;
			c = q + m * r;
		}
	}
	return c;
}

bool Collision::cast(const Vector3d &from, const Vector3d &to, float &t, Common::String *hitName) const {
	bool hit = false;
	t = 1;
	for (const Object &o : _objects) {
		if (!o.enabled)
			continue;
		for (const Face &f : o.faces) {
			const float d0 = Vector3d::dotProduct(from - f.vertices[0], f.normal);
			const float d1 = Vector3d::dotProduct(to - f.vertices[0], f.normal);
			if (d0 < 0 || d1 >= 0)
				continue;
			const float u = d0 / (d0 - d1);
			if (u >= t || !inside(f, from + (to - from) * u))
				continue;
			t = u;
			hit = true;
			if (hitName)
				*hitName = ((const Scene::Model *)o.model)->file.objects[o.object].name;
		}
	}
	return hit;
}

} // End of namespace X3D
