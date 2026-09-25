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

	// One logic step of dt seconds: animation playback, then the posed vertices
	void update(float dt);
	void draw(const Camera &cam, int width, int height);

	Camera camera; // #CAMERA# values; position and angles are set by the unit
	float scale = 1; // #SCENE# unit length (eye height and collision sphere derive from it)

	struct Model;

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
		Common::Array<Common::Array<float> > worldNormals; // like worldVertices
		bool lit = false; // reached by the scene's lights (light= after it in the script)
		// Lit colours per vertex array, 6 bytes per vertex (D rgb, S rgb), and the frame
		// they were computed for
		mutable Common::Array<Common::Array<byte> > colors;
		mutable Common::Array<uint32> colorFrame;
		Common::Array<float> bounds; // per object: world bounding sphere x, y, z, radius (< 0: nothing drawn)
	};

	// X3d_Object_Hide by object name, in every loaded file
	void hideObject(const Common::String &name, bool hidden = true);

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
	int addFaceClip(const Common::String &faceObject, const Common::String &path);
	void setNode(int node, bool enabled, bool running);
	void setNodeFrame(int node, float frame);
	void setNodeFps(int node, float fps);

	// An object's world origin, by name
	Math::Vector3d objectPosition(const Common::String &name) const;

	const Common::String &dir() const { return _dir; } // asset directory, e.g. "U01/"

	// Files loaded by object= lines (not their LODs), in script order
	const Common::Array<Model *> &models() const { return _models; }

	// The object under output pixel (x, y) and the camera-space depth of the hit
	// (docs/engine-spec/interaction.md, Picking). Returns false when nothing is hit.
	bool pick(const Camera &cam, int width, int height, float x, float y,
	          const Model *&model, uint &object, float &depth);

private:
	// An animation being played (animation.md, Per-frame playback)
	struct Playback {
		const A3DFile *file = nullptr;
		uint animation = 0;
		float fps = 30, frame = 0;
		bool loop = true, running = true, backward = false;
		float stopAt = -1; // stop target, < 0 for none

		void advance(float dt);
	};

	// An object's playback, plus a scripted clip that replaces it while active (slot 1)
	struct AnimNode {
		Model *model;
		uint object;
		Playback base, clip;
		bool clipActive = false;
		bool enabled = true; // disabled nodes are neither advanced nor applied
	};

	AnimNode *findNode(const Common::String &objectName);

	Model *loadModel(const Common::String &path);
	void bindAnimation(const Common::String &path, float fps);
	void addNode(const A3DFile *file, uint animation, Model *m, uint object, float fps);
	void animate(const A3DFile &file, uint animation, Model &m, uint object, float frame);
	void pose(Model &m); // world vertices and bounds from the live transforms
	void attachLod(Model *base, uint baseObject, const Model *lod, uint lodObject, float threshold);
	void drawObject(const Model &m, uint object);
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
	uint32 _frame = 0;
	bool inView(const float *sphere) const;
	uint32 texture(const Common::String &mapName);

	Common::String _dir;  // asset directory, e.g. "U01/"
	byte _ambient[3] = { 255, 255, 255 };
	Common::Array<Model *> _models;
	Common::Array<Model *> _lodModels; // only drawn through their base objects
	Common::Array<A3DFile *> _animationFiles;
	Common::Array<AnimNode> _nodes;

	// The view of the frame being drawn, for culling: eye, axes, half-extents per unit depth
	float _eye[3], _right[3], _up[3], _forward[3], _halfWidth, _halfHeight;
	// Camera-facing ($Z$) objects: world offset -> world, rotating it as if the camera
	// had yaw pi/2 (row vectors, 3x3)
	float _facing[9];
	Renderer *_renderer;
	Common::HashMap<Common::String, uint32, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _textures;
};

} // End of namespace X3D

#endif // X3D_SCENE_H
