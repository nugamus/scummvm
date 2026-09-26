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
#include "x3d/renderer.h"
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
			_renderer->deleteTexture(t._value);
}

bool Scene::load(const Common::String &scriptName) {
	// Asset directory from the unit number; U00 borrows U04's (E-0034)
	const Common::String unit = scriptName.substr(0, 3);
	_dir = (unit.equalsIgnoreCase("U00") ? Common::String("U04") : unit) + "/";
	_dataDir = unit + "/";

	Common::SeekableReadStream *s = openBinChunk(Common::Path(_dataDir + "SCENE.BIN"), "#SCENE#");
	if (s) {
		for (byte &c : ambient)
			c = s->readUint32LE();
		scale = s->readFloatLE();
		delete s;
	}
	s = openBinChunk(Common::Path(_dataDir + "SCENE.BIN"), "#CAMERA#");
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
		if (keyword == 4) {
			// Every light of the file reaches every object loaded so far (lighting.md)
			loadLights(field);
			for (Model *m : _models)
				m->lit = true;
		} else if (keyword == 2 && !_models.empty()) {
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
				// Only a LOD whose root is welded as the base's is attached (E-0530)
				if (!_models.back()->file.objects.empty() && !lod->file.objects.empty() &&
				    _models.back()->file.objects[0].welded == lod->file.objects[0].welded)
					attachLod(_models.back(), 0, lod, 0, d * d);
			}
		}
	}

	// Camera types from the names, then names with one cut to start at their '*'
	// ("$Z$*U02_10" -> "*U02_10"); animation nodes keep the full names (E-0271)
	for (Common::Array<Model *> *list : { &_models, &_lodModels })
		for (Model *m : *list)
			for (O3DObject &o : m->file.objects) {
				o.cameraType = o.name.contains("$XYZ$") ? 1 : o.name.contains("$Z$") ? 2 : o.name.contains("$XZ$") ? 3 : 0;
				const size_t star = o.name.findFirstOf('*');
				if (o.cameraType && star != Common::String::npos)
					o.name = o.name.substr(star);
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

	// A material whose name was loaded before is that earlier material: texture,
	// transparency, draw mode, tiling and colours (E-0484)
	for (O3DMaterial &mat : m->file.materials) {
		if (_materials.contains(mat.name))
			mat = _materials[mat.name];
		else
			_materials[mat.name] = mat;
	}

	m->worldVertices.resize(m->file.objects.size());
	m->lods.resize(m->file.objects.size());
	m->hiddenObjects.resize(m->file.objects.size());
	m->unpickable.resize(m->file.objects.size());
	m->pickWhenHidden.resize(m->file.objects.size());
	m->worldNormals.resize(m->file.objects.size());
	m->colors.resize(m->file.objects.size());
	m->colorFrame.resize(m->file.objects.size());
	m->bounds.resize(m->file.objects.size() * 4);
	pose(*m);
	return m;
}

void Scene::pose(Model &m) {
	const Common::Array<O3DObject> &objects = m.file.objects;

	// Each object transforms its own vertex range; welded objects write into their top
	// object's shared array (animation.md, Welded objects)
	for (uint i = 0; i < objects.size(); i++) {
		m.worldVertices[i].resize(objects[i].vertices.size());
		m.worldNormals[i].resize(objects[i].normals.size());
	}
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
		const Common::Array<float> &nin = objects[top].normals;
		Common::Array<float> &nout = m.worldNormals[top];
		for (uint v = first * 3; v < (first + count) * 3 && v + 2 < in.size(); v += 3) {
			const float x = in[v], y = in[v + 1], z = in[v + 2];
			for (int k = 0; k < 3; k++)
				out[v + k] = x * w[k] + y * w[4 + k] + z * w[8 + k] + w[12 + k];
			if (v + 2 < nin.size()) {
				// Normals turn with the owner, without translation, renormalised
				float n[3], len = 0;
				for (int k = 0; k < 3; k++) {
					n[k] = nin[v] * w[k] + nin[v + 1] * w[4 + k] + nin[v + 2] * w[8 + k];
					len += n[k] * n[k];
				}
				len = len > 0 ? 1 / sqrtf(len) : 0;
				for (int k = 0; k < 3; k++)
					nout[v + k] = n[k] * len;
			}
		}
	}

	for (uint32 &state : m.colorFrame)
		state = 0; // lit colours follow the new pose

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
	m->animated = true;
	AnimNode n;
	n.name = m->file.objects[object].name;
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

void Scene::advance(float dt) {
	_stepDt = dt;
	for (AnimNode &n : _nodes) {
		if (!n.enabled || !n.model)
			continue;
		// A rewound clip hands back to the node's own animation at frame 1 (E-0057)
		if (n.clipActive && n.clip.backward && !n.clip.running) {
			n.clipActive = false;
			const A3DAnimation &a = n.base.file->animations[n.base.animation];
			n.base.frame = CLIP(1.0f, (float)a.firstFrame, (float)a.lastFrame);
			n.base.running = true;
		}
		Playback &p = n.clipActive ? n.clip : n.base;
		n.prevFrame = p.frame;
		n.prevClip = n.clipActive;
		p.advance(dt);
		animate(*p.file, p.animation, *n.model, p.object >= 0 ? p.object : n.object, p.frame);
	}
}

bool Scene::interpolate(float alpha) {
	bool moved = false;
	for (AnimNode &n : _nodes) {
		if (!n.enabled || !n.model)
			continue;
		const Playback &p = n.clipActive ? n.clip : n.base;
		float frame = p.frame;
		const float d = p.frame - n.prevFrame;
		// Not across a loop wrap, a turn or a jump the unit made
		if (n.prevFrame >= 0 && n.prevClip == n.clipActive && d != 0 && fabsf(d) <= 1.5f * _stepDt * p.fps) {
			frame = n.prevFrame + d * alpha;
			moved = true;
		}
		// Every node is applied again in list order, so later nodes still override earlier
		// ones (a talker's mouth over the body, E-0606)
		animate(*p.file, p.animation, *n.model, p.object >= 0 ? p.object : n.object, frame);
	}
	return moved;
}

void Scene::poseAll() {
	// ponytail: re-poses every animated file each step; track dirty objects if it shows up in profiles
	for (Model *m : _models) {
		if (m->animated) {
			m->file.updateWorld();
			pose(*m);
		}
	}
}

const Common::Array<byte> &Scene::lighting(const Model &m, uint owner) const {
	Common::Array<byte> &out = m.colors[owner];
	if (m.colorFrame[owner] == _lightingState)
		return out;
	m.colorFrame[owner] = _lightingState;

	// D starts at the ambient, each light adds colour * multiplier * falloff * cos; the
	// excess over 255 becomes S (lighting.md, Per-vertex colour)
	const Common::Array<float> &v = m.worldVertices[owner], &n = m.worldNormals[owner];
	out.resize(v.size() / 3 * 6);
	for (uint i = 0; i < v.size() / 3; i++) {
		float d[3] = { (float)ambient[0], (float)ambient[1], (float)ambient[2] };
		if (m.lit && i * 3 + 2 < n.size()) {
			for (const Light &l : _lights) {
				if (l.hidden)
					continue;
				float L[3], d2 = 0;
				for (int k = 0; k < 3; k++) {
					L[k] = l.position[k] - v[i * 3 + k];
					d2 += L[k] * L[k];
				}
				float kf = 1;
				if (l.attenuate) {
					if (d2 >= l.outer * l.outer)
						continue;
					if (d2 > l.inner * l.inner)
						kf = 1 - (d2 - l.inner * l.inner) / (l.outer * l.outer - l.inner * l.inner);
				}
				const float len = sqrtf(d2);
				if (len == 0)
					continue;
				const float c = (L[0] * n[i * 3] + L[1] * n[i * 3 + 1] + L[2] * n[i * 3 + 2]) / len;
				if (c <= 0)
					continue;
				for (int k = 0; k < 3; k++)
					d[k] += l.color[k] * l.multiplier * kf * c;
			}
		}
		for (int k = 0; k < 3; k++) {
			float s = 0;
			if (d[k] > 255) {
				s = MIN(d[k] - 255, 255.0f);
				d[k] = 255;
			} else if (d[k] < 0) {
				d[k] = 0;
			}
			out[i * 6 + k] = (byte)d[k];
			out[i * 6 + 3 + k] = (byte)s;
		}
	}
	return out;
}

void Scene::loadLights(const Common::String &path) {
	Common::File f;
	if (!f.open(Common::Path(_dir + path))) {
		warning("Unable to load %s%s", _dir.c_str(), path.c_str());
		return;
	}
	f.skip(32); // signature
	const uint32 count = f.readUint32LE();
	for (uint32 i = 0; i < count && !f.err(); i++) {
		Light l;
		f.skip(32); // name
		for (float &c : l.position)
			c = f.readFloatLE();
		f.read(l.color, 3);
		l.inner = f.readFloatLE();
		l.outer = f.readFloatLE();
		l.multiplier = f.readFloatLE();
		l.hidden = f.readUint32LE() != 0;
		l.attenuate = f.readUint32LE() != 0;
		if (f.readUint32LE()) {
			f.skip(5 * 4); // spot target and angles: no corpus sample (Q-0013)
			warning("%s: spot lights are drawn as omni lights", path.c_str());
		}
		_lights.push_back(l);
	}
	_lightingState++;
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
	// X3d_Object_Hide / _Unhide (obj, 1): the object and everything below it
	for (Model *m : _models) {
		const Common::Array<O3DObject> &objects = m->file.objects;
		for (uint i = 0; i < objects.size(); i++)
			for (int a = i; a >= 0; a = objects[a].parent)
				if (objects[a].name.equalsIgnoreCase(name)) {
					m->hiddenObjects[i] = hidden;
					break;
				}
	}
}

void Scene::hideObjectOnly(const Common::String &name, bool hidden) {
	for (Model *m : _models)
		for (uint i = 0; i < m->file.objects.size(); i++)
			if (m->file.objects[i].name.equalsIgnoreCase(name))
				m->hiddenObjects[i] = hidden;
}

void Scene::hideParent(const Common::String &name, bool hidden) {
	Model *m;
	uint o;
	if (!findObject(name, m, o) || m->file.objects[o].parent < 0)
		return;
	const int p = m->file.objects[o].parent;
	for (uint i = 0; i < m->file.objects.size(); i++)
		for (int a = i; a >= 0; a = m->file.objects[a].parent)
			if (a == p) {
				m->hiddenObjects[i] = hidden;
				break;
			}
}

void Scene::setPickWhenHidden(const Common::String &name, bool pick) {
	Model *m;
	uint o;
	if (findObject(name, m, o))
		m->pickWhenHidden[o] = pick;
}

void Scene::setObjectMap(const Common::String &object, const Common::String &mapName) {
	Model *m;
	uint o;
	if (!findObject(object, m, o) || m->file.objects[o].faces.empty())
		return;
	m->file.materials[m->file.objects[o].faces[0].material].textureMap = mapName;
}

void Scene::hideAll() {
	for (Model *m : _models)
		for (uint i = 0; i < m->hiddenObjects.size(); i++)
			m->hiddenObjects[i] = true;
}

Common::StringArray Scene::siblings(const Common::String &name) const {
	Common::StringArray out;
	Model *m;
	uint o;
	if (!findObject(name, m, o))
		return out;
	const Common::Array<O3DObject> &objects = m->file.objects;
	for (uint i = 0; i < objects.size(); i++)
		if (i != o && objects[i].parent == objects[o].parent)
			out.push_back(objects[i].name);
	return out;
}

Scene::AnimNode *Scene::findNode(const Common::String &objectName) {
	// Steps name hotspot nodes without their '*' (op 4 "U01_24" is node "*U01_24")
	for (AnimNode &n : _nodes)
		if (n.name.equalsIgnoreCase(objectName) || (n.name.hasPrefix("*") && n.name.substr(1).equalsIgnoreCase(objectName)))
			return &n;
	return nullptr;
}

bool Scene::findObject(const Common::String &name, Model *&model, uint &object) const {
	for (int i = _models.size() - 1; i >= 0; i--)
		for (uint o = 0; o < _models[i]->file.objects.size(); o++)
			if (_models[i]->file.objects[o].name.equalsIgnoreCase(name)) {
				model = _models[i];
				object = o;
				return true;
			}
	return false;
}

void Scene::renameObject(const Common::String &from, const Common::String &to) {
	Model *m;
	uint o;
	if (findObject(from, m, o)) {
		for (AnimNode &n : _nodes)
			if (n.model == m && n.object == o && n.name.equalsIgnoreCase(from))
				n.name = to;
		m->file.objects[o].name = to;
	}
}

void Scene::renameNode(const Common::String &from, const Common::String &to) {
	if (AnimNode *n = findNode(from))
		n->name = to;
}

void Scene::setPickable(const Common::String &namePrefix, bool pickable) {
	for (Model *m : _models)
		for (uint i = 0; i < m->file.objects.size(); i++)
			if (m->file.objects[i].name.hasPrefixIgnoreCase(namePrefix))
				m->unpickable[i] = !pickable;
}

Scene::Playback &Scene::active(AnimNode &n) {
	return n.clipActive ? n.clip : n.base;
}

void Scene::runNodeTo(const Common::String &name, float target, bool backward) {
	if (AnimNode *n = findNode(name)) {
		Playback &p = active(*n);
		p.running = true;
		p.backward = backward;
		p.stopAt = target;
	}
}

void Scene::pauseNode(const Common::String &name) {
	if (AnimNode *n = findNode(name))
		active(*n).running = false;
}

void Scene::setNodeFrame(const Common::String &name, float frame) {
	if (AnimNode *n = findNode(name)) {
		Playback &p = active(*n);
		const A3DAnimation &a = p.file->animations[p.animation];
		p.frame = CLIP(frame, p.first >= 0 ? p.first : (float)a.firstFrame, p.last >= 0 ? p.last : (float)a.lastFrame);
	}
}

void Scene::setNodeFps(const Common::String &name, float fps) {
	if (AnimNode *n = findNode(name))
		active(*n).fps = fps;
}

void Scene::setNodeLoop(const Common::String &name, bool loop) {
	if (AnimNode *n = findNode(name))
		active(*n).loop = loop;
}

void Scene::setNodeRange(const Common::String &name, float first, float last) {
	if (AnimNode *n = findNode(name)) {
		active(*n).first = first;
		active(*n).last = last;
	}
}

void Scene::stepNode(const Common::String &name, float dt) {
	if (AnimNode *n = findNode(name)) {
		Playback &p = active(*n);
		const bool running = p.running;
		p.running = true;
		p.advance(dt);
		p.running = running && p.running;
	}
}

float Scene::nodeFrame(const Common::String &name) {
	AnimNode *n = findNode(name);
	return n ? active(*n).frame : 0;
}

float Scene::nodeLastFrame(const Common::String &name) {
	AnimNode *n = findNode(name);
	if (!n)
		return 0;
	Playback &p = active(*n);
	return p.last >= 0 ? p.last : p.file->animations[p.animation].lastFrame;
}

bool Scene::nodeRunning(const Common::String &name) {
	AnimNode *n = findNode(name);
	return n && active(*n).running;
}

void Scene::loadClip(const Common::String &name, const Common::String &path, float fps, float frame) {
	playClip(name, path);
	if (AnimNode *n = findNode(name)) {
		n->clip.object = n->model->file.objects[n->object].parent;
		n->clip.fps = fps;
		n->clip.running = false;
		n->clip.frame = frame;
	}
}

void Scene::setClip(const Common::String &name, const Common::String &path, const Common::String &subAnimation,
                    int slot, bool activate) {
	AnimNode *n = findNode(name);
	const A3DFile *file = n ? clipFile(path) : nullptr;
	if (!file)
		return;
	uint animation = 0;
	int object = -1;
	if (!subAnimation.empty()) {
		while (animation < file->animations.size() && !file->animations[animation].name.equalsIgnoreCase(subAnimation))
			animation++;
		const Common::Array<O3DObject> &objects = n->model->file.objects;
		for (uint o = 0; o < objects.size() && object < 0; o++)
			if (objects[o].name.equalsIgnoreCase(subAnimation))
				object = o;
		if (animation == file->animations.size() || object < 0) {
			warning("Clip %s has no sub-animation %s", path.c_str(), subAnimation.c_str());
			return;
		}
	}
	Playback p;
	p.file = file;
	p.path = path;
	p.animation = animation;
	p.object = object;
	p.fps = n->base.fps;
	p.frame = file->animations[animation].firstFrame;
	p.running = false;
	p.loop = false;
	slot = CLIP(slot, 1, 15);
	if (!activate) {
		// clip holds slot n->slot; the others wait in slots[]
		if (slot == n->slot)
			n->clip = p;
		else
			n->slots[slot] = p;
		return;
	}
	if (n->slot != slot) {
		n->slots[n->slot] = n->clip;
		n->slot = slot;
	}
	n->clip = p;
	n->clipActive = true;
}

void Scene::activateSlot(const Common::String &name, int slot) {
	AnimNode *n = findNode(name);
	if (!n)
		return;
	if (slot <= 0) {
		n->clipActive = false;
		return;
	}
	slot = MIN(slot, 15);
	if (n->slot != slot) {
		n->slots[n->slot] = n->clip;
		n->clip = n->slots[slot];
		n->slot = slot;
	}
	n->clipActive = n->clip.file != nullptr;
}

Common::String Scene::clipPath(const Common::String &name) {
	AnimNode *n = findNode(name);
	return n && n->clipActive ? n->clip.path : Common::String();
}

int Scene::activeSlot(const Common::String &name) {
	AnimNode *n = findNode(name);
	return n && n->clipActive ? n->slot : 0;
}

O3DObject *Scene::object(const Common::String &name) {
	Model *m;
	uint o;
	return findObject(name, m, o) ? &m->file.objects[o] : nullptr;
}

Scene::Model *Scene::addModel(const Common::String &path, const Common::String &animation, float fps) {
	Model *m = loadModel(path);
	if (!m)
		return nullptr;
	m->lit = !_lights.empty();
	for (O3DObject &o : m->file.objects) {
		o.cameraType = o.name.contains("$XYZ$") ? 1 : o.name.contains("$Z$") ? 2 : o.name.contains("$XZ$") ? 3 : 0;
		const size_t star = o.name.findFirstOf('*');
		if (o.cameraType && star != Common::String::npos)
			o.name = o.name.substr(star);
	}
	_models.push_back(m);
	// One node for the whole file on the whole tree (u03.md cutscene step 2), whatever the
	// root's name
	if (!animation.empty()) {
		if (const A3DFile *file = clipFile(animation))
			addNode(file, 0, m, 0, fps);
	}
	return m;
}

bool Scene::addObjectNode(const Common::String &object, const Common::String &path, float fps) {
	Model *m;
	uint o;
	const A3DFile *file = findObject(object, m, o) ? clipFile(path) : nullptr;
	if (!file)
		return false;
	addNode(file, 0, m, o, fps);
	return true;
}

bool Scene::addTrackNode(const Common::String &object) {
	Model *m;
	uint o;
	if (!findObject(object, m, o))
		return false;
	for (uint i = 0; i < _nodes.size(); i++) {
		if (_nodes[i].model != m)
			continue;
		const A3DFile *file = _nodes[i].base.file;
		for (uint a = 0; a < file->animations.size(); a++)
			if (file->animations[a].name.equalsIgnoreCase(object)) {
				addNode(file, a, m, o, 30);
				return true;
			}
	}
	return false;
}

void Scene::nameNodes(Model *m, const Common::String &name) {
	for (AnimNode &n : _nodes)
		if (n.model == m)
			n.name = name;
}

void Scene::removeModel(Model *m) {
	// Its nodes stay as disabled tombstones: talkers keep node indices
	for (AnimNode &n : _nodes)
		if (n.model == m) {
			n.model = nullptr;
			n.enabled = false;
			n.name.clear();
		}
	for (uint i = 0; i < _models.size(); i++)
		if (_models[i] == m) {
			_models.remove_at(i);
			delete m;
			return;
		}
}

void Scene::endClip(const Common::String &name) {
	if (AnimNode *n = findNode(name))
		n->clipActive = false;
}

void Scene::setNodePingPong(const Common::String &name, bool pingPong) {
	if (AnimNode *n = findNode(name))
		active(*n).pingPong = pingPong;
}

void Scene::enableNode(const Common::String &name, bool enabled) {
	if (AnimNode *n = findNode(name))
		n->enabled = enabled;
}

void Scene::startAnimation(const Common::String &objectName) {
	if (AnimNode *n = findNode(objectName))
		n->base.running = true;
}

void Scene::setAnimationState(const Common::String &objectName, float frame, bool paused, float fps, bool loop) {
	// INFOOBJ finds its node by "equal or contains" (E-0271): *U02_10 is node $Z$*U02_10
	AnimNode *n = findNode(objectName);
	Common::String lower = objectName;
	lower.toLowercase();
	for (uint i = 0; i < _nodes.size() && !n; i++) {
		Common::String name = _nodes[i].name;
		name.toLowercase();
		if (name.contains(lower))
			n = &_nodes[i];
	}
	if (!n)
		return;
	const A3DAnimation &a = n->base.file->animations[n->base.animation];
	n->base.frame = CLIP(frame, (float)a.firstFrame, (float)a.lastFrame);
	if (paused)
		n->base.running = false;
	n->base.fps = fps;
	n->base.loop = loop;
}

const A3DFile *Scene::clipFile(const Common::String &path) {
	if (_clipFiles.contains(path))
		return _clipFiles[path];
	Common::File f;
	A3DFile *file = new A3DFile();
	if (!f.open(Common::Path(_dir + path)) || !file->load(f) || file->animations.empty()) {
		warning("Unable to load %s%s", _dir.c_str(), path.c_str());
		delete file;
		file = nullptr;
	} else {
		_animationFiles.push_back(file);
	}
	_clipFiles[path] = file;
	return file;
}

void Scene::playClip(const Common::String &objectName, const Common::String &path) {
	AnimNode *n = findNode(objectName);
	const A3DFile *file = n ? clipFile(path) : nullptr;
	if (!file)
		return;
	n->clip = n->base;
	n->clip.file = file;
	n->clip.path = path;
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

bool Scene::findFace(const Common::String &faceObject, const Common::String &owner, Model *&model, uint &object) const {
	model = nullptr;
	object = 0;
	bool owned = false;
	// Newest file first: a model loaded at run time replaces an older one (U03's clown)
	for (int mi = _models.size() - 1; mi >= 0; mi--) {
		Model *m = _models[mi];
		for (uint i = 0; i < m->file.objects.size() && !owned; i++)
			if (m->file.objects[i].name.equalsIgnoreCase(faceObject)) {
				bool below = false;
				for (int a = m->file.objects[i].parent; a >= 0 && !owner.empty() && !below; a = m->file.objects[a].parent)
					below = m->file.objects[a].name.equalsIgnoreCase(owner) || m->file.objects[a].name.equalsIgnoreCase("*" + owner);
				if (!model || below) {
					model = m;
					object = i;
					owned = below;
				}
			}
	}
	return model != nullptr;
}

Math::Vector3d Scene::facePosition(const Common::String &faceObject, const Common::String &owner) const {
	Model *m;
	uint o;
	if (!findFace(faceObject, owner, m, o))
		return Math::Vector3d();
	return Math::Vector3d(m->file.objects[o].world[12], m->file.objects[o].world[13], m->file.objects[o].world[14]);
}

int Scene::addFaceClip(const Common::String &faceObject, const Common::String &path, const Common::String &owner) {
	Model *model;
	uint object;
	findFace(faceObject, owner, model, object);
	Common::File f;
	A3DFile *file = new A3DFile();
	if (!model || !f.open(Common::Path(_dir + path)) || !file->load(f)) {
		delete file;
		return -1;
	}
	for (uint a = 0; a < file->animations.size(); a++) {
		if (!file->animations[a].name.equalsIgnoreCase(faceObject))
			continue;
		_animationFiles.push_back(file);
		addNode(file, a, model, object, 15);
		_nodes.back().enabled = false;
		_nodes.back().base.running = false;
		return _nodes.size() - 1;
	}
	delete file;
	return -1;
}

void Scene::setNode(int node, bool enabled, bool running) {
	_nodes[node].enabled = enabled && _nodes[node].model;
	_nodes[node].base.running = running;
}

void Scene::setNodeFrame(int node, float frame) {
	_nodes[node].base.frame = frame;
}

void Scene::setNodeFps(int node, float fps) {
	_nodes[node].base.fps = fps;
}

Common::Array<Math::Vector3d> Scene::surfacePoints(const Common::String &name, const Math::Vector3d &eye) const {
	Common::Array<Math::Vector3d> points;
	Model *m;
	uint o;
	if (!findObject(name, m, o))
		return points;
	// The faces of the object and of everything below it (a hotspot may be a dummy)
	for (uint d = 0; d < m->file.objects.size(); d++) {
		int a = d;
		while (a >= 0 && a != (int)o)
			a = m->file.objects[a].parent;
		int owner = d;
		while (owner >= 0 && m->file.objects[owner].vertices.empty())
			owner = m->file.objects[owner].parent;
		if (a < 0 || owner < 0)
			continue;
		const Common::Array<float> &v = m->worldVertices[owner];
		for (const O3DFace &face : m->file.objects[d].faces) {
			Math::Vector3d c;
			uint n = 0;
			for (uint32 index : face.indices)
				if (index * 3 + 2 < v.size()) {
					c += Math::Vector3d(v[index * 3], v[index * 3 + 1], v[index * 3 + 2]);
					n++;
				}
			if (n)
				points.push_back(c / (float)n);
		}
	}
	Common::sort(points.begin(), points.end(), [&](const Math::Vector3d &a, const Math::Vector3d &b) {
		return (a - eye).getSquareMagnitude() < (b - eye).getSquareMagnitude();
	});
	points.push_back(objectCenter(name));
	return points;
}

Math::Vector3d Scene::objectCenter(const Common::String &name) const {
	Model *m;
	uint o;
	if (!findObject(name, m, o))
		return Math::Vector3d();
	const float *b = &m->bounds[o * 4];
	if (b[3] >= 0)
		return Math::Vector3d(b[0], b[1], b[2]);
	return Math::Vector3d(m->file.objects[o].world[12], m->file.objects[o].world[13], m->file.objects[o].world[14]);
}

Math::Vector3d Scene::objectPosition(const Common::String &name) const {
	for (const Model *m : _models)
		for (const O3DObject &o : m->file.objects)
			if (o.name.equalsIgnoreCase(name))
				return Math::Vector3d(o.world[12], o.world[13], o.world[14]);
	return Math::Vector3d();
}

void Scene::Playback::advance(float dt) {
	if (!running)
		return;
	const A3DAnimation &a = file->animations[animation];
	const float lo = first >= 0 ? first : a.firstFrame;
	const float hi = last >= 0 ? last : a.lastFrame;
	frame += (backward ? -dt : dt) * fps;
	if (!loop && pingPong) {
		if (frame >= hi || frame <= lo) {
			frame = CLIP(frame, lo, hi);
			backward = frame >= hi;
		}
	} else if (!loop) {
		frame = CLIP(frame, lo, hi);
		if (backward ? frame <= lo : frame >= hi)
			running = false;
	} else if (frame > hi) {
		frame = fmodf(frame, hi) + lo;
	} else if (frame < lo) {
		frame = hi - (lo - frame);
	}
	if (stopAt >= 0 && fabsf(frame - stopAt) <= 2 * dt * fps) {
		frame = stopAt;
		running = false;
		stopAt = -1;
	}
}

void Scene::syncPlayback(Common::Serializer &s, Playback &p) {
	s.syncAsUint32LE(p.animation);
	s.syncAsFloatLE(p.fps);
	s.syncAsFloatLE(p.frame);
	s.syncAsByte(p.loop);
	s.syncAsByte(p.running);
	s.syncAsByte(p.backward);
	s.syncAsByte(p.pingPong);
	s.syncAsFloatLE(p.stopAt);
	s.syncAsFloatLE(p.first);
	s.syncAsFloatLE(p.last);
	s.syncAsSint32LE(p.object);
}

void Scene::syncState(Common::Serializer &s) {
	s.syncBytes(ambient, 3);

	// Every object's hidden and pickable flags (the original saves only the hotspots'
	// visibility and re-runs the unit's load hook for the rest; the superset is simpler)
	uint32 models = _models.size();
	s.syncAsUint32LE(models);
	for (uint32 i = 0; i < models; i++) {
		uint32 n = i < _models.size() ? _models[i]->hiddenObjects.size() : 0;
		s.syncAsUint32LE(n);
		for (uint32 o = 0; o < n; o++) {
			byte hidden = 0, unpickable = 0;
			if (s.isSaving()) {
				hidden = _models[i]->hiddenObjects[o];
				unpickable = _models[i]->unpickable[o];
			}
			s.syncAsByte(hidden);
			s.syncAsByte(unpickable);
			if (s.isLoading() && i < _models.size() && o < _models[i]->hiddenObjects.size()) {
				_models[i]->hiddenObjects[o] = hidden;
				_models[i]->unpickable[o] = unpickable;
			}
		}
	}

	// Animation nodes in creation order; talk's mouth nodes come after the scene's and are
	// created again by the unit's start, so a load skips the ones that do not exist yet
	uint32 nodes = _nodes.size();
	s.syncAsUint32LE(nodes);
	for (uint32 i = 0; i < nodes; i++) {
		AnimNode dummy;
		AnimNode &n = i < _nodes.size() ? _nodes[i] : dummy;
		byte enabled = n.enabled, clipActive = n.clipActive;
		s.syncAsByte(enabled);
		s.syncAsByte(clipActive);
		syncPlayback(s, n.base);
		Common::String path = n.clip.path;
		s.syncString(path);
		if (s.isLoading()) {
			n.enabled = enabled;
			n.clipActive = clipActive && !path.empty();
			if (!path.empty()) {
				n.clip = n.base;
				n.clip.file = clipFile(path);
				n.clip.path = path;
				if (!n.clip.file)
					n.clipActive = false;
			}
		}
		syncPlayback(s, n.clip);
		s.syncAsSint32LE(n.slot, 2);
		for (int k = 1; k < 16; k++) {
			Common::String extra = n.slots[k].path;
			s.syncString(extra, 2);
			if (s.isLoading())
				n.slots[k].file = extra.empty() ? nullptr : clipFile(extra);
			if (s.isLoading())
				n.slots[k].path = extra;
			if (!extra.empty())
				syncPlayback(s, n.slots[k]);
		}
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

uint32 Scene::texture(const Common::String &mapName) {
	if (_textures.contains(mapName))
		return _textures[mapName];

	// Map names end ".TGA"; the file is the .dmf of the same name in Maps/ (E-0038)
	Common::String name = mapName;
	const size_t dot = name.findFirstOf('.');
	if (dot != Common::String::npos)
		name.erase(dot);

	uint32 id = 0;
	Common::File f;
	Graphics::Surface *surface = nullptr;
	if (f.open(Common::Path(_dir + "Maps/" + name + ".dmf")))
		surface = loadDMF(f);
	if (surface) {
		id = _renderer->createTexture(*surface);
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
	// 1,000,000 (nothing is cut off); TinyGL's depth buffer needs a tighter range: 1 to
	// 20000, or out to the farthest object
	const float sy = (4.0f / 3.0f) / tanf(cam.fov * (float)M_PI / 360.0f);
	const float sx = sy * height / width;
	const float n = 1;
	float f = 20000;
	for (const Model *m : _models)
		for (uint i = 0; !m->hidden && i < m->file.objects.size(); i++) {
			const float *b = &m->bounds[i * 4];
			const float d[3] = { b[0] - cam.position[0], b[1] - cam.position[1], b[2] - cam.position[2] };
			f = MAX(f, sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) + b[3]);
		}
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

	_renderer->begin3D(projection, view);
	if (memcmp(ambient, _litAmbient, 3)) {
		memcpy(_litAmbient, ambient, 3);
		_lightingState++;
	}

	for (int k = 0; k < 3; k++) {
		_eye[k] = p[k];
		_right[k] = right[k];
		_up[k] = up[k];
		_forward[k] = forward[k];
	}
	_halfWidth = 1 / sx;
	_halfHeight = 1 / sy;

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
				if (!maxDetail && lod.threshold <= s) {
					drawn = lod.model;
					object = lod.object;
				}
			}
			drawObject(*drawn, object);
		}
	}

	// The held-back faces, farthest key first; equal keys: last queued first
	_deferredOrder.resize(_deferredCount);
	for (uint i = 0; i < _deferredCount; i++)
		_deferredOrder[i] = &_deferred[i];
	Common::sort(_deferredOrder.begin(), _deferredOrder.end(), [](const Deferred *x, const Deferred *y) {
		return x->key != y->key ? x->key > y->key : x->order > y->order;
	});
	for (const Deferred *d : _deferredOrder) {
		_renderer->setBlend(d->additive ? Renderer::kAdditive : Renderer::kAlpha, d->keyed);
		drawFace(*d);
	}
	_deferredCount = 0;
	_renderer->setBlend(Renderer::kOpaque, true);
}

void Scene::faceCamera(const Model &m, uint object, Common::Array<float> &v) const {
	// Welded or not, the object's own vertices turn about its own origin (E-0270)
	const float *w = m.file.objects[object].world;
	Common::Array<bool> &done = _faceDone;
	done.resize(0);
	done.resize(v.size() / 3, false);
	for (const O3DFace &face : m.file.objects[object].faces)
		for (uint32 index : face.indices) {
			if (index * 3 + 2 >= v.size() || done[index])
				continue;
			done[index] = true;
			float d[3], out[3];
			for (int k = 0; k < 3; k++)
				d[k] = v[index * 3 + k] - w[12 + k];
			for (int k = 0; k < 3; k++)
				out[k] = w[12 + k] + d[0] * _facing[k] + d[1] * _facing[3 + k] + d[2] * _facing[6 + k];
			for (int k = 0; k < 3; k++)
				v[index * 3 + k] = out[k];
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
	if (objects[object].cameraType == 2) {
		_facingVertices.resize(vertices->size());
		Common::copy(vertices->begin(), vertices->end(), _facingVertices.begin());
		faceCamera(m, object, _facingVertices);
		vertices = &_facingVertices;
	}

	for (const O3DFace &face : objects[object].faces) {
		const O3DMaterial &mat = m.file.materials[face.material];
		// Held-back faces are built in place
		const bool deferred = mat.transparency || mat.mode == 2;
		if (deferred && _deferredCount == _deferred.size())
			_deferred.push_back(Deferred());
		Deferred opaque;
		Deferred &f = deferred ? _deferred[_deferredCount++] : opaque;
		f.tex = mat.textureMap.empty() ? 0 : texture(mat.textureMap);
		f.clamp = !mat.wrap;
		// The colour key cuts texels only in draw modes 1..3 (E-0481)
		f.keyed = mat.mode >= 1 && mat.mode <= 3;
		f.additive = mat.mode == 2;
		f.alpha = mat.transparency ? (byte)(255 - 2.55f * MIN<uint32>(mat.transparency, 100)) : 255;

		// Class 0 draws the texture as stored; class 2 is lit per vertex (lighting.md)
		const Common::Array<byte> *lit = mat.renderClass == 0 ? nullptr : &lighting(m, owner);
		f.lit = lit != nullptr;
		f.hasUV = !face.uvs.empty();
		f.anySpecular = false;
		f.count = 0;
		float farthest = -1e30f;
		for (uint k = 0; k < face.indices.size() && f.count < 64; k++) {
			const uint32 index = face.indices[k] * 3;
			if (index + 2 >= vertices->size())
				break;
			float depth = 0;
			for (int c = 0; c < 3; c++) {
				f.xyz[f.count * 3 + c] = (*vertices)[index + c];
				depth += ((*vertices)[index + c] - _eye[c]) * _forward[c];
			}
			farthest = MAX(farthest, depth);
			if (f.hasUV) {
				f.uv[f.count * 2] = face.uvs[k * 2];
				f.uv[f.count * 2 + 1] = face.uvs[k * 2 + 1];
			}
			if (lit) {
				const byte *c = &(*lit)[face.indices[k] * 6];
				for (int j = 0; j < 3; j++) {
					// Untextured faces: the diffuse colour times the light
					f.rgb[f.count * 3 + j] = f.tex ? c[j] : mat.colors[1][j] * c[j] / 256;
					f.spec[f.count * 3 + j] = f.tex ? c[3 + j] : 0;
					f.anySpecular |= f.spec[f.count * 3 + j] != 0;
				}
			}
			f.count++;
		}
		if (deferred) {
			f.key = (int)farthest;
			f.order = _deferredCount - 1;
		} else {
			_renderer->setBlend(Renderer::kOpaque, f.keyed);
			drawFace(f);
		}
	}
}

void Scene::drawFace(const Deferred &f) {
	_renderer->setTexture(f.tex);
	if (f.tex)
		_renderer->setClamp(f.clamp);
	_renderer->drawFan(f.xyz, f.hasUV ? f.uv : nullptr, f.lit ? f.rgb : nullptr, f.count, f.alpha);

	// Light beyond 255 is added on top: pixel = texture * D / 255 + S
	if (f.anySpecular) {
		_renderer->setBlend(Renderer::kAdditive, false);
		_renderer->setTexture(0);
		_renderer->drawFan(f.xyz, nullptr, f.spec, f.count);
		_renderer->setBlend(f.alpha == 255 && !f.additive ? Renderer::kOpaque : f.additive ? Renderer::kAdditive : Renderer::kAlpha, f.keyed);
	}
}

bool Scene::pick(const Camera &cam, int width, int height, float x, float y,
                 const Model *&model, uint &object, float &depth) {
	// The render projection and camera axes (no roll: picking ignores the head bob)
	const float ky = (4.0f / 3.0f) / tanf(cam.fov * (float)M_PI / 360.0f), kx = ky * height / width;
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
			// Welded objects are tested with their top object, whose vertices they share;
			// each is skipped when hidden, as the draw skips it
			if (m->unpickable[i] || objects[i].vertices.empty())
				continue;

			// The drawn level of detail supplies the faces
			const Model *drawn = m;
			uint top = i;
			float s2 = 0;
			for (int k = 0; k < 3; k++)
				s2 += (objects[i].world[12 + k] - cam.position[k]) * (objects[i].world[12 + k] - cam.position[k]);
			for (const Lod &lod : m->lods[i])
				if (!maxDetail && lod.threshold <= s2) {
					drawn = lod.model;
					top = lod.object;
				}
			const Common::Array<O3DObject> &dobjects = drawn->file.objects;
			// Camera-facing objects are picked as drawn (E-0270)
			Common::Array<float> &facing = _facingVertices;
			const Common::Array<float> *vp = &drawn->worldVertices[top];
			for (uint o = top; o < dobjects.size(); o++) {
				if (dobjects[o].cameraType != 2)
					continue;
				int owner = o;
				while (owner >= 0 && dobjects[owner].vertices.empty())
					owner = dobjects[owner].parent;
				if (owner != (int)top || (o != top && !dobjects[o].welded))
					continue;
				if (vp != &facing) {
					facing.resize(vp->size());
					Common::copy(vp->begin(), vp->end(), facing.begin());
					vp = &facing;
				}
				faceCamera(*drawn, o, facing);
			}
			const Common::Array<float> &v = *vp;

			// Camera-space vertices, then projected
			Common::Array<float> &cam3 = _pickCamera, &screen = _pickScreen;
			cam3.resize(v.size());
			screen.resize(v.size() / 3 * 2);
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
				const uint base = drawn == m ? o : i;
				if ((m->hiddenObjects[base] && !m->pickWhenHidden[base]) || m->unpickable[base])
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
					// The any-corner test (E-0607): corners k = 1..n-1, then 0, with
					// n_k = (v[k-1] - v[k]) x (v[k+1] - v[k]); front at the first k where
					// n_k . v[k-1] < -0.01 (collinear corners give 0 and are passed over)
					float nrm[3] = { 0, 0, 0 };
					const float *v0 = nullptr;
					for (uint c = 1; c <= n && !v0; c++) {
						const uint k = c % n;
						const float *pa = &cam3[f.indices[(k + n - 1) % n] * 3], *pb = &cam3[f.indices[k] * 3], *pc = &cam3[f.indices[(k + 1) % n] * 3];
						const float e1[3] = { pa[0] - pb[0], pa[1] - pb[1], pa[2] - pb[2] };
						const float e2[3] = { pc[0] - pb[0], pc[1] - pb[1], pc[2] - pb[2] };
						const float nk[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
						if (nk[0] * pa[0] + nk[1] * pa[1] + nk[2] * pa[2] < -0.01f) {
							memcpy(nrm, nk, sizeof(nrm));
							v0 = pa;
						}
					}
					if (!v0)
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
