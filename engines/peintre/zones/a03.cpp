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
#include "peintre/shell.h"
#include "peintre/sound.h"

namespace Peintre {

// Zones 4, 7, 12, 13, 16, 18 (games/mission-sunlight/docs/a03.md).

namespace {

/** A voice waited for (a click cuts it), then a skippable movie; result 1. */
class VoiceMovie : public SlotRun {
public:
	VoiceMovie(Shell &s, const char *voice, uint movie) : SlotRun(s), _movie(movie) {
		_s.voice(voice);
	}

	bool run(int &result) override {
		result = 1;
		if (_step == 0) {
			if (_s.voiceWait()) {
				_s.movie(_movie, true);
				go(1);
			}
			return false;
		}
		return !_s.moviePlaying();
	}

private:
	uint _movie;
};

/** Zone 4, object 6: three places, then the moving target (0x40241b). */
class ThreePlaces : public SlotRun {
public:
	ThreePlaces(Shell &s) : SlotRun(s) {
		_s.movie(7, false);
	}

	bool run(int &result) override {
		switch (_step) {
		case 0:
			if (_s.moviePlaying())
				break;
			_s.background("a03_012a");
			_s.drawSlots();
			_s.voice("a03_012");
			go(1);
			break;
		case 1:
			if (_s.voiceWait())
				go(2);
			break;
		case 2: {
			int over = -1;
			for (int h = 0; h < 3; h++)
				if (kHotSpots[h].contains(_s.mouse()))
					over = h;
			_s.setCursor(over >= 0 ? kCursorButton : kCursorDefault);
			if (!_s.down() && after(250)) {
				_s.voice("A03_012b");
				_count = 0;
			}
			if (_aborted) {
				result = 0;
				return true;
			}
			if (over >= 0 && _s.clicked()) {
				_seen[over] = true;
				_h = over;
				_s.stopVoice();
				_s.movie(8 + over, false);
				go(3);
			}
			break;
		}
		case 3:
			if (!_s.moviePlaying())
				go(4);
			break;
		case 4:
			if (_count < 60) {
				_count++;
				break;
			}
			if (_s.down())
				break;
			if (_seen[0] && _seen[1] && _seen[2]) {
				const Common::String name = Common::String::format("A03_012%c", 'e' + _h);
				_s.background(name);
				_s.drawSlots();
				_target.load(name);
				_target.play(true);
				_s.sound("A03_012e", true);
				go(5);
			} else {
				_s.voice("A03_012c");
				_s.background("a03_012a");
				_s.drawSlots();
				go(2);
			}
			break;
		case 5: {
			if (_s.odd())
				_target.step();
			const int f = _target.next;
			const Common::Point m = _s.mouse();
			const bool over = (f >= 2 && f <= 13 && kRectsA[f - 2].contains(m)) ||
							  (f >= 16 && f <= 27 && kRectsB[f - 16].contains(m));
			_s.setCursor(over ? kCursorButton : kCursorDefault);
			if (_aborted) {
				_s.stopSound("A03_012e");
				result = 0;
				return true;
			}
			if (over && _s.clicked()) {
				_target.hide();
				_s.stopSound("A03_012e");
				_s.movie(14, false);
				go(6);
			}
			break;
		}
		case 6:
			if (_s.moviePlaying())
				break;
			_s.voice("A03_012d");
			go(7);
			break;
		case 7:
			if (_s.down() || _s.vm()->sound()->isStreamPlaying())
				break;
			go(8);
			break;
		default:
			if (after(60)) {
				result = 1;
				return true;
			}
			break;
		}
		return false;
	}

	void draw(Graphics::Surface &dst) override {
		_target.draw(dst);
	}

private:
	static const Common::Rect kHotSpots[3];
	static const Common::Rect kRectsA[12];
	static const Common::Rect kRectsB[12];

	bool _seen[3] = { false, false, false };
	uint _h = 0;
	Anim _target;
};

// Hot spots 0x4a4f88; rects A (0x4a4fb8 at f - 4, so f = 2, 3 read hot spots 1, 2) for
// f = 2..13 and B (0x4a5078) for f = 16..27.
const Common::Rect ThreePlaces::kHotSpots[3] = {
	rectWH(190, 76, 28, 48), rectWH(370, 76, 29, 45), rectWH(530, 70, 37, 46)
};
const Common::Rect ThreePlaces::kRectsA[12] = {
	rectWH(370, 76, 29, 45), rectWH(530, 70, 37, 46), rectWH(281, 24, 15, 10), rectWH(280, 24, 23, 18),
	rectWH(290, 24, 26, 29), rectWH(305, 35, 28, 28), rectWH(327, 47, 29, 25), rectWH(347, 53, 34, 23),
	rectWH(371, 54, 33, 21), rectWH(393, 48, 35, 20), rectWH(418, 36, 22, 24), rectWH(434, 24, 18, 24)
};
const Common::Rect ThreePlaces::kRectsB[12] = {
	rectWH(447, 24, 12, 8), rectWH(443, 24, 22, 13), rectWH(435, 24, 25, 19), rectWH(427, 28, 23, 28),
	rectWH(403, 39, 33, 27), rectWH(378, 47, 40, 28), rectWH(354, 53, 45, 24), rectWH(331, 47, 44, 29),
	rectWH(312, 37, 38, 34), rectWH(298, 28, 30, 32), rectWH(284, 24, 27, 24), rectWH(282, 24, 17, 13)
};

/** Zone 7, object 12: three hot spots in any order (0x402c20). */
class ThreeDoors : public SlotRun {
public:
	ThreeDoors(Shell &s) : SlotRun(s) {
		_s.background("A03_023a");
		_s.drawSlots();
		_s.voice("A03_023");
	}

	bool run(int &result) override {
		// Table 0x4a5198 by mask: background letter index L and movie M (39..50).
		static const int8 kL[7][3] = {
			{ 2, 3, 4 }, { -1, 13, 11 }, { 7, -1, 12 }, { -1, -1, 10 }, { 6, 5, -1 }, { -1, 9, -1 }, { 8, -1, -1 }
		};
		static const int8 kM[7][3] = {
			{ 39, 40, 41 }, { -1, 48, 47 }, { 45, -1, 44 }, { -1, -1, 50 }, { 43, 42, -1 }, { -1, 49, -1 }, { 46, -1, -1 }
		};
		static const Common::Rect kHotSpots[3] = {
			rectWH(597, 187, 39, 38), rectWH(597, 258, 39, 37), rectWH(597, 324, 39, 38)
		};
		switch (_step) {
		case 0:
			if (_s.voiceWait())
				go(1);
			break;
		case 1: {
			int over = -1;
			for (int h = 0; h < 3; h++)
				if (!(_mask & (1 << h)) && kHotSpots[h].contains(_s.mouse()))
					over = h;
			_s.setCursor(over >= 0 ? kCursorButton : kCursorDefault);
			if (_aborted) {
				result = 0;
				return true;
			}
			if (over >= 0 && _s.clicked()) {
				_h = over;
				go(2);
			}
			break;
		}
		case 2:
			if (_s.down())
				break;
			_s.movie(kM[_mask][_h], false);
			go(3);
			break;
		case 3: {
			if (_s.moviePlaying())
				break;
			const uint old = _mask;
			_mask |= 1 << _h;
			if (_mask == 7) {
				_s.background("A03_02");
				_s.drawSlots();
				_s.voice("A03_023e");
				go(4);
			} else {
				_s.background(Common::String::format("A03_023%c", 'a' + kL[old][_h]));
				_s.drawSlots();
				go(1);
			}
			break;
		}
		case 4:
			if (_s.voiceWait()) {
				_s.voice("A03_023f");
				go(5);
			}
			break;
		default:
			if (_s.voiceWait()) {
				result = 1;
				return true;
			}
			break;
		}
		return false;
	}

private:
	uint _mask = 0;
	uint _h = 0;
};

/** Zone 13, object 21: which piece fits (0x404378). */
class WhichPiece : public SlotRun {
public:
	WhichPiece(Shell &s) : SlotRun(s) {
		_dragBank.load("a03_042a");
		_rest.load("a03_042b");
		_s.movie(16, false);
	}

	bool run(int &result) override {
		static const Common::Rect kTarget = rectWH(379, 109, 48, 52);
		switch (_step) {
		case 0:
			if (_s.moviePlaying())
				break;
			showPieces();
			_s.voice("A03_042");
			go(1);
			break;
		case 1:
			if (_s.voiceWait()) {
				go(2);
				_count = 375;
			}
			break;
		case 2: {
			int over = -1;
			for (int i = 0; i < 3; i++)
				if (kPieces[i].pick.contains(_s.mouse()))
					over = i;
			_s.setCursor(over >= 0 ? kCursorTake : kCursorDefault);
			if (_aborted) {
				result = 0;
				return true;
			}
			if (++_count > 625) {
				_s.voice("a03_042b");
				_count = 0;
			}
			if (over >= 0 && _s.clicked()) {
				_i = over;
				_s.restore(spriteRect(_rest, _i, kPieces[_i].pos.x, kPieces[_i].pos.y));
				_grab = _s.mouse();
				_drag = true;
				_s.sound("ha_clic1");
				_s.setCursor(kCursorDrag);
				_step = 3;
			}
			break;
		}
		case 3:
			if (_s.down())
				break;
			_drag = false;
			_s.setCursor(kCursorDefault);
			if (kTarget.contains(dragPos())) {
				_s.sound("ha_clic2");
				_s.movie(17 + _i, false);
				go(4);
			} else {
				_rest.draw(_s.page(), _i, kPieces[_i].pos.x, kPieces[_i].pos.y);
				go(2);
			}
			break;
		case 4:
			if (_s.moviePlaying())
				break;
			if (_i == 1) {
				_s.sound("reussit");
				_s.background("A03_04");
				_s.drawSlots();
				_s.voice("A03_042c");
				go(5);
			} else {
				showPieces();
				go(2);
			}
			break;
		case 5:
			if (_s.voiceWait())
				go(6);
			break;
		default:
			if (after(50)) {
				result = 1;
				return true;
			}
			break;
		}
		return false;
	}

	void draw(Graphics::Surface &dst) override {
		if (_drag) {
			const Common::Point p = dragPos();
			_dragBank.draw(dst, _i, p.x, p.y);
		}
	}

private:
	struct Piece {
		Common::Point pos, drag;
		Common::Rect pick;
	};
	static const Piece kPieces[3];

	void showPieces() {
		_s.background("a03_042e");
		_s.drawSlots();
		for (uint i = 0; i < 3; i++)
			_rest.draw(_s.page(), i, kPieces[i].pos.x, kPieces[i].pos.y);
	}

	/** The reference point: the drag position moved with the cursor. */
	Common::Point dragPos() const {
		return Common::Point(kPieces[_i].drag.x + _s.mouse().x - _grab.x, kPieces[_i].drag.y + _s.mouse().y - _grab.y);
	}

	SpriteBank _dragBank, _rest;
	uint _i = 0;
	bool _drag = false;
	Common::Point _grab;
};

const WhichPiece::Piece WhichPiece::kPieces[3] = {
	{ Common::Point(135, 76), Common::Point(168, 109), rectWH(143, 85, 55, 52) },
	{ Common::Point(135, 185), Common::Point(169, 216), rectWH(143, 192, 55, 52) },
	{ Common::Point(135, 296), Common::Point(169, 327), rectWH(143, 302, 55, 52) }
};

/** Zone 16, object 26: six pieces on six places (0x405816). */
class SixPieces : public SlotRun {
public:
	SixPieces(Shell &s) : SlotRun(s) {
		_bank.load("a03_05");
		for (int i = 0; i < 6; i++)
			_at[i] = -1;
		_s.movie(51, true);
	}

	bool run(int &result) override {
		switch (_step) {
		case 0:
			if (_s.moviePlaying())
				break;
			_s.background("a03_05");
			_s.drawSlots();
			_s.voice("A03_05b");
			go(1);
			break;
		case 1:
			if (_s.voiceWait()) {
				_s.setCursor(kCursorBusy);
				go(2);
			}
			break;
		case 2:
			if (after(75)) {
				_s.background("a03_05e");
				_s.drawSlots();
				_s.movie(53, false);
				go(3);
			}
			break;
		case 3:
			if (_s.moviePlaying())
				break;
			// The background holds the empty board (158, 210, 322, 150).
			_s.background("a03_05e");
			_s.drawSlots();
			go(4);
			break;
		case 4:
			redraw();
			go(5);
			break;
		case 5: {
			int over = -1;
			for (int i = 0; i < 6; i++)
				if (_at[i] < 0 && kPieces[i].pick.contains(_s.mouse()))
					over = i;
			_s.setCursor(over >= 0 ? kCursorTake : kCursorDefault);
			if (_aborted) {
				result = 0;
				return true;
			}
			if (after(250)) {
				_s.voice("a03_05d");
				_count = 0;
			}
			if (over >= 0 && _s.clicked()) {
				_i = over;
				_s.restore(spriteRect(_bank, _i, kPieces[_i].home.x, kPieces[_i].home.y));
				_grab = _s.mouse();
				_drag = true;
				_s.sound("cf_clic1");
				_step = 6;
			}
			break;
		}
		case 6: {
			if (_s.down())
				break;
			_drag = false;
			const Common::Point ref = dragPos();
			int place = -1;
			for (int j = 0; j < 6 && place < 0; j++)
				if (kPieces[j].drop.contains(ref) && !taken(j))
					place = j;
			if (place >= 0) {
				_at[_i] = place;
				_bank.draw(_s.page(), _i, kPlaces[_i][place].x, kPlaces[_i][place].y);
				_s.sound("cf_clic2");
				bool all = true;
				for (int i = 0; i < 6; i++)
					all = all && _at[i] >= 0;
				if (all)
					go(7);
				else
					_step = 5;
			} else {
				_bank.draw(_s.page(), _i, kPieces[_i].home.x, kPieces[_i].home.y);
				_s.sound("cf_clic3");
				_step = 5;
			}
			break;
		}
		case 7: {
			if (!after(40))
				break;
			bool wrong = false;
			for (int i = 0; i < 6; i++) {
				if (_at[i] != i) {
					_at[i] = -1;
					wrong = true;
				}
			}
			if (!wrong) {
				_s.background("a03_05c");
				_s.drawSlots();
				_s.sound(_attempts < 2 ? "a03_05f" : _attempts < 4 ? "a03_05g" : "a03_05h");
				go(8);
			} else {
				_attempts++;
				_s.voice("a03_05e");
				go(4);
			}
			break;
		}
		default:
			if (after(50)) {
				result = 1;
				return true;
			}
			break;
		}
		return false;
	}

	void draw(Graphics::Surface &dst) override {
		if (_drag) {
			const Common::Point p = dragPos();
			_bank.draw(dst, _i, p.x, p.y);
		}
	}

private:
	struct Piece {
		Common::Point home;
		Common::Rect pick, drop;
	};
	static const Piece kPieces[6];
	static const Common::Point kPlaces[6][6];

	bool taken(int place) const {
		for (int i = 0; i < 6; i++)
			if (_at[i] == place)
				return true;
		return false;
	}

	/** The empty board, each unplaced piece at home, each placed piece at its place. */
	void redraw() {
		_s.restore(rectWH(158, 210, 322, 150));
		for (int i = 0; i < 6; i++) {
			_s.restore(spriteRect(_bank, i, kPieces[i].home.x, kPieces[i].home.y));
			if (_at[i] < 0)
				_bank.draw(_s.page(), i, kPieces[i].home.x, kPieces[i].home.y);
			else
				_bank.draw(_s.page(), i, kPlaces[i][_at[i]].x, kPlaces[i][_at[i]].y);
		}
	}

	Common::Point dragPos() const {
		return Common::Point(kPieces[_i].home.x + _s.mouse().x - _grab.x, kPieces[_i].home.y + _s.mouse().y - _grab.y);
	}

	SpriteBank _bank;
	int _at[6];
	uint _attempts = 0;
	uint _i = 0;
	bool _drag = false;
	Common::Point _grab;
};

// 0x4a4b88 (home and P), 0x4a4cd8 (pick rects, drop rect of place i).
const SixPieces::Piece SixPieces::kPieces[6] = {
	{ Common::Point(532, 129), rectWH(507, 114, 52, 36), rectWH(274, 294, 33, 38) },
	{ Common::Point(609, 127), rectWH(600, 112, 20, 27), rectWH(370, 276, 20, 28) },
	{ Common::Point(522, 216), rectWH(513, 204, 20, 32), rectWH(421, 258, 17, 30) },
	{ Common::Point(606, 217), rectWH(598, 206, 17, 25), rectWH(392, 243, 17, 23) },
	{ Common::Point(521, 312), rectWH(515, 294, 19, 26), rectWH(245, 268, 15, 27) },
	{ Common::Point(607, 315), rectWH(600, 293, 17, 26), rectWH(444, 257, 14, 26) }
};

const Common::Point SixPieces::kPlaces[6][6] = {
	{ Common::Point(290, 313), Common::Point(381, 288), Common::Point(430, 275), Common::Point(401, 254), Common::Point(253, 276), Common::Point(452, 259) },
	{ Common::Point(289, 316), Common::Point(380, 291), Common::Point(429, 278), Common::Point(400, 257), Common::Point(252, 279), Common::Point(451, 262) },
	{ Common::Point(289, 309), Common::Point(380, 284), Common::Point(429, 271), Common::Point(400, 250), Common::Point(252, 272), Common::Point(451, 255) },
	{ Common::Point(289, 313), Common::Point(380, 288), Common::Point(429, 275), Common::Point(400, 254), Common::Point(252, 276), Common::Point(451, 259) },
	{ Common::Point(286, 321), Common::Point(377, 296), Common::Point(426, 283), Common::Point(397, 262), Common::Point(249, 284), Common::Point(448, 267) },
	{ Common::Point(288, 324), Common::Point(379, 299), Common::Point(428, 286), Common::Point(399, 265), Common::Point(251, 287), Common::Point(450, 270) }
};

/** Zone 18, object 28: click at the right moment (0x405fa6). */
class RightMoment : public SlotRun {
public:
	RightMoment(Shell &s) : SlotRun(s) {
		for (uint k = 0; k < 4; k++)
			_scenes[k].load(Common::String::format("a03_06%c", 'b' + k));
		_s.movie(27, false);
	}

	bool run(int &result) override {
		static const int kWindows[4][2] = { { 23, 31 }, { 35, 41 }, { 36, 44 }, { 19, 25 } };
		static const Common::Rect kSpot = rectWH(293, 133, 124, 35);
		switch (_step) {
		case 0:
			if (_s.moviePlaying())
				break;
			_s.background("a03_06f");
			_s.drawSlots();
			_s.voice("a03_06b");
			go(1);
			break;
		case 1:
			if (_s.voiceWait())
				go(2);
			break;
		case 2:
			_s.sound(sceneSound());
			_scenes[_k].play();
			_waitRelease = false;
			go(3);
			break;
		case 3: {
			Anim &a = _scenes[_k];
			if (_s.odd())
				a.step();
			const bool over = a.next >= kWindows[_k][0] && a.next <= kWindows[_k][1] && kSpot.contains(_s.mouse());
			_s.setCursor(over ? kCursorButton : kCursorDefault);
			if (_aborted) {
				_s.stopSound(sceneSound());
				result = 0;
				return true;
			}
			if (_waitRelease && !_s.down())
				_waitRelease = false;
			if (over && _s.clicked() && !_waitRelease) {
				if (_k == 2) {
					_s.stopSound(sceneSound());
					a.hide();
					_s.background("a03_06");
					_s.drawSlots();
					go(4);
					break;
				}
				_s.voice(Common::String::format("a03_06%c", 'c' + _k));
				_waitRelease = true;
			}
			if (!a.playing) {
				_s.stopSound(sceneSound());
				a.hide();
				_k = (_k + 1) % 4;
				go(2);
			}
			break;
		}
		case 4:
			if (_s.down())
				break;
			_s.voice("a03_06e");
			go(5);
			break;
		case 5:
		case 6:
			if (_s.voiceWait()) {
				_s.voice(_step == 5 ? "a03_06g" : "a03_06h");
				go(_step + 1);
			}
			break;
		case 7:
			if (_s.voiceWait())
				go(8);
			break;
		default:
			if (after(60)) {
				result = 1;
				return true;
			}
			break;
		}
		return false;
	}

	void draw(Graphics::Surface &dst) override {
		_scenes[_k].draw(dst);
	}

private:
	Common::String sceneSound() const {
		return Common::String::format("a03_06%c", 'i' + _k);
	}

	Anim _scenes[4];
	uint _k = 0;
	bool _waitRelease = false;
};

} // End of anonymous namespace

SlotRun *placeA03(Shell &s, uint zone, uint object) {
	switch (object) {
	case 5:
		return new VoiceSlot(s, "A03_011");        // zone 4
	case 6:
		return new ThreePlaces(s);                 // zone 4
	case 7:
		return new MovieSlot(s, 15, true);         // zone 4, A03_013
	case 10:
		return new VoiceMovie(s, "A03_021", 30);   // zone 7
	case 11:
		return new VoiceMovie(s, "A03_022", 31);   // zone 7
	case 12:
		return new ThreeDoors(s);                  // zone 7
	case 19:
		return new MovieSlot(s, 62, true);         // zone 12, A03_03
	case 20:
		return new VoiceSlot(s, "A03_041");        // zone 13
	case 21:
		return new WhichPiece(s);                  // zone 13
	case 26:
		return new SixPieces(s);                   // zone 16
	case 28:
		return new RightMoment(s);                 // zone 18
	default:
		return nullptr;
	}
}

} // End of namespace Peintre
