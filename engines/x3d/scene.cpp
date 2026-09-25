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

		// Animations, lights (Q-0019) and cameras are not used yet
		if (keyword == 1) {
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

	// Static view: bake each object's world transform into its vertices
	m->worldVertices.resize(m->file.objects.size());
	m->lods.resize(m->file.objects.size());
	m->hiddenObjects.resize(m->file.objects.size());
	for (uint i = 0; i < m->file.objects.size(); i++) {
		const O3DObject &o = m->file.objects[i];
		const float *w = o.world;
		Common::Array<float> &out = m->worldVertices[i];
		out.resize(o.vertices.size());
		for (uint v = 0; v < o.vertices.size(); v += 3) {
			const float x = o.vertices[v], y = o.vertices[v + 1], z = o.vertices[v + 2];
			for (int k = 0; k < 3; k++)
				out[v + k] = x * w[k] + y * w[4 + k] + z * w[8 + k] + w[12 + k];
		}
	}
	return m;
}

void Scene::hideObject(const Common::String &name) {
	for (Model *m : _models)
		for (uint i = 0; i < m->file.objects.size(); i++)
			if (m->file.objects[i].name.equalsIgnoreCase(name))
				m->hiddenObjects[i] = true;
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
	const float right[3] = { -sinf(a), -cosf(a), 0 };
	const float up[3] = { cosf(e) * cosf(a), -cosf(e) * sinf(a), sinf(e) };
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
	const Common::Array<float> &vertices = m.worldVertices[owner];

	for (const O3DFace &face : objects[object].faces) {
		const O3DMaterial &mat = m.file.materials[face.material];
		const TGLuint tex = mat.textureMap.empty() ? 0 : texture(mat.textureMap);
		// Lighting is not specified (Q-0019): texture x ambient, or the second colour
		if (tex) {
			tglEnable(TGL_TEXTURE_2D);
			tglBindTexture(TGL_TEXTURE_2D, tex);
			tglColor3ub(_ambient[0], _ambient[1], _ambient[2]);
		} else {
			tglDisable(TGL_TEXTURE_2D);
			tglColor3ub(mat.colors[1][0] * _ambient[0] / 255, mat.colors[1][1] * _ambient[1] / 255,
			            mat.colors[1][2] * _ambient[2] / 255);
		}

		tglBegin(TGL_TRIANGLE_FAN);
		for (uint k = 0; k < face.indices.size(); k++) {
			const uint32 index = face.indices[k] * 3;
			if (index + 2 >= vertices.size())
				break;
			if (!face.uvs.empty())
				tglTexCoord2f(face.uvs[k * 2], face.uvs[k * 2 + 1]);
			tglVertex3f(vertices[index], vertices[index + 1], vertices[index + 2]);
		}
		tglEnd();
	}
}

} // End of namespace X3D
