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

// Zone WA: Walhalla, Wotan's world (games/ring/docs/wa.md).

#include "common/textconsole.h"

#include "ring/resources.h"
#include "ring/ring/zone.h"
#include "ring/ring/zones.h"

namespace Ring {
namespace WA {

using namespace Api;

enum {
	kBeam = 50000, kInk = 50101, kPaper = 50102, kStylet = 50103, kInkStylet = 50104, kAshes = 50105,
	kDesk = 50100, kLeaf = 50201, kLeafPlace = 50202, kSky = 50203, kTask = 50300, kConch = 50301, kBark = 50302,
	kGolem = 50400, kFeather = 50401, kFlower = 50402, kPart = 50431, kPiece = 50451, kGrid = 50499,
	kItems = 50500, kSword = 50501, kApple = 50502, kBranches = 50503, kRope = 50504, kSwitches = 50600,
	kRopePlace = 50601, kNorns = 50700,
	// Bytes, words, dwords
	kLeafTask = 50001, kTaskDone = 50003, kConchState = 50005, kSwitch = 50007, kLetterHold = 50011,
	kProgress = 50012, kTree = 50000, kGolemSum = 50000, kCells = 51000
};

// WA's score is SY's float 90008.
static void score4(float n) { w().setVarFloat(90008, w().varFloat(90008) + n); }

static void progress() {
	int p = byte_(kProgress) + 1;
	setByte(kProgress, p);
	if (p == 2) {
		show(kNorns, 0);
		accOff(kNorns);
		accOn(kNorns, 1);
		movOff(50103, 2, 2);
		movOn(50103, 3, 3);
	} else if (p == 4) {
		show(kNorns, 1);
		accOff(kNorns);
		accOn(kNorns, 2);
	}
}

static const char *byLanguage(const char *forItalianSpanishDutch, const char *forSwedish, const char *other) {
	int lang = g_engine->languageId();
	return lang == 4 || lang == 5 || lang == 7 ? forItalianSpanishDutch : lang == 6 ? forSwedish : other;
}

static void leafDone() { // the third step of the leaf (0x437d60 at 50203, 0x43a050 at 50201)
	hide(kLeafPlace, 1);
	show(kLeafPlace, 2);
	setByte(kLeafTask, 3);
	cin("1858");
	bag().add(kLeaf);
	score4(5);
	progress();
}

void enter(RingEngine *vm, int entry) {
	switch (entry) {
	case 0:
		bag().removeAll();
		bag().add(kBeam);
		bag().add(kGolem);
		cin("1880");
		cin("1881");
		if (Rotation *r = w().rotation(50001))
			r->setAlpha(320.0f);
		rotAct(50001);
		puz(50002);
		play(50001);
		break;
	case 10: // resumed after Erda: world 4's record "bru" (spec/bag.md)
		bag().removeAll();
		if (w().varByte(90020) == 0) {
			if (Rotation *r = w().rotation(dword(90024))) {
				rotAct(r->id);
				r->frozen = w().varByte(90028) != 0;
			}
		} else {
			puz(dword(90024));
		}
		if (!vm->loadWorldState("bru"))
			warning("Ring: Wrong Erda AS - WA -> Can not find file");
		break;
	case 999: { // a test entry (Q-0072)
		static const int objects[] = { kBeam, kRope, kGolem, kSword, kFlower, kApple, kLeaf, kBark };
		for (int o : objects)
			bag().add(o);
		w().setVarFloat(90008, 90.0f);
		show(1);
		setByte(kProgress, 4);
		puz(50100);
		break;
	}
	default:
		warning("Ring: WA entry %d is not implemented", entry);
		break;
	}
}

void onClick(RingEngine *vm, int object, int value) {
	int h = held();
	switch (object) {
	case kDesk:
		if (h) {
			drop();
		} else if (byte_(kProgress) == 4) {
			vm->rotSetRolTo(50108, 250.0f, 15.0f, 85.7f);
			cin("1849");
			puz(50100);
			stop(51006);
		} else if (byte_(kProgress) > 4) {
			cin("1850");
			if (Rotation *r = w().rotation(50601))
				r->setAlpha(250.0f);
			rotAct(50601);
			stop(51006);
		}
		break;
	case kAshes:
		if (h) {
			drop();
		} else if (byte_(kLetterHold) == 1) { // the letter burns
			w().pauseOnFrame(kDesk, 8, 50, 1000, 2);
			setByte(kProgress, 5);
			score4(3);
			show(kDesk, 8);
			hide(kDesk, 7);
		}
		break;
	case kLeafPlace:
		if (h == kSword) {
			cin("1855");
			score4(5);
			show(kLeafPlace, 0);
			setByte(kLeafTask, 1);
			accOff(kLeafPlace);
		}
		if (h)
			drop();
		break;
	case kSky:
		if (h == kConch) {
			if (!bag().has(kFeather)) {
				cin("1859");
			} else {
				cin("1856");
				if (byte_(kLeafTask) == 1) {
					hide(kLeafPlace, 0);
					show(kLeafPlace, 1);
					setByte(kLeafTask, 2);
					cin("1857");
					score4(5);
					accOff(kSky);
					if (byte_(kTaskDone) == 1)
						leafDone();
				}
			}
		}
		if (h)
			drop();
		break;
	case kTask:
		if (!h) {
			puz(50303);
			break;
		}
		if (h == kBeam) {
			show(kTask, 0);
			accOff(kTask);
			accOn(kBark);
			setByte(kTaskDone, 1);
			score4(5);
			play(51013, true);
			movOff(50301, 0, 1);
			movOn(50301, 2, 3);
			movOff(50302, 0, 2);
			movOn(50302, 3, 5);
			movOff(50303, 0, 0);
			movOn(50303, 1, 1);
			if (byte_(kConchState) < 2) {
				show(kConch, 3);
			} else {
				hide(kConch);
				show(kConch, 1);
				if (byte_(kConchState) == 3)
					show(kConch, 2);
			}
			show(kTask, 0);
			cin("1854");
			progress();
		}
		rotAct(50303);
		drop();
		break;
	case kConch:
		if (h) {
			if (h == kBeam && byte_(kConchState) == 0) {
				show(kConch, 0);
				setByte(kConchState, 1);
				cin("1852");
				rot(50304, 190.0f, 15.0f, 85.3f);
			} else if (h == kSword && byte_(kConchState) == 1) {
				bag().remove(kSword);
				vm->rotSetRolTo(50304, 145.0f, 9.0f, 85.7f);
				setByte(kConchState, 2);
				score4(5);
				movOff(50304, 0, 0);
				movOn(50304, 1, 1);
				accOn(kConch, 1);
				hide(kConch);
				vm->renderFrame();
				cin("1853");
				rot(50304, 190.0f, 15.0f, 85.3f);
				show(kConch, 1);
				if (byte_(kTaskDone) == 0)
					show(kConch, 4);
			}
			drop();
		} else if (value == 1 && byte_(kConchState) == 2) {
			setByte(kConchState, 3);
			show(kConch, 2);
			score4(5);
			accOff(kConch, 1);
			bag().add(kConch);
		} else if (value == 0) {
			if (byte_(kConchState) < 2) {
				puz(50304);
			} else {
				show(kConch, 5);
				accOff(kConch, 0);
				bag().add(kSword);
			}
		}
		break;
	case kBark:
		if (h) {
			drop();
		} else {
			bag().add(kBark);
			score4(5);
			accOff(kBark);
		}
		break;
	case kGolem:
		if (h == kGolem && value == 0) {
			bag().remove(kGolem);
			show(kGolem, 1);
			for (int p = kPart; p <= kPart + 6; p++)
				show(p, 0);
			for (int p = kPart; p <= kPart + 5; p++)
				accOn(p);
			accOn(kPart + 6, 1);
			accOff(kGolem, 0);
			movOff(50400);
			score4(2);
		}
		if (h)
			drop();
		break;
	case kFlower:
	case kSword:
	case kApple:
		if (h) {
			drop();
			break;
		}
		bag().add(object);
		accOff(object);
		if (object == kFlower) {
			hide(kFlower);
			score4(2);
		} else if (object == kSword) {
			score4(2);
			show(kItems, 0);
		} else {
			hide(kItems, 5);
			score4(3);
		}
		break;
	case kBranches: {
		static const int items[] = { kFlower, kApple, kLeaf, kBark }, units[] = { 1, 10, 100, 1000 };
		if (value >= 10 && value <= 13) {
			if (!h)
				puz(value + 50491);
			break;
		}
		if (value < 0 || value > 3)
			break;
		int tree = word(kTree);
		if (!h) {
			// The item is there, as coded (Q-0071).
			bool there = value == 0 ? tree % 10 == 1 : value == 1 ? tree % 100 >= 2 : value == 2 ? tree % 1000 >= 12 : tree % 10000 >= 112;
			if (there) {
				bag().add(items[value]);
				hide(kItems, value + 1);
				setWord(kTree, tree - units[value]);
				score4(-2);
			}
			break;
		}
		if (h == items[value]) {
			bag().remove(h);
			show(kItems, value + 1);
			setWord(kTree, tree + units[value]);
			score4(2);
		}
		if (word(kTree) == 1111) { // the tree done: the Rope
			setWord(kTree, 11111);
			show(kItems, 6);
			vm->renderFrame();
			cin("1851");
			rotAct(50502);
			bag().add(kRope);
			score4(3);
			progress();
		}
		drop();
		break;
	}
	case kSwitches:
		if (h) {
			drop();
		} else if (value >= 0 && value <= 3) {
			play(50018);
			w().pauseAnimations(kSwitches, 0, false);
			hide(kSwitches, 6);
			hide(kSwitches, 7);
			show(kSwitches, 1);
			bool on = byte_(kSwitch + value) == 0;
			setByte(kSwitch + value, on);
			if (on)
				show(kSwitches, 2 + value);
			else
				hide(kSwitches, 2 + value);
		}
		break;
	case kRopePlace:
		if (h == kRope && byte_(kProgress) == 6) { // the ending
			bag().remove(kRope);
			w().setVarFloat(90008, 100.0f);
			cin("1860");
			cin("1861");
			vm->plyCinMul(byLanguage("1863", "1864", "1862"));
			puz(51001);
			play(50021);
		}
		if (h)
			drop();
		break;
	case kNorns:
		if (h) {
			drop();
		} else {
			puz(50701);
			score4(5);
			play(50009);
		}
		break;
	default:
		break;
	}
}

// The grid: a piece's right cell (10 x column + row).
static int rightCell(int piece) {
	static const int cells[] = { 30, 61, 1, 33, 26, 46, 42 };
	return cells[piece - kPiece];
}

void onTake(RingEngine *vm, int object, int value) {
	int h = held();
	bool taken = false;
	if (object >= kInk && object <= kInkStylet) { // the desk
		if (!h) {
			if (value < 3 && !bag().has(object)) {
				bag().add(object);
				hide(kDesk, value);
				taken = true;
			} else if (value == 9 && word(50102) > 0 && !bag().has(kInkStylet)) {
				hide(kDesk, 6);
				bag().add(kInkStylet);
				taken = true;
			}
		} else if (value < 3 && h == object) { // put back
			bag().remove(h);
			show(kDesk, value);
		} else if (value == 9 && h == kInk && word(50101) == 0) {
			show(kDesk, 4);
			bag().remove(h);
			accOff(kInk, 0);
			setWord(50101, 101);
		} else if (value == 9 && h == kStylet && word(50101) > 0) {
			hide(kDesk, 4);
			show(kDesk, 6);
			accOff(kStylet, 0);
			bag().remove(h);
			setWord(50102, 103);
		} else if (value == 8 && h == kPaper) {
			show(kDesk, 3);
			bag().remove(h);
			accOff(kPaper, 0);
			setWord(50103, 102);
		} else if (value == 8 && h == kInkStylet && word(50103) > 0) { // the letter
			show(kDesk, 5);
			bag().remove(h);
			setWord(50105, 104);
			w().pauseOnFrame(kDesk, 7, 30, 10000, 2);
			show(kDesk, 7);
			score4(5);
		}
	} else if (object >= kPart && object <= kPart + 6) { // the golem's parts
		if (!h) {
			if (value == 0 && !bag().has(object)) {
				hide(object, 0);
				bag().add(object);
				taken = true;
			}
		} else {
			if (h == object && value >= 1) {
				show(object, 1);
				accOff(object, 0, 1);
				score4(1);
				setDword(kGolemSum, dword(kGolemSum) + value);
				if (dword(kGolemSum) == 7654321) { // all seven: the grid opens
					cin("1865");
					cin("1866");
					movOff(50401, 1, 1);
					movOn(50401, 2, 2);
					movOff(50402, 1, 1);
					movOn(50402, 2, 2);
					show(kGolem, 0);
					rot(50402, 180.0f, 0.3f, 85.3f);
				} else if (dword(kGolemSum) == 654321) {
					accOn(kPart + 6);
				}
			} else if (h >= kPart && h <= kPart + 6) {
				show(h, 0); // back on the golem
			}
			bag().remove(h);
		}
	} else if (object == kGrid && h >= kPiece && h <= kPiece + 6) { // a piece into a cell
		int c = value / 10, r = value % 10, i = 7 * r + c;
		w().movePictures(h, 0, 162 + 53 * c, 115 + 37 * r);
		setDword(kCells + value, h);
		accOn(h, i + 1);
		accOff(kGrid, i);
		show(h);
		if (rightCell(h) == value)
			score4(1);
		bag().remove(h);
		bool six = true;
		for (int p = kPiece; p <= kPiece + 5; p++)
			six = six && dword(kCells + rightCell(p)) == p;
		if (six && dword(kCells + rightCell(kPiece + 6)) == kPiece + 6) { // the Feather
			cin("1867");
			movOff(50402, 2, 2);
			bag().add(kFeather);
			score4(2);
			rot(50402, 180.0f, 0.3f, 85.3f);
			progress();
		} else if (six) {
			accOn(kPiece + 6, 0);
		}
	} else if (object >= kPiece && object <= kPiece + 6) { // a piece clicked
		if (!h) {
			if (value == 0) {
				hide(object);
				bag().add(object);
				accOff(object, 0);
				taken = true;
			} else {
				int cell = value - 1, i = 7 * (cell % 10) + cell / 10;
				if (rightCell(object) == cell)
					score4(-1);
				setDword(kCells + cell, 0);
				accOff(object, i + 1);
				accOn(kGrid, i);
				hide(object);
				bag().add(object);
				taken = true;
			}
		} else if (h >= kPiece && h <= kPiece + 6) { // the held piece back to its place
			w().restorePictures(h, 0);
			accOn(h, 0);
			show(h);
			bag().remove(h);
		}
	}
	if (!taken) {
		vm->keepHand();
		drop();
	}
}

void onHold(RingEngine *vm, int phase, int id) {
	if (id == 50003) { // the letter
		if (phase == 1) {
			setByte(kLetterHold, 1);
		} else if (phase == 2) { // no ashes in time: the desk is reset
			setByte(kLetterHold, 0);
			for (int p = 3; p <= 6; p++)
				hide(kDesk, p);
			for (int p = 0; p <= 2; p++)
				show(kDesk, p);
			for (int i = 50101; i <= 50105; i++)
				setWord(i, 0);
			for (int o = kInk; o <= kInkStylet; o++)
				accOn(o);
		}
	} else if (id == 50004 && phase == 2) { // the ashes
		hide(kDesk);
		if (Rotation *r = w().rotation(50601))
			r->setAlpha(250.0f);
		rotAct(50601);
		cin("1872");
	}
}

void onBeforeMove(RingEngine *vm, int from, int to, int kind) {
	if (kind != 0)
		return;
	if (from == 50001 && to == 50101 && !playing(51006))
		play(51006, true);
	if (from == 50304 && to == 50302) {
		if (byte_(kTaskDone) != 0)
			cin(byte_(kConchState) < 2 ? "1870" : "1871");
		else
			cin(byte_(kConchState) < 2 ? "1868" : "1869");
	}
	if (from == 50103 && to == 50701) {
		stop(51006);
		play(51007, true);
	}
}

void onAfterMove(RingEngine *vm, int to, int from, int kind) {
	if (kind != 0)
		return;
	auto area = [&](int place, int path, std::initializer_list<int> music) {
		if (to == place && from == path) {
			stop(51006);
			for (int m : music)
				play(m, true);
		} else if (to == path && from == place) {
			for (int m : music)
				stop(m);
			play(51006, true);
		}
	};
	area(50201, 50102, { 51002, 51012 });
	if (to == 50201 && byte_(kLeafTask) == 2 && byte_(kTaskDone) == 1)
		leafDone();
	if (to == 50301 && from == 50104) {
		stop(51006);
		play(51004, true);
		if (byte_(kTaskDone) > 0)
			play(51013, true);
	} else if (to == 50104 && from == 50301) {
		stop(51004);
		stop(51013);
		play(51006, true);
	}
	area(50401, 50105, { 51003, 51010 });
	area(50501, 50106, { 51001, 51011 });
	if (to == 50103 && from == 50701) {
		stop(51007);
		play(51006, true);
	}
	if (to == 50108 && from == 50601)
		play(51006, true);
}

void onAnimation(RingEngine *vm, int id, int frame) {
	if (id != 50001) // the switches' wheel
		return;
	bool s0 = byte_(kSwitch), s1 = byte_(kSwitch + 1), s2 = byte_(kSwitch + 2), s3 = byte_(kSwitch + 3);
	bool wrong = (s0 && s2 && (frame == 35 || frame == 12)) || (s1 && s3 && (frame == 49 || frame == 24));
	bool aligned = (s1 && s2 && (frame == 17 || frame == 41)) || (s0 && s3 && (frame == 4 || frame == 30));
	if (wrong) {
		w().pauseAnimations(kSwitches, 0, true);
		hide(kSwitches, 1);
		hide(kSwitches, 6);
		if (s0 && s2)
			hide(kSwitches, 7);
		else
			show(kSwitches, 7);
		stop(50018);
		if (byte_(kProgress) == 6) {
			score4(-5);
			setByte(kProgress, 5);
		}
	} else if (aligned) { // the beam aligned
		w().pauseAnimations(kSwitches, 0, true);
		hide(kSwitches, 1);
		hide(kSwitches, 7);
		show(kSwitches, 6);
		stop(50018);
		setByte(kProgress, 6);
		score4(5);
	}
}

void onSound(RingEngine *vm, int id, int type, int reason, int ended) {
	if (!ended)
		return;
	// "a -> p, n": PuzSetAct(p), play(n).
	static const struct {
		int sound, puzzle, next;
	} chain[] = {
		{ 50001, 50001, 50002 }, { 50009, 50703, 50010 }, { 50010, 50702, 50011 }, { 50011, 50703, 50012 },
		{ 50012, 50701, 50013 }, { 50013, 50703, 50014 }, { 50014, 50702, 50015 }, { 50015, 50703, 50016 }
	};
	for (const auto &c : chain) {
		if (c.sound == id) {
			puz(c.puzzle);
			play(c.next);
			return;
		}
	}
	if (id >= 50026 && id <= 50035) { // the ending's messages 51004..51013
		puz(51004 + (id - 50026));
		play(id + 1);
		return;
	}
	switch (id) {
	case 50002:
		rotAct(50001);
		play(50003);
		break;
	case 50003:
		play(51006, true);
		break;
	case 50016:
		accOff(kNorns);
		rotAct(50701);
		break;
	case 50021:
		vm->plyCinMul(byLanguage("1873", "1874", "1875"));
		puz(51002);
		play(50022);
		break;
	case 50022:
		vm->plyCinMul(byLanguage("1876", "1877", "1878"));
		puz(51003);
		play(51008, true);
		play(50026);
		break;
	case 50036: // back to the hub, world 4 done
		cin("1879");
		vm->timStoAll();
		snd().stopAll(0x400);
		AS::returnFromWorld(vm, 4);
		break;
	default:
		break;
	}
}

} // End of namespace WA
} // End of namespace Ring
