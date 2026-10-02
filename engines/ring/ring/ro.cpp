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

// Zone RO: Alberich's world, the egg and the pipes (games/ring/docs/ro.md).

#include "common/textconsole.h"

#include "ring/resources.h"
#include "ring/ring/zone.h"
#include "ring/ring/zones.h"

namespace Ring {
namespace RO {

using namespace Api;

enum {
	kFirePower = 40000, kEgg = 40010, kTiles = 40011, kRing = 40012, kCrown = 40013, kLever = 40060,
	kDial = 40101, kPipe = 40201, kKeys = 40202, kCave = 40203,
	// Bytes, strings
	kFireUsed = 40000, kGrid = 40501, kDials = 40601, kTilesSolved = 40701, kDialsSolved = 40702,
	kCrownPlaced = 40703, kRingPlaced = 40801, kPipesSolved = 40802, kLeverByte = 40804, kTileSize = 40805,
	kSwitch = 40200, kPlayed = 40901, kKeysPlayed = 40902, kSwitchFrame = 40901, kPipeFrame = 40911
};

static int s_lever = 0;     ///< 0x4a1cc0: the lever's last position
static float s_pos = 0.0f;  ///< 0x4a1cd4: the dragged position

// RO's score is SY's float 90006 (world 2's), set to fixed values.
static void setScore(float v) { w().setVarFloat(90006, v); }
static void vol(int sound, int v) { snd().setVolume(sound, v); }

// One picture per frame from `from` to `to` (Escape held ends it early).
static void stepTo(int object, int from, int to) {
	for (int i = from; ; i += from < to ? 1 : -1) {
		hide(object);
		show(object, i);
		g_engine->renderFrame();
		if (i == to || g_engine->escapePressed())
			break;
	}
}

// A tile cell: the tile there moves to the first empty neighbour (up, down, left, right)
// with its picture; returns the direction taken (0 none, -10, 10, -1, 1).
static int moveTile(int cell, bool sound) {
	int t = byte_(kGrid + cell), dir = 0;
	if (t != 0 && t != 99) {
		for (int d : { -10, 10, -1, 1 }) {
			if (byte_(kGrid + cell + d) == 0) {
				dir = d;
				break;
			}
		}
	}
	if (dir) {
		setByte(kGrid + cell + dir, t);
		setByte(kGrid + cell, 0);
		if (PuzzleImage *img = w().image(kTiles, t / 10 - 2, t % 10 - 1)) {
			int step = byte_(kTileSize);
			if (dir == -10 || dir == 10)
				img->y += dir > 0 ? step : -step;
			else
				img->x += dir * step;
		}
	}
	if (sound) {
		vol(40103, 80 + rnd(20));
		play(40103);
	}
	return dir;
}

void enter(RingEngine *vm, int entry) {
	switch (entry) {
	case 0: // from N2
		rot(40000, 0.0f, 0.0f, 85.3f, false);
		bag().remove(70000);
		cin("1506");
		bag().add(kFirePower);
		bag().add(kRing);
		bag().add(kCrown);
		puz(40100);
		play(40700);
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
			warning("Ring: Wrong Erda AS - RO -> Can not find file");
		break;
	default:
		warning("Ring: RO entry %d is not implemented", entry);
		break;
	}
}

void onClick(RingEngine *vm, int object, int value) {
	int h = held();
	if (object == kEgg) {
		if (!h) {
			if (value != 0)
				return;
			if (byte_(kFireUsed) == 0) {
				puz(40010);
				return;
			}
			int a = byte_(kTilesSolved), b = byte_(kDialsSolved), c = byte_(kCrownPlaced);
			if (!a && !b && !c) {
				puz(40011);
				show(kTiles);
				accOn(kTiles);
			}
			if (a == 1 && !b && !c)
				puz(40013);
			if (a == 1 && b == 1 && !c)
				puz(40012);
			return;
		}
		if (value == 0 && h == kFirePower && byte_(kFireUsed) == 0) {
			cin("1780");
			setScore(51.8f);
			puz(40011);
			vm->renderFrame();
			if (byte_(kTilesSolved) == 0) // shuffled by 2000 random moves
				for (int i = 0; i < 2000; i++)
					moveTile((rnd(4) + 2) * 10 + rnd(4) + 1, false);
			vm->renderFrame();
			setByte(kFireUsed, 1);
		} else if (value == 2 && h == kRing) {
			show(kEgg, 0);
			bag().remove(kRing);
			setByte(kRingPlaced, 1);
		} else if (value == 2 && h == kCrown && byte_(kRingPlaced) == 1) {
			hide(kEgg, 0);
			cin("1781");
			setScore(78.6f);
			bag().remove(kCrown);
			setByte(kCrownPlaced, 1);
			puz(40103);
			stop(40002);
			puz(40101);
			play(40706);
		}
		drop();
	} else if (object == kTiles) {
		if (h) {
			drop();
			return;
		}
		int dir = moveTile(value, true);
		bool solved = value == 12 && dir == 10;
		for (int r = 2, last = -1; solved && r <= 5; r++) {
			for (int c = 1; c <= 4 && solved; c++) {
				int t = byte_(kGrid + 10 * r + c);
				solved = t >= last;
				last = t;
			}
		}
		if (solved) {
			vm->renderFrame();
			hide(kTiles);
			accOff(kTiles);
			accOff(kEgg, 1);
			accOn(kEgg, 2);
			setByte(kTilesSolved, 1);
			cin("1782");
			setScore(64.3f);
			puz(40013);
			for (int d = 0; d < 5; d++)
				show(kDial + d, 0);
			vm->timSta(0, 50);
			vm->timSta(1, 30);
		}
	} else if (object == kPipe) {
		if (h) {
			drop();
			return;
		}
		int p = value;
		accOff(kLever);
		movOff(40060, 0, 0);
		if (s_lever / 10 != p + 1) { // the pipe's switch
			setByte(kSwitch + p, byte_(kSwitch + p) == 0);
			show(kPipe, p + 7);
			vol(40602, 80 + rnd(20));
			play(40602);
		} else { // the pipe plays
			show(kPipe, p);
			Common::String s = w().varString(kPlayed);
			w().setVarString(kPlayed, s.substr(s.size() >= 6 ? s.size() - 6 : 0) + (char)('0' + p));
		}
	}
}

void onButtonDown(RingEngine *vm, int object, int value) {
	if (object != kKeys)
		return;
	if (held()) {
		drop();
		return;
	}
	int k = value;
	for (int i = 1; i <= 15; i++)
		hide(kKeys, i);
	show(kKeys, k + 1);
	play(40500 + k);
	if (k >= 7) {
		Common::String s = w().varString(kKeysPlayed);
		s = s.substr(s.size() >= 7 ? s.size() - 7 : 0) + (char)('0' + k - 7);
		w().setVarString(kKeysPlayed, s);
		if (s == "01276534" || s == "01476534" || s == "01276532" || s == "01476532") {
			setScore(100.0f);
			play(40603);
		}
	}
}

// The release: to the nearest multiple of 10, one picture per frame.
static int settle(int object) {
	int n = (int)s_pos, t = (int)(s_pos * 0.1f) * 10;
	if (t + 5 < s_pos)
		t += 10;
	stepTo(object, n, t);
	return t;
}

void onDrag(RingEngine *vm, int object, int value, int phase) {
	Drag &d = vm->drag();
	int ddx = ABS(d.current.x - d.previous.x), ddy = ABS(d.current.y - d.previous.y);
	if (object == kLever) {
		if (phase == 1) {
			d.mode = 2;
			d.limit = Common::Rect(0, 0, 640, 480);
			play(40102, true);
			s_pos = byte_(kLeverByte);
		} else if (phase == 3 && d.current.x != d.previous.x) {
			if (held()) {
				drop();
				return;
			}
			if (!playing(40102))
				play(40102, true);
			vol(40102, ddx / 2 + 80);
			s_pos += d.current.x < d.previous.x ? ddx / 6.0f : -ddx / 6.0f;
			s_pos = CLIP(s_pos, 0.0f, 71.0f);
			hide(kLever);
			show(kLever, (int)s_pos);
		} else if (phase == 2) {
			stop(40102);
			int t = settle(kLever);
			accOff(kLever);
			accOn(kLever, t / 10);
			setByte(kLeverByte, t);
			s_lever = t;
		}
	} else if (object == kDial) {
		int dialObject = kDial + value, byteId = kDials + value;
		if (phase == 1) {
			s_pos = byte_(byteId);
			d.mode = 2;
			d.limit = Common::Rect(0, 0, 640, 480);
			play(40102, true);
		} else if (phase == 3 && d.current.y != d.previous.y) {
			if (held()) {
				drop();
				return;
			}
			if (!playing(40102))
				play(40102, true);
			vol(40102, ddy / 2 + 80);
			s_pos += d.current.y > d.previous.y ? ddy : -ddy;
			if (s_pos < 0.0f)
				s_pos = 97.0f;
			if (s_pos > 97.0f)
				s_pos = 0.0f;
			hide(dialObject);
			show(dialObject, (int)s_pos);
		} else if (phase == 2) {
			int t = settle(dialObject);
			stop(40102);
			setByte(byteId, t);
			int last = byte_(kDials + 4);
			if (byte_(kDials) == 10 && byte_(kDials + 1) == 90 && byte_(kDials + 2) == 60 && byte_(kDials + 3) == 50 &&
				(last == 50 || last == 40)) {
				setByte(kDialsSolved, 1);
				for (int i = 0; i < 5; i++) {
					hide(kDial + i);
					accOff(kDial + i);
				}
				cin(last == 50 ? "1783" : "1784");
				setScore(75.0f);
				puz(40012);
			}
		}
	}
}

void onBeforeMove(RingEngine *vm, int from, int to, int kind) {
	if (kind != 2 || (from != 40060 && to != 40005))
		return;
	// Leaving the pipe room: the lever back to 0, the keyboard and the pipes reset.
	vol(40102, 100);
	play(40102, true);
	for (int v = byte_(kLeverByte); ; v--) {
		hide(kLever);
		show(kLever, v);
		vm->renderFrame();
		if (v <= 0 || vm->escapePressed())
			break;
	}
	setByte(kLeverByte, 0);
	stop(40102);
	if (byte_(kPipesSolved) == 1)
		cin("1785");
	accOff(kLever);
	accOn(kLever, 0);
	accOn(kPipe);
	accOff(kKeys);
	hide(kLever);
	hide(kPipe);
	hide(kKeys);
	setByte(kPipesSolved, 0);
	w().setVarString(kKeysPlayed, "00000000");
	s_lever = 0;
}

void onAfterMove(RingEngine *vm, int to, int from, int kind) {
	if (kind == 1) {
		if (from == 40000 || to == 40010)
			setByte(kLeverByte, 0); // Q-0060
		if (from == 40005 || to == 40060) {
			play(40003, true);
			vol(40003, 88);
		}
	} else if (kind == 2 && (from == 40060 || to == 40005)) {
		vol(40003, 82);
	}
}

void onTimer(RingEngine *vm, int id) {
	// Dials 0 and 1 turn by themselves to 10 and 90.
	if (id == 0 || id == 1) {
		int counter = id == 0 ? 40806 : 40807, target = id == 0 ? 10 : 90;
		int v = byte_(counter) + 1;
		setByte(counter, v);
		show(kDial + id, v);
		if (v >= target) {
			vm->timSto(id);
			setByte(kDials + id, target);
		}
	}
}

void onAnimation(RingEngine *vm, int id, int frame) {
	if (id >= 40100 && id <= 40106) { // pipe p playing
		int p = id - 40100;
		setByte(kPipeFrame + p, frame);
		if (frame == 1) {
			accOff(kLever);
			accOff(kPipe);
			vol(40600, 80 + rnd(20));
			play(40600);
		} else if (frame == 30) {
			vol(40601, 80 + rnd(20));
			play(40601);
			play(byte_(kSwitch + p) != 0 ? 40200 + p : 40300 + p);
		} else if (frame == 70) {
			accOn(kLever, p + 1);
			accOn(kPipe);
			movOn(40060, 0, 0);
			int sum = 0;
			for (int i = 0; i < 7; i++)
				sum += byte_(kSwitch + i);
			if (p == 0 && w().varString(kPlayed) == "6543210" && sum == 7) { // the pipes solved
				setByte(kPipesSolved, 1);
				w().setVarString(kPlayed, "0000000");
				play(40400);
				setScore(85.7f);
				accOff(kLever);
				accOff(kPipe);
				stepTo(kLever, 10, 1);
			}
		}
	} else if (id >= 40201 && id <= 40207) { // pipe p's switch
		setByte(kSwitchFrame + (id - 40201), frame);
		bool done = true;
		for (int i = 0; i < 7; i++)
			done = done && byte_(kSwitchFrame + i) == 26 && byte_(kPipeFrame + i) == 70;
		if (done) {
			accOn(kLever, byte_(kLeverByte) / 10);
			movOn(40060, 0, 0);
		}
	}
}

void onSound(RingEngine *vm, int id, int type, int reason, int ended) {
	if (!ended)
		return;
	// "a -> p, n": PuzSetAct(p), play(n).
	static const struct {
		int sound, puzzle, next;
	} chain[] = {
		{ 40700, 40101, 40701 }, { 40706, 40102, 40707 }, { 40707, 40101, 40708 }, { 40708, 40104, 40709 },
		{ 40709, 40102, 40710 }, { 40710, 40101, 40711 }, { 40711, 40103, 40702 }, { 40702, 40102, 40703 },
		{ 40703, 40103, 40704 }
	};
	for (const auto &c : chain) {
		if (c.sound == id) {
			puz(c.puzzle);
			play(c.next);
			return;
		}
	}
	switch (id) {
	case 40701:
		rotAct(40000);
		break;
	case 40704: { // the end of part 1: the cave changes, the way to the pipes opens
		snd().stopAll(0x400);
		cin("1788");
		s_lever = 0;
		for (int r = 40000; r <= 40004; r++)
			vm->setSoundItem(r, 40002, false);
		for (int r = 40000; r <= 40005; r++) {
			vm->setSoundItem(r, 40001, false);
			vm->setSoundItem(r, 40604, true);
		}
		vm->setSoundItem(40060, 40604, true);
		show(kCave);
		movOff(40000, 1, 1);
		movOn(40004, 0, 0);
		rotAct(40000);
		static const struct {
			int rotation, index;
			const char *ride;
		} rides[] = { { 40000, 0, "ro0102" }, { 40001, 2, "ro0201" }, { 40001, 3, "ro0203" }, { 40002, 0, "ro0302" },
					  { 40003, 0, "ro0402" }, { 40004, 0, "1796" }, { 40004, 1, "ro0502" } };
		for (const auto &r : rides)
			w().setRide(r.rotation, r.index, r.ride);
		break;
	}
	case 40400: // the pipes solved: the keyboard
		cin("1787");
		show(kKeys, 0);
		accOn(kKeys);
		break;
	case 40603: // the tune: back to the hub, world 2 done
		stop(40003);
		vm->timStoAll();
		cin("1786");
		AS::returnFromWorld(vm, 2);
		break;
	default:
		break;
	}
}

} // End of namespace RO
} // End of namespace Ring
