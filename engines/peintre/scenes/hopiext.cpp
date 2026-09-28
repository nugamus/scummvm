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

#include "peintre/scenes/scenes.h"

namespace Peintre {

// The hospital courtyard, scene 2 (hopiext.md).

namespace {

enum {
	kCloche, kGrille, kPlak, kGrilledrt, kGrillegche, kArbre11, kPorteent,
	kKorde1, kKorde2, kKorde3, kKorde4
};

const ObjectDef kObjects[] = {
	{ "cloche", 0xFF, false }, { "grille", 0xFF, false }, { "plak", 0xFF, false },
	{ "grilledrt", 4, false }, { "grillegche", 4, false }, { "arbre11", 3, false },
	{ "porteent", 6, false }, { "korde1", 0xFF, false }, { "korde2", 0xFF, false },
	{ "korde3", 0xFF, false }, { "korde4", 0xFF, false }
};

enum { kAnimGate, kAnimBell };

const AnimDef kAnims[] = {
	{ "grille.3da", "ciel", false }, { "cloche.3da", "cloche", false }
};

const uint32 kGateOpen = 0x4abcfc;

class Hopiext : public SceneScript {
public:
	void init(World &w) override {
		setObjects(w, kObjects, ARRAYSIZE(kObjects));
		setAnims(w, kAnims, ARRAYSIZE(kAnims));
		w.loadBoxSet(1, "BOX1.3DI");
		w.setAmbience("hopi_ext");
		static const int kGateObjects[] = { kCloche, kKorde1, kKorde2, kKorde3, kKorde4, kGrilledrt, kGrillegche };
		const bool open = w.var(kGateOpen) != 0;
		if (open) {
			poseAt(w, kAnimGate, w.anims[kAnimGate].length - 1);
			w.setBoxSet(1);
		}
		for (int i : kGateObjects)
			w.objects[i].cursorType = open ? 0xFF : 4;
		if (w.var(0x4abd00))
			w.objects[kPlak].cursorType = 0xFF;
		if (w.zoneSolved(14))
			w.objects[kArbre11].cursorType = 0x3C;
	}

	void frame(World &w) override {
		int idx = w.objectIndex(w.pick());
		// arbre11 only within 1500 (0x4a2534).
		if (idx == kArbre11 && w.distance(node(w, idx)) >= 1500)
			idx = -1;
		switch (clickedObject(w, idx)) {
		case kArbre11:
			w.requestZone(14);
			break;
		case kCloche:
		case kKorde1:
		case kKorde2:
		case kKorde3:
		case kKorde4:
			if (!w.var(kGateOpen)) {
				w.anims[kAnimBell].playing = true;
				startSound(w, "cloche");
				w.var(kGateOpen) = 1;
			}
			break;
		case kPorteent:
			w.requestScene(kSceneHopiint);
			break;
		case kGrilledrt:
		case kGrillegche:
			if (!w.var(kGateOpen)) {
				w.anims[kAnimGate].playing = true;
				w.var(kGateOpen) = 1;
			}
			break;
		default:
			break;
		}
		if (w.camera().z < -1000)
			w.requestScene(kScenePont);
		w.clearClick();

		const int32 e = w.elapsed();
		if (stepAnim(w, kAnimGate, e)) {
			w.var(kGateOpen) = 1;
			w.anims[kAnimGate].playing = false;
			w.setBoxSet(1);
			w.objects[kCloche].cursorType = 0xFF;
			w.objects[kGrilledrt].cursorType = 0xFF;
			w.objects[kGrillegche].cursorType = 0xFF;
		}
		if (stepAnim(w, kAnimBell, e)) {
			w.anims[kAnimBell].frame = 1;
			w.anims[kAnimBell].playing = false;
			w.anims[kAnimGate].playing = true;
			startSound(w, "portail");
			w.objects[kCloche].cursorType = 0xFF;
		}
	}
};

} // End of anonymous namespace

SceneScript *createHopiext() {
	return new Hopiext();
}

} // End of namespace Peintre
