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

// The bedroom seen from the Yellow House: scene 6 with `chambrev` (chambrev.md).

namespace {

enum { kPorte, kPorte02 };

const ObjectDef kObjects[] = { { "porte", 0xFF, false }, { "porte02", 6, false } };
const AnimDef kAnims[] = { { "portev.3da", "porte", false } };

const uint32 kDoorOpen = 0x4abd3c;

class Chambrev : public SceneScript {
public:
	void init(World &w) override {
		setObjects(w, kObjects, ARRAYSIZE(kObjects));
		setAnims(w, kAnims, ARRAYSIZE(kAnims));
		w.setAmbience("chambrVG");
		const bool open = w.var(kDoorOpen) == 1;
		if (open)
			poseAt(w, 0, w.anims[0].length - 1);
		w.objects[kPorte].cursorType = open ? 0xFF : 4;
	}

	void frame(World &w) override {
		switch (clickedObject(w, w.objectIndex(w.pick()))) {
		case kPorte:
			if (!w.var(kDoorOpen)) {
				w.anims[0].playing = true;
				startSound(w, "placard");
			}
			break;
		case kPorte02:
			w.requestScene(kSceneMaisonj);
			break;
		default:
			break;
		}
		w.clearClick();
		if (stepAnim(w, 0, 1)) {
			w.anims[0].playing = false;
			w.var(kDoorOpen) = 1;
			w.objects[kPorte].cursorType = 0xFF;
		}
	}
};

} // End of anonymous namespace

SceneScript *createChambrev() {
	return new Chambrev();
}

} // End of namespace Peintre
