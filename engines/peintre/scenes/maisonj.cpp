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

// The Yellow House, scene 7 (maisonj.md).

namespace {

enum { kPot01, kPot02, kNuages, kEscargot, kPendule, kPortail, kPortemj, kPortail01 };

const ObjectDef kObjects[] = {
	{ "pot01", 0xFF, false }, { "pot02", 0xFF, false }, { "nuages", 0xFF, true },
	{ "escargot", 2, false }, { "pendule", 3, false }, { "portail", 0xFF, false },
	{ "portemj", 0xFF, false }, { "portail01", 0xFF, false }
};

enum { kAnimSnail, kAnimClouds, kAnimGate, kAnimDoor, kAnimTrain };

// Q-0236: nothing starts the clouds' track.
const AnimDef kAnims[] = {
	{ "escargot.3da", "escargot", false }, { "nuages.3da", "nuages", false },
	{ "portail.3da", "portail", false }, { "porte-mj.3da", "portemj", false },
	{ "train.3da", "TRAIN02", true }
};

const uint32 kBundleB = 0x4abd14, kMapSunny = 0x4abd94, kSnailTaken = 0x4abd98;
const uint32 kSnailShown = 0x4abd9c, kGateOpen = 0x4abda0, kDoorOpen = 0x4abda4, kPot01Used = 0x4abda8;

class Maisonj : public SceneScript {
public:
	void init(World &w) override {
		setObjects(w, kObjects, ARRAYSIZE(kObjects));
		setAnims(w, kAnims, ARRAYSIZE(kAnims));
		for (int i = 1; i <= 3; i++)
			w.loadBoxSet(i, Common::String::format("BOX%d.3DI", i));
		updateBoxSet(w);
		w.setAmbience("maisjaun");
		_trainSound = false;

		static const char *const kMaps[][2] = {
			{ "WMAP01", "WMJ01" }, { "WMAP02", "WMJ02" }, { "BMAP01", "BMJ01" },
			{ "BMAP02", "BMJ02" }, { "JMAP01", "JMJ01" }, { "JMAP02", "JMJ02" }
		};
		for (const auto &m : kMaps)
			w.loadTexture(m[0], m[1]);
		_world = w.findObject("world");
		w.retexture(_world, "MJ01", "WMAP01");
		w.retexture(_world, "MJ02", "WMAP02");
		w.objects[kPot01].cursorType = 4;
		w.objects[kPot02].cursorType = 4;
		_mapDone = false;
		if (w.var(kMapSunny)) {
			sunnyMap(w);
			w.objects[kPot01].cursorType = 0xFF;
		}
		w.objects[kPot02].cursorType = 0xFF;
		_pot01Done = w.var(kPot01Used) != 0;
		if (_pot01Done)
			w.objects[kPot01].cursorType = 0xFF;

		if (w.var(kSnailTaken))
			w.setHidden(node(w, kEscargot), true);
		if (!w.var(kSnailShown))
			w.setHidden(node(w, kEscargot), true);
		else
			w.anims[kAnimSnail].playing = true;

		const bool gate = w.var(kGateOpen) != 0, door = w.var(kDoorOpen) != 0;
		if (gate)
			poseAt(w, kAnimGate, w.anims[kAnimGate].length - 1);
		w.objects[kPortail].cursorType = w.objects[kPortail01].cursorType = gate ? 0xFF : 4;
		if (door)
			poseAt(w, kAnimDoor, w.anims[kAnimDoor].length - 1);
		w.objects[kPortemj].cursorType = door ? 0xFF : 4;
		if (w.zoneSolved(12))
			w.objects[kPendule].cursorType = 0x3C;
	}

	void frame(World &w) override {
		switch (clickedObject(w, w.objectIndex(w.pick()))) {
		case kPot01:
			if (!_pot01Done) {
				w.var(kSnailShown) = 1;
				w.setHidden(node(w, kEscargot), false);
				w.var(kPot01Used) = 1;
				_pot01Done = true;
				w.anims[kAnimSnail].playing = true;
				w.objects[kPot02].cursorType = 4;
			}
			if (!_mapDone)
				w.requestMovie("pluie");
			break;
		case kPot02:
			if (_pot01Done) {
				sunnyMap(w);
				w.var(kMapSunny) = 1;
				w.var(kBundleB) = 1;
				w.objects[kPot01].cursorType = 0xFF;
				w.objects[kPot02].cursorType = 0xFF;
			}
			break;
		case kEscargot:
			takeItem(w, kEscargot, 11, kSnailTaken);
			break;
		case kPortail:
		case kPortail01:
			if (!w.var(kGateOpen)) {
				w.anims[kAnimGate].playing = true;
				startSound(w, "portail01");
			}
			break;
		case kPortemj:
			if (!w.var(kDoorOpen)) {
				w.anims[kAnimDoor].playing = true;
				startSound(w, "placard");
			}
			break;
		case kPendule:
			w.requestZone(12);
			break;
		default:
			break;
		}

		// Q-0237: nothing here leaves for hopiext (7 -> 2).
		const Camera &c = w.camera();
		if (c.z < -12500)
			w.requestScene(kScenePont);
		else if (c.x < -8500)
			w.requestScene(kSceneTerrasse);
		else if (c.x > -6233 && c.x < -6000 && c.z > 2000)
			w.requestScene(kSceneChambre);
		w.clearClick();

		const int32 e = w.elapsed();
		loopAnim(w, kAnimSnail, e);
		if (stepAnim(w, kAnimClouds, e))
			w.anims[kAnimClouds].playing = false;
		if (stepAnim(w, kAnimGate, e)) {
			w.anims[kAnimGate].playing = false;
			w.var(kGateOpen) = 1;
			w.objects[kPortail].cursorType = w.objects[kPortail01].cursorType = 0xFF;
			updateBoxSet(w);
		}
		if (stepAnim(w, kAnimDoor, e)) {
			w.anims[kAnimDoor].playing = false;
			w.var(kDoorOpen) = 1;
			w.objects[kPortemj].cursorType = 0xFF;
			updateBoxSet(w);
		}
		// The train passes once per visit.
		if (stepAnim(w, kAnimTrain, e)) {
			w.anims[kAnimTrain].playing = false;
			w.anims[kAnimTrain].frame = 1;
		}
		if (w.anims[kAnimTrain].playing && w.anims[kAnimTrain].frame > 70 && !_trainSound) {
			startSound(w, "pastrain");
			_trainSound = true;
		}
	}

private:
	void sunnyMap(World &w) {
		w.retexture(_world, "WMAP01", "JMAP01");
		w.retexture(_world, "WMAP02", "JMAP02");
		_mapDone = true;
	}

	void updateBoxSet(World &w) {
		const bool gate = w.var(kGateOpen) != 0, door = w.var(kDoorOpen) != 0;
		w.setBoxSet(gate ? (door ? 2 : 1) : (door ? 3 : 0));
	}

	int _world = -1;
	bool _mapDone = false;     ///< 0x59905c
	bool _pot01Done = false;   ///< 0x599060
	bool _trainSound = false;  ///< pastrain played this visit
};

} // End of anonymous namespace

SceneScript *createMaisonj() {
	return new Maisonj();
}

} // End of namespace Peintre
