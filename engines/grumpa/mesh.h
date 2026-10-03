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

// A view's camera (docs/spec/scene.md): the Direct3D view and projection matrices the
// original hands to SetTransform (row vectors, left-handed: clip = v * view * proj).
struct Camera {
	float view[16];
	float proj[16];
};

// One pre-rendered view of a scene (CFXView entry, from Scene_<NNN>.scn): its colour
// background, its depth buffer and its camera.
struct SceneView {
	Common::String colour, depth;  // "<n>_<k>_IS.jpg", "<n>_<k>_IZ.fxi"
	Camera cam;
};

// A scene light (type 0x11 CFXLight): the D3DLIGHT7 block of its .abi record, enabled at
// load. d[0] type (1 = point), d[1..4] diffuse rgba, d[13..15] position, d[19] range,
// d[21..23] attenuation.
struct SceneLight {
	uint32 id = 0;
	float d[26] = {};
	bool on = true;
};

// An animated 2D sprite prop (type 0x0d CFXSprite, E-0106/E-0107, docs/spec/scene.md): a
// JPG frame sequence (and optional _Z depth frames) drawn at a screen position in its view.
struct SceneSprite {
	Common::String name;   // frame-0 JPG base, e.g. "cannons_0000.jpg"
	uint32 id = 0;
	int x = 0, y = 0;
	bool active = false;   // +0x10c: animating/updating (E-0111)
	bool visible = false;  // +0x110: drawn
	int layer = 0;         // +0x114: render order (1 behind the 3D actors, 4 in front)
	int fps = 0;           // +0x1e0: frames per second (one frame per 50/fps game ticks)
	uint32 anim = 0;       // +0x1e4: animation flags (4 forward, 8 backward, 2 ping-pong, 1 loop)
	int view = -1;         // +0x1c8: the view it belongs to (-1 every view)
	bool keyed = false;    // +0x1f8: blit with a source colour key
	int32 keyColor = -1;   // +0x208: key COLORREF (0x00BBGGRR), -1 = frame 0's pixel (0,0)
	int frameCount = 0;    // frames found on disk (entering the scene)
	bool hasDepth = false; // _Z depth frames exist
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

// A 3D animated-mesh actor (type 0x1a CFXStaticCharacter, E-0114): an .anb mesh (+ .tga
// texture) authored in world space, drawn through the current view's camera, lit by the
// scene's lights. `visible` gates whether it is drawn (docs/spec/scene.md).
struct SceneMesh {
	Common::String anb, tga;
	uint32 id = 0;
	bool active = false;
	bool visible = false;
	Mesh mesh;                   // loaded once on scene entry
	Graphics::Surface texture;   // .tga, as 32-bit ARGB (alpha used when the .tga has it)
	bool alpha = false;          // 32-bit .tga: alpha-blended
};

// Everything the engine reads from a Scene_<NNN>.abi today.
struct SceneData {
	Common::Array<SceneView> views;     // from Scene_<NNN>.scn
	int view = 0;                       // the current view (185 op 30 changes it)
	Common::Array<SceneLight> lights;
	Common::Array<SceneSprite> sprites;
	Common::Array<SceneTrigger> triggers;
	Common::Array<SceneMesh> meshes;
};

/** A look-at camera (dev views of a lone mesh): Direct3D-style left-handed matrices. */
Camera lookAtCamera(const Vec3 &eye, const Vec3 &target, float fovY, float zn, float zf);

/** Rasterise `mesh` into `screen` (RGB555) through `cam` like the original's Direct3D 7
 *  device (docs/spec/scene.md): counter-clockwise faces culled, Gouraud lighting from
 *  `lights` over the 0x1e1e1e ambient, `tex` (ARGB8888, may be null) modulated with
 *  bilinear filtering, alpha-blended when `alpha`. `depth` (16-bit, screen-sized) is the
 *  z-buffer: tested less-or-equal and written. */
void renderMesh(Graphics::ManagedSurface &screen, const Mesh &mesh, const Camera &cam,
				const Common::Array<SceneLight> &lights, Common::Array<uint16> &depth,
				const Graphics::Surface *tex = nullptr, bool alpha = false);

} // End of namespace Grumpa

#endif // GRUMPA_MESH_H
