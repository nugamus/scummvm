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

// The hospital ward, scene 8 (hopiint.md).

namespace {

enum { kArmoire, kLampe, kBonnet, kCroix, kBougie, kMirroirvg, kFou, kPlak, kEchik };

const ObjectDef kObjects[] = {
	{ "ARMOIRE", 0xFF, false }, { "LAMPE", 2, false }, { "BONNET", 2, false },
	{ "CROIX", 2, false }, { "BOUGIE", 2, false }, { "MIRROIRVG", 3, false },
	{ "FOU", 3, false }, { "PLAK", 6, false }, { "ECHIK", 3, false }
};

enum { kAnimWardrobe, kAnimFou };

const AnimDef kAnims[] = {
	{ "hopiint.3da", "ARMOIRE", false }, { "fou.3da", "FOU", false }
};

const uint32 kWardrobeOpen = 0x4abd00, kCross = 0x4abd04, kLamp = 0x4abd08, kCap = 0x4abd0c, kCandle = 0x4abd10;

class Hopiint : public SceneScript {
public:
	void init(World &w) override {
		setObjects(w, kObjects, ARRAYSIZE(kObjects));
		setAnims(w, kAnims, ARRAYSIZE(kAnims));
		w.setAmbience("hopi_int");
		if (w.var(kWardrobeOpen)) {
			poseAt(w, kAnimWardrobe, w.anims[kAnimWardrobe].length - 1);
			w.objects[kArmoire].cursorType = 0xFF;
		} else {
			w.objects[kArmoire].cursorType = 4;
		}
		static const struct { int object; uint32 flag; } kTaken[] = {
			{ kBonnet, kCap }, { kLampe, kLamp }, { kCroix, kCross }, { kBougie, kCandle }
		};
		for (const auto &t : kTaken)
			if (w.var(t.flag))
				w.setHidden(node(w, t.object), true);
		if (w.zoneSolved(13))
			w.objects[kEchik].cursorType = 0x3C;
		if (w.zoneSolved(15))
			w.objects[kMirroirvg].cursorType = 0x3C;
	}

	void frame(World &w) override {
		int idx = w.objectIndex(w.pick());
		// PLAK only within 3000 (0x4a2538).
		if (idx == kPlak && w.distance(node(w, idx)) >= 3000)
			idx = -1;
		switch (clickedObject(w, idx)) {
		case kArmoire:
			if (!w.var(kWardrobeOpen)) {
				startSound(w, "armoire");
				w.anims[kAnimWardrobe].playing = true;
			}
			break;
		case kBougie:
			takeItem(w, kBougie, 22, kCandle);
			break;
		case kLampe:
			takeItem(w, kLampe, 21, kLamp);
			break;
		case kBonnet:
			takeItem(w, kBonnet, 20, kCap);
			break;
		case kCroix:
			takeItem(w, kCroix, 23, kCross);
			break;
		case kMirroirvg:
			w.requestZone(15);
			break;
		case kFou:
		case kEchik:
			w.requestZone(13);
			break;
		case kPlak:
			w.requestScene(kSceneHopiext);
			break;
		default:
			break;
		}
		// The madman moves while the camera is within 1500 of ECHIK (0x4a253c).
		w.anims[kAnimFou].playing = w.distance(node(w, kEchik)) < 1500;
		w.clearClick();

		if (stepAnim(w, kAnimWardrobe, w.elapsed())) {
			w.anims[kAnimWardrobe].playing = false;
			w.var(kWardrobeOpen) = 1;
			w.objects[kArmoire].cursorType = 0xFF;
		}
		loopAnim(w, kAnimFou, w.elapsed());
	}
};

} // End of anonymous namespace

SceneScript *createHopiint() {
	return new Hopiint();
}

} // End of namespace Peintre
