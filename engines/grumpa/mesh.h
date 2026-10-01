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

#ifndef GRUMPA_MESH_H
#define GRUMPA_MESH_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

namespace Grumpa {

struct Vec3 {
	float x, y, z;
	Vec3(float x_ = 0, float y_ = 0, float z_ = 0) : x(x_), y(y_), z(z_) {}
	Vec3 operator-(const Vec3 &o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
	Vec3 cross(const Vec3 &o) const { return Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x); }
	float dot(const Vec3 &o) const { return x * o.x + y * o.y + z * o.z; }
};

// A face references three vertices and (separately) three texture coordinates (E-0014).
struct Face {
	uint16 v[3];
	uint16 uv[3];
};

// One drawable section of a mesh: vertices, normals, triangles and UVs.
struct MeshSection {
	Common::Array<Vec3> verts;
	Common::Array<Vec3> normals;
	Common::Array<Face> faces;
	Common::Array<float> u, v;  // per-uv-index texture coordinates
};

struct Mesh {
	int frames = 0;
	Common::Array<MeshSection> sections;
	bool empty() const { return sections.empty(); }
};

// A fixed camera for a pre-rendered view (docs/spec/scene.md, E-0105): eye position, look
// basis and the per-axis projection scales from the .abi camera block (NDC = proj*v/z).
struct Camera {
	Vec3 eye;
	Vec3 forward, up;
	float projX, projY;  // camera block[2], block[3]; 1.0 == 90 degrees
	float farZ;          // camera block[19], the far/range value
};

// One pre-rendered view of a scene: its id, the camera id and the 26-float camera block
// (docs/spec/scene.md). Read from a type-0x11 record in Scene_<NNN>.abi.
struct SceneView {
	uint32 id = 0;
	uint32 camId = 0;
	float cam[26] = {};
};

// An animated 2D sprite prop (type 0x0d, E-0106/E-0107): a JPG frame sequence drawn at a
// screen position with a colour key. `frames`/`fps` come from the record's animation block.
struct SceneSprite {
	Common::String name;   // frame-0 JPG base, e.g. "cannons_0000.jpg"
	uint32 id = 0;
	int x = 0, y = 0;
	bool active = false;   // +0x10c: animating/updating (E-0111)
	bool visible = false;  // +0x110: drawn
	int frames = 0, fps = 0;
};

// One command in a trigger's list (E-0109/E-0110): apply `opcode` to the actor `targetId`
// after `when` ticks (-1 = immediately on the click). `hasCond` marks a command guarded by a
// condition on another actor's state (honoured later; for now such commands are skipped).
struct SceneCommand {
	int when = -1;
	int targetId = 0;
	int opcode = 0;
	int arg1 = 0, arg2 = 0;
	bool hasCond = false;
};

// A clickable trigger (type 0x19, E-0108): a screen polygon and the commands it runs when
// clicked. `once` goes true after it fires if it disables itself (opcode 13 on its own id).
struct SceneTrigger {
	uint32 id = 0;
	Common::Array<Common::Point> poly;
	Common::Array<SceneCommand> cmds;
	bool spent = false;
};

// A 3D animated-mesh actor (type 0x1a, E-0114): an .anb mesh (+ .tga texture) authored in
// world space, drawn through the view camera. `active` gates whether it is drawn (the scene's
// default-state objects start active; the rest are shown by commands).
struct SceneMesh {
	Common::String anb, tga;
	uint32 id = 0;
	bool active = false;
	bool visible = false;
	Mesh mesh;   // loaded once on scene entry
};

// Everything the engine reads from a Scene_<NNN>.abi today.
struct SceneData {
	Common::Array<SceneView> views;
	Common::Array<SceneSprite> sprites;
	Common::Array<SceneTrigger> triggers;
	Common::Array<SceneMesh> meshes;
};

/** Build the view camera from its 26-float .abi block (E-0105). */
Camera cameraFromBlock(const float cam[26]);

/** Rasterise `mesh` (flat-shaded) into `screen` through `cam`, z-testing against `depth`
 *  (16-bit, the scene's .fxi, `dw`x`dh`) when `depth` is non-empty. Its own z-buffer
 *  otherwise. */
void renderMesh(Graphics::ManagedSurface &screen, const Mesh &mesh, const Camera &cam,
				const Common::Array<uint16> *depth, int dw = 0, int dh = 0);

} // End of namespace Grumpa

#endif // GRUMPA_MESH_H
