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

// Zone N2: Nibelheim again, Alberich's second world (games/ring/docs/n2.md).

#include "common/textconsole.h"

#include "ring/resources.h"
#include "ring/ring/zone.h"
#include "ring/ring/zones.h"

namespace Ring {
namespace N2 {

using namespace Api;

enum {
	kFire = 70000, kAlberich = 70001, kConsole = 70100, kCover1 = 70101, kCover2 = 70102, kHandle = 70103,
	kHologram = 70105, kCross = 70106, kDam = 70108, kMime = 70300, kCage = 70303, kTear = 70304,
	kCasing = 70404, kThermometer = 70405, kBubbles = 70406, kCentaur = 70500, kChrysoberyl = 70503,
	kNib = 70700, kMessage = 72000,
	// Bytes, word, string
	kTearIn = 70001, kCasingClosed = 70005, kDamOpen = 70012, kLine = 70013, kRound = 70014, kGiven = 70015,
	kCrossWord = 70016, kFaces = 70099
};

// N2's score is SY's float 90006.
static void score2(float n) { w().setVarFloat(90006, w().varFloat(90006) + n); }

// A dialogue line ended: the close-up of string 70099's character at byte 70013 (puzzle
// character + base), the next line, the index on. The Nib's lines skip '0' and ' '.
static void face(int next, int base, bool skipBlank) {
	Common::String faces = w().varString(kFaces);
	int i = byte_(kLine);
	char c = i >= 0 && i < (int)faces.size() ? faces[i] : ' ';
	if (!(skipBlank && (c == '0' || c == ' ')))
		puz(base + c);
	play(next);
	setByte(kLine, i + 1);
}

// 0x433ee0: Alberich's test begins once the dam is open, the tear out and the Cage and the
// three creatures in the bag.
static void testBegins() {
	if (byte_(kDamOpen) != 1 || byte_(kTearIn) != 0)
		return;
	for (int o : { (int)kCage, (int)kCentaur, kCentaur + 1, kCentaur + 2 })
		if (!bag().has(o))
			return;
	setByte(kLine, 11);
	puz(70305);
	play(70017);
}

// The fire's 3D sound 70107, on or off where the heater switches it.
static void fireSound(bool on) {
	for (int owner : { 70000, 70001, 70400, 70100, 70101, 70100, 70102 })
		g_engine->setSoundItem(owner, 70107, on);
}

// 0x433fa0: the heater off (the tear taken out of its casing).
static void heaterOff() {
	g_engine->setSoundItem(70410, 70412, false);
	g_engine->setSoundItem(70400, 70412, false);
	for (int v = 95; v >= 71; v--) {
		g_engine->setSoundItemVolume(70411, 70412, v);
		g_engine->renderFrame();
	}
	g_engine->setSoundItem(70411, 70412, false);
	fireSound(false);
	play(70000, true);
}

// 0x4340c0: the heater on.
static void heaterOn() {
	stop(70000);
	for (int owner : { 70411, 70410, 70400 })
		g_engine->setSoundItem(owner, 70412, true);
	for (int v = 70; v <= 94; v++) {
		g_engine->setSoundItemVolume(70411, 70412, v);
		g_engine->renderFrame();
	}
	fireSound(true);
}

static void consoleAccOn() {
	accOn(kCover1, 0);
	accOn(kCover2, 0);
	accOn(kCover1, 2);
	accOn(kCover2, 2);
}

void enter(RingEngine *vm, int entry) {
	switch (entry) {
	case 0: // from AS: the message
		bag().removeAll();
		vm->timStoAll();
		bag().add(kFire);
		cin("1507");
		cin("1508");
		puz(72001);
		hide(kMessage);
		// ponytail: CurSet(0x36), the empty Windows cursor, lasts until the next frame's tracking (Q-0041)
		play(72001);
		break;
	case 10: // resumed after Erda: world 2's record "log" (spec/bag.md)
		bag().removeAll();
		if (w().varByte(90018) == 0) {
			if (Rotation *r = w().rotation(dword(90022))) {
				rotAct(r->id);
				r->frozen = w().varByte(90026) != 0;
			}
		} else {
			puz(dword(90022));
		}
		if (!vm->loadWorldState("log"))
			warning("Ring: Wrong Erda AS - N2 -> Can not find file");
		break;
	case 999: // a test entry
		bag().removeAll();
		vm->timStoAll();
		bag().add(kFire);
		play(70000, true);
		rotAct(70500);
		break;
	default:
		warning("Ring: N2 entry %d is not implemented", entry);
		break;
	}
}

void onClick(RingEngine *vm, int object, int value) {
	int h = held();
	switch (object) {
	case kMime:
		if (h == kChrysoberyl) {
			bag().remove(kChrysoberyl);
			cin("1495");
			show(kCage);
			bag().add(kCage);
			score2(5);
		}
		if (h)
			drop();
		break;
	case kFire: // at the fire place 70600
		if (h == kFire) {
			score2(2);
			cin("1389");
			rot(70500, 243.0f, 0.0f, 85.7f, false);
		}
		if (h)
			drop();
		break;
	case kConsole:
		if (h) {
			drop();
		} else if (value == 1) {
			vm->rotSetRolTo(70100, 270.4f, 10.4f, 85.7f);
			puz(70100);
			consoleAccOn();
			accOn(kConsole);
			vm->setMouse(505, 205);
		}
		break;
	case kHologram:
		if (h) {
			drop();
		} else {
			show(kCross, word(kCrossWord));
			puz(70102);
		}
		break;
	case kCasing:
		if (h) {
			drop();
			break;
		}
		setByte(kCasingClosed, 0);
		if (value == 0) { // opening
			hide(kCasing);
			movOff(70411);
			accOff(kCasing, 0);
			if (byte_(kTearIn) == 0) {
				show(kCasing, 2);
			} else {
				show(kCasing, 4);
				show(kCasing, 1);
			}
			show(kCasing, 0);
		} else if (value == 1 && byte_(kTearIn) == 1) { // the tear taken
			hide(kCasing, 1);
			bag().add(kTear);
			score2(5);
			hide(kCasing, 6);
			setByte(kTearIn, 0);
		}
		break;
	case kCentaur:
	case kCentaur + 1:
	case kCentaur + 2:
	case kChrysoberyl:
		if (h) {
			if (value == 1 && h >= kCentaur && h <= kCentaur + 2) { // a creature given to Alberich
				bag().remove(h);
				setByte(kGiven, h - kCentaur);
				for (int o = kCentaur; o <= kCentaur + 2; o++)
					accOff(o);
				play(71010 + (h - kCentaur));
			}
			drop();
		} else if (value == 0) {
			if (object == kChrysoberyl) {
				show(kChrysoberyl);
				score2(5);
			} else {
				hide(object);
				score2(3);
			}
			accOff(object);
			bag().add(object);
		}
		break;
	case kNib:
		if (h) {
			if (h == kFire && value == 1) {
				accOff(kNib);
				score2(5);
				hide(kNib);
				stop(70701);
				vm->setSoundItem(70200, 70701, false);
				cin("1494");
			}
			drop();
		} else if (value == 0) {
			setByte(kLine, 31);
			puz(70600);
			play(70043);
		}
		break;
	default:
		break;
	}
}

void onButtonDown(RingEngine *vm, int object, int value) {
	if (object == kCover1 || object == kCover2) {
		if (held()) {
			drop();
			return;
		}
		if (value == 1) { // opening
			vm->rotSetRolTo(70100, 270.4f, 10.4f, 85.7f);
			puz(70100);
			hide(kCover2);
			accOff(kCover1, 1, 2);
			accOff(kCover2, 1, 2);
			accOff(kConsole);
			accOff(kHandle);
			if (object == kCover1) {
				show(kCover1, 5);
				show(kConsole, 1);
				show(kCover1, 2);
			} else {
				show(kCover2, 0);
				show(kConsole, 2);
				show(kCover2, 2);
			}
		} else if (value == 0) { // closing
			if (object == kCover1) {
				accOff(kCover1, 1, 2);
				show(kCover1, 4);
			} else {
				accOff(kCover2, 1, 2);
				accOff(kHologram);
				show(kCover2, 3);
			}
			hide(kConsole);
		}
	} else if (object == kCasing && value == 2) { // closing the casing
		accOff(kCasing, 1, 2);
		show(kCasing, byte_(kTearIn) == 0 ? 3 : 5);
		hide(kCasing, 0);
		hide(kCasing, 1);
	}
}

// The drags' counters (the original's globals).
static int s_counter, s_offset;

static int crossAngle(int x, int y) { // as NI's (ni.md, "Drag")
	int base = x < 1 ? (y > 0 ? 30 : 20) : (y > 0 ? 0 : 10);
	int part = x < 1 ? (y > 0 ? (y + 40 + x) / 6 : (y - x + 40) / 6) : (y > 0 ? (x - y + 40) / 6 : (40 - y - x) / 6);
	return base + MAX(part, 0);
}

static int wrap19(int v) {
	if (v < 0)
		v += ((18 - v) / 19) * 19;
	return v > 18 ? v % 19 : v;
}

static void stepsTo(int object, int from, int to) {
	for (int i = from; i != to; i += from < to ? 1 : -1) {
		hide(object);
		show(object, i);
		g_engine->renderFrame();
	}
	hide(object);
	show(object, to);
	g_engine->renderFrame();
}

void onDrag(RingEngine *vm, int object, int value, int phase) {
	Drag &d = vm->drag();
	int dx = ABS(d.current.x - d.press.x), dy = ABS(d.current.y - d.press.y);
	bool xMoved = d.current.x != d.previous.x, yMoved = d.current.y != d.previous.y;
	switch (object) {
	case kHandle:
		if (phase == 1) {
			s_counter = 0;
			d.mode = 2;
			d.limit = Common::Rect(495, 194, 598, 284);
		} else if (phase == 3 && xMoved) {
			s_counter = d.current.x < d.press.x ? 0 : CLIP(dx / 5, 0, 12);
			hide(kHandle);
			show(kHandle, s_counter);
			if (!playing(70401))
				play(70401); // not declared in N2 (Q-0040)
		} else if (phase == 2) {
			if (!playing(70401))
				stop(70401); // as coded
			accOff(kHandle);
			if (s_counter < 7) {
				stepsTo(kHandle, s_counter, 0);
				accOn(kHandle);
			} else {
				stepsTo(kHandle, s_counter, 12);
				vm->renderFor(1000);
				stepsTo(kHandle, 12, 0);
				accOn(kHandle);
				cin("1498");
				rot(70001, 270.0f, 0.0f, 85.7f, false);
				testBegins();
			}
		}
		break;
	case kCross: {
		auto rel = [&](int v, int ref) { return v < ref ? -ABS(v - ref) : ABS(v - ref); };
		if (phase == 1) {
			d.reference = Common::Point(243, 276);
			s_offset = wrap19(crossAngle(rel(d.current.x, 243), rel(d.current.y, 276)) - word(kCrossWord));
		} else if (phase == 3) {
			int x = rel(d.current.x, d.reference.x), y = rel(d.current.y, d.reference.y);
			if (!xMoved || ABS(x) > 40 || ABS(y) > 40)
				break;
			s_counter = wrap19(s_offset + crossAngle(x, y));
			hide(kCross);
			show(kCross, s_counter);
			setWord(kCrossWord, s_counter);
		} else if (phase == 2) {
			int v = s_counter;
			if (v != 12) {
				int step = (v >= 1 && v <= 6) || (v >= 13 && v <= 15) ? -1 : 1;
				while (v != 0 && v != 12 && !vm->shouldQuit()) {
					v = v + step > 18 ? 0 : v + step < 0 ? 18 : v + step;
					w().hideAndFree(kCross);
					show(kCross, v);
					vm->renderFrame();
				}
			}
			s_counter = v;
			setWord(kCrossWord, v);
			setByte(10104, v == 12); // NI's variable, as coded (E-0131)
			movOn(70101, v == 12 ? 2 : 1, v == 12 ? 2 : 1);
			movOff(70101, v == 12 ? 1 : 2, v == 12 ? 1 : 2);
		}
		break;
	}
	case kDam:
		if (phase == 1) {
			d.mode = 2;
			d.limit = Common::Rect(389, 270, 434, 390);
		} else if (phase == 3 && yMoved) {
			int v = value == 0 ? (d.current.y < d.press.y ? 0 : dy / 3) : (d.current.y > d.press.y ? 13 : 13 - dy / 3);
			hide(kDam);
			if (v > 12) {
				show(kDam, 14);
				show(kDam, 13);
			} else {
				hide(kDam, 14);
				show(kDam, MAX(v, 0));
			}
		} else if (phase == 2) {
			hide(kDam);
			if (value == 0 && dy > 19 && d.current.y >= d.press.y) { // the dam opens: the fire and the Nib go out
				show(kDam, 13);
				accOff(kDam, 0);
				accOn(kDam, 1);
				setByte(kDamOpen, 1);
				show(kDam, 14);
				play(70101);
				cin("1496");
				cin("1497");
				score2(10);
				hide(kFire);
				hide(kDam);
				accOff(kNib);
				hide(kNib);
				stop(70701);
				vm->setSoundItem(70200, 70701, false);
				break;
			}
			if (value == 1 && (dy < 20 || d.current.y > d.press.y)) { // stays open
				show(kDam, 13);
				setByte(kDamOpen, 1);
				show(kDam, 14);
				break;
			}
			show(kDam, 0);
			if (value == 1) {
				accOn(kDam, 0);
				accOff(kDam, 1);
			}
			setByte(kDamOpen, 0);
			hide(kDam, 14);
		}
		break;
	default:
		break;
	}
}

void onAccessibility(RingEngine *vm, int object, int value) {
	if (object == kConsole && value == 0) { // the console's border: leave it
		Common::Point mouse = vm->mouse();
		rotAct(70100);
		consoleAccOn();
		accOn(kConsole, 4);
		vm->setMouse(mouse.x, mouse.y);
	}
}

void onBeforeMove(RingEngine *vm, int from, int to, int kind) {
	if (from == 70000 || from == 70001)
		vm->timSto(0);
	if (kind == 3) {
		if (to == 70501 || to == 70511 || to == 70521)
			play(70501);
		if (from == 70501 || from == 70511 || from == 70521)
			play(70502);
	}
}

void onAfterMove(RingEngine *vm, int to, int value, int kind) {
	if ((to == 70000 || to == 70001) && byte_(kDamOpen) == 0)
		vm->timSta(0, 10000);
	if (kind == 0 && value == 7)
		testBegins();
	else if (kind == 0 && value == 16)
		vm->gameOver(1); // the ride 70101 -> 70600 with the cross not at 12
}

void onTimer(RingEngine *vm, int id) {
	if (id == 0)
		play(70004 + rnd(13)); // Alberich's remarks while the fire burns
}

void onAnimation(RingEngine *vm, int id, int frame) {
	auto consoleBack = [&]() {
		puz(70100);
		consoleAccOn();
		accOn(kConsole);
		accOn(kHandle);
	};
	switch (id) {
	case 70300:
		if (frame == 3)
			play(70301);
		break;
	case 70102: // cover 1 closing
		if (frame == 1) {
			play(70103);
		} else if (frame == 36) {
			hide(kCover1, 4);
			hide(kCover1, 0);
			hide(kCover1, 5);
			consoleBack();
		}
		break;
	case 70103: // cover 1 opening
		if (frame == 36) {
			play(70102);
		} else if (frame == 1) {
			accOn(kCover1, 1);
			show(kConsole, 1);
		}
		break;
	case 70104: // cover 2 opening
		if (frame == 1) {
			play(70104);
		} else if (frame == 15) {
			snd().stop(70104, 0x400);
			accOn(kCover2, 1);
			accOn(kHologram, 0);
		}
		break;
	case 70106: // cover 2 closing
		if (frame == 15) {
			play(70105);
		} else if (frame == 1) {
			snd().stop(70105, 0x400);
			hide(kCover1, 0);
			hide(kCover2, 0);
			consoleBack();
		}
		break;
	case 70422: // the casing opening, empty, or with the tear (70424)
	case 70424:
		if (frame == 1) {
			if (id == 70424)
				show(kCasing, 6);
			accOn(kCasing, 1, 2);
		} else if (frame == 2) {
			stop(70407);
		} else if (frame == 14) {
			play(70407);
		} else if (frame == 15) {
			stop(70405);
		} else if (frame == 26) {
			play(70405);
			if (id == 70424) {
				hide(kBubbles);
				heaterOff();
			}
		}
		break;
	case 70423: // the casing closing, empty, or with the tear (70425)
	case 70425:
		if (frame == 27 && id == 70423) {
			movOn(70411);
			accOn(kCasing, 0);
		} else if (frame == 26) {
			if (id == 70425) {
				movOn(70411);
				accOn(kCasing, 0);
				setByte(kCasingClosed, 1);
				show(kBubbles);
				heaterOn();
				if (byte_(kDamOpen) == 0)
					show(kFire);
			}
			stop(70406);
			stop(70408);
		} else if (frame == 15) {
			play(70406);
		} else if (frame == 2) {
			play(70408);
			if (id == 70425)
				hide(kCasing, 6);
		}
		break;
	default:
		break;
	}
}

void onSound(RingEngine *vm, int id, int type, int reason, int ended) {
	if (!ended)
		return;
	int r = byte_(kRound), c = byte_(kGiven);
	// The message: "-> n" is PuzSetAct(n), play(n); some with a video first.
	static const struct {
		int sound, next;
		const char *video;
	} message[] = {
		{ 72001, 72002, nullptr }, { 72002, 72003, "1499" }, { 72003, 72004, nullptr }, { 72004, 72005, nullptr },
		{ 72005, 72006, "1500" }, { 72006, 72007, nullptr }, { 72007, 72008, nullptr }, { 72008, 72009, nullptr },
		{ 72009, 72010, "1501" }
	};
	for (const auto &m : message) {
		if (m.sound == id) {
			if (m.video)
				cin(m.video);
			puz(m.next);
			play(m.next);
			return;
		}
	}
	if (id >= 70056 && id <= 70065) {
		face(id + 1, 70251, false);
	} else if (id >= 70043 && id <= 70054) {
		face(id + 1, 70551, true);
	} else if ((id >= 70017 && id <= 70021) || (id >= 70024 && id <= 70026)) {
		face(id + 1, 70251, false);
	} else if (id >= 71001 && id <= 71003) { // a round begins
		cin(Common::String::format("N2_%dA", r));
		show(kAlberich, r - 1);
		if (id == 71001)
			accOn(kCentaur, 1);
	} else if (id >= 71010 && id <= 71012) { // the creature given
		cin(Common::String::format("N2_%d%c", r, 'C' + c));
		hide(kAlberich);
		play(71100 + 10 * (r - 1) + c);
	} else if (id >= 71100 && id <= 71129) {
		if (r < 3) {
			if (r == 2 && c == 0) {
				vm->gameOver(2);
			} else {
				play(71001 + r);
				setByte(kRound, r + 1);
				for (int o = kCentaur; o <= kCentaur + 2; o++)
					accOn(o);
			}
		} else if (c != 2) {
			vm->gameOver(2);
		} else {
			puz(70303);
			play(70001);
		}
	}
	switch (id) {
	case 72010:
		cin("1502");
		play(70000, true);
		setByte(kLine, 0);
		puz(70301);
		play(70056);
		break;
	case 70066:
		if (Rotation *rot0 = w().rotation(70300))
			rot0->setAlpha(160.0f);
		rotAct(70300);
		break;
	case 70055:
		accOff(kNib, 0);
		accOn(kNib, 1);
		rotAct(70200);
		break;
	case 70022:
		testBegins();
		rotAct(70001);
		cin("1503");
		puz(70303);
		setByte(kLine, 51);
		play(70024);
		break;
	case 70027:
		puz(70000);
		play(71001);
		break;
	case 70101: // after the dam
		puz(70305);
		play(70023);
		break;
	case 70023:
		accOff(kDam);
		puz(70102);
		break;
	case 70001: // the end
		puz(70306);
		play(70002);
		break;
	case 70002:
		puz(70303);
		play(70003);
		break;
	case 70003: // on to RO (0x43d8f0(0))
		cin("1504");
		cin("1505");
		vm->timStoAll();
		snd().stopAll(0x400);
		w().setVarFloat(90006, 50.0f);
		vm->goZone(kZoneRO, 0);
		break;
	default:
		break;
	}
}

} // End of namespace N2
} // End of namespace Ring
