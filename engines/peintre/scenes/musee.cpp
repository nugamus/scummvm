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
#include "peintre/sound.h"
#include "peintre/scenes/scenes.h"

namespace Peintre {

// The museum, scene 0 (musee.md).

namespace {

enum {
	kPaintingFirst = 0, kPaintingLast = 10,
	kRobot = 11, kEtoile = 12, kPrtholl = 13, kPrtarles = 14, kPrtauvers = 15, kVase = 16,
	kEcran = 17, kCou = 18, kOeild = 19, kOeilg = 20, kTete = 21
};

const ObjectDef kObjects[] = {
	{ "m01_02", 4, false }, { "m01_03", 4, false }, { "m03_01", 4, false },
	{ "m03_02", 4, false }, { "m03_03", 4, false }, { "m03_04", 4, false },
	{ "m03_05", 4, false }, { "m03_06", 4, false }, { "m04_01", 4, false },
	{ "m04_02", 4, false }, { "m04_03", 4, false }, { "robot", 4, false },
	{ "etoile", 2, false }, { "prtholl", 1, true }, { "prtarles", 1, true },
	{ "prtauvers", 1, true }, { "vase", 0xFF, false }, { "ecran", 4, false },
	{ "cou", 4, false }, { "oeild", 4, false }, { "oeilg", 4, false }, { "tete", 4, false }
};

// The painting of each object 0..10: its scene and act.
const int kPaintingScene[11] = { 3, 4, 5, 6, 7, 8, 10, 9, 11, 12, 13 };
const int kPaintingAct[11] = { 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3 };

enum { kAnimEtoile, kAnimRobot01, kAnimRobot02, kAnimRobot03 };

const AnimDef kAnims[] = {
	{ "etoile.3da", "etoile", false }, { "robot01.3da", "robot", false },
	{ "robot02.3da", "robot", false }, { "robot03.3da", "robot", false }
};

// Paintings turned to colour when their scene is complete (init step 9).
struct Colour {
	int object;
	const char *grey, *colour;
	int scene;
};

const Colour kColours[] = {
	{ 1, "MANGEUR", "RVBMANGE", kSceneMangeurs }, { 0, "PATATHOM", "RVBPATAT", kSceneMaisonet },
	{ 2, "CAFE", "RVBCAFE", kSceneCafe }, { 3, "CHAMBRE", "RVBCHAMB", kSceneChambre },
	{ 5, "HOSTO", "RVBHOSTO", kSceneHopiint }, { 6, "TERRASSE", "RVBTERRA", kSceneTerrasse },
	{ 7, "PONT", "RVBPONT", kScenePont }, { 4, "MAISONJ", "RVBMAISO", kSceneMaisonj },
	{ 9, "CHAMP", "RVBCHAMP", kSceneChamp }, { 10, "EGLISE", "RVBEGLIS", kSceneEglise }
};

const char *const kTextures[] = {
	"ROBI3N", "RVBTERRA", "RVBMAISO", "RVBCAFE", "RVBPONT", "RVBPATAT", "RVBCHAMB", "RVBPOSTE",
	"RVBHOSTO", "RVBMANGE", "RVBCHAMP", "RVBJARDI", "RVBEGLIS"
};

const char *const kLines[] = { "a50_01", "a50_01c", "a50_01d", "a50_01e", "a50_01h" };

// The 3D block (musee.md "State").
const uint32 kAfterEnd = 0x4aba5c, kEndMovie = 0x4aba60;
const uint32 kFlown = 0x4aba48, kStarTaken = 0x4aba44, kStarClicked = 0x4abbe8;
const uint32 kStarInBar = 0x4abbd8, kRobotMet = 0x4abbf0;
const uint32 kActStarted = 0x4aba50;   ///< + 4 * (act - 1)
const uint32 kActCurrent = 0x4abbdc;   ///< + 4 * (act - 1)

class Musee : public SceneScript {
public:
	void init(World &w) override;
	void frame(World &w) override;

private:
	bool anyUnsolved(World &w, int from, int to) {
		for (int z = from; z <= to; z++)
			if (!w.zoneSolved(z))
				return true;
		return false;
	}
	void clearActs(World &w) {
		for (int a = 0; a < 3; a++)
			w.var(kActCurrent + 4 * a) = 0;
	}
	void setParts(World &w, byte type) {
		w.objects[kRobot].cursorType = type;
		for (int i = kEcran; i <= kTete; i++)
			w.objects[i].cursorType = type;
	}
	void speak(World &w);
	void dialogue(World &w);
	void hints(World &w);
	void animate(World &w);

	bool _noDoor = true;       ///< 0x4aef64
	bool _starRuns = false;    ///< 0x4aeb24: the star's track may pass frame 52 (Q-0404)
	bool _speaking = false;    ///< 0x59901c
	int _line = 0;             ///< 0x599020
	bool _hint = false;        ///< 0x599014
	bool _scroll = false;      ///< 0x599010
	uint32 _scrollTicks = 0;   ///< 0x599018
	int _scrollStep = 0;       ///< 0x4e3128
	bool _scrollUp = true;     ///< 0x599000
};

void Musee::init(World &w) {
	// Q-0404: nothing here hides the star once taken.
	setObjects(w, kObjects, ARRAYSIZE(kObjects));
	_noDoor = true;

	byte &sun = w.sunflowers();
	if ((sun == 1 && !w.var(kActStarted)) || (sun == 3 && !w.var(kActStarted + 4)) ||
		(sun == 15 && !w.var(kActStarted + 8)))
		sun = 0;

	setAnims(w, kAnims, ARRAYSIZE(kAnims));
	for (int i = 1; i <= 4; i++)
		w.loadBoxSet(i, Common::String::format("BOX%d.3DI", i));
	poseAt(w, kAnimRobot01, 1);

	// Box set: the last matching line wins.
	w.setBoxSet(1);
	if (w.zoneSolved(0) && !w.var(kActStarted)) {
		w.setBoxSet(2);
		w.setHidden(node(w, kPrtarles), true);
		clearActs(w);
	}
	if (w.zoneSolved(1) && w.zoneSolved(2) && w.zoneSolved(3) && !w.var(kActStarted + 4)) {
		w.setBoxSet(3);
		w.setHidden(node(w, kPrtarles), true);
		clearActs(w);
	}
	if (!anyUnsolved(w, 4, 18) && !w.var(kActStarted + 8)) {
		w.setBoxSet(0);
		w.setHidden(node(w, kPrtauvers), true);
		clearActs(w);
	}
	static const int kActZones[3][2] = { { 1, 3 }, { 4, 18 }, { 19, 24 } };
	static const int kActDoor[3] = { kPrtholl, kPrtarles, kPrtauvers };
	for (int a = 0; a < 3; a++) {
		if (w.var(kActCurrent + 4 * a) && w.var(kActStarted + 4 * a) &&
			anyUnsolved(w, kActZones[a][0], kActZones[a][1])) {
			w.setBoxSet(4);
			w.setHidden(node(w, kActDoor[a]), false);
			_noDoor = false;
		}
	}
	if (w.var(kAfterEnd))
		w.setBoxSet(1);

	if (w.zoneSolved(0))
		w.objects[kVase].cursorType = 0xFF;
	else if (w.var(kStarTaken))
		w.objects[kVase].cursorType = 3;

	if (!w.var(kStarInBar)) {
		for (int i = kAnimRobot01; i <= kAnimRobot03; i++)
			w.anims[i].playing = false;
		w.var(kRobotMet) = 0;
		Camera &c = w.camera();
		c.x = -39;
		c.y = -209;
		c.z = 361;
		c.pitch = 4066;
		c.yaw = 20;
		c.roll = 0;
	}
	if (w.var(kRobotMet))
		w.anims[kAnimRobot02].playing = true;

	_speaking = false;
	_line = 0;
	_hint = false;
	_scroll = false;
	_scrollStep = 0;   // 0x4e3128
	_scrollUp = true;  // 0x599000
	for (const char *t : kTextures)
		w.loadTexture(t, t);
	w.retexture(node(w, kEcran), "ROBI3", "ROBI3N");

	for (const Colour &c : kColours)
		if (w.sceneComplete(c.scene))
			w.retexture(node(w, c.object), c.grey, c.colour);

	w.setAmbience("musee");
	if (w.var(kFlown))
		setParts(w, 0xFF);
}

void Musee::speak(World &w) {
	debugC(1, kDebugScript, "Musee: the robot speaks");
	w.vm()->sound()->playStream(kLines[0]);
	_line = 1;
	_speaking = true;
}

void Musee::dialogue(World &w) {
	if (!_speaking || w.vm()->sound()->isStreamPlaying())
		return;
	debugC(1, kDebugScript, "Musee: robot line %d ended", _line - 1);
	if (_line == 1) {
		w.vm()->sound()->playStream(kLines[1]);
		w.retexture(node(w, kEcran), "ROBI3N", "ROBI3");
		_scroll = true;
		_line = 2;
	} else if (_line == 2) {
		w.retexture(node(w, kEcran), "ROBI3", "ROBI3N");
		_scroll = false;
		if (!w.var(kStarTaken) && w.anims[kAnimEtoile].frame != 52) {
			w.anims[kAnimEtoile].playing = true;
			startSound(w, "etoile");
		}
		setParts(w, 4);
		_speaking = false;
	}
}

void Musee::hints(World &w) {
	if (_hint && !w.vm()->sound()->isStreamPlaying())
		_hint = false;
	if (_hint || _speaking || !w.var(kRobotMet))
		return;
	const int32 x = w.camera().x, z = w.camera().z;
	bool play = false;
	if (!_noDoor) {
		if (!w.var(kAfterEnd))
			play = (w.var(kActCurrent) && x > -3000 && z < 4600) ||
				   (w.var(kActCurrent + 4) && z < 6500) ||
				   (w.var(kActCurrent + 8) && x < 3000 && z < 4600);
	} else {
		const bool r1 = z >= 5301 && x >= -429 && x <= 199;
		const bool r2 = x > 1500 && z > 3950 && z < 4500;
		if (w.zoneSolved(0) && !w.var(kActStarted) && (r1 || r2))
			play = true;
		if (w.zoneSolved(1) && w.zoneSolved(2) && w.zoneSolved(3) && !w.var(kActStarted + 4) && r2)
			play = true;
		if (!w.zoneSolved(0) && ((x < -1400 && z > 3950 && z < 4500) || r1 || r2))
			play = true;
	}
	if (play) {
		debugC(1, kDebugScript, "Musee: hint at %d, %d", x, z);
		w.vm()->sound()->playStream(kLines[2]);
		_hint = true;
	}
}

void Musee::animate(World &w) {
	// 0x42b5fe: frames by the elapsed ticks, per-record end rules.
	const int32 e = w.elapsed();
	AnimRecord &star = w.anims[kAnimEtoile];
	if (star.playing) {
		// Before it is taken the track stops at frame 52 (the star on the stand); either
		// stop still poses that tick's frame.
		star.frame += e;
		if (star.frame >= 52 && !_starRuns) {
			star.frame = 52;
			star.playing = false;
		} else if (star.frame >= star.length) {
			star.playing = false;
		}
		w.pose(kAnimEtoile, star.frame);
	}
	if (stepAnim(w, kAnimRobot01, e)) {
		w.anims[kAnimRobot01].playing = false;
		w.anims[kAnimRobot02].playing = true;
	}
	loopAnim(w, kAnimRobot02, e);
	if (stepAnim(w, kAnimRobot03, e)) {
		w.setHidden(node(w, kRobot), true);
		for (int i = kAnimRobot01; i <= kAnimRobot03; i++)
			w.anims[i].playing = false;
	}

	// The robot's screen (ecran) scrolls once more than 10 ticks have passed; the tick
	// count runs all the time (0x42b776). 0x4399d0 calls 0x42ad1c for each UV in turn,
	// which moves its v by 0x7f0000 and counts: six calls up, then six down, so the
	// direction turns part-way through a node's UVs (E-0375).
	_scrollTicks += e;
	if (_scroll && _scrollTicks > 10) {
		_scrollTicks = 0;
		const int n = node(w, kEcran);
		if (n >= 0) {
			Common::Array<int32> &uvs = w.scene3D().nodes[n].uvs;
			for (uint i = 0; i + 1 < uvs.size(); i += 2) {
				if (_scrollUp) {
					uvs[i + 1] += 0x7F0000;
					if (++_scrollStep == 6)
						_scrollUp = false;
				} else {
					uvs[i + 1] -= 0x7F0000;
					if (--_scrollStep == 0)
						_scrollUp = true;
				}
			}
		}
	}
}

void Musee::frame(World &w) {
	int idx = w.objectIndex(w.pick());
	// The museum's hover and clicks need the object within 1,200.
	if (idx >= 0 && w.distance(node(w, idx)) >= 1200)
		idx = -1;
	const int clicked = clickedObject(w, idx);

	if (clicked >= kPaintingFirst && clicked <= kPaintingLast) {
		const int act = kPaintingAct[clicked] - 1;
		if (!w.var(kActStarted + 4 * act)) {
			w.var(kActStarted + 4 * act) = 1;
			clearActs(w);
			w.var(kActCurrent + 4 * act) = 1;
		}
		if (kPaintingScene[clicked] == kSceneChambre)
			w.var(0x4abd14) = 1;
		w.startFlight(kPaintingScene[clicked]);
	} else if (clicked == kVase) {
		if (w.var(kStarClicked)) {
			w.var(kStarClicked) = 0;
			w.requestZone(0);
		}
	} else if (clicked == kEtoile) {
		_starRuns = true;
		w.takeItem(node(w, kEtoile), 0);
		poseAt(w, kAnimEtoile, 166);
		w.anims[kAnimEtoile].playing = true;
		w.var(kStarClicked) = 1;
		w.var(kStarTaken) = 1;
		w.objects[kVase].cursorType = 3;
	} else if (clicked == kRobot || (clicked >= kEcran && clicked <= kTete)) {
		if (!_speaking && !w.var(kFlown)) {
			speak(w);
			setParts(w, 0xFF);
		}
	}

	// Meeting the robot.
	if (!w.anims[kAnimRobot01].playing && !w.anims[kAnimRobot02].playing &&
		!w.anims[kAnimRobot03].playing && !w.var(kRobotMet) &&
		w.distance(node(w, kRobot)) < 1500) {
		speak(w);
		w.anims[kAnimRobot01].playing = true;
		startSound(w, "robot2", true);
		w.var(kRobotMet) = 1;
		setParts(w, 0xFF);
	}
	// Back from the option menu (0x4e3120, set by 0x42f515): cleared only when acted on
	// (E-0017).
	if (w.localVar(0x4e3120) == 1 && !w.var(kFlown)) {
		setParts(w, 4);
		_speaking = false;
		w.localVar(0x4e3120) = 0;
	}
	dialogue(w);
	hints(w);

	// The end of the game.
	if (w.var(kAfterEnd) && w.camera().z < 3500) {
		w.stopAllSounds();
		w.var(kEndMovie) = 1;
		w.requestMovie("fin");
	}

	w.clearClick();
	animate(w);
}

} // End of anonymous namespace

SceneScript *createMusee() {
	return new Musee();
}

} // End of namespace Peintre
