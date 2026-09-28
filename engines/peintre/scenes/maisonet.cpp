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

// The cottage at Nuenen, scene 3 (maisonet.md).

namespace {

enum { kPoule, kPelle, kOiso, kTerre, kPlume, kNid, kPorte04 };

const ObjectDef kObjects[] = {
	{ "poule", 0xFF, false }, { "pelle", 4, false }, { "oiso", 0xFF, false },
	{ "terre", 0xFF, false }, { "plume", 2, false }, { "nid", 3, false },
	{ "porte04", 6, false }
};

enum { kAnimHen, kAnimSpade, kAnimBirdOut, kAnimBirdBack };

const AnimDef kAnims[] = {
	{ "poule2.3da", "poule", false }, { "pelle.3da", "pelle", false },
	{ "oisaller.3da", "oiso", false }, { "oisrturn.3da", "oiso", false }
};

const uint32 kFeather = 0x4abc64, kEarth = 0x4abc68, kHenDone = 0x4abc74, kDug = 0x4abc84, kBirdFlown = 0x4abc94;

class Maisonet : public SceneScript {
public:
	void init(World &w) override {
		setObjects(w, kObjects, ARRAYSIZE(kObjects));
		setAnims(w, kAnims, ARRAYSIZE(kAnims));
		if (w.var(kEarth))
			w.setHidden(node(w, kTerre), true);
		if (w.var(kFeather))
			w.setHidden(node(w, kPlume), true);
		if (w.var(kBirdFlown)) {
			w.anims[kAnimBirdOut].playing = true;
			w.anims[kAnimBirdBack].playing = false;
		}
		poseAt(w, kAnimHen, w.var(kHenDone) ? w.anims[kAnimHen].length - 1 : 1);
		if (!w.var(kDug)) {
			poseAt(w, kAnimSpade, 1);
			w.setHidden(node(w, kTerre), true);
		} else {
			// The code reads the hen track's length here, not the spade's.
			poseAt(w, kAnimSpade, w.anims[kAnimHen].length - 1);
			w.objects[kPelle].cursorType = 0xFF;
		}
		if (w.var(kEarth))
			w.setHidden(node(w, kTerre), true);
		if (w.zoneSolved(1))
			w.objects[kNid].cursorType = 0x3C;
		w.setAmbience("ferme");
	}

	// porte04 only within 5000 (0x4a2544).
	bool reachable(World &w, int object) override {
		return object != kPorte04 || w.distance(node(w, object)) < 5000;
	}

	void frame(World &w) override {
		int idx = w.objectIndex(w.pick());
		if (idx >= 0 && !reachable(w, idx))
			idx = -1;
		switch (clickedObject(w, idx)) {
		case kPlume:
			takeItem(w, kPlume, 2, kFeather);
			break;
		case kPelle:
			if (!w.var(kDug)) {
				startSound(w, "pelle");
				w.anims[kAnimSpade].playing = true;
				w.setHidden(node(w, kTerre), false);
			}
			break;
		case kTerre:
			takeItem(w, kTerre, 1, kEarth);
			break;
		case kNid:
			w.requestZone(1);
			break;
		case kPorte04:
			w.requestScene(kSceneMangeurs);
			break;
		default:
			break;
		}

		const AnimRecord &hen = w.anims[kAnimHen];
		if (!hen.playing && hen.frame == 1 && !w.var(kHenDone) && w.distance(node(w, kPoule)) < 2000) {
			startSound(w, "poule");
			w.anims[kAnimHen].playing = true;
		}
		if (!w.anims[kAnimBirdOut].playing && !w.anims[kAnimBirdBack].playing && !w.var(kBirdFlown) &&
			w.distance(node(w, kNid)) < 5000) {
			startSound(w, "oiseau");
			w.anims[kAnimBirdOut].playing = true;
		}
		w.clearClick();

		const int32 e = w.elapsed();
		const int32 half = MAX<int32>(e / 2, 1);
		if (stepAnim(w, kAnimHen, e)) {
			w.anims[kAnimHen].playing = false;
			w.var(kHenDone) = 1;
		}
		if (stepAnim(w, kAnimSpade, e)) {
			w.anims[kAnimSpade].playing = false;
			if (!w.var(kEarth))
				w.setHidden(node(w, kTerre), false);
			w.var(kDug) = 1;
			w.objects[kPelle].cursorType = 0xFF;
			w.objects[kTerre].cursorType = 2;
		}
		if (stepAnim(w, kAnimBirdOut, half)) {
			w.var(kBirdFlown) = 1;
			w.anims[kAnimBirdOut].playing = false;
			w.anims[kAnimBirdOut].frame = 1;
			w.anims[kAnimBirdBack].playing = true;
		}
		if (stepAnim(w, kAnimBirdBack, half)) {
			w.anims[kAnimBirdBack].playing = false;
			w.anims[kAnimBirdBack].frame = 1; // Q-0402
			w.anims[kAnimBirdOut].playing = true;
			w.anims[kAnimBirdOut].frame = 1;
		}
	}
};

} // End of anonymous namespace

SceneScript *createMaisonet() {
	return new Maisonet();
}

} // End of namespace Peintre
