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

// The night cafe, scene 5 (cafe.md).

namespace {

enum {
	kKe, kGant, kBriket, kClef, kCarte, kPomme, kBarporte, kOmbre, kOrloge, kLampe4,
	kTheiere, kPichet, kMirroir, kPorte01, kPorte02
};

const ObjectDef kObjects[] = {
	{ "ke", 4, false }, { "gant", 3, false }, { "briket", 2, false }, { "clef", 4, false },
	{ "carte", 2, false }, { "pomme", 2, false }, { "barporte", 0xFF, false },
	{ "ombre", 0xFF, false }, { "orloge", 2, false }, { "lampe4", 3, false },
	{ "theiere", 2, false }, { "pichet", 2, false }, { "mirroir", 0xFF, false },
	{ "porte01", 6, false }, { "porte02", 6, false }
};

enum { kAnimCue, kAnimBarDoor };

const AnimDef kAnims[] = {
	{ "billard.3da", "ke", false }, { "portebar.3da", "barporte", false }
};

const uint32 kShadow = 0x4abca0, kCard = 0x4abca4, kLighter = 0x4abca8, kJug = 0x4abcac;
const uint32 kBarOpen = 0x4abcb0, kTeapot = 0x4abcb4, kClock = 0x4abcb8;

// The mirror's texture by the camera's z (cafe.md "The mirror").
struct MirrorBand {
	int32 minZ;
	const char *texture;
	bool zone;
};

const MirrorBand kMirror[] = {
	{ 0xaf1, "MIRROIR3", false }, { 0x8fd, "MIRROIR2", false }, { 0x7d1, "MIRROIR1", false },
	{ 0x321, "MIRROIRG", true }, { 0x1f5, "MIRROIR1", false }, { 0x72, "MIRROIR2", false },
	{ INT32_MIN, "MIRROIR3", false }
};

class Cafe : public SceneScript {
public:
	void init(World &w) override {
		setObjects(w, kObjects, ARRAYSIZE(kObjects));
		setAnims(w, kAnims, ARRAYSIZE(kAnims));
		for (const char *t : { "MIRROIR1", "MIRROIR2", "MIRROIR3" })
			w.loadTexture(t, t);
		// The current mirror texture's name (0x650fe0), set by the init (E-0374).
		_mirror = "MIRROIRG"; // the .3DC's own name for it (mirroirg)
		w.setAmbience("cafe_int");

		if (w.var(kClock)) {
			w.setHidden(node(w, kOrloge), true);
		} else {
			startSound(w, "tictac", true);
			w.objects[kOrloge].cursorType = 0xFF;
		}
		if (w.var(kLighter))
			w.setHidden(node(w, kBriket), true);
		if (w.var(kCard))
			w.setHidden(node(w, kCarte), true);
		if (w.var(kJug))
			w.setHidden(node(w, kPichet), true);
		if (w.var(kTeapot))
			w.setHidden(node(w, kTheiere), true);
		if (w.var(kBarOpen)) {
			w.setHidden(node(w, kClef), true);
			poseAt(w, kAnimBarDoor, w.anims[kAnimBarDoor].length - 1);
		}
		if (w.var(kShadow)) {
			w.setHidden(node(w, kKe), false);
			w.setHidden(node(w, kOmbre), true);
			w.objects[kKe].cursorType = 0xFF;
			poseAt(w, kAnimCue, w.anims[kAnimCue].length - 1);
		} else {
			w.objects[kKe].cursorType = 4;
			w.objects[kOmbre].cursorType = 0xFF;
		}
		_clockRuns = true;
		_clockMark = _clockTicks = 0;
		_clockState = 0;
		_cuePlayed = false;
		if (w.zoneSolved(4))
			w.objects[kLampe4].cursorType = 0x3C;
		if (w.zoneSolved(5))
			w.objects[kGant].cursorType = 0x3C;
		if (w.zoneSolved(6))
			w.objects[kMirroir].cursorType = 0x3C;
	}

	void frame(World &w) override {
		const int h = w.pick();
		if (w.carrying()) {
			// Q-0405: the click is a level, a long one drops the object at once.
			// Only a click on a node drops it (the frame's carry branch needs a pick).
			if (h >= 0 && w.click()) {
				debugC(1, kDebugScript, "Drop %s on %s", w.scene3D().nodes[w.carriedNode()].name.c_str(),
					   h >= 0 ? w.scene3D().nodes[h].name.c_str() : "nothing");
				if (h >= 0 && h == node(w, kBarporte) && w.carriedNode() == node(w, kClef)) {
					startSound(w, "serrure");
					w.anims[kAnimBarDoor].playing = true;
					w.var(kBarOpen) = 1;
					w.autosave();
				} else {
					w.setHidden(w.carriedNode(), false);
				}
				w.dropCarried();
			}
		} else {
			clicks(w, clickedObject(w, w.objectIndex(h)));
		}
		w.clearClick();

		if (stepAnim(w, kAnimCue, w.elapsed())) {
			w.objects[kKe].cursorType = 0xFF;
			w.objects[kOmbre].cursorType = 2;
			_cuePlayed = true;
			w.anims[kAnimCue].playing = false;
		}
		if (stepAnim(w, kAnimBarDoor, 1))
			w.anims[kAnimBarDoor].playing = false;

		mirror(w);
		runClock(w);
	}

private:
	void clicks(World &w, int clicked) {
		switch (clicked) {
		case kGant:
			w.requestZone(5);
			break;
		case kMirroir:
			if (w.objects[kMirroir].cursorType == 3 || w.objects[kMirroir].cursorType == 0x3C)
				w.requestZone(6);
			break;
		case kLampe4:
			w.requestZone(4);
			break;
		case kOrloge:
			if (_clockState == 3) {
				w.stopSound("pendule");
				w.stopSound("tictac");
				takeItem(w, kOrloge, 19, kClock);
			}
			break;
		case kBriket:
			takeItem(w, kBriket, 6, kLighter);
			break;
		case kTheiere:
			takeItem(w, kTheiere, 25, kTeapot);
			break;
		case kCarte:
			takeItem(w, kCarte, 5, kCard);
			break;
		case kPichet:
			takeItem(w, kPichet, 9, kJug);
			break;
		case kKe:
			if (!_cuePlayed && !w.anims[kAnimCue].playing && !w.isHidden(node(w, kKe)) && !w.var(kShadow)) {
				startSound(w, "billiard");
				w.anims[kAnimCue].playing = true;
			}
			break;
		case kOmbre:
			if (_cuePlayed && !w.anims[kAnimCue].playing)
				takeItem(w, kOmbre, 16, kShadow);
			break;
		case kClef:
			w.carry(node(w, kClef));
			break;
		case kPorte01:
		case kPorte02:
			w.requestScene(kSceneTerrasse);
			break;
		default:
			break;
		}
	}

	void mirror(World &w) {
		byte &type = w.objects[kMirroir].cursorType;
		type = 0x25;
		const int32 z = w.camera().z;
		for (const MirrorBand &b : kMirror) {
			if (z < b.minZ)
				continue;
			if (!_mirror.equalsIgnoreCase(b.texture)) {
				w.retexture(node(w, kMirroir), _mirror, b.texture);
				_mirror = b.texture;
			}
			if (b.zone)
				type = w.zoneSolved(6) ? 0x3C : 3;
			break;
		}
	}

	void runClock(World &w) {
		_clockTicks += w.elapsed();
		if (_clockTicks - _clockMark <= 199 || !_clockRuns || w.var(kClock))
			return;
		_clockState++;
		_clockMark = _clockTicks;
		// 0x41b613 on every UV of orloge.
		const int o = node(w, kOrloge);
		switch (_clockState) {
		case 1:
			scrollUVs(w, o, 0x80, 0);
			break;
		case 2:
			scrollUVs(w, o, -0x80, 0x80);
			break;
		case 3:
			scrollUVs(w, o, 0x80, 0);
			_clockRuns = false;
			startSound(w, "pendule");
			w.objects[kOrloge].cursorType = 2;
			break;
		default:
			break;
		}
	}

	Common::String _mirror;    ///< 0x650fe0
	bool _clockRuns = true;    ///< 0x4aa4c8
	uint32 _clockMark = 0;     ///< 0x650fd4
	uint32 _clockTicks = 0;    ///< 0x650fd8
	int _clockState = 0;       ///< 0x650fdc
	bool _cuePlayed = false;   ///< 0x650fc4
};

} // End of anonymous namespace

SceneScript *createCafe() {
	return new Cafe();
}

} // End of namespace Peintre
