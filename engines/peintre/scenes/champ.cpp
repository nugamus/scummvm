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

// The wheat field with crows, scene 12 (champ.md).

namespace {

enum { kFaux, kCoclicot, kGun, kSabots };

const ObjectDef kObjects[] = {
	{ "faux", 0xFF, false }, { "coclicot", 3, false }, { "gun", 2, false }, { "sabots", 3, false }
};

enum { kAnimCrows, kAnimScythe };

const AnimDef kAnims[] = {
	{ "korbeaux.3da", "corbopere", false }, { "faux.3da", "animfaux01", false }
};

const uint32 kGunTaken = 0x4abcf4, kScytheUsed = 0x4abcf8;

class Champ : public SceneScript {
public:
	void init(World &w) override {
		setObjects(w, kObjects, ARRAYSIZE(kObjects));
		setAnims(w, kAnims, ARRAYSIZE(kAnims));
		w.setHidden(node(w, kFaux), w.sunflowers() != 4);
		if (w.var(kScytheUsed)) {
			poseAt(w, kAnimScythe, w.anims[kAnimScythe].length);
			w.objects[kFaux].cursorType = 0xFF;
		} else {
			w.objects[kFaux].cursorType = 4;
		}
		if (w.var(kGunTaken))
			w.setHidden(node(w, kGun), true);
		if (w.zoneSolved(22))
			w.objects[kCoclicot].cursorType = 0x3C;
		if (w.zoneSolved(24))
			w.objects[kSabots].cursorType = 0x3C;
		w.setAmbience("champ");
	}

	void frame(World &w) override {
		switch (clickedObject(w, w.objectIndex(w.pick()))) {
		case kCoclicot:
			w.requestZone(22);
			break;
		case kSabots:
			w.requestZone(24);
			break;
		case kFaux:
			if (!w.var(kScytheUsed)) {
				w.anims[kAnimScythe].playing = true;
				startSound(w, "faux");
			}
			break;
		case kGun:
			takeItem(w, kGun, 32, kGunTaken);
			startSound(w, "pistolet");
			w.anims[kAnimCrows].playing = true;
			startSound(w, "corbeaux");
			break;
		default:
			break;
		}
		w.clearClick();

		const int32 e = w.elapsed();
		if (stepAnim(w, kAnimCrows, e))
			w.anims[kAnimCrows].playing = false;
		if (stepAnim(w, kAnimScythe, e)) {
			w.objects[kFaux].cursorType = 0xFF;
			w.var(kScytheUsed) = 1;
			w.anims[kAnimScythe].playing = false;
		}

		if (w.camera().x < -16000)
			w.requestScene(kSceneEglise);
		else if (w.camera().z < -0x157c)
			w.requestScene(kSceneJardin);
	}
};

} // End of anonymous namespace

SceneScript *createChamp() {
	return new Champ();
}

} // End of namespace Peintre
