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

// Zone AS: the hub, Ish's island and the ring dial (games/ring/docs/as.md).

#include "common/system.h"
#include "common/textconsole.h"

#include "ring/bag.h"
#include "ring/resources.h"
#include "ring/ring.h"
#include "ring/sound.h"
#include "ring/world.h"
#include "ring/ring/zones.h"

namespace Ring {
namespace AS {

enum {
	kObjRingPlace = 80007, kObjDeath = 80020, kObjSoundPoints = 80012, kObjSky16 = 80016, kObjDial = 80018, kObjWorlds = 80019,
	kObjDialButtons = 80021, kObjSky = 80022,
	kRotIsland = 80001, kRotChamber = 80101,
	kByteSway = 80001, kByteTarget = 80004, kByteFrame = 80005,
	kFloatSign = 80001, kFloatAmount = 80002,
	kByteWorldDone = 90001 ///< + 0..3: NI, N2, FO, WA done (SY's variables)
};

static void showRotation(RingEngine *vm, int id, float alpha, float beta, float ran, bool setBeta = true) {
	if (Rotation *r = vm->world().rotation(id)) {
		r->setAlpha(alpha);
		if (setBeta)
			r->beta = beta;
		r->ran = ran;
		vm->rotSetAct(id);
	}
}

static void startTimers(RingEngine *vm) {
	vm->timSta(2, 100000);
	vm->timSta(3, 220000);
	vm->timSta(4, 150000);
}

void enter(RingEngine *vm, int entry) {
	int lang = vm->languageId();
	switch (entry) {
	case 999: // a new game
		showRotation(vm, kRotIsland, 90.0f, 0.0f, 85.3f, false);
		startTimers(vm);
		break;
	case 998: // the intro's chain of pictures and sounds (onSound)
		vm->plyCinMul(lang < 4 || (lang > 5 && lang != 7) ? "1164" : "1163");
		vm->plyCin("1166");
		vm->puzSetAct(80011);
		vm->sounds().play(80100, false);
		break;
	case 5: // down from the sky
		vm->plyCin("1047");
		showRotation(vm, 80003, 270.0f, 0.0f, 85.3f);
		break;
	case 6: // the end
		vm->plyCinMul(lang == 4 || lang == 5 || lang == 7 ? "1160" : lang == 6 ? "1161" : "1162");
		vm->puzSetAct(80001);
		vm->sounds().play(80107, false);
		break;
	default:
		warning("Ring: AS entry %d is not implemented", entry);
		break;
	}
}

void onClick(RingEngine *vm, int object, int value) {
	World &w = vm->world();
	Sounds &snd = vm->sounds();
	// With an object in hand every click drops it; Death on the ring's place ends the game.
	if (int held = vm->bag().held()) {
		if (object == kObjRingPlace && held == kObjDeath) {
			vm->bag().removeAll();
			snd.stopAll(0x400);
			vm->timStoAll();
			vm->goZone(kZoneAS, 6);
		}
		vm->dropObject();
		return;
	}
	switch (object) {
	case kObjSoundPoints: {
		static const int sounds[] = { 80028, 80025, 80021, 80024, 80022, 80026, 80027, 80023 };
		if (value >= 0 && value < 8)
			snd.play(sounds[value], false);
		break;
	}
	case kObjDial: { // a done world's mark: its picture and monologue again
		static const int ambient[] = { 80201, 80203, 80204, 80205 }, volume[] = { 80, 90, 90, 80 };
		static const int monologue[] = { 80040, 80049, 80058, 80068 };
		if (value >= 0 && value < 4 && w.varByte(kByteWorldDone + value) == 1) {
			vm->puzSetAct(80002 + value);
			snd.setVolume(ambient[value], volume[value]);
			snd.play(monologue[value], false);
		}
		break;
	}
	case kObjWorlds: { // entering the world on the puzzle, unless it is done
		static const int world[] = { 1, 2, 1, 3, 2, 4 };
		static const int zone[] = { 0, kZoneNI, kZoneN2, kZoneFO, kZoneWA };
		if (value >= 0 && value < 6 && w.varByte(kByteWorldDone - 1 + world[value]) == 0) {
			// 0x44a7d0, 0x436270, 0x443710, 0x43ad00: entered before, the world resumes (E-0091)
			int n = world[value];
			vm->timStoAll();
			if (w.varByte(90008 + n) == 0)
				vm->goZone(zone[n], 0);
			else
				vm->goZone(w.var(World::kVarDword, 90012 + n), 10);
		}
		break;
	}
	case kObjDialButtons:
		if (value == 0) { // go: ride to the world the ring points at
			static const struct {
				int frame, puzzle;
				const char *video;
			} go[] = { { 11, 80007, "1143" }, { 21, 80009, "1144" }, { 31, 80008, "1145" }, { 41, 80010, "1146" } };
			int target = w.varByte(kByteTarget);
			if (target == 1) {
				vm->puzSetAct(80006);
				if (w.varByte(kByteWorldDone) == 0) {
					vm->plyCin("1141");
				} else {
					vm->plyCin("1142");
					w.showPresentation(kObjWorlds, 0, true, g_system->getMillis());
				}
			}
			for (const auto &g : go) {
				if (target == g.frame) {
					vm->puzSetAct(g.puzzle);
					vm->plyCin(g.video);
				}
			}
		} else if (value >= 1 && value <= 4) { // turn the ring by 10 × value frames
			snd.play(80080, false);
			snd.play(80082, false);
			int target = w.varByte(kByteFrame) + 10 * value;
			w.setVarByte(kByteTarget, target > 49 ? target - 50 : target);
			w.pauseAnimations(kObjDial, 1, false);
			w.setAccessibilities(kObjDialButtons, false, 0, 4);
		}
		break;
	case kObjSky:
		returnFromWorld(vm, 5);
		break;
	default:
		break;
	}
}

void onAnimation(RingEngine *vm, int id, int frame) {
	World &w = vm->world();
	if (id != 80001)
		return;
	if (frame == w.varByte(kByteTarget)) { // the ring stops on its target
		w.pauseAnimations(kObjDial, 1, true);
		w.setAccessibilities(kObjDialButtons, true, 0, 4);
		vm->sounds().stop(80082, 0x400);
		vm->sounds().play(80081, false);
	}
	w.setVarByte(kByteFrame, frame);
}

void onBeforeMove(RingEngine *vm, int from, int to, int kind) {
	// From a world's puzzle back to the chamber: the ride's video.
	if (kind != 2 || to != kRotChamber)
		return;
	switch (from) {
	case 80006: vm->plyCin(vm->world().varByte(kByteWorldDone) == 0 ? "1151" : "1152"); break;
	case 80007: vm->plyCin("1153"); break;
	case 80009: vm->plyCin("1154"); break;
	case 80008: vm->plyCin("1155"); break;
	case 80010: vm->plyCin("1156"); break;
	default: break;
	}
}

void onAfterMove(RingEngine *vm, int to, int from, int kind) {
	if (kind == 2 && to == kRotChamber && from >= 80006 && from <= 80010)
		vm->world().setVarByte(80003, 0);
}

void onTimer(RingEngine *vm, int id) {
	World &w = vm->world();
	switch (id) {
	case 2:
	case 3:
	case 4: { // a voice, then the sway, the flicker and a whisper
		static const uint32 sway[] = { 20, 30, 10 };
		vm->sounds().play(80016 + id, false);
		if (!vm->timerRunning(5))
			vm->timSta(5, sway[id - 2]);
		if (!vm->timerRunning(6))
			vm->timSta(6, 10);
		if (!vm->sounds().typePlaying(kSoundDialogue))
			vm->sounds().play(80004 + vm->rnd().getRandomNumber(11), false);
		break;
	}
	case 5: { // the view sways from side to side and settles (E-0059)
		w.setVarByte(kByteSway, w.varByte(kByteSway) + 1);
		float d = w.varFloat(kFloatSign) * w.varFloat(kFloatAmount);
		if (Rotation *r = w.rotation(vm->currentRotation())) {
			r->beta += d;
			r->alpha += d * 0.5f;
		}
		w.setVarFloat(kFloatSign, w.varFloat(kFloatSign) * -1.0f);
		w.setVarFloat(kFloatAmount, w.varFloat(kFloatAmount) * (float)(5.0 / 6.0));
		if (w.varByte(kByteSway) > 50) {
			w.setVarByte(kByteSway, 0);
			w.setVarFloat(kFloatAmount, 2.0f);
			vm->timSto(5);
			vm->timSto(6);
			w.showPresentation(kObjSky16, -1, false);
		}
		break;
	}
	case 6: // 80016 flickers
		w.showPresentation(kObjSky16, -1, vm->rnd().getRandomNumber(9) % 2 == 0, g_system->getMillis());
		break;
	default:
		break;
	}
}

void onSound(RingEngine *vm, int id, int type, int reason, int ended) {
	if (!ended)
		return;
	World &w = vm->world();
	Sounds &snd = vm->sounds();
	// The four monologues; at a chain's end its world's ambient sound comes back up.
	static const struct {
		int first, last, ambient, state;
	} chains[] = { { 80040, 80048, 80201, 2 }, { 80049, 80057, 80203, 3 }, { 80058, 80067, 80204, 4 }, { 80068, 80080, 80205, 5 } };
	for (const auto &c : chains) {
		if (id >= c.first && id < c.last) {
			snd.play(id + 1, false);
			return;
		}
		if (id == c.last) {
			snd.setVolume(c.ambient, 100);
			w.setVarByte(80002, c.state);
			return;
		}
	}
	switch (id) {
	case 80100: vm->puzSetAct(80012); snd.play(80101, false); break;
	case 80101: vm->puzSetAct(80011); snd.play(80102, false); break;
	case 80102: vm->puzSetAct(80012); snd.play(80103, false); break;
	case 80103: vm->plyCin("1157"); vm->puzSetAct(80013); snd.play(80104, false); break;
	case 80104: snd.play(80105, false); break;
	case 80105: vm->plyCin("1158"); vm->puzSetAct(80014); snd.play(80106, false); break;
	case 80106:
		startTimers(vm);
		showRotation(vm, kRotIsland, 270.0f, -26.0f, 85.3f);
		break;
	case 80107: // the end: Isha's picture and words (0x431190(7, 0), games/ring/docs/sy.md)
		vm->plyCin("1159");
		vm->setZone(kZoneSY);
		vm->puzSetAct(1);
		w.showPresentation(7, 0, true, g_system->getMillis());
		snd.play(90001, false);
		break;
	default:
		break;
	}
}

void returnFromWorld(RingEngine *vm, int n) {
	World &w = vm->world();
	static const int monologue[] = { 80040, 80049, 80058, 80068 };
	if (n >= 1 && n <= 4) {
		vm->setZone(kZoneAS);
		if (n == 3)
			vm->sounds().setTypeVolume(2, 100); // FO's ending set it to 0
		w.setAccessibilities(kObjDial, true, n - 1, n - 1);
		w.setVarByte(kByteWorldDone - 1 + n, 1);
		vm->bag().removeAll();
		if (w.varByte(kByteWorldDone) == 1 && w.varByte(kByteWorldDone + 1) == 1 && w.varByte(kByteWorldDone + 2) == 1 &&
			w.varByte(kByteWorldDone + 3) == 1)
			vm->bag().add(kObjDeath);
		w.showPresentation(kObjDial, n + 1, true, g_system->getMillis());
		showRotation(vm, kRotChamber, 90.0f, 0.0f, 85.3f, false);
		vm->sounds().play(monologue[n - 1], false);
		if (n == 1) {
			w.setAccessibilities(kObjWorlds, false, 0, 0);
			w.setAccessibilities(kObjWorlds, true, 1, 1);
		}
	} else if (n == 5) {
		vm->goZone(kZoneAS, 5);
	} else if (n == 13) {
		vm->bag().removeAll();
		vm->timStoAll();
		vm->sounds().stopAll(0x400);
		vm->setZone(kZoneAS);
		showRotation(vm, kRotChamber, 90.0f, 0.0f, 85.3f, false);
		vm->bag().hide(); // 0x419350
		startTimers(vm);
	}
}

} // End of namespace AS
} // End of namespace Ring
