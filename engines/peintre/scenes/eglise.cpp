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

// The church at Auvers, scene 13 (eglise.md).

namespace {

enum { kCerf, kGerbe, kEglise10 };

const ObjectDef kObjects[] = {
	{ "cerf", 2, false }, { "gerbe", 2, false }, { "eglise10", 3, false }
};

const AnimDef kAnims[] = { { "eglise.3DA", "cerf", true } };

const uint32 kKiteInChurch = 0x4abcd8, kKiteTaken = 0x4abcdc, kSheaf = 0x4abce0;

class Eglise : public SceneScript {
public:
	void init(World &w) override {
		setObjects(w, kObjects, ARRAYSIZE(kObjects));
		setAnims(w, kAnims, ARRAYSIZE(kAnims));
		if (w.var(kKiteInChurch) == 1 || w.var(kKiteTaken) == 1)
			w.setHidden(node(w, kCerf), true);
		if (w.var(kSheaf) == 1)
			w.setHidden(node(w, kGerbe), true);
		if (w.zoneSolved(23))
			w.objects[kEglise10].cursorType = 0x3C;
		w.setAmbience("eglise");
	}

	void frame(World &w) override {
		switch (clickedObject(w, w.objectIndex(w.pick()))) {
		case kEglise10:
			w.requestZone(23);
			break;
		case kGerbe:
			takeItem(w, kGerbe, 34, kSheaf);
			break;
		case kCerf:
			takeItem(w, kCerf, 33, kKiteTaken);
			break;
		default:
			break;
		}
		if (w.camera().x > -2000)
			w.requestScene(kSceneChamp);
		else if (w.camera().z > 0x5fb4)
			w.requestScene(kSceneJardin);
		w.clearClick();

		// 0x4243cb: half the elapsed ticks; back to 1 only on reaching the length exactly.
		AnimRecord &a = w.anims[0];
		if (a.playing) {
			a.frame += w.elapsed() >> 1;
			if (a.frame == a.length)
				a.frame = 1;
			w.pose(0, a.frame);
		}
	}
};

} // End of anonymous namespace

SceneScript *createEglise() {
	return new Eglise();
}

} // End of namespace Peintre
