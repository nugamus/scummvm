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

// A face references three vertices and (separately) three texture coordinates (E-0014). The
// loader points each corner at the vertex the original's vertex buffer holds for its uv index
// (E-0600).
struct Face {
	uint16 v[3];
	uint16 uv[3];
};

// One drawable section of a mesh: vertices and normals of every frame (frame k's `nv` vertices
// at k * nv), triangles and UVs.
struct MeshSection {
	uint nv = 0;
	Common::Array<Vec3> verts;
	Common::Array<Vec3> normals;
	Common::Array<Face> faces;
	Common::Array<float> u, v;  // per-uv-index texture coordinates
	Common::Array<uint16> vertOfUv;  // the position vertex each uv index (GPU vertex) carries
};

struct Mesh {
	int frames = 0;  // the frames the original reads (E-0600); the sections hold them all
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

// A condition on an actor's state slot (docs/spec/events.md, E-0201): actor[id].slot[slot]
// ==, >, <, != value for mode 0..3; `link` 1 ends an AND group.
struct SceneCond {
	int32 id = 0, slot = 0, value = 0, mode = 0, link = 0;
};

// One command (E-0109, E-0200): deliver `opcode` to the actor `targetId` (-1 every actor).
// `when` -1 runs it now; any other value is a scene number: it waits until that scene is
// entered. `conds` are checked when the command is pushed (events.cpp).
struct SceneCommand {
	int when = -1;
	int targetId = 0;
	int opcode = 0;
	int arg1 = 0, arg2 = 0;
	Common::Array<SceneCond> conds;
};
typedef Common::Array<SceneCommand> CommandList;

// A trigger (type 0x19, E-0108, E-0207): a screen polygon and the commands it runs when it
// fires (a click inside the polygon, or opcode 0). `spent` is its latch (opcode 13).
struct SceneTrigger {
	uint32 id = 0;
	Common::Array<Common::Point> poly;
	CommandList cmds;
	Common::Array<SceneCond> conds;  // +0x190, honoured when hasConds
	bool spent = false;
	bool active = true, visible = true;  // +0x10c, +0x110
	int view = -1;           // +0x174: the view it belongs to (-1 any)
	bool click = true;       // +0x178: 1 click, 0 walk-in
	bool proximity = true;   // +0x17c: gated on the player character's sphere (E-0705)
	bool hasConds = false;   // +0x180
	bool proximityOn = true; // +0x154 (opcodes 14/15)
	bool once = true;        // +0x170: the gate passes once per stay in the sphere
	uint32 gate = 1;         // +0x188: bit 1 the player's character, 2 the companion, 4 the fighters
	int32 who = -1;          // +0x150: the player's character id required (-1 any)
	int32 whoCompanion = -1; // +0x14c: the companion's id required (-1 any)
	Vec3 centre;             // +0x13c: the sphere, after the polygon
	float radius = 0.0f;
	bool inside = false;     // +0x158: the player's latch for `once`
	bool insideCompanion = false;  // +0x15c: the companion's
	bool insideFighter[4] = { false, false, false, false };  // +0x160..+0x16c: actors 91..94's
};

// A CFXSound actor (0x18/0x2a, dialogue.cpp, docs/spec/dialogue.md).
struct SceneSound {
	uint32 id = 0;
	bool active = false, visible = false;
	bool volSet = false, panSet = false;
	int32 volume = 0, pan = 0;       // DirectSound hundredths of a dB
	bool loop = false;
	bool onEntry = false;            // start on the scene-entry broadcast 0x17
	int speaker = 0;                 // the character saying it; 0 = not speech
	Common::String name;             // the .wav
	Common::Array<SceneCommand> onEnd;  // run when it stops
};

// A sprite's command lists and autoplay flag (E-0208), kept beside SceneSprite for the VM.
struct SpriteHooks {
	uint32 id = 0;
	bool autoplay = false;   // +0x1d4: plays on the scene-entry broadcast
	CommandList onEnd, onForward, onBackward;  // +0x14c, +0x12c, +0x13c
	CommandList onContact;   // a 0x1a mesh's third list: a contact sphere touched (E-1681)
};

// A logic actor (E-0204/E-0205): 0x21 script, 0x22/0x25 counter, 0x23/0x26 timer,
// 0x24/0x27 flag, from the scene's .abi or Actors/global.atx.
struct SceneLogic {
	uint32 id = 0;
	uint32 type = 0;
	bool active = false, visible = false, latch = false;
	Common::Array<int32> state;   // state slots; slot 0 always exists (E-0201)
	int32 f0 = 0, f1 = 0;         // counter: max, fire; timer: limit (ms); flag: fire; script: guarded
	int32 count = 0;              // counter count, timer elapsed ms, script pending
	Common::Array<SceneCond> conds;  // script guard
	CommandList cmds;
	Common::String film;          // 0x07 cut scene: the .mpg under Movies/ (E-1806)
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
	// Animation (E-0601, docs/spec/scene.md), as read; the VM runs it (events.cpp).
	int fps = 0;                 // +0x1b4
	uint32 anim = 0;             // +0x1b8: 1 loop, 2 ping-pong, 4 forward, 8 backward, 0x10 there and back
	bool playing = false;        // +0x1d4
	int frame = 0;               // +0x1c0
	bool autoplay = false;       // +0x1dc: plays on the scene-entry broadcast
	bool platform = false;       // +0x1d0: walked on (E-1600)
	// Contact spheres riding a vertex of the current frame (E-1681): the air bubbles.
	struct Contact {
		uint32 vertex;
		float radius;
	};
	Common::Array<Contact> contacts;  // +0x25c
	uint32 contactFlags = 0;     // +0x1bc: bit 1 tests the player's character
	bool contactOnce = false;    // +0x1e4: once per touch
	bool contactOn = true;       // +0x1e0: opcodes 14 / 15
	bool contactLatch = false;   // +0x234
	// The delay timer (+0x278): a play waits `ticks` updates before running.
	bool timerOn = false, timerCounting = false, timerRandom = false;
	int32 timerMin = 0, timerMax = 0, timerFixed = 0;  // ms
	int32 timerTicks = 0;
};

// Everything the engine reads from a Scene_<NNN>.abi today.
struct SceneData {
	Common::Array<SceneView> views;     // from Scene_<NNN>.scn
	int view = 0;                       // the current view (185 op 30 changes it)
	Common::Array<SceneLight> lights;
	Common::Array<SceneSprite> sprites;
	Common::Array<SceneTrigger> triggers;
	Common::Array<SceneMesh> meshes;
	Common::Array<SpriteHooks> spriteHooks;  // the event VM's view of the sprites and meshes
	Common::Array<SceneLogic> logic;         // 0x21..0x27, 0x07 films
	Common::Array<SceneSound> sounds;        // 0x18/0x2a (dialogue.cpp)
};

/** The screen bounding rectangle of the 8 projected corners of a placed mesh's box (E-0900). */
Common::Rect screenRect(const Mesh &m, const Camera &cam);

/** A look-at camera (dev views of a lone mesh): Direct3D-style left-handed matrices. */
Camera lookAtCamera(const Vec3 &eye, const Vec3 &target, float fovY, float zn, float zf);

/** Rasterise `mesh` into `screen` (RGB555) through `cam` like the original's Direct3D 7
 *  device (docs/spec/scene.md): counter-clockwise faces culled, Gouraud lighting from
 *  `lights` over the 0x1e1e1e ambient, `tex` (ARGB8888, may be null) modulated with
 *  bilinear filtering, alpha-blended when `alpha`. `depth` (16-bit, screen-sized) is the
 *  z-buffer: tested less-or-equal and written. Draws `frame` (nothing when it is not one of
 *  the mesh's frames); triangles are clipped to the near plane. Not `lit`: lighting off,
 *  every vertex white. `zWrite` off: tested, not written; `zBias` pulls the tested depth
 *  that many 16-bit steps nearer. */
void renderMesh(Graphics::ManagedSurface &screen, const Mesh &mesh, const Camera &cam,
				const Common::Array<SceneLight> &lights, Common::Array<uint16> &depth,
				const Graphics::Surface *tex = nullptr, bool alpha = false, int frame = 0,
				bool lit = true, bool zWrite = true, int zBias = 0);

/** Frame `frame` of `src` placed by Direct3D's yaw-pitch-roll `rot` (pitch, yaw, roll: roll
 *  about Z, then pitch about X, then yaw about Y, row vectors) and moved to `pos` (items.cpp). */
void placeMesh(const Mesh &src, int frame, const float rot[3], const float pos[3], Mesh &out);

} // End of namespace Grumpa

#endif // GRUMPA_MESH_H
