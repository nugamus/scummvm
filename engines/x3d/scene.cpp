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

#include "common/file.h"
#include "common/textconsole.h"

#include "graphics/surface.h"

#include "x3d/dmf.h"
#include "x3d/scene.h"

namespace X3D {

Common::SeekableReadStream *openBinChunk(const Common::Path &file, const char *name) {
	// Table of {char name[20], u32 offset, u32 size} before a trailing u32 count (E-0025)
	Common::File f;
	if (!f.open(file))
		return nullptr;
	f.seek(-4, SEEK_END);
	const uint32 count = f.readUint32LE();
	f.seek(-(int32)(count * 28 + 4), SEEK_END);
	for (uint32 i = 0; i < count; i++) {
		char entry[21];
		f.read(entry, 20);
		entry[20] = 0;
		const uint32 offset = f.readUint32LE();
		const uint32 size = f.readUint32LE();
		if (!strcmp(entry, name)) {
			f.seek(offset);
			return f.readStream(size);
		}
	}
	return nullptr;
}

Scene::~Scene() {
	for (Model *m : _models)
		delete m;
	for (Model *m : _lodModels)
		delete m;
	for (A3DFile *f : _animationFiles)
		delete f;
	for (auto &t : _textures)
		if (t._value)
			tglDeleteTextures(1, &t._value);
}

bool Scene::load(const Common::String &scriptName) {
	// Asset directory from the unit number; U00 borrows U04's (E-0034)
	const Common::String unit = scriptName.substr(0, 3);
	_dir = (unit.equalsIgnoreCase("U00") ? Common::String("U04") : unit) + "/";

	Common::SeekableReadStream *s = openBinChunk(Common::Path(_dir + "SCENE.BIN"), "#SCENE#");
	if (s) {
		for (byte &c : _ambient)
			c = s->readUint32LE();
		scale = s->readFloatLE();
		delete s;
	}
	s = openBinChunk(Common::Path(_dir + "SCENE.BIN"), "#CAMERA#");
	if (s) {
		camera.fov = s->readFloatLE();
		delete s;
	}

	// The .X3D script (docs/formats/README.md)
	Common::File script;
	if (!script.open(Common::Path(unit + "/" + scriptName)))
		return false;
	const Common::String text = script.readString(0, script.size());

	static const char *const keywords[] = { "scene=", "object=", "animation=", "camera=", "light=", "lod=" };
	uint i = 0;
	while (i < text.size()) {
		const char c = text[i];
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			i++;
			continue;
		}
		if (c == ';') {
			while (i < text.size() && text[i] != '\n')
				i++;
			continue;
		}

		int keyword = -1;
		for (int k = 0; k < ARRAYSIZE(keywords) && keyword < 0; k++)
			if (!scumm_strnicmp(text.c_str() + i, keywords[k], strlen(keywords[k])))
				keyword = k;
		if (keyword < 0) {
			warning("X3D script %s: unknown keyword at offset %u", scriptName.c_str(), i);
			return false;
		}
		i += strlen(keywords[keyword]);

		// A quoted field; its first value ends at a quote, CR or comma, then ",value"
		Common::String field, value;
		if (i < text.size() && text[i] == '"')
			i++;
		while (i < text.size() && text[i] != '"' && text[i] != '\r' && text[i] != ',')
			field += text[i++];
		if (i < text.size() && text[i] == ',')
			while (++i < text.size() && text[i] != '"' && text[i] != '\r')
				value += text[i];
		while (i < text.size() && text[i] != '\n')
			i++;
		field.replace('\\', '/');

		// Lights (Q-0019) and cameras are not used yet
		if (keyword == 2 && !_models.empty()) {
			bindAnimation(field, value.empty() ? 30.0f : atof(value.c_str()));
		} else if (keyword == 1) {
			Model *m = loadModel(field);
			if (m) {
				m->hidden = field.hasPrefixIgnoreCase("static/col");
				_models.push_back(m);
			}
		} else if (keyword == 5 && !_models.empty()) {
			Model *lod = loadModel(field);
			if (lod) {
				const float d = MAX(0.0, atof(value.c_str()));
				_lodModels.push_back(lod);
				if (!_models.back()->file.objects.empty() && !lod->file.objects.empty())
					attachLod(_models.back(), 0, lod, 0, d * d);
			}
		}
	}

	return !_models.empty();
}

Scene::Model *Scene::loadModel(const Common::String &path) {
	Common::File f;
	Model *m = new Model();
	if (!f.open(Common::Path(_dir + path)) || !m->file.load(f)) {
		warning("Unable to load %s%s", _dir.c_str(), path.c_str());
		delete m;
		return nullptr;
	}

	m->worldVertices.resize(m->file.objects.size());
	m->lods.resize(m->file.objects.size());
	m->hiddenObjects.resize(m->file.objects.size());
	m->bounds.resize(m->file.objects.size() * 4);
	pose(*m);
	return m;
}

void Scene::pose(Model &m) {
	const Common::Array<O3DObject> &objects = m.file.objects;

	// Each object transforms its own vertex range; welded objects write into their top
	// object's shared array (animation.md, Welded objects)
	for (uint i = 0; i < objects.size(); i++)
		m.worldVertices[i].resize(objects[i].vertices.size());
	for (uint i = 0; i < objects.size(); i++) {
		const O3DObject &o = objects[i];
		int top = i;
		while (top >= 0 && objects[top].vertices.empty())
			top = objects[top].parent;
		if (top < 0)
			continue;
		const uint first = o.welded ? o.weldFirst : 0;
		const uint count = o.welded ? o.ownCount : objects[top].vertices.size() / 3;
		if (!o.welded && top != (int)i)
			continue;
		const Common::Array<float> &in = objects[top].vertices;
		Common::Array<float> &out = m.worldVertices[top];
		const float *w = o.world;
		for (uint v = first * 3; v < (first + count) * 3 && v + 2 < in.size(); v += 3) {
			const float x = in[v], y = in[v + 1], z = in[v + 2];
			for (int k = 0; k < 3; k++)
				out[v + k] = x * w[k] + y * w[4 + k] + z * w[8 + k] + w[12 + k];
		}
	}

	// Bounding spheres of what each object draws, for view culling
	for (uint i = 0; i < objects.size(); i++) {
		int owner = i;
		while (owner >= 0 && objects[owner].vertices.empty())
			owner = objects[owner].parent;
		float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
		bool any = false;
		if (owner >= 0) {
			const Common::Array<float> &v = m.worldVertices[owner];
			for (const O3DFace &face : objects[i].faces) {
				for (uint32 index : face.indices) {
					if (index * 3 + 2 >= v.size())
						continue;
					for (int k = 0; k < 3; k++) {
						lo[k] = MIN(lo[k], v[index * 3 + k]);
						hi[k] = MAX(hi[k], v[index * 3 + k]);
					}
					any = true;
				}
			}
		}
		float *b = &m.bounds[i * 4];
		float r2 = 0;
		for (int k = 0; k < 3; k++) {
			b[k] = (lo[k] + hi[k]) / 2;
			r2 += (hi[k] - lo[k]) * (hi[k] - lo[k]) / 4;
		}
		b[3] = any ? sqrtf(r2) : -1;
	}
}

void Scene::bindAnimation(const Common::String &path, float fps) {
	Common::File f;
	A3DFile *file = new A3DFile();
	if (!f.open(Common::Path(_dir + path)) || !file->load(f) || file->animations.empty()) {
		warning("Unable to load %s%s", _dir.c_str(), path.c_str());
		delete file;
		return;
	}
	_animationFiles.push_back(file);
	Model *m = _models.back();
	if (m->file.objects.empty())
		return;

	// A character's root ("*" in its name) drives the whole file; otherwise each child of
	// the root drives the object of the same name under the file's first object
	const A3DAnimation &root = file->animations[0];
	if (root.name.contains('*')) {
		addNode(file, 0, m, 0, fps);
		return;
	}
	for (uint c = 1; c < file->animations.size(); c++) {
		if (file->animations[c].parent != 0)
			continue;
		// Depth first under the first object: file order, since parents come first
		for (uint o = 1; o < m->file.objects.size(); o++) {
			if (m->file.objects[o].name.equalsIgnoreCase(file->animations[c].name)) {
				addNode(file, c, m, o, fps);
				break;
			}
		}
	}
}

void Scene::addNode(const A3DFile *file, uint animation, Model *m, uint object, float fps) {
	AnimNode n;
	n.model = m;
	n.object = object;
	n.base.file = file;
	n.base.animation = animation;
	n.base.fps = fps;
	n.base.frame = file->animations[animation].firstFrame;
	_nodes.push_back(n);
}

void Scene::animate(const A3DFile &file, uint animation, Model &m, uint object, float frame) {
	file.sample(animation, frame, m.file.objects[object]);

	// Children pair up by position, both in file order
	uint o = object + 1;
	for (uint a = animation + 1; a < file.animations.size(); a++) {
		if (file.animations[a].parent != (int)animation)
			continue;
		while (o < m.file.objects.size() && m.file.objects[o].parent != (int)object)
			o++;
		if (o == m.file.objects.size())
			break;
		animate(file, a, m, o++, frame);
	}
}

void Scene::update(float dt) {
	for (AnimNode &n : _nodes) {
		// A rewound clip hands back to the node's own animation at frame 1 (E-0057)
		if (n.clipActive && n.clip.backward && !n.clip.running) {
			n.clipActive = false;
			const A3DAnimation &a = n.base.file->animations[n.base.animation];
			n.base.frame = CLIP(1.0f, (float)a.firstFrame, (float)a.lastFrame);
			n.base.running = true;
		}
		Playback &p = n.clipActive ? n.clip : n.base;
		p.advance(dt);
		animate(*p.file, p.animation, *n.model, n.object, p.frame);
	}

	// ponytail: re-poses every animated file each step; track dirty objects if it shows up in profiles
	for (Model *m : _models) {
		bool animated = false;
		for (const AnimNode &n : _nodes)
			animated |= n.model == m;
		if (animated) {
			m->file.updateWorld();
			pose(*m);
		}
	}
}

bool Scene::inView(const float *sphere) const {
	if (sphere[3] < 0)
		return false;
	float v[3];
	for (int k = 0; k < 3; k++)
		v[k] = sphere[k] - _eye[k];
	const float depth = v[0] * _forward[0] + v[1] * _forward[1] + v[2] * _forward[2];
	const float x = v[0] * _right[0] + v[1] * _right[1] + v[2] * _right[2];
	const float y = v[0] * _up[0] + v[1] * _up[1] + v[2] * _up[2];
	const float r = sphere[3];
	// Signed distances to the four side planes (positive inside), then the near plane
	return depth * _halfWidth - x > -r * sqrtf(1 + _halfWidth * _halfWidth) &&
	       depth * _halfWidth + x > -r * sqrtf(1 + _halfWidth * _halfWidth) &&
	       depth * _halfHeight - y > -r * sqrtf(1 + _halfHeight * _halfHeight) &&
	       depth * _halfHeight + y > -r * sqrtf(1 + _halfHeight * _halfHeight) &&
	       depth > -r;
}

void Scene::hideObject(const Common::String &name, bool hidden) {
	for (Model *m : _models)
		for (uint i = 0; i < m->file.objects.size(); i++)
			if (m->file.objects[i].name.equalsIgnoreCase(name))
				m->hiddenObjects[i] = hidden;
}

Scene::AnimNode *Scene::findNode(const Common::String &objectName) {
	for (AnimNode &n : _nodes)
		if (n.model->file.objects[n.object].name.equalsIgnoreCase(objectName))
			return &n;
	return nullptr;
}

void Scene::startAnimation(const Common::String &objectName) {
	if (AnimNode *n = findNode(objectName))
		n->base.running = true;
}

void Scene::setAnimationState(const Common::String &objectName, float frame, bool paused, float fps, bool loop) {
	AnimNode *n = findNode(objectName);
	if (!n)
		return;
	const A3DAnimation &a = n->base.file->animations[n->base.animation];
	n->base.frame = CLIP(frame, (float)a.firstFrame, (float)a.lastFrame);
	if (paused)
		n->base.running = false;
	n->base.fps = fps;
	n->base.loop = loop;
}

void Scene::playClip(const Common::String &objectName, const Common::String &path) {
	AnimNode *n = findNode(objectName);
	Common::File f;
	A3DFile *file = new A3DFile();
	if (!n || !f.open(Common::Path(_dir + path)) || !file->load(f) || file->animations.empty()) {
		warning("Unable to play %s%s on %s", _dir.c_str(), path.c_str(), objectName.c_str());
		delete file;
		return;
	}
	_animationFiles.push_back(file);
	n->clip = n->base;
	n->clip.file = file;
	n->clip.animation = 0;
	n->clip.frame = file->animations[0].firstFrame;
	n->clip.running = true;
	n->clip.loop = false;
	n->clipActive = true;
}

void Scene::rewindClip(const Common::String &objectName) {
	AnimNode *n = findNode(objectName);
	if (!n || !n->clipActive)
		return;
	n->clip.backward = true;
	n->clip.running = true;
	n->clip.stopAt = 1;
}

bool Scene::clipPlaying(const Common::String &objectName) {
	AnimNode *n = findNode(objectName);
	return n && n->clipActive && n->clip.running;
}

void Scene::Playback::advance(float dt) {
	if (!running)
		return;
	const A3DAnimation &a = file->animations[animation];
	const float first = a.firstFrame, last = a.lastFrame;
	frame += (backward ? -dt : dt) * fps;
	if (!loop) {
		frame = CLIP(frame, first, last);
		if (backward ? frame <= first : frame >= last)
			running = false;
	} else if (frame > last) {
		frame = fmod(frame, last) + first;
	} else if (frame < first) {
		frame = last - (first - frame);
	}
	if (stopAt >= 0 && fabs(frame - stopAt) <= 2 * dt * fps) {
		frame = stopAt;
		running = false;
		stopAt = -1;
	}
}

void Scene::attachLod(Model *base, uint baseObject, const Model *lod, uint lodObject, float threshold) {
	// Pair the i-th children of both objects, in file order, then attach this pair
	const Common::Array<O3DObject> &bo = base->file.objects, &lo = lod->file.objects;
	uint j = 0;
	for (uint i = 0; i < bo.size(); i++) {
		if (bo[i].parent != (int)baseObject)
			continue;
		while (j < lo.size() && lo[j].parent != (int)lodObject)
			j++;
		if (j == lo.size())
			break;
		attachLod(base, i, lod, j++, threshold);
	}

	Common::Array<Lod> &lods = base->lods[baseObject];
	uint at = 0;
	while (at < lods.size() && lods[at].threshold < threshold)
		at++;
	lods.insert_at(at, Lod{ lod, lodObject, threshold });
}

TGLuint Scene::texture(const Common::String &mapName) {
	if (_textures.contains(mapName))
		return _textures[mapName];

	// Map names end ".TGA"; the file is the .dmf of the same name in Maps/ (E-0038)
	Common::String name = mapName;
	const size_t dot = name.findFirstOf('.');
	if (dot != Common::String::npos)
		name.erase(dot);

	TGLuint id = 0;
	Common::File f;
	Graphics::Surface *surface = nullptr;
	if (f.open(Common::Path(_dir + "Maps/" + name + ".dmf")))
		surface = loadDMF(f);
	if (surface) {
		tglGenTextures(1, &id);
		tglBindTexture(TGL_TEXTURE_2D, id);
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_MIN_FILTER, TGL_LINEAR);
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_MAG_FILTER, TGL_LINEAR);
		tglTexImage2D(TGL_TEXTURE_2D, 0, TGL_RGBA, surface->w, surface->h, 0, TGL_RGBA, TGL_UNSIGNED_BYTE, surface->getPixels());
		surface->free();
		delete surface;
	} else {
		warning("Unable to load map %s", mapName.c_str());
	}
	_textures[mapName] = id;
	return id;
}

void Scene::draw(const Camera &cam, int width, int height) {
	// Projection: x by 1/tan(fov/2) in a 4:3 frame, y by 4/3 of that (E-0040). Wider
	// outputs keep the 4:3 frame's vertical extent. The original's near/far are 0.1 and
	// 1,000,000; TinyGL's depth buffer needs a tighter range.
	// ponytail: fixed 1..20000 depth range, derive it from the scene bounds if a unit is larger
	const float sy = (4.0f / 3.0f) / tan(cam.fov * M_PI / 360.0);
	const float sx = sy * height / width;
	const float n = 1, f = 20000;
	const float projection[16] = {
		sx, 0, 0, 0,
		0, sy, 0, 0,
		0, 0, (f + n) / (n - f), -1,
		0, 0, 2 * f * n / (n - f), 0
	};

	// View: camera axes right, up, forward (docs/engine-spec/scene.md); GL looks down -z
	const float a = cam.yaw, e = cam.pitch;
	const float r = cam.roll * M_PI / 180;
	const float right0[3] = { -sinf(a), -cosf(a), 0 };
	const float up0[3] = { cosf(e) * cosf(a), -cosf(e) * sinf(a), sinf(e) };
	float right[3], up[3];
	for (int k = 0; k < 3; k++) {
		right[k] = cosf(r) * right0[k] + sinf(r) * up0[k];
		up[k] = -sinf(r) * right0[k] + cosf(r) * up0[k];
	}
	const float forward[3] = { sinf(e) * cosf(a), -sinf(e) * sinf(a), -cosf(e) };
	const float *p = cam.position;
	float view[16] = {
		right[0], up[0], -forward[0], 0,
		right[1], up[1], -forward[1], 0,
		right[2], up[2], -forward[2], 0,
		0, 0, 0, 1
	};
	for (int k = 0; k < 3; k++)
		view[12 + k] = -(p[0] * view[k] + p[1] * view[4 + k] + p[2] * view[8 + k]);

	tglViewport(0, 0, width, height);
	tglClearColor(0, 0, 0, 1);
	tglClear(TGL_COLOR_BUFFER_BIT | TGL_DEPTH_BUFFER_BIT);
	tglMatrixMode(TGL_PROJECTION);
	tglLoadMatrixf(projection);
	tglMatrixMode(TGL_MODELVIEW);
	tglLoadMatrixf(view);

	for (int k = 0; k < 3; k++) {
		_eye[k] = p[k];
		_right[k] = right[k];
		_up[k] = up[k];
		_forward[k] = forward[k];
	}
	_halfWidth = 1 / sx;
	_halfHeight = 1 / sy;
	_boundTexture = ~0u;

	// E-0058: camera type 2 draws with the view rotation of yaw pi/2 and the camera's
	// pitch, keeping the object's origin where the true view puts it. In world terms an
	// offset d from the origin becomes d * R(pi/2, e) * R(a, e)^T.
	{
		const float fixedRight[3] = { -1, 0, 0 };
		const float fixedUp[3] = { 0, -cosf(e), sinf(e) };
		const float fixedForward[3] = { 0, -sinf(e), -cosf(e) };
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				_facing[i * 3 + j] = fixedRight[i] * right0[j] + fixedUp[i] * up0[j] + fixedForward[i] * forward[j];
	}

	tglEnable(TGL_DEPTH_TEST);
	tglDisable(TGL_CULL_FACE);
	tglDisable(TGL_LIGHTING);
	tglEnable(TGL_ALPHA_TEST);
	tglAlphaFunc(TGL_GREATER, 0.5f);

	for (const Model *m : _models) {
		if (m->hidden)
			continue;
		for (uint i = 0; i < m->file.objects.size(); i++) {
			if (m->hiddenObjects[i])
				continue;
			// ponytail: culls on the base object's sphere; a LOD far outside it would be missed
			if (!inView(&m->bounds[i * 4]))
				continue;
			// Level of detail by squared distance from the object's origin to the camera
			const float *origin = m->file.objects[i].world + 12;
			float s = 0;
			for (int k = 0; k < 3; k++)
				s += (origin[k] - p[k]) * (origin[k] - p[k]);
			const Model *drawn = m;
			uint object = i;
			for (const Lod &lod : m->lods[i]) {
				if (lod.threshold <= s) {
					drawn = lod.model;
					object = lod.object;
				}
			}
			drawObject(*drawn, object);
		}
	}
}

void Scene::drawObject(const Model &m, uint object) {
	// Weld objects index the vertices of the nearest ancestor that has some (Q-0020)
	const Common::Array<O3DObject> &objects = m.file.objects;
	int owner = object;
	while (owner >= 0 && objects[owner].vertices.empty())
		owner = objects[owner].parent;
	if (owner < 0)
		return;
	const Common::Array<float> *vertices = &m.worldVertices[owner];

	// ponytail: only camera type 2 ($Z$, the only one in U01); types 1 and 3 fix the yaw at
	// -pi/2 instead (E-0045, E-0058)
	Common::Array<float> facing;
	const O3DObject &o = objects[object];
	if (o.name.contains("$Z$") && owner == (int)object) {
		const float *w = o.world;
		facing.resize(o.vertices.size());
		for (uint v = 0; v < o.vertices.size(); v += 3) {
			float d[3];
			for (int k = 0; k < 3; k++)
				d[k] = o.vertices[v] * w[k] + o.vertices[v + 1] * w[4 + k] + o.vertices[v + 2] * w[8 + k];
			for (int k = 0; k < 3; k++)
				facing[v + k] = w[12 + k] + d[0] * _facing[k] + d[1] * _facing[3 + k] + d[2] * _facing[6 + k];
		}
		vertices = &facing;
	}

	for (const O3DFace &face : objects[object].faces) {
		const O3DMaterial &mat = m.file.materials[face.material];
		const TGLuint tex = mat.textureMap.empty() ? 0 : texture(mat.textureMap);
		// Lighting is not specified (Q-0019): texture x ambient, or the second colour
		if (tex != _boundTexture) {
			if (tex) {
				tglEnable(TGL_TEXTURE_2D);
				tglBindTexture(TGL_TEXTURE_2D, tex);
			} else {
				tglDisable(TGL_TEXTURE_2D);
			}
			_boundTexture = tex;
		}
		if (tex) {
			tglColor3ub(_ambient[0], _ambient[1], _ambient[2]);
		} else {
			tglColor3ub(mat.colors[1][0] * _ambient[0] / 255, mat.colors[1][1] * _ambient[1] / 255,
			            mat.colors[1][2] * _ambient[2] / 255);
		}

		tglBegin(TGL_TRIANGLE_FAN);
		for (uint k = 0; k < face.indices.size(); k++) {
			const uint32 index = face.indices[k] * 3;
			if (index + 2 >= vertices->size())
				break;
			if (!face.uvs.empty())
				tglTexCoord2f(face.uvs[k * 2], face.uvs[k * 2 + 1]);
			tglVertex3f((*vertices)[index], (*vertices)[index + 1], (*vertices)[index + 2]);
		}
		tglEnd();
	}
}

bool Scene::pick(const Camera &cam, int width, int height, float x, float y,
                 const Model *&model, uint &object, float &depth) {
	// The render projection and camera axes (no roll: picking ignores the head bob)
	const float ky = (4.0f / 3.0f) / tan(cam.fov * M_PI / 360.0), kx = ky * height / width;
	const float cx = width / 2.0f, cy = height / 2.0f;
	const float a = cam.yaw, e = cam.pitch;
	const float right[3] = { -sinf(a), -cosf(a), 0 };
	const float up[3] = { cosf(e) * cosf(a), -cosf(e) * sinf(a), sinf(e) };
	const float forward[3] = { sinf(e) * cosf(a), -sinf(e) * sinf(a), -cosf(e) };

	depth = 1e6f;
	model = nullptr;
	for (const Model *m : _models) {
		if (m->hidden)
			continue;
		const Common::Array<O3DObject> &objects = m->file.objects;
		for (uint i = 0; i < objects.size(); i++) {
			// Welded objects are tested with their top object, whose vertices they share
			if (m->hiddenObjects[i] || objects[i].vertices.empty())
				continue;

			// The drawn level of detail supplies the faces
			const Model *drawn = m;
			uint top = i;
			float s2 = 0;
			for (int k = 0; k < 3; k++)
				s2 += (objects[i].world[12 + k] - cam.position[k]) * (objects[i].world[12 + k] - cam.position[k]);
			for (const Lod &lod : m->lods[i])
				if (lod.threshold <= s2) {
					drawn = lod.model;
					top = lod.object;
				}
			const Common::Array<O3DObject> &dobjects = drawn->file.objects;
			const Common::Array<float> &v = drawn->worldVertices[top];

			// Camera-space vertices, then projected
			Common::Array<float> cam3(v.size()), screen(v.size() / 3 * 2);
			for (uint j = 0; j < v.size(); j += 3) {
				float d[3];
				for (int k = 0; k < 3; k++)
					d[k] = v[j + k] - cam.position[k];
				const float X = d[0] * right[0] + d[1] * right[1] + d[2] * right[2];
				const float Y = d[0] * up[0] + d[1] * up[1] + d[2] * up[2];
				const float Z = d[0] * forward[0] + d[1] * forward[1] + d[2] * forward[2];
				cam3[j] = X;
				cam3[j + 1] = Y;
				cam3[j + 2] = Z;
				if (Z > 0) {
					screen[j / 3 * 2] = cx + cx * X * kx / Z;
					screen[j / 3 * 2 + 1] = cy - cy * Y * ky / Z;
				}
			}

			for (uint o = top; o < dobjects.size(); o++) {
				// The top's own faces and those of every welded object below it
				int owner = o;
				while (owner >= 0 && dobjects[owner].vertices.empty())
					owner = dobjects[owner].parent;
				if (owner != (int)top || (o != top && !dobjects[o].welded))
					continue;
				for (const O3DFace &f : dobjects[o].faces) {
					const uint n = f.indices.size();
					if (n < 3)
						continue;
					bool usable = true;
					for (uint32 index : f.indices)
						usable &= index * 3 + 2 < cam3.size() && cam3[index * 3 + 2] > 0.1f;
					// ponytail: faces crossing the near plane are skipped, not clipped
					if (!usable)
						continue;
					const float *v0 = &cam3[f.indices[0] * 3], *v1 = &cam3[f.indices[1] * 3], *v2 = &cam3[f.indices[2] * 3];
					const float e1[3] = { v0[0] - v1[0], v0[1] - v1[1], v0[2] - v1[2] };
					const float e2[3] = { v2[0] - v1[0], v2[1] - v1[1], v2[2] - v1[2] };
					float nrm[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
					if (nrm[0] * v0[0] + nrm[1] * v0[1] + nrm[2] * v0[2] >= -0.01f)
						continue; // back face

					bool inside = true;
					for (uint k = 0; k < n && inside; k++) {
						const float *si = &screen[f.indices[k] * 2], *sj = &screen[f.indices[(k + 1) % n] * 2];
						inside = (y - si[1]) * (sj[0] - si[0]) + (x - si[0]) * (si[1] - sj[1]) <= 0;
					}
					if (!inside)
						continue;

					// Depth where the view ray through the pixel meets the face's plane
					const float len = sqrtf(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
					const float r[3] = { (x - cx) / cx / kx, (cy - y) / cy / ky, 1 };
					const float mr = (nrm[0] * r[0] + nrm[1] * r[1] + nrm[2] * r[2]) / len;
					if (mr == 0)
						continue;
					const float d = (nrm[0] * v0[0] + nrm[1] * v0[1] + nrm[2] * v0[2]) / len / mr;
					if (d > 0 && d < depth) {
						// The face's own object: a welded card in a hand is its own hotspot
						// (E-0076). Through a LOD, the base object.
						depth = d;
						model = m;
						object = drawn == m ? o : i;
					}
				}
			}
		}
	}
	return model != nullptr;
}

} // End of namespace X3D
