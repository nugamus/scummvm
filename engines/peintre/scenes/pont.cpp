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
#include "peintre/scenes/scenes.h"

namespace Peintre {

// The Langlois bridge, scene 9 (pont.md).

namespace {

enum { kApple01, kChemise, kTrappe, kPouli03, kEncre01, kPouli02, kPoignee04 };

const ObjectDef kObjects[] = {
	{ "apple01", 2, false }, { "chemise", 3, false }, { "trappe", 0xFF, false },
	{ "pouli03", 0xFF, false }, { "encre01", 2, false }, { "pouli02", 0xFF, false },
	{ "poignee04", 4, false }
};

enum { kAnimBridge, kAnimTrapdoor, kAnimTrees, kAnimApples, kAnimTrain };

// The trees' track "plays" but its step returns at once: it is never posed.
const AnimDef kAnims[] = {
	{ "pond.3da", "ciel", false }, { "trappe.3da", "trappe", false },
	{ "arbres.3da", "ciel", true }, { "pommes.3da", "apple01", false },
	{ "train.3da", "train01", true }
};

const uint32 kTrapdoorOpen = 0x4abd7c, kBridgeDown = 0x4abd80, kHandleFitted = 0x4abd84;
const uint32 kApplesFallen = 0x4abd88, kApplesTaken = 0x4abd8c, kInk = 0x4abd90;

class Pont : public SceneScript {
public:
	void init(World &w) override {
		setObjects(w, kObjects, ARRAYSIZE(kObjects));
		_trainPassed = false;
		setAnims(w, kAnims, ARRAYSIZE(kAnims));
		w.loadBoxSet(1, "BOX2.3DI");
		w.setBoxSet(1);
		w.setAmbience("pont");
		_poignee02 = w.findObject("poignee02");
		poseAt(w, kAnimBridge, 1);
		poseAt(w, kAnimApples, 1);

		if (w.var(kInk))
			w.setHidden(node(w, kEncre01), true);
		if (w.var(kApplesFallen))
			poseAt(w, kAnimApples, w.anims[kAnimApples].length - 1);
		if (w.var(kApplesTaken))
			w.setHidden(node(w, kApple01), true);
		if (w.var(kBridgeDown)) {
			poseAt(w, kAnimBridge, w.anims[kAnimBridge].length - 1);
			w.setBoxSet(0);
			setPulleys(w, 0xFF);
			fitHandle(w);
		} else {
			setPulleys(w, 4);
			w.setBoxSet(1);
		}
		if (!w.var(kHandleFitted))
			w.setHidden(_poignee02, true);
		else
			w.setHidden(node(w, kPoignee04), true);
		if (w.var(kTrapdoorOpen)) {
			poseAt(w, kAnimTrapdoor, w.anims[kAnimTrapdoor].length - 1);
			w.objects[kTrappe].cursorType = 0xFF;
		} else {
			w.objects[kTrappe].cursorType = 4;
		}
		if (w.zoneSolved(18))
			w.objects[kChemise].cursorType = 0x3C;
	}

	void frame(World &w) override {
		const int h = w.pick();
		if (w.carrying()) {
			// Q-0405: the click is a level, a long one drops the object at once.
			if (w.click()) {
				debugC(1, kDebugScript, "Drop %s on %s", w.scene3D().nodes[w.carriedNode()].name.c_str(),
					   h >= 0 ? w.scene3D().nodes[h].name.c_str() : "nothing");
				if (h >= 0 && h == node(w, kPouli03)) {
					w.setHidden(_poignee02, false);
					if (!w.var(kBridgeDown)) {
						startSound(w, "pontlev2");
						w.anims[kAnimBridge].playing = true;
						w.autosave();
					}
					w.var(kHandleFitted) = 1;
				} else {
					w.setHidden(node(w, kPoignee04), false);
				}
				w.dropCarried();
			}
		} else {
			switch (clickedObject(w, w.objectIndex(h))) {
			case kTrappe:
				if (!w.var(kTrapdoorOpen)) {
					startSound(w, "trappe");
					w.anims[kAnimTrapdoor].playing = true;
				}
				break;
			case kChemise:
				w.requestZone(18);
				break;
			case kApple01:
				w.var(kApplesFallen) = 1;
				takeItem(w, kApple01, 7, kApplesTaken);
				break;
			case kEncre01:
				takeItem(w, kEncre01, 28, kInk);
				break;
			case kPoignee04:
				w.carry(node(w, kPoignee04));
				break;
			case kPouli02:
				if (!w.var(kBridgeDown)) {
					fitHandle(w);
					startSound(w, "pontlev2");
					w.anims[kAnimBridge].playing = true;
				}
				break;
			default:
				break;
			}
		}

		// The apples fall when the camera is within 2000 (0x4a255c).
		if (w.distance(node(w, kApple01)) < 2000)
			w.anims[kAnimApples].playing = true;
		const Camera &c = w.camera();
		if (c.x > 5500 && c.z > 11000)
			w.requestScene(kSceneHopiext);
		else if (c.x > 12000)
			w.requestScene(kSceneMaisonj);
		w.clearClick();

		const int32 e = w.elapsed();
		if (stepAnim(w, kAnimBridge, e)) {
			w.anims[kAnimBridge].playing = false;
			w.var(kBridgeDown) = 1;
			w.setBoxSet(0);
			setPulleys(w, 0xFF);
		}
		if (stepAnim(w, kAnimTrapdoor, e)) {
			w.anims[kAnimTrapdoor].playing = false;
			w.var(kTrapdoorOpen) = 1;
			w.objects[kTrappe].cursorType = 0xFF;
		}
		if (stepAnim(w, kAnimApples, e)) {
			w.anims[kAnimApples].playing = false;
			w.var(kApplesFallen) = 1;
		}
		// The train passes once per visit.
		if (stepAnim(w, kAnimTrain, e)) {
			w.anims[kAnimTrain].playing = false;
			w.anims[kAnimTrain].frame = 1;
			if (!_trainPassed) {
				startSound(w, "pastrain");
				_trainPassed = true;
			}
		}
	}

private:
	void setPulleys(World &w, byte type) {
		w.objects[kPouli02].cursorType = type;
		w.objects[kPouli03].cursorType = type;
	}

	// 0x42cf99.
	void fitHandle(World &w) {
		w.setHidden(_poignee02, false);
		w.setHidden(node(w, kPoignee04), true);
		setPulleys(w, 0xFF);
		w.var(kHandleFitted) = 1;
	}

	int _poignee02 = -1;
	bool _trainPassed = false;  ///< 0x598ff4
};

} // End of anonymous namespace

SceneScript *createPont() {
	return new Pont();
}

} // End of namespace Peintre
