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

#include "common/debug.h"

#include "peintre/detection.h"
#include "peintre/peintre.h"
#include "peintre/sound.h"
#include "peintre/scenes/scenes.h"

namespace Peintre {

// The scenes' own code (games/mission-sunlight/docs/<scene>.md): one file per scene.

SceneScript *createSceneScript(int scene, const Common::String &bundle) {
	switch (scene) {
	case kSceneMusee: return createMusee();
	default: return nullptr;
	}
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
