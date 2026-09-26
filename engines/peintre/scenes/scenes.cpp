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

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/tokenizer.h"

#include "peintre/detection.h"
#include "peintre/peintre.h"
#include "peintre/sound.h"
#include "peintre/scenes/scenes.h"

namespace Peintre {

// The scenes' own code (games/mission-sunlight/docs/<scene>.md): one file per scene.

namespace {

/**
 * Dev harness around the first scene loaded (game domain of the dev ini):
 * dev_vars=addr=value,... sets 3D block words before its init, dev_camera=x,y,z,pitch,yaw
 * places the viewer after it; with debug level 3 the camera is logged every 15 ticks.
 */
class DevScript : public SceneScript {
public:
	DevScript(SceneScript *s) : _s(s) {}

	void init(World &w) override {
		static bool first = true;
		if (first && ConfMan.hasKey("dev_vars")) {
			Common::StringTokenizer t(ConfMan.get("dev_vars"), ",");
			while (!t.empty()) {
				const Common::String v = t.nextToken();
				const size_t eq = v.findFirstOf('=');
				if (eq != Common::String::npos)
					w.var(strtoul(v.c_str(), nullptr, 0)) = strtoul(v.c_str() + eq + 1, nullptr, 0);
			}
		}
		_s->init(w);
		if (first && ConfMan.hasKey("dev_camera")) {
			int32 c[5] = { 0, 0, 0, 0, 0 };
			Common::StringTokenizer t(ConfMan.get("dev_camera"), ",");
			for (int i = 0; i < 5 && !t.empty(); i++)
				c[i] = atoi(t.nextToken().c_str());
			Camera &cam = w.camera();
			cam.x = c[0];
			cam.y = c[1];
			cam.z = c[2];
			cam.pitch = c[3];
			cam.yaw = c[4];
		}
		first = false;
	}

	void frame(World &w) override {
		_s->frame(w);
		if (gDebugLevel >= 3 && ++_ticks % 15 == 0) {
			const Camera &c = w.camera();
			debugC(3, kDebugScript, "Camera %d,%d,%d,%d,%d", c.x, c.y, c.z, c.pitch, c.yaw);
		}
	}

private:
	Common::ScopedPtr<SceneScript> _s;
	uint _ticks = 0;
};

SceneScript *createScript(int scene, const Common::String &bundle) {
	switch (scene) {
	case kSceneMusee: return createMusee();
	case kSceneAuberge: return createAuberge();
	case kSceneHopiext: return createHopiext();
	case kSceneMaisonet: return createMaisonet();
	case kSceneMangeurs: return createMangeurs();
	case kSceneCafe: return createCafe();
	case kSceneChambre: return bundle == "chambrev" ? createChambrev() : createChambreb();
	case kSceneMaisonj: return createMaisonj();
	case kSceneHopiint: return createHopiint();
	case kScenePont: return createPont();
	case kSceneTerrasse: return createTerrasse();
	case kSceneJardin: return createJardin();
	case kSceneChamp: return createChamp();
	case kSceneEglise: return createEglise();
	default: return nullptr;
	}
}

} // End of anonymous namespace

SceneScript *createSceneScript(int scene, const Common::String &bundle) {
	SceneScript *s = createScript(scene, bundle);
	return s && (ConfMan.hasKey("dev_vars") || ConfMan.hasKey("dev_camera") || gDebugLevel >= 3) ? new DevScript(s) : s;
}

void setObjects(World &w, const ObjectDef *defs, uint count) {
	w.objects.clear();
	for (uint i = 0; i < count; i++) {
		SceneObject o;
		o.name = defs[i].name;
		o.cursorType = defs[i].cursorType;
		o.startHidden = defs[i].hidden;
		o.node = -1;
		w.objects.push_back(o);
	}
	w.resolveObjects();
	for (const SceneObject &o : w.objects)
		if (o.node < 0)
			debugC(1, kDebugScript, "Scene object %s is not in the scene", o.name.c_str());
}

void setAnims(World &w, const AnimDef *defs, uint count) {
	w.anims.clear();
	for (uint i = 0; i < count; i++) {
		AnimRecord a;
		a.anim = defs[i].anim;
		a.node = defs[i].node;
		a.playing = defs[i].playing;
		w.anims.push_back(a);
	}
	w.loadAnims();
	for (AnimRecord &a : w.anims) {
		a.frame = 1;
		if (a.nodeIndex < 0)
			debugC(1, kDebugScript, "Animation %s: node %s is not in the scene", a.anim.c_str(), a.node.c_str());
	}
}

int clickedObject(World &w, int idx) {
	static int hovered = -1;
	if (idx != hovered && idx >= 0 && gDebugLevel >= 2) {
		const Common::Point m = w.vm()->mouse();
		debugC(2, kDebugScript, "Hover %s at %d, %d", w.objects[idx].name.c_str(), m.x, m.y);
	}
	hovered = idx;
	w.applyHover(-1);
	if (idx >= 0 && w.click() && w.cursor() == kCursorArrow) {
		debugC(1, kDebugScript, "Clicked %s", w.objects[idx].name.c_str());
		return idx;
	}
	w.applyHover(idx);
	return -1;
}

bool stepAnim(World &w, uint rec, int32 step, int32 end) {
	AnimRecord &a = w.anims[rec];
	if (!a.playing)
		return false;
	a.frame += step;
	// Q-0400: whether a track ends on reaching its length or on passing it.
	const bool ended = a.frame >= end;
	if (ended)
		a.frame = end;
	w.pose(rec, a.frame);
	return ended;
}

void poseAt(World &w, uint rec, int32 frame) {
	w.anims[rec].frame = frame;
	w.pose(rec, frame);
}

void takeItem(World &w, int objIdx, int item, uint32 flag) {
	w.takeItem(node(w, objIdx), item);
	w.var(flag) = 1;
}

void startSound(World &w, const Common::String &name, bool loop) {
	if (!w.vm()->sound()->isStaticPlaying(name))
		w.playSound(name, loop);
}

void scrollUVs(World &w, int node, int32 du, int32 dv, uint32 wrap) {
	if (node < 0)
		return;
	Common::Array<int32> &uvs = w.scene3D().nodes[node].uvs;
	for (uint i = 0; i + 1 < uvs.size(); i += 2) {
		uvs[i] += du;
		uvs[i + 1] += dv;
		if (wrap && du)
			uvs[i] &= wrap;
		if (wrap && dv)
			uvs[i + 1] &= wrap;
	}
}

} // End of namespace Peintre
