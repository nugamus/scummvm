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
#include "common/system.h"
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
 * places the viewer after it, dev_look=<node>[,distance] in front of a node. Debug level 2 lists where the table objects show every 150
 * ticks, level 3 logs the camera every 15 ticks.
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
		if (first && ConfMan.hasKey("dev_look"))
			lookAt(w, ConfMan.get("dev_look"));
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
		goTo(w);
		if (gDebugLevel >= 2 && ++_ticks % 150 == 60)
			listObjects(w);
		if (gDebugLevel >= 3 && _ticks % 15 == 0) {
			const Camera &c = w.camera();
			debugC(3, kDebugScript, "Camera %d,%d,%d,%d,%d", c.x, c.y, c.z, c.pitch, c.yaw);
		}
	}

private:
	/**
	 * dev_goto=ms:node[,distance[,yaw]];... (ms since the engine started): once due and once
	 * the node is in the current scene, the viewer stands in front of it (lookAt).
	 */
	static void goTo(World &w) {
		static Common::Array<Common::String> pending;
		static bool parsed = false;
		if (!parsed) {
			parsed = true;
			Common::StringTokenizer t(ConfMan.get("dev_goto"), ";");
			while (!t.empty())
				pending.push_back(t.nextToken());
		}
		for (uint i = 0; i < pending.size(); i++) {
			const size_t colon = pending[i].findFirstOf(':');
			if (colon == Common::String::npos || g_system->getMillis() < (uint32)atoi(pending[i].c_str()))
				continue;
			const Common::String arg = pending[i].substr(colon + 1);
			debugC(1, kDebugScript, "dev_goto %s at %u ms", arg.c_str(), g_system->getMillis());
			if (arg.hasPrefix("@")) {
				// @scene,x,y,z,pitch,yaw: a camera in that scene.
				int32 v[6] = { -1, 0, 0, 0, 0, 0 };
				Common::StringTokenizer c(arg.substr(1), ",");
				for (int k = 0; k < 6 && !c.empty(); k++)
					v[k] = atoi(c.nextToken().c_str());
				if (v[0] != w.scene())
					continue;
				Camera &cam = w.camera();
				cam.x = v[1];
				cam.y = v[2];
				cam.z = v[3];
				cam.pitch = v[4];
				cam.yaw = v[5];
				pending.remove_at(i);
				return;
			}
			Common::StringTokenizer n(arg, ",");
			if (w.findObject(n.nextToken()) < 0)
				continue;
			lookAt(w, arg);
			pending.remove_at(i);
			return;
		}
	}

	/** The centre of a node's vertices in world space (render3d's parent x local order). */
	static bool worldCentre(World &w, int node, double c[3]) {
		const Common::Array<Node> &nodes = w.scene3D().nodes;
		if (node < 0)
			return false;
		Common::Array<int> chain;
		for (int k = node; k >= 0; k = nodes[k].parent)
			chain.insert_at(0, k);
		double r[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 }, p[3] = { 0, 0, 0 };
		for (int k : chain) {
			const Node &nd = nodes[k];
			double l[9], nr[9], np[3];
			for (int j = 0; j < 9; j++)
				l[j] = nd.rotation[j] / 32768.0;
			const double lp[3] = { (double)nd.position.x, (double)nd.position.y, (double)nd.position.z };
			for (int a = 0; a < 3; a++) {
				for (int b = 0; b < 3; b++)
					nr[3 * a + b] = r[3 * a] * l[b] + r[3 * a + 1] * l[3 + b] + r[3 * a + 2] * l[6 + b];
				np[a] = p[a] + r[3 * a] * lp[0] + r[3 * a + 1] * lp[1] + r[3 * a + 2] * lp[2];
			}
			memcpy(r, nr, sizeof(r));
			memcpy(p, np, sizeof(p));
		}
		debugC(1, kDebugScript, "dev_look: %s has %d vertices, %d children", nodes[node].name.c_str(),
			   nodes[node].vertices.size(), nodes[node].children.size());
		if (nodes[node].vertices.empty()) {
			memcpy(c, p, sizeof(p));
			return true;
		}
		c[0] = c[1] = c[2] = 0;
		for (const Vec3i &v : nodes[node].vertices) {
			c[0] += r[0] * v.x + r[1] * v.y + r[2] * v.z + p[0];
			c[1] += r[3] * v.x + r[4] * v.y + r[5] * v.z + p[1];
			c[2] += r[6] * v.x + r[7] * v.y + r[8] * v.z + p[2];
		}
		for (int i = 0; i < 3; i++)
			c[i] /= nodes[node].vertices.size();
		return true;
	}

	/** dev_look=<node>[,distance[,yaw]]: the viewer `distance` (default 1200) in front of a node. */
	static void lookAt(World &w, const Common::String &arg) {
		Common::StringTokenizer t(arg, ",");
		const Common::String name = t.nextToken();
		const double dist = t.empty() ? 1200 : atoi(t.nextToken().c_str());
		double c[3];
		if (!worldCentre(w, w.findObject(name), c)) {
			warning("dev_look: no node %s", name.c_str());
			return;
		}
		// The yaw whose forward (x, z) points best away from the node's approach side:
		// try each yaw, stand `dist` behind the node along it, keep the first that sees it.
		Camera &cam = w.camera();
		int32 best = 0;
		double bestDot = -2;
		const double dir[2] = { c[0] - cam.x, c[2] - cam.z };
		const double len = sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
		for (int32 yaw = 0; yaw < 4096; yaw += 8) {
			int32 m[9];
			cameraMatrix(0, yaw, 0, m);
			const double d = len > 0 ? (m[2] * dir[0] + m[8] * dir[1]) / 32768.0 / len : m[8] / 32768.0;
			if (d > bestDot) {
				bestDot = d;
				best = yaw;
			}
		}
		if (!t.empty())
			best = atoi(t.nextToken().c_str()) & 0xFFF;
		int32 m[9];
		cameraMatrix(0, best, 0, m);
		cam.x = (int32)(c[0] - m[2] / 32768.0 * dist);
		cam.z = (int32)(c[2] - m[8] / 32768.0 * dist);
		cam.y = (int32)c[1] - 700; // eye height over an object on the floor
		// Aim down at the node from the eye 700 above it (y points down; lower pitch looks down).
		cam.pitch = (int32)(-atan2(700.0, dist) * 4096 / (2 * M_PI)) & 0xFFF;
		cam.yaw = best;
		debugC(1, kDebugScript, "dev_look %s: centre %d, %d, %d; camera %d, %d, %d yaw %d", name.c_str(),
			   (int)c[0], (int)c[1], (int)c[2], cam.x, cam.y, cam.z, best);
	}

	/** Logs where each table object shows on screen: the hit pixel nearest its centre. */
	void listObjects(World &w) {
		const int c = w.pickAt(320, 240);
		const Common::Point m = w.vm()->mouse();
		const int u = w.pickAt(m.x, m.y);
		debugC(2, kDebugScript, "Centre node %s, node under the mouse %s", c >= 0 ? w.scene3D().nodes[c].name.c_str() : "none",
			   u >= 0 ? w.scene3D().nodes[u].name.c_str() : "none");
		// dev_list=node,node: more nodes to list than the table's.
		Common::Array<SceneObject> list = w.objects;
		Common::StringTokenizer extra(ConfMan.get("dev_list"), ",");
		while (!extra.empty()) {
			SceneObject o;
			o.name = extra.nextToken();
			o.node = w.findObject(o.name);
			list.push_back(o);
		}
		for (const SceneObject &o : list) {
			if (o.node < 0)
				continue;
			int n = 0;
			int32 sx = 0, sy = 0;
			for (int y = 2; y < 480; y += 4)
				for (int x = 2; x < 640; x += 4)
					if (w.pickAt(x, y) == o.node) {
						n++;
						sx += x;
						sy += y;
					}
			if (!n)
				continue;
			const int cx = sx / n, cy = sy / n;
			int bx = cx, by = cy, best = INT32_MAX;
			for (int y = 2; y < 480; y += 4)
				for (int x = 2; x < 640; x += 4)
					if (w.pickAt(x, y) == o.node && (x - cx) * (x - cx) + (y - cy) * (y - cy) < best) {
						best = (x - cx) * (x - cx) + (y - cy) * (y - cy);
						bx = x;
						by = y;
					}
			debugC(2, kDebugScript, "Object %s at %d, %d (%d samples)", o.name.c_str(), bx, by, n);
		}
	}

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
	return s && (ConfMan.hasKey("dev_vars") || ConfMan.hasKey("dev_camera") || ConfMan.hasKey("dev_look") || ConfMan.hasKey("dev_goto") || gDebugLevel >= 2) ? new DevScript(s) : s;
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
	if (a.frame <= 1)
		debugC(1, kDebugScript, "Track %s runs", a.anim.c_str());
	a.frame += step;
	// Q-0400: whether a track ends on reaching its length or on passing it.
	const bool ended = a.frame >= end;
	if (ended) {
		a.frame = end;
		debugC(2, kDebugScript, "Track %s at its end (%d)", a.anim.c_str(), end);
	}
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
