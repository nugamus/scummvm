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


#ifndef X3D_SCENE_H
#define X3D_SCENE_H

#include "common/array.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/path.h"
#include "common/serializer.h"
#include "common/str.h"

#include "math/vector3d.h"

#include "x3d/a3d.h"
#include "x3d/o3d.h"

namespace Common {
class SeekableReadStream;
}

namespace X3D {

// Chunk `name` (e.g. "#SCENE#") of a .BIN chunk container, or nullptr
Common::SeekableReadStream *openBinChunk(const Common::Path &file, const char *name);

// docs/engine-spec/scene.md, Camera and projection
struct Camera {
	float position[3] = {};
	float yaw = 0;           // a, radians
	float pitch = M_PI / 2;  // e, radians from straight down
	float fov = 90;          // horizontal, degrees
	float roll = 0;          // degrees, about the view direction
};

class Renderer;

class Scene {
public:
	explicit Scene(Renderer *renderer) : _renderer(renderer) {}
	~Scene();

	// Loads a unit from its .X3D script name, e.g. "U01.X3D"
	bool load(const Common::String &scriptName);

	// One logic step of dt seconds: animation playback, then the posed vertices. Split in
	// two so unit code can move objects after the animation (U03's policeman).
	void update(float dt) { advance(dt); poseAll(); }
	void advance(float dt);
	// Samples the running nodes between the last two advances (0 < alpha < 1) for
	// rendering above the step rate (animation.md, Engine model); true if any moved
	bool interpolate(float alpha);
	void poseAll();

	struct Model;
	// Models added and removed by unit code (U03's clown and cutscene): the file, with an
	// optional .A3D bound as the script's animation= would; not in collision
	Model *addModel(const Common::String &path, const Common::String &animation = "", float fps = 15);
	void removeModel(Model *m);
	void nameNodes(Model *m, const Common::String &name); // its nodes answer to name
	// A new node playing the .A3D's root animation on an existing object, named after it
	// (U07's tipping plank)
	bool addObjectNode(const Common::String &object, const Common::String &path, float fps);
	// A new node, last in the list, playing the loaded track named after the object on it
	// (XSceneAnim_AddNode: U04's chest lid *U04_26, E-0230, E-0056)
	bool addTrackNode(const Common::String &object);
	O3DObject *object(const Common::String &name); // newest file first
	void draw(const Camera &cam, int width, int height);

	Camera camera; // #CAMERA# values; position and angles are set by the unit
	bool maxDetail = false; // enhancement: never switch to a distant level of detail
	float scale = 1; // #SCENE# unit length (eye height and collision sphere derive from it)

	// A lower-detail stand-in for a base object (docs/engine-spec/scene.md, Levels of detail)
	struct Lod {
		const Model *model;
		uint object;
		float threshold; // squared distance
	};

	struct Model {
		O3DFile file;
		bool hidden = false;
		Common::Array<Common::Array<float> > worldVertices; // per object; empty for weld objects
		Common::Array<Common::Array<Lod> > lods;            // per object, by threshold
		Common::Array<bool> hiddenObjects;
		Common::Array<bool> unpickable;
		Common::Array<bool> pickWhenHidden;
		bool animated = false;
		Common::Array<Common::Array<float> > worldNormals; // like worldVertices
		bool lit = false; // reached by the scene's lights (light= after it in the script)
		// Lit colours per vertex array, 6 bytes per vertex (D rgb, S rgb), and the lighting
		// state they were computed for (0: posed since)
		mutable Common::Array<Common::Array<byte> > colors;
		mutable Common::Array<uint32> colorFrame;
		Common::Array<float> bounds; // per object: world bounding sphere x, y, z, radius (< 0: nothing drawn)
	};

	// X3d_Object_Hide by object name, in every loaded file
	void hideObject(const Common::String &name, bool hidden = true);
	void hideObjectOnly(const Common::String &name, bool hidden = true); // not its children
	void hideAll(); // every object of every file (U05's game over)
	void hideParent(const Common::String &name, bool hidden = true); // and its subtree
	// Hidden objects the pick still sees (U04's hole and ladder place, its hover hook)
	void setPickWhenHidden(const Common::String &name, bool pick);
	// The map of the first material of the object's first face (U04's gagged Monet)
	void setObjectMap(const Common::String &object, const Common::String &mapName);
	// The names of the object's siblings (same parent, itself excluded; top-level objects
	// of its file when it has no parent)
	Common::StringArray siblings(const Common::String &name) const;

	// Animation nodes by object name (animation.md, E-0056/E-0057; interaction.md)
	void startAnimation(const Common::String &objectName);
	void setAnimationState(const Common::String &objectName, float frame, bool paused, float fps, bool loop);
	// Plays a whole .A3D once on the node's object in its clip slot, holding the last pose
	void playClip(const Common::String &objectName, const Common::String &path);
	// Plays the clip backward to frame 1, then returns to the node's own animation at frame 1
	void rewindClip(const Common::String &objectName);
	bool clipPlaying(const Common::String &objectName);

	// Mouth clips for talk (sound.md, Talkers): a node that plays the sub-animation named
	// like the face object from a whole-body .A3D, created disabled and paused. -1 if the
	// file or the object is missing.
	// owner: prefer the face object below the object of that name (two talkers may share
	// a face name, U05's $$$DUMMY.*visage)
	int addFaceClip(const Common::String &faceObject, const Common::String &path, const Common::String &owner = "");
	// The global position of the face object addFaceClip binds to (sound.md, Say)
	Math::Vector3d facePosition(const Common::String &faceObject, const Common::String &owner) const;
	void setNode(int node, bool enabled, bool running);
	void setNodeFrame(int node, float frame);
	void setNodeFps(int node, float fps);

	// An object's world origin, by name
	Math::Vector3d objectPosition(const Common::String &name) const;
	// The centre of what the object draws (its origin when it draws nothing)
	Math::Vector3d objectCenter(const Common::String &name) const;
	// The centres of the object's faces, nearest to eye first, then its centre: points to
	// aim at, for thin or partly covered objects
	Common::Array<Math::Vector3d> surfacePoints(const Common::String &name, const Math::Vector3d &eye) const;
	// An object by name in X3D's lookup order (newest file first), or false
	bool findObject(const Common::String &name, Model *&model, uint &object) const;
	void renameObject(const Common::String &from, const Common::String &to);
	void renameNode(const Common::String &from, const Common::String &to);
	void setPickable(const Common::String &namePrefix, bool pickable);

	// Scripted control of an animation node (u01.md "Run node to f")
	bool hasNode(const Common::String &name) { return findNode(name) != nullptr; }
	void runNodeTo(const Common::String &name, float target, bool backward);
	void pauseNode(const Common::String &name);
	void setNodeFrame(const Common::String &name, float frame);
	void setNodeFps(const Common::String &name, float fps);
	void setNodeLoop(const Common::String &name, bool loop);
	void setNodeRange(const Common::String &name, float first, float last);
	void stepNode(const Common::String &name, float dt); // advances even when paused
	float nodeFrame(const Common::String &name);
	float nodeLastFrame(const Common::String &name);
	bool nodeRunning(const Common::String &name);
	// Replaces the node's animation with a whole .A3D in its clip slot, paused, driving the
	// node object's parent (U01's siding clip, u01.md)
	void loadClip(const Common::String &name, const Common::String &path, float fps, float frame);
	// A clip made the node's active slot, paused at its first frame, not looping; with a
	// sub-animation name, only that animation plays, on the object of the same name
	// (u02.md). The node* controls above then act on it; endClip goes back to slot 0.
	void setClip(const Common::String &name, const Common::String &path, const Common::String &subAnimation = "",
	             int slot = 1, bool activate = true);
	void activateSlot(const Common::String &name, int slot); // 0: the node's own animation
	int activeSlot(const Common::String &name);
	Common::String clipPath(const Common::String &name); // the active clip's .A3D, or ""
	void endClip(const Common::String &name);
	void setNodePingPong(const Common::String &name, bool pingPong);
	void enableNode(const Common::String &name, bool enabled);

	byte ambient[3] = { 255, 255, 255 };

	// Saved state (save.md): ambient, hidden and unpickable objects, animation nodes
	void syncState(Common::Serializer &s);

	const Common::String &dir() const { return _dir; } // asset directory, e.g. "U01/"
	// The unit's own data: SCENE.BIN, INFOOBJ/INFOACT, voices ("U00/" while assets are U04's)
	const Common::String &dataDir() const { return _dataDir; }

	// Files loaded by object= lines (not their LODs), in script order
	const Common::Array<Model *> &models() const { return _models; }

	// The object under output pixel (x, y) and the camera-space depth of the hit
	// (docs/engine-spec/interaction.md, Picking). Returns false when nothing is hit.
	bool pick(const Camera &cam, int width, int height, float x, float y,
	          const Model *&model, uint &object, float &depth);

	// Enhancement (hotspot overlay): a tint over the drawn faces of these objects, then
	// their outline; after draw(), with its view
	void drawHighlight(const Common::Array<Common::Pair<const Model *, uint> > &objects);

private:
	// An animation being played (animation.md, Per-frame playback)
	struct Playback {
		const A3DFile *file = nullptr;
		uint animation = 0;
		float fps = 30, frame = 0;
		bool loop = true, running = true, backward = false;
		bool pingPong = false; // not looping: turns round at both ends instead of stopping
		float stopAt = -1; // stop target, < 0 for none
		float first = -1, last = -1; // range override, < 0: the animation's
		int object = -1;             // the object it drives, < 0: the node's
		Common::String path;         // a clip's .A3D, for saves

		void advance(float dt);
	};

	// An object's playback, plus a scripted clip that replaces it while active (slot 1)
	struct AnimNode {
		Common::String name; // the object's name unless the unit renames it
		Model *model;
		uint object;
		Playback base, clip;
		bool clipActive = false;
		// Numbered clip slots (animation.md "Sub-slots"): clip is slot `slot`; the others
		// wait in slots[] until made active
		int slot = 1;
		Playback slots[16];
		bool enabled = true; // disabled nodes are neither advanced nor applied
		float prevFrame = -1; // the active playback's frame before the last advance
		bool prevClip = false;
	};

	float _stepDt = 0; // the last advance's dt
	AnimNode *findNode(const Common::String &objectName);
	const A3DFile *clipFile(const Common::String &path); // loaded once, owned by the scene
	void syncPlayback(Common::Serializer &s, Playback &p);
	Playback &active(AnimNode &n);

	Model *loadModel(const Common::String &path);
	void bindAnimation(const Common::String &path, float fps);
	void addNode(const A3DFile *file, uint animation, Model *m, uint object, float fps);
	void animate(const A3DFile &file, uint animation, Model &m, uint object, float frame);
	void pose(Model &m); // world vertices and bounds from the live transforms
	// addFaceClip's face lookup: newest file first, the one below owner preferred
	bool findFace(const Common::String &faceObject, const Common::String &owner, Model *&model, uint &object) const;
	void attachLod(Model *base, uint baseObject, const Model *lod, uint lodObject, float threshold);
	void drawObject(const Model &m, uint object);
	// The level of detail draw() shows for the base object: file and object
	void drawnLod(const Model *m, uint i, const Model *&drawn, uint &object) const;
	// The vertices the object's faces index as drawn (its owner's, camera-facing turned),
	// nullptr when it has none
	const Common::Array<float> *drawnVertices(const Model &m, uint object);

	// Translucent and additive faces, drawn after everything else, farthest first
	// (scene.md, Drawing order and blending)
	struct Deferred {
		float xyz[3 * 64], uv[2 * 64];
		byte rgb[3 * 64], spec[3 * 64];
		uint count;
		bool hasUV, lit, anySpecular, clamp, keyed, additive;
		uint32 tex;
		byte alpha;
		int key;
		uint order;
	};
	Common::Array<Deferred> _deferred; // the first _deferredCount are this frame's; kept for reuse
	uint _deferredCount = 0;
	Common::Array<const Deferred *> _deferredOrder;
	void drawFace(const Deferred &f);
	const Common::Array<byte> &lighting(const Model &m, uint owner) const;
	void loadLights(const Common::String &path);

	// A .L3D omni light (lighting.md; spot lights have no corpus sample)
	struct Light {
		float position[3];
		byte color[3];
		float inner, outer, multiplier;
		bool hidden, attenuate;
	};
	Common::Array<Light> _lights;
	// Bumped when the ambient or the lights change; lit colours of another state are stale
	uint32 _lightingState = 1;
	byte _litAmbient[3] = { 255, 255, 255 };
	bool inView(const float *sphere) const;
	uint32 texture(const Common::String &mapName);

	Common::String _dir;  // asset directory, e.g. "U01/"
	Common::String _dataDir;
	Common::Array<Model *> _models;
	Common::Array<Model *> _lodModels; // only drawn through their base objects
	Common::Array<A3DFile *> _animationFiles;
	// Materials by exact name: the first loaded wins for every later file (E-0484)
	Common::HashMap<Common::String, O3DMaterial> _materials;
	Common::HashMap<Common::String, const A3DFile *, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _clipFiles;
	Common::Array<AnimNode> _nodes;

	// The view of the frame being drawn, for culling: eye, axes, half-extents per unit depth
	float _eye[3], _right[3], _up[3], _forward[3], _halfWidth, _halfHeight;
	// Camera-facing ($Z$) objects: world offset -> world, rotating it as if the camera
	// had yaw pi/2 (row vectors, 3x3)
	float _facing[9];
	// The object's camera-type-2 faces turned toward the camera about its origin, in a copy
	// of the owner's world vertices (E-0270)
	void faceCamera(const Model &m, uint object, Common::Array<float> &vertices) const;
	// Scratch arrays reused by drawObject, faceCamera and pick
	Common::Array<float> _facingVertices, _pickCamera, _pickScreen;
	mutable Common::Array<bool> _faceDone;
	struct Edge {
		float a[3], b[3]; // ends in memcmp order
		bool front;
	};
	Common::Array<Edge> _edges; // drawHighlight's
	Common::Array<float> _lines;
	Renderer *_renderer;
	Common::HashMap<Common::String, uint32, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _textures;
};

} // End of namespace X3D

#endif // X3D_SCENE_H
