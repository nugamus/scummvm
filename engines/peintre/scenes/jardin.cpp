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

// The garden at Auvers, scene 11 (jardin.md).

namespace {

enum { kRato, kFuzz, kAuberge, kCerf, kPartoche, kBarriere, kPorte01, kPorte02 };

const ObjectDef kObjects[] = {
	{ "rato", 2, false }, { "fuzz", 2, false }, { "auberge", 0xFF, false },
	{ "cerf", 4, false }, { "partoche", 3, false }, { "barriere", 0xFF, false },
	{ "porte01", 6, false }, { "porte02", 6, false }
};

enum { kAnimGate, kAnimKite, kAnimFly, kAnimFlyOff, kAnimFly3 };

// Q-0236: nothing starts papiyon3.
const AnimDef kAnims[] = {
	{ "barriere.3da", "barriere", false }, { "cerfvol.3da", "cerf", false },
	{ "papiyon1.3da", "fuzz", true }, { "papiyon2.3da", "fuzz", false },
	{ "papiyon3.3da", "fuzz", false }
};

const uint32 kButterfly = 0x4abcbc, kRake = 0x4abcc0, kGateOpen = 0x4abcc4, kKiteFlown = 0x4abccc;
const uint32 kKiteInChurch = 0x4abcd8;

class Jardin : public SceneScript {
public:
	void init(World &w) override {
		setObjects(w, kObjects, ARRAYSIZE(kObjects));
		setAnims(w, kAnims, ARRAYSIZE(kAnims));
		w.loadBoxSet(1, "BOX1.3DI");
		w.setBoxSet(w.var(kGateOpen) == 1 ? 1 : 0);
		w.setAmbience("jardin");
		_aild = w.findObject("aild");
		_ailg = w.findObject("ailg");
		if (w.var(kButterfly))
			w.setHidden(node(w, kFuzz), true);
		if (w.var(kRake))
			w.setHidden(node(w, kRato), true);
		if (w.var(kGateOpen)) {
			poseAt(w, kAnimGate, w.anims[kAnimGate].length);
			w.objects[kBarriere].cursorType = 0xFF;
		} else {
			w.objects[kBarriere].cursorType = 4;
		}
		if (w.var(kKiteFlown) == 1) {
			w.setHidden(node(w, kCerf), true);
		} else {
			poseAt(w, kAnimKite, 1);
			w.anims[kAnimKite].playing = false;
		}
		_flyTicks = 0;
		_flyInterval = 5;
		if (w.zoneSolved(19))
			w.objects[kPartoche].cursorType = 0x3C;
	}

	void frame(World &w) override {
		const int h = w.pick();
		// The wings are not in the table: they hover like entry 0 and click as the butterfly.
		const bool wing = h >= 0 && (h == _aild || h == _ailg);
		int clicked = clickedObject(w, wing ? kRato : w.objectIndex(h));
		if (wing && clicked == kRato)
			clicked = kFuzz;
		switch (clicked) {
		case kRato:
			takeItem(w, kRato, 30, kRake);
			break;
		case kFuzz:
			w.takeItem(node(w, kFuzz), 29);
			w.var(kButterfly) = 1;
			break;
		case kPorte01:
		case kPorte02:
			w.requestScene(kSceneAuberge);
			break;
		case kCerf:
			w.anims[kAnimKite].playing = true;
			startSound(w, "clochjar");
			w.var(kKiteFlown) = 1;
			w.var(kKiteInChurch) = 0;
			break;
		case kPartoche:
			w.requestZone(19);
			break;
		case kBarriere:
			w.anims[kAnimGate].playing = true;
			startSound(w, "barriere");
			break;
		default:
			break;
		}

		const int32 e = w.elapsed();
		_flyTicks += e;
		if (w.anims[kAnimFly].playing && _flyTicks > _flyInterval) {
			w.anims[kAnimFly].playing = false;
			w.anims[kAnimFlyOff].playing = true;
			_flyTicks = 0;
			_flyInterval += 20;
		}
		const Camera &c = w.camera();
		if (c.z < 300 && c.x > 10000)
			w.requestScene(kSceneEglise);
		else if (c.z < -500 && c.x < -9300)
			w.requestScene(kSceneChamp);
		w.clearClick();

		if (stepAnim(w, kAnimGate, e)) {
			w.anims[kAnimGate].playing = false;
			w.var(kGateOpen) = 1;
			w.objects[kBarriere].cursorType = 0xFF;
			w.setBoxSet(1);
		}
		if (stepAnim(w, kAnimKite, e))
			w.anims[kAnimKite].playing = false;
		if (stepAnim(w, kAnimFly, e))
			w.anims[kAnimFly].frame = 1;
		if (stepAnim(w, kAnimFlyOff, e)) {
			w.anims[kAnimFlyOff].playing = false;
			w.anims[kAnimFlyOff].frame = 1; // Q-0402
			w.anims[kAnimFly].playing = true;
			w.anims[kAnimFly].frame = 1;
		}
		if (stepAnim(w, kAnimFly3, e))
			w.anims[kAnimFly3].playing = false;
	}

private:
	int _aild = -1, _ailg = -1;
	uint32 _flyTicks = 0;      ///< 0x599064
	uint32 _flyInterval = 5;   ///< 0x599074
};

} // End of anonymous namespace

SceneScript *createJardin() {
	return new Jardin();
}

} // End of namespace Peintre
