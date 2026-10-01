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

// Zone RH: the Rhine, Alberich's world, second part (games/ring/docs/rh.md).

#include "common/textconsole.h"

#include "ring/resources.h"
#include "ring/ring/zone.h"
#include "ring/ring/zones.h"

namespace Ring {
namespace RH {

using namespace Api;

enum {
	kBrutality = 10000, kHelmetFrog = 10504, kCellsNI = 10505,
	kGuard = 20001,       ///< + the tunnel: 20001..20003
	kKey = 20004,         ///< Indifference, Mistrust, Selfishness (+ the tunnel), Disgust 20007
	kDisgust = 20007, kFish = 20201, kStand = 20202, kNecklace = 20203, kStatue = 20401, kCellsHolder = 20402,
	kCells = 20403, kDaughter = 20501, kGold = 20700, kSpeaking = 21001, kMachinePictures = 21003,
	// Bytes
	kTunnel = 21001, kFishFree = 20200, kStandReady = 20201, kNecklaceState = 20202, kNecklaceGiven = 20301,
	kDaughterState = 20500
};

static int tunnel() { return byte_(kTunnel); }

// With the four keys in the bag the way to the goldfish opens.
static void keysTest() {
	for (int k = kKey; k <= kDisgust; k++)
		if (!bag().has(k))
			return;
	movOn(20101, 1, 1);
	play(23009);
}

// The guard of tunnel b lets the player on: its way on opens.
static void wayOn(int b) {
	movOff(20010 + 10 * b, 0, 0);
	movOn(20010 + 10 * b, 1, 1);
	rotAct(20010 + 10 * b);
}

void enter(RingEngine *vm, int entry) {
	switch (entry) {
	case 0: // from NI's water: the message of NI
		bag().remove(kCellsNI);
		play(23005, true);
		cin("1706");
		puz(22001);
		play(22001);
		break;
	case 10: // resumed after Erda: NI's world record "alb" (spec/bag.md)
		bag().removeAll();
		if (w().varByte(90017) == 0) {
			if (Rotation *r = w().rotation(dword(90021))) {
				rotAct(r->id);
				r->frozen = w().varByte(90025) != 0;
			}
		} else {
			puz(dword(90021));
		}
		if (!vm->loadWorldState("alb"))
			warning("Ring: Wrong Erda AS / RH");
		break;
	case 999: // a test entry (Q-0030)
		bag().removeAll();
		static const int objects[] = { kBrutality, kHelmetFrog, kNecklace, 20004, 20005, 20006, 20007 };
		for (int o : objects)
			bag().add(o);
		rotAct(20501);
		break;
	default:
		warning("Ring: RH entry %d is not implemented", entry);
		break;
	}
}

void onClick(RingEngine *vm, int object, int value) {
	int h = held(), b = tunnel();
	switch (object) {
	case kGuard:
	case kGuard + 1:
	case kGuard + 2:
		if (h)
			break;
		puz(20000);
		show(kSpeaking, 0);
		play(value == 0 ? 20021 + b : 20031 + b);
		return;
	case kKey:
	case kKey + 1:
	case kKey + 2:
		if (h)
			break;
		bag().add(object);
		score(2);
		hide(object);
		accOff(object);
		wayOn(b);
		keysTest();
		return;
	case kDisgust:
		if (h) {
			drop();
			vm->keepHand();
			return;
		}
		if (value == 0) {
			bag().add(kDisgust);
			score(2);
			hide(kDisgust, 0);
			accOff(kDisgust, 0, 1);
			keysTest();
		}
		return;
	case kFish:
		if (h) {
			if (h == kFish) {
				bag().remove(kFish);
				setByte(kFishFree, 1);
				accOn(kStand, 5);
				accOff(kStand, 3, 4);
				show(kFish, 0);
			}
			vm->keepHand();
			drop();
			return;
		}
		if (value == 1 && byte_(kFishFree) == 1) {
			if (byte_(kNecklaceState) != 0) {
				vm->keepHand();
				cin("1694");
			} else { // the fish is taken, and also goes in hand
				cin("1693");
				bag().add(kFish);
				setByte(kFishFree, 0);
				accOff(kStand, 5);
				accOn(kStand, 3, 4);
				hide(kFish, 0);
			}
		} else {
			vm->keepHand();
		}
		return;
	case kStand:
		if (h) {
			if (value == 0 && h == kNecklace) {
				setByte(kNecklaceState, 0);
				bag().remove(kNecklace);
				hide(kStand);
				show(kStand, 0);
			} else if (value == 2 && h == kFish) {
				bag().remove(kFish);
				score(3);
				accOff(kFish);
				setByte(kNecklaceState, 2);
				cin("1688");
				hide(kStand);
				puz(20203);
				play(20201);
			}
			drop();
			return;
		}
		if (value == 0 && byte_(kStandReady) == 1 && byte_(kNecklaceState) == 0) {
			setByte(kNecklaceState, 1);
			bag().add(kNecklace);
			hide(kStand);
			show(kStand, 1);
		} else if (value == 1) {
			if (byte_(kStandReady) != 0) {
				bool taken = byte_(kNecklaceState) != 0;
				cin(taken ? "1690" : "1689");
				hide(kStand);
				show(kStand, taken ? 1 : 0);
			}
			puz(20202);
		} else if (value == 9) {
			cin(byte_(kNecklaceState) != 0 ? "1692" : "1691");
			rotAct(20201);
		}
		return;
	case 20204:
	case 20304:
	case 20404:
	case 20502: { // the key machines: Disgust, Mistrust, Selfishness, Indifference
		static const struct {
			int machine, key, sound;
		} machines[] = { { 20204, 20007, 20202 }, { 20304, 20005, 20304 }, { 20404, 20006, 20402 }, { 20502, 20004, 20504 } };
		for (const auto &m : machines) {
			if (m.machine == object && h == m.key) {
				bag().remove(m.key);
				score(5);
				accOff(object);
				puz(20204);
				play(m.sound);
				if (object == 20502)
					setByte(kDaughterState, 4);
			}
		}
		if (h)
			drop();
		return;
	}
	case 20301:
		if (h) {
			if (value == 2 && h == kNecklace) {
				score(2);
				bag().remove(kNecklace);
				cin("1676");
				accOff(20301, 1);
				rotAct(20301);
			}
			drop();
		} else if (value == 0) {
			if (byte_(kNecklaceState) == 2 && !bag().has(kNecklace)) {
				cin("1678");
				rot(20302, 10.0f, 0.0f, 85.3f, false);
			} else {
				cin("1677");
				vm->gameOver(1);
			}
		} else if (value == 1) {
			vm->rotSetRolTo(20301, 42.0f, 23.0f, 85.7f);
			cin("1679");
			puz(20301);
			play(20301);
		} else if (value == 9) {
			cin("1680");
			rotAct(20301);
		}
		return;
	case 20302:
		if (h)
			break;
		if (value == 0) {
			if (bag().has(kNecklace)) {
				cin("1682");
				rot(20303, 10.0f, 0.0f, 85.3f, false);
			} else {
				cin("1681");
				vm->gameOver(1);
			}
		} else if (value == 1) {
			vm->rotSetRolTo(20302, 320.0f, 26.0f, 85.7f);
			score(2);
			accOff(20302, 1, 2);
			cin("1683");
			puz(20302);
			play(20302);
		}
		return;
	case 20303:
		if (h) {
			if (value == 1 && h == kHelmetFrog) {
				show(20303, 0);
				cin("1684");
				play(23010, true);
				puz(20303);
			} else if (value == 2 && h == kNecklace) {
				setByte(kNecklaceGiven, 1);
				accOn(20303, 0);
				score(2);
				accOff(20303, 1, 2);
				hide(20303, 0);
				play(20303);
			}
			drop();
		} else if (value == 0 && byte_(kNecklaceGiven) == 1) {
			cin("1685");
			rotAct(20304);
		} else if (value == 1) {
			vm->rotSetRolTo(20303, 140.0f, 26.0f, 85.7f);
			cin("1686");
			vm->gameOver(3);
		} else if (value == 9) {
			snd().stop(23010, 0x400);
			cin("1687");
			rotAct(20303);
		}
		return;
	case kStatue:
		if (h == kBrutality) {
			cin("1675");
			score(3);
			puz(20401);
		}
		if (h)
			drop();
		return;
	case kCells:
		if (h == kNecklace) {
			bag().remove(kNecklace);
			hide(kCellsHolder, 1);
			score(2);
			play(20401);
		}
		if (h)
			drop();
		return;
	case kDaughter: {
		if (h)
			break;
		int state = byte_(kDaughterState);
		if (value == 0) {
			if (state != 0) {
				puz(20502);
			} else {
				setByte(kDaughterState, 1);
				snd().stop(23011, 0x400); // never started (Q-0031)
				puz(20501);
				score(2);
				play(20501);
			}
		} else if (value == 1) {
			if (state < 2) {
				cin("1670");
				vm->gameOver(4);
			} else if (state == 3) {
				cin("1671");
				score(2);
				rot(20503, 0.0f, 0.0f, 85.3f, false);
			}
		} else if (value == 2 && state == 2) {
			setByte(kDaughterState, 3);
			snd().stop(23011, 0x400);
			cin("1672");
			accOn(kDaughter, 1);
			accOff(kDaughter, 2);
			puz(20503);
			play(20503);
		} else if (value == 3) {
			cin("1673");
			setByte(kDaughterState, 2);
			hide(kDaughter, 0);
			accOff(kDaughter, 0, 1);
			accOn(kDaughter, 2);
			rotAct(20501);
		} else if (value == 9) {
			rotAct(20501);
		}
		return;
	}
	case kHelmetFrog: // the dive at 20601
		if (h == kHelmetFrog) {
			cin("1668");
			play(23010, true);
			rot(20701, 0.0f, 0.0f, 85.3f, false);
			drop();
		} else if (!h) {
			cin("1669");
			vm->gameOver(3);
		} else {
			drop();
		}
		return;
	case kGold:
		if (h)
			break;
		if (byte_(kDaughterState) != 4) {
			cin("1667");
			vm->gameOver(2);
		} else { // back to Nibelheim, NI's entry 3 (0x44a7d0(3))
			score(1);
			snd().stopAll(0x400);
			cin("1666");
			bag().remove(kCells);
			vm->goZone(kZoneNI, 3);
		}
		return;
	default:
		break;
	}
	if (h)
		drop();
}

void onBeforeMove(RingEngine *vm, int from) {
	if (from == 20401)
		vm->timSto(0);
}

void onAfterMove(RingEngine *vm, int to, int kind) {
	if (to == 20401)
		vm->timSta(0, 50);
	if (kind == 0) {
		// Each test on its own: the last that holds wins.
		for (int b = 0; b < 3; b++) {
			if (to >= 20010 + 10 * b) {
				setByte(kTunnel, b);
				if (!bag().has(kKey + b)) {
					movOn(20010 + 10 * b, 0, 0);
					movOff(20010 + 10 * b, 1, 1);
				}
			}
		}
	} else if (kind == 1 && to >= 20011 && to <= 20031) {
		int guard = kGuard + tunnel();
		show(guard, 0);
		w().setAnimationId(guard, 2, 0, 20001, false);
		show(guard, 2);
	}
}

void onTimer(RingEngine *vm, int id) {
	// The statue's head follows the view (0x444d30).
	static int last = -1;
	if (id != 0)
		return;
	int a = (int)(vm->rotGetAlp(20401) - 35.0f);
	if (a <= 0 || a >= 146)
		return;
	int f = (int)(a * (5.0 / 19.0));
	if (f >= 1 && f <= 28 && f != last) {
		w().setAnimationFrame(kStatue, 0, f);
		last = f;
	}
}

void onAnimation(RingEngine *vm, int id, int frame) {
	int guard = kGuard + tunnel();
	if (id == 20001 && frame == 5) {
		hide(guard, 2);
		show(guard, 4);
		play(20011 + tunnel());
	} else if (id == 20003 && frame == 2) {
		play(23014);
	}
}

void onSound(RingEngine *vm, int id, int type, int reason, int ended) {
	if (!ended)
		return;
	int b = tunnel(), guard = kGuard + b;
	if (id >= 20011 && id <= 20013) { // a guard's first words
		hide(guard, 2);
		hide(guard, 4);
		w().setAnimationId(guard, 2, 0, 20002, false);
		show(guard, 3);
		accOn(guard, 0, 1);
		return;
	}
	if (id >= 20021 && id <= 20023) { // he lets the player on
		cin(Common::String::format("rh_%d", b + 1));
		cin("1696");
		hide(guard);
		wayOn(b);
		return;
	}
	if (id >= 20031 && id <= 20033) { // he leaves his key
		cin(Common::String::format("rh_%d_l0", b + 1));
		accOff(guard);
		accOn(kKey + b);
		show(kKey + b, 0);
		hide(guard);
		puz(20011 + 10 * b);
		return;
	}
	switch (id) {
	case 20201:
		cin("1698");
		bag().add(kNecklace);
		play(23009);
		accOff(kStand);
		rotAct(20201);
		movOn(20201, 0, 0);
		break;
	case 20202:
		cin("1699");
		show(kMachinePictures, 0);
		show(20204, 0);
		movOff(20202, 2, 2);
		movOff(20202, 0, 0);
		movOff(20203, 0, 0);
		movOn(20202, 1, 1);
		movOn(20203, 1, 3);
		rotAct(20202);
		break;
	case 20301:
		hide(20301, 1);
		break;
	case 20302:
		hide(20302, 1);
		bag().add(kNecklace);
		cin("1700");
		if (Rotation *r = w().rotation(20302))
			r->beta = 0.3f;
		rotAct(20302);
		break;
	case 20303:
		snd().stop(23010, 0x400);
		cin("1687");
		play(23009);
		rot(20303, 325.0f, 0.3f, 85.3f);
		break;
	case 20304:
		cin("1701");
		show(20304, 0);
		show(kMachinePictures, 1);
		movOff(20304, 2, 2);
		movOff(20304, 0, 0);
		movOff(20305, 0, 0);
		movOn(20304, 1, 1);
		movOn(20305, 1, 2);
		rotAct(20304);
		break;
	case 20401:
		cin("1674");
		bag().add(kCells);
		play(23009);
		hide(kCellsHolder);
		cin("1702");
		if (Rotation *r = w().rotation(20402))
			r->setAlpha(0.0f);
		rotAct(20402);
		break;
	case 20402:
		cin("1703");
		show(20404, 0);
		show(kMachinePictures, 2);
		movOff(20402, 0, 0);
		movOff(20403, 0, 0);
		movOn(20402, 1, 1);
		movOn(20403, 1, 2);
		rotAct(20402);
		break;
	case 20501:
		puz(20502);
		play(20502);
		break;
	case 20502:
		hide(kDaughter, 2);
		accOn(kDaughter, 3, 4);
		break;
	case 20503:
		hide(kDaughter, 3);
		cin("1704");
		if (Rotation *r = w().rotation(20501))
			r->setAlpha(0.0f);
		rotAct(20501);
		break;
	case 20504:
		cin("1705");
		show(20502, 0);
		movOff(20503, 0, 0);
		movOff(20504, 0, 0);
		movOn(20504, 1, 2);
		rotAct(20503);
		break;
	case 22001: // the message
		puz(22003);
		play(22003);
		break;
	case 22003:
		puz(22002);
		play(22002);
		break;
	case 22002:
		cin("1697");
		bag().add(kBrutality);
		bag().add(kHelmetFrog);
		rot(20010, 1.5f, -4.3f, 79.3f);
		break;
	default:
		break;
	}
}

} // End of namespace RH
} // End of namespace Ring
