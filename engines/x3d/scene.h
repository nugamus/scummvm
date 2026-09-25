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
#include "common/hashmap.h"
#include "common/path.h"
#include "common/str.h"

#include "graphics/tinygl/tinygl.h"

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
};

class Scene {
public:
	~Scene();

	// Loads a unit from its .X3D script name, e.g. "U01.X3D"
	bool load(const Common::String &scriptName);
	void draw(const Camera &cam, int width, int height);

	Camera camera; // #CAMERA# values; position and angles are set by the unit

private:
	struct Model {
		O3DFile file;
		bool hidden = false;
		Common::Array<Common::Array<float> > worldVertices; // per object; empty for weld objects
	};

	void loadObject(const Common::String &path, bool hidden);
	TGLuint texture(const Common::String &mapName);

	Common::String _dir;  // asset directory, e.g. "U01/"
	byte _ambient[3] = { 255, 255, 255 };
	Common::Array<Model *> _models;
	Common::HashMap<Common::String, TGLuint, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _textures;
};

} // End of namespace X3D

#endif // X3D_SCENE_H
