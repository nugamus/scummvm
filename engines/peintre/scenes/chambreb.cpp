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

// Van Gogh's bedroom, the painting's version: scene 6 with `chambreb` (chambreb.md).

namespace {

enum {
	kObject09, kPorte, kChaise1, kCadre, kObject02, kTiroir, kCamee, kShoes, kObject07,
	kPapier, kMirroircas, kOmbrelle, kOreiller, kTab01, kTab02, kBougie, kObject01,
	kMirroir1, kRasoir, kMirroirmor, kPorte02, kPapierferm, kObject10, kObject01b,
	kShoes01, kTabpay, kObject06
};

const ObjectDef kObjects[] = {
	{ "Object09", 2, false }, { "porte", 0xFF, false }, { "CHAISE1", 0xFF, false },
	{ "cadre", 2, false }, { "Object02", 2, false }, { "TIROIR", 0xFF, false },
	{ "camee", 0xFF, false }, { "shoes", 0xFF, false }, { "Object07", 2, false },
	{ "papier", 0xFF, false }, { "mirroircas", 0xFF, true }, { "ombrelle", 0xFF, false },
	{ "oreiller", 3, false }, { "tab01", 3, false }, { "tab02", 3, false },
	{ "bougie", 0xFF, false }, { "Object01", 0xFF, false }, { "mirroir1", 0xFF, false },
	{ "rasoir", 0xFF, false }, { "mirroirmor", 2, true }, { "porte02", 6, false },
	{ "papierferm", 0xFF, false }, { "Object10", 0xFF, false }, { "Object01", 0xFF, false },
	{ "shoes01", 0xFF, false }, { "tabpay", 3, false }, { "Object06", 0xFF, false }
};

enum { kAnimChair, kAnimShoes, kAnimMirror, kAnimPaper, kAnimCupboard, kAnimDrawer };

// Q-0220: nothing in the scene code starts the mirror track.
const AnimDef kAnims[] = {
	{ "chaise.3da", "CHAISE1", false }, { "chaussur.3da", "perpompe", true },
	{ "mirroir.3da", "mirroircas", false }, { "papier.3da", "papier", false },
	{ "porte.3da", "porte", false }, { "tiroir.3da", "TIROIR", false }
};

const uint32 kBundleB = 0x4abd14;
const uint32 kItem15 = 0x4abd18, kItem18 = 0x4abd1c, kItem10 = 0x4abd20, kItem12 = 0x4abd24;
const uint32 kItem8 = 0x4abd28, kItem13 = 0x4abd2c, kItem24 = 0x4abd30, kShoesTaken = 0x4abd34;
const uint32 kCupboardOpen = 0x4abd38, kDrawerOpen = 0x4abd40, kChairMoved = 0x4abd44;
const uint32 kPaperOpen = 0x4abd48, kMirrorBroken = 0x4abd4c, kShoesFree = 0x4abd50;
const uint32 kMirrorSound = 0x4abd5c, kUnk60 = 0x4abd60, kUnk70 = 0x4abd70;

class Chambreb : public SceneScript {
public:
	void init(World &w) override {
		setObjects(w, kObjects, ARRAYSIZE(kObjects));
		setAnims(w, kAnims, ARRAYSIZE(kAnims));
		w.setAmbience("chambrVG");
		_drawer = _toggle = _paper = _chair = _cupboard = false;
		_perpompe = w.findObject("perpompe");

		if (!w.var(kShoesFree)) {
			w.anims[kAnimShoes].playing = false;
			for (int i : { kShoes, kObject01, kShoes01, kObject01b, kObject06 })
				w.objects[i].cursorType = 0xFF;
		} else if (!w.var(kShoesTaken)) {
			w.anims[kAnimShoes].playing = false;
			for (int i : { kShoes, kObject01, kShoes01, kObject06 })
				w.objects[i].cursorType = 2;
		} else {
			hideShoes(w);
		}
		// Q-0220: the role of 0x4abd60.
		if (!w.var(kUnk60) && !w.var(kUnk70) && w.var(kBundleB))
			w.var(kUnk60) = 1;

		static const struct { int object; uint32 flag; } kTaken[] = {
			{ kObject09, kItem15 }, { kObject07, kItem18 }, { kCadre, kItem10 },
			{ kObject02, kItem12 }, { kOmbrelle, kItem8 }, { kCamee, kItem13 }
		};
		for (const auto &t : kTaken)
			if (w.var(t.flag))
				w.setHidden(node(w, t.object), true);

		if (w.var(kCupboardOpen)) {
			poseAt(w, kAnimCupboard, w.anims[kAnimCupboard].length);
			_cupboard = true;
			w.objects[kPorte].cursorType = 0xFF;
			w.objects[kCamee].cursorType = 2;
			w.objects[kOmbrelle].cursorType = 2;
		} else {
			w.objects[kCamee].cursorType = 0xFF;
			w.objects[kOmbrelle].cursorType = 0xFF;
			w.objects[kPorte].cursorType = 4;
		}
		if (w.var(kItem15))
			w.objects[kTiroir].cursorType = 4;
		if (w.var(kMirrorBroken)) {
			w.setHidden(node(w, kMirroir1), true);
			w.setHidden(node(w, kMirroircas), false);
			poseAt(w, kAnimMirror, w.anims[kAnimMirror].length);
			if (!w.var(kItem24))
				w.setHidden(node(w, kMirroirmor), false);
		}
		if (w.var(kDrawerOpen)) {
			poseAt(w, kAnimDrawer, w.anims[kAnimDrawer].length);
			_drawer = true;
			w.objects[kTiroir].cursorType = 3;
		}
		if (w.var(kChairMoved)) {
			poseAt(w, kAnimChair, w.anims[kAnimChair].length);
			_chair = true;
			w.objects[kChaise1].cursorType = 0xFF;
		} else {
			w.objects[kChaise1].cursorType = 4;
			w.objects[kPorte].cursorType = 0xFF;
		}
		if (w.var(kPaperOpen)) {
			poseAt(w, kAnimPaper, w.anims[kAnimPaper].length);
			_paper = true;
			w.objects[kPapierferm].cursorType = 0xFF;
		} else {
			poseAt(w, kAnimPaper, 1);
			w.objects[kPapierferm].cursorType = 4;
		}
		w.objects[kTabpay].cursorType = 0xFF;
		w.objects[kTab01].cursorType = 0xFF;
		if (w.zoneSolved(7)) {
			w.objects[kTabpay].cursorType = 3;
			w.retexture(node(w, kTabpay), "TOILES2", "RASOIR");
			w.objects[kTab01].cursorType = 3;
			w.retexture(node(w, kTab01), "TOILES", "RASOIR");
			w.objects[kOreiller].cursorType = 0x3C;
		}
		// 0x4e30f4: set by the return from zone 11 while 0x4abd5c = 0, never cleared
		// (E-0017); the saved 0x4abd5c prevents a replay.
		if (w.localVar(0x4e30f4) == 1 && !w.var(kMirrorSound)) {
			w.var(kMirrorSound) = 1;
			startSound(w, "miroir");
		}
		if (w.zoneSolved(8))
			w.objects[kTabpay].cursorType = 0x3C;
		if (w.zoneSolved(9))
			w.objects[kTiroir].cursorType = 0x3C;
		if (w.zoneSolved(10))
			w.objects[kTab02].cursorType = 0x3C;
		if (w.zoneSolved(11))
			w.objects[kTab01].cursorType = 0x3C;
	}

	void frame(World &w) override {
		clicks(w, clickedObject(w, w.objectIndex(w.pick())));
		w.clearClick();
		animate(w);
	}

private:
	void hideShoes(World &w) {
		w.setHidden(node(w, kShoes), true);
		w.setHidden(node(w, kShoes01), true);
		w.setHidden(_perpompe, true);
	}

	void clicks(World &w, int clicked) {
		const byte type = clicked >= 0 ? w.objects[clicked].cursorType : 0;
		switch (clicked) {
		case kObject09:
			takeItem(w, kObject09, 15, kItem15);
			w.objects[kTiroir].cursorType = 4;
			break;
		case kCadre:
			takeItem(w, kCadre, 10, kItem10);
			break;
		case kObject02:
			takeItem(w, kObject02, 12, kItem12);
			break;
		case kTiroir:
			if (!_drawer && w.var(kItem15)) {
				w.anims[kAnimDrawer].playing = true;
				startSound(w, "tiroir");
			} else if (_drawer) {
				w.requestZone(9);
			}
			break;
		case kPapierferm:
			if (!_paper) {
				w.anims[kAnimPaper].playing = true;
				w.vm()->sound()->playStream("sm03_012");
			}
			break;
		case kCamee:
			if (_cupboard)
				takeItem(w, kCamee, 13, kItem13);
			break;
		case kObject07:
			takeItem(w, kObject07, 18, kItem18);
			break;
		case kOmbrelle:
			if (_cupboard)
				takeItem(w, kOmbrelle, 8, kItem8);
			break;
		case kMirroirmor:
			takeItem(w, kMirroirmor, 24, kItem24);
			break;
		case kPorte:
			if (_chair && !_cupboard) {
				w.anims[kAnimCupboard].playing = true;
				startSound(w, "placard");
			}
			break;
		case kChaise1:
			if (!_chair) {
				w.anims[kAnimChair].playing = true;
				startSound(w, "chaiseVG");
			}
			break;
		case kOreiller:
			w.requestZone(7);
			break;
		case kTab02:
			w.requestZone(10);
			break;
		case kTabpay:
			if (type == 3 || type == 0x3C)
				w.requestZone(8);
			break;
		case kTab01:
			if (type == 3 || type == 0x3C)
				w.requestZone(11);
			break;
		case kPorte02:
			w.requestScene(kSceneMaisonj);
			break;
		case kObject06:
		case kObject10:
		case kShoes:
		case kShoes01:
			if (w.var(kShoesFree)) {
				hideShoes(w);
				w.takeItem(node(w, kShoes), 14);
				w.var(kShoesTaken) = 1;
			}
			break;
		default:
			break;
		}
	}

	void animate(World &w) {
		if (stepAnim(w, kAnimChair, 1)) {
			w.objects[kPorte].cursorType = 4;
			w.objects[kChaise1].cursorType = 0xFF;
			w.var(kChairMoved) = 1;
			_chair = true;
			w.anims[kAnimChair].playing = false;
		}
		_toggle = !_toggle;
		if (_toggle && stepAnim(w, kAnimShoes, 1))
			w.anims[kAnimShoes].frame = 1;
		if (stepAnim(w, kAnimMirror, 1)) {
			w.setHidden(node(w, kMirroirmor), false);
			w.anims[kAnimMirror].playing = false;
		}
		if (stepAnim(w, kAnimPaper, 1)) {
			w.objects[kPapierferm].cursorType = 0xFF;
			w.var(kPaperOpen) = 1;
			_paper = true;
			w.anims[kAnimPaper].playing = false;
		}
		if (stepAnim(w, kAnimCupboard, 1)) {
			w.objects[kPorte].cursorType = 0xFF;
			w.objects[kCamee].cursorType = 2;
			w.objects[kOmbrelle].cursorType = 2;
			w.var(kCupboardOpen) = 1;
			_cupboard = true;
			w.anims[kAnimCupboard].playing = false;
		}
		if (stepAnim(w, kAnimDrawer, 1)) {
			w.objects[kTiroir].cursorType = 3;
			w.var(kDrawerOpen) = 1;
			_drawer = true;
			w.anims[kAnimDrawer].playing = false;
		}
	}

	int _perpompe = -1;
	bool _drawer = false;    ///< 0x650fb0
	bool _toggle = false;    ///< 0x650fb4
	bool _paper = false;     ///< 0x650fb8
	bool _chair = false;     ///< 0x650fbc
	bool _cupboard = false;  ///< 0x650fc0
};

} // End of anonymous namespace

SceneScript *createChambreb() {
	return new Chambreb();
}

} // End of namespace Peintre
