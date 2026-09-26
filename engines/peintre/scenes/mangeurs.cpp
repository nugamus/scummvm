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

// The potato eaters' room, scene 4 (mangeurs.md).

namespace {

enum {
	kBuche, kChaise1, kFagot, kFenetre, kPortepoel, kBersso, kPatat01, kPendul, kPot,
	kVapeur, kTheieres, kPorte
};

const ObjectDef kObjects[] = {
	{ "buche", 0xFF, false }, { "chaise1", 0xFF, false }, { "fagot", 0xFF, false },
	{ "fenetre", 0xFF, false }, { "portepoel", 4, false }, { "bersso", 0xFF, false },
	{ "patat01", 2, false }, { "pendul", 0xFF, false }, { "POT", 3, false },
	{ "vapeur", 0xFF, true }, { "theieres", 0xFF, false }, { "porte", 6, false }
};

enum { kAnimLog, kAnimChair, kAnimFaggot, kAnimWindow, kAnimStove };

// Q-0236: nothing starts the log, chair and faggot tracks.
const AnimDef kAnims[] = {
	{ "buche.3da", "buche", false }, { "chaise.3da", "chaise1", false },
	{ "fagot.3da", "fagot", false }, { "fenetre.3da", "fenetre", false },
	{ "portpoel.3da", "portepoel", false }
};

const uint32 kPotato = 0x4abbfc, kStoveOpen = 0x4abc00, kCuckoo = 0x4abc08, kLit = 0x4abc18;
const uint32 kKettle = 0x4abc1c, kLogBurns = 0x4abc2c, kFaggotBurnt = 0x4abc3c, kWindowShut = 0x4abc58;

class Mangeurs : public SceneScript {
public:
	void init(World &w) override {
		setObjects(w, kObjects, ARRAYSIZE(kObjects));
		setAnims(w, kAnims, ARRAYSIZE(kAnims));
		_fire = w.findObject("fire");
		_windowTicks = _babyTicks = 0;
		_crying = _shut = false;
		_cuckooDone = w.var(kCuckoo) == 0;
		if (w.var(kStoveOpen)) {
			poseAt(w, kAnimStove, w.anims[kAnimStove].length);
			w.objects[kPortepoel].cursorType = 0xFF;
			w.objects[kFagot].cursorType = 4;
		}
		if (!w.var(kWindowShut)) {
			poseAt(w, kAnimWindow, w.anims[kAnimWindow].length / 2);
			startSound(w, "fenclaq", true);
		} else {
			poseAt(w, kAnimWindow, w.anims[kAnimWindow].length);
			_shut = _crying = true;
		}
		if (w.var(kKettle))
			w.setHidden(node(w, kTheieres), true);
		if (w.var(kFaggotBurnt)) {
			w.setHidden(node(w, kFagot), true);
			w.objects[kBuche].cursorType = 4;
		}
		if (w.var(kLogBurns)) {
			w.setHidden(node(w, kBuche), true);
			startSound(w, "feu", true);
			if (!w.var(kKettle))
				startSound(w, "bouilloi", true);
		}
		if (w.var(kPotato))
			w.setHidden(node(w, kPatat01), true);
		if (_crying)
			w.objects[kBersso].cursorType = 3;
		w.setAmbience("mangeurs");
		_kettleTicks = 0;
		if (w.zoneSolved(2))
			w.objects[kBersso].cursorType = 0x3C;
		if (w.zoneSolved(3))
			w.objects[kPot].cursorType = 0x3C;
	}

	void frame(World &w) override {
		scrollUVs(w, _fire, 0x550000, 0, 0xFFFFFF);
		scrollUVs(w, node(w, kVapeur), 0x400000, 0, 0xFFFFFF);

		const int h = w.pick();
		if (w.carrying()) {
			// Q-0405: the click is a level, a long one drops the object at once.
			// Cursor 40 while hovering is replaced before it is drawn (interaction.md).
			if (w.click()) {
				debugC(1, kDebugScript, "Drop %s on %s", w.scene3D().nodes[w.carriedNode()].name.c_str(),
					   h >= 0 ? w.scene3D().nodes[h].name.c_str() : "nothing");
				const Common::String &carried = w.scene3D().nodes[w.carriedNode()].name;
				if (h >= 0 && h == _fire && carried == "fagot") {
					startSound(w, "finfeu");
					w.var(kFaggotBurnt) = 1;
					scrollUVs(w, _fire, 0, 0x550000);
					w.objects[kBuche].cursorType = 4;
					w.vm()->writeResume(0);
				} else if (h >= 0 && h == _fire && carried == "buche") {
					startSound(w, "feu", true);
					w.var(kLogBurns) = 1;
					w.var(kLit) = 1;
					_kettleTicks = 0;
					w.vm()->writeResume(0);
				} else {
					w.setHidden(w.carriedNode(), false);
				}
				w.dropCarried();
			}
		} else {
			switch (clickedObject(w, w.objectIndex(h))) {
			case kFagot:
				if (w.objects[kFagot].cursorType != 0xFF)
					w.carry(node(w, kFagot));
				break;
			case kBuche:
				if (w.var(kFaggotBurnt))
					w.carry(node(w, kBuche));
				break;
			case kBersso:
				if (w.objects[kBersso].cursorType != 0xFF)
					w.requestZone(2);
				break;
			case kTheieres:
				if (w.var(kLit) && w.objects[kTheieres].cursorType != 0xFF) {
					takeItem(w, kTheieres, 3, kKettle);
					w.stopSound("bouilloi");
				}
				break;
			case kPatat01:
				if (w.objects[kPatat01].cursorType != 0xFF)
					takeItem(w, kPatat01, 4, kPotato);
				break;
			case kPot:
				if (w.objects[kPot].cursorType != 0xFF)
					w.requestZone(3);
				break;
			case kFenetre:
				if (w.objects[kFenetre].cursorType != 0xFF && !_shut)
					shutWindow(w);
				break;
			case kPorte:
				w.requestScene(kSceneMaisonet);
				break;
			case kPortepoel:
				w.anims[kAnimStove].playing = true;
				w.var(kStoveOpen) = 1;
				startSound(w, "porpoel");
				break;
			default:
				break;
			}
		}

		const int32 e = w.elapsed();
		_babyTicks += e;
		_kettleTicks += e;
		if (!_cuckooDone) {
			startSound(w, "coucou");
			w.var(kCuckoo) = 0;
			_babyTicks = 0;
			_cuckooDone = true;
		}
		if (_babyTicks > 50 && !_crying && !w.var(kKettle)) {
			startSound(w, "bebe", true);
			w.objects[kFenetre].cursorType = 4;
			_crying = true;
		}
		if (_crying && !_shut) {
			_windowTicks += e;
			if (_windowTicks > 500)
				shutWindow(w);
		}
		if (_kettleTicks > 150 && w.var(kLit) && !w.var(kKettle)) {
			w.setHidden(node(w, kVapeur), false);
			startSound(w, "bouilloi", true);
			w.objects[kTheieres].cursorType = 2;
		}
		w.clearClick();

		if (stepAnim(w, kAnimLog, e)) {
			w.setHidden(node(w, kBuche), true);
			w.anims[kAnimLog].playing = false;
			scrollUVs(w, _fire, 0, 0x550000);
		}
		if (stepAnim(w, kAnimChair, e / 2))
			w.anims[kAnimChair].frame = 1;
		if (stepAnim(w, kAnimFaggot, e)) {
			w.setHidden(node(w, kFagot), true);
			w.anims[kAnimFaggot].playing = false;
			scrollUVs(w, _fire, 0, -0x550000);
		}
		if (stepAnim(w, kAnimWindow, e))
			w.anims[kAnimWindow].playing = false;
		if (stepAnim(w, kAnimStove, 1)) {
			w.objects[kFagot].cursorType = 4;
			w.objects[kPortepoel].cursorType = 0xFF;
			w.anims[kAnimStove].playing = false;
		}
	}

private:
	void shutWindow(World &w) {
		_shut = true;
		startSound(w, "chaise");
		w.stopSound("bebe");
		w.stopSound("fenclaq");
		w.anims[kAnimWindow].playing = true;
		w.var(kWindowShut) = 1;
		w.objects[kBersso].cursorType = 3;
		w.objects[kFenetre].cursorType = 0xFF;
	}

	int _fire = -1;
	uint32 _windowTicks = 0;  ///< 0x599034
	uint32 _babyTicks = 0;    ///< 0x599024
	uint32 _kettleTicks = 0;  ///< 0x599040
	bool _crying = false;     ///< 0x599038
	bool _shut = false;       ///< 0x59903c
	bool _cuckooDone = true;  ///< 0x599044
};

} // End of anonymous namespace

SceneScript *createMangeurs() {
	return new Mangeurs();
}

} // End of namespace Peintre
