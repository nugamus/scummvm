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

// The Auberge Ravoux, scene 1 (auberge.md).

namespace {

enum {
	kHors, kSaccoche, kBlanche, kStetoscop, kPortebas, kPortehaut, kCasquette, kTableau,
	kPorte01, kPorte02, kSac, kSac01
};

const ObjectDef kObjects[] = {
	{ "hors", 0xFF, false }, { "saccoche", 4, false }, { "blanche", 0xFF, false },
	{ "stetoscop", 2, false }, { "portebas", 6, false }, { "portehaut", 6, false },
	{ "casquette", 3, false }, { "tableau", 0xFF, false }, { "porte01", 6, false },
	{ "porte02", 6, false }, { "sac", 4, false }, { "sac01", 4, false }
};

enum { kAnimBag, kAnimUpper, kAnimLower };

const AnimDef kAnims[] = {
	{ "saccoch.3da", "saccoche", false }, { "portehau.3da", "portehaut", false },
	{ "portebas.3da", "portebas", false }
};

const uint32 kStethoscope = 0x4abce4, kBagOpen = 0x4abce8, kLowerOpen = 0x4abcec, kUpperOpen = 0x4abcf0;

class Auberge : public SceneScript {
public:
	void init(World &w) override {
		setObjects(w, kObjects, ARRAYSIZE(kObjects));
		setAnims(w, kAnims, ARRAYSIZE(kAnims));
		w.loadBoxSet(1, "BOXBAS.3DI");
		w.loadBoxSet(2, "BOXHAUT.3DI");
		w.setAmbience("auberge");
		_bag = w.var(kBagOpen) != 0;
		if (w.var(kBagOpen)) {
			poseAt(w, kAnimBag, w.anims[kAnimBag].length / 2);
			setBag(w, 0xFF);
		}
		if (w.var(kStethoscope))
			w.setHidden(node(w, kStetoscop), true);
		if (w.var(kLowerOpen)) {
			poseAt(w, kAnimLower, w.anims[kAnimLower].length / 2);
			w.setBoxSet(1);
			w.objects[kPortebas].cursorType = 0xFF;
		}
		if (w.var(kUpperOpen)) {
			poseAt(w, kAnimUpper, w.anims[kAnimUpper].length / 2);
			w.setBoxSet(2);
			w.objects[kPortehaut].cursorType = 0xFF;
		}
		if (w.sunflowers() == 5)
			w.objects[kTableau].cursorType = 3;
		if (w.zoneSolved(20))
			w.objects[kCasquette].cursorType = 0x3C;
	}

	void frame(World &w) override {
		switch (clickedObject(w, w.objectIndex(w.pick()))) {
		case kPorte01:
		case kPorte02:
			w.requestScene(kSceneJardin);
			break;
		case kSaccoche:
		case kSac:
		case kSac01:
			if (!_bag) {
				w.anims[kAnimBag].playing = true;
				setBag(w, 0xFF);
				_bag = true;
			}
			break;
		case kStetoscop:
			takeItem(w, kStetoscop, 31, kStethoscope);
			break;
		case kCasquette:
			w.requestZone(20);
			break;
		case kTableau:
			if (w.objects[kTableau].cursorType == 3)
				w.requestZone(21);
			break;
		case kPortebas:
			if (w.objects[kPortebas].cursorType == 6) {
				w.anims[kAnimLower].playing = true;
				w.objects[kPortebas].cursorType = 0xFF;
			}
			break;
		case kPortehaut:
			if (w.objects[kPortehaut].cursorType == 6) {
				w.anims[kAnimUpper].playing = true;
				w.objects[kPortebas].cursorType = 0xFF; // sic: portebas's, not its own
			}
			break;
		default:
			break;
		}
		if (w.sunflowers() == 34)
			w.objects[kTableau].cursorType = 3;
		w.clearClick();

		const int32 e = w.elapsed();
		if (stepAnim(w, kAnimBag, e, w.anims[kAnimBag].length / 2)) {
			w.anims[kAnimBag].playing = false;
			w.objects[kSaccoche].cursorType = 0xFF;
			w.var(kBagOpen) = 1;
			_bag = true;
		}
		if (stepAnim(w, kAnimUpper, e, w.anims[kAnimUpper].length / 2)) {
			w.anims[kAnimUpper].playing = false;
			w.var(kUpperOpen) = 1;
			w.setBoxSet(2);
			w.objects[kPortehaut].cursorType = 0xFF;
		}
		if (stepAnim(w, kAnimLower, e, w.anims[kAnimLower].length / 2)) {
			w.anims[kAnimLower].playing = false;
			w.var(kLowerOpen) = 1;
			w.setBoxSet(1);
			w.objects[kPortebas].cursorType = 0xFF;
		}
	}

private:
	void setBag(World &w, byte type) {
		w.objects[kSaccoche].cursorType = type;
		w.objects[kSac].cursorType = type;
		w.objects[kSac01].cursorType = type;
	}

	bool _bag = false; ///< 0x4e2aec
};

} // End of anonymous namespace

SceneScript *createAuberge() {
	return new Auberge();
}

} // End of namespace Peintre
