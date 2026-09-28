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

#include "peintre/peintre.h"
#include "peintre/sound.h"
#include "peintre/scenes/scenes.h"

namespace Peintre {

// The cafe terrace, scene 10 (terrasse.md).

namespace {

enum { kLettre, kDe, kManivelle, kLunette, kKasket, kDrapo, kPorte01, kPorte02, kPlatode };

const ObjectDef kObjects[] = {
	{ "lettre", 2, false }, { "de", 0xFF, false }, { "MANIVELLE", 0xFF, false },
	{ "lunette", 2, false }, { "KASKET", 3, false }, { "drapo", 3, false },
	{ "porte01", 6, false }, { "porte02", 6, false }, { "platode", 2, false }
};

const AnimDef kAnims[] = { { "terrasse.3da", "storeho", false } };

const uint32 kDie = 0x4abd68, kSpectacles = 0x4abd6c, kLetter = 0x4abd70, kAwning = 0x4abd74, kVisited = 0x4abd78;

class Terrasse : public SceneScript {
public:
	void init(World &w) override {
		setObjects(w, kObjects, ARRAYSIZE(kObjects));
		setAnims(w, kAnims, ARRAYSIZE(kAnims));
		if (!w.var(kVisited)) {
			w.vm()->sound()->playStream("apierre");
			w.var(kVisited) = 1;
		}
		w.setAmbience("cafenuit");
		if (w.var(kDie)) {
			w.setHidden(node(w, kDe), true);
			w.objects[kPlatode].cursorType = 0xFF;
		}
		if (w.var(kLetter))
			w.setHidden(node(w, kLettre), true);
		if (w.var(kSpectacles))
			w.setHidden(node(w, kLunette), true);
		if (w.var(kAwning)) {
			w.anims[0].playing = false;
			poseAt(w, 0, w.anims[0].length - 1);
			w.objects[kManivelle].cursorType = 0xFF;
		} else {
			poseAt(w, 0, 1);
			w.objects[kManivelle].cursorType = 4;
		}
		if (w.zoneSolved(16))
			w.objects[kDrapo].cursorType = 0x3C;
		if (w.zoneSolved(17))
			w.objects[kKasket].cursorType = 0x3C;
	}

	void frame(World &w) override {
		switch (clickedObject(w, w.objectIndex(w.pick()))) {
		case kManivelle:
			if (!w.var(kAwning)) {
				w.anims[0].playing = true;
				startSound(w, "store");
			}
			break;
		case kLettre:
			takeItem(w, kLettre, 27, kLetter);
			break;
		case kKasket:
			w.requestZone(17);
			break;
		case kDrapo:
			if (w.var(kAwning))
				w.requestZone(16);
			break;
		case kLunette:
			takeItem(w, kLunette, 17, kSpectacles);
			break;
		case kPlatode:
			if (w.objects[kPlatode].cursorType == 2) {
				w.takeItem(node(w, kDe), 26);
				w.var(kDie) = 1;
				w.objects[kPlatode].cursorType = 0xFF;
			}
			break;
		case kPorte01:
		case kPorte02:
			w.requestScene(kSceneCafe);
			break;
		default:
			break;
		}
		if (w.camera().z < -2500)
			w.requestScene(kSceneMaisonj);
		w.clearClick();

		// The playing word is not cleared at the end: the end branch repeats.
		if (stepAnim(w, 0, w.elapsed())) {
			w.var(kAwning) = 1;
			w.objects[kManivelle].cursorType = 0xFF;
		}
	}
};

} // End of anonymous namespace

SceneScript *createTerrasse() {
	return new Terrasse();
}

} // End of namespace Peintre
