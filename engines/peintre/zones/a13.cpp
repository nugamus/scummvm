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

#include "peintre/shell.h"

namespace Peintre {

// Zones 5, 6, 8, 9, 10, 11, 14, 15, 17 (games/mission-sunlight/docs/a13.md).

namespace {

/** Zone 8, object 13: six rounds of two choices (0x4035fb). */
class SixRounds : public SlotRun {
public:
	SixRounds(Shell &s) : SlotRun(s) {
		_s.voice("A13_031");
	}

	bool run(int &result) override {
		static const Common::Rect kChoices[2] = { rectWH(122, 39, 217, 170), rectWH(122, 224, 217, 170) };
		static const uint kAnswers[6] = { 1, 1, 0, 0, 0, 1 };
		switch (_step) {
		case 0:
			if (!_s.voiceWait())
				break;
			_s.background("a13_031");
			_s.drawSlots();
			_s.voice("a13_031b");
			go(1);
			break;
		case 1:
			if (_s.voiceWait())
				go(2);
			break;
		case 2:
			_second.hide();
			_first.load(Common::String::format("a13_031%c", 'a' + 2 * _r));
			_second.load(Common::String::format("a13_031%c", 'b' + 2 * _r));
			_first.play();
			_s.setCursor(kCursorBusy);
			_s.sound("cc_clic4");
			go(3);
			break;
		case 3:
			if (_s.odd())
				_first.step();
			if (!_first.playing)
				go(4);
			break;
		case 4: {
			int over = -1;
			for (int c = 0; c < 2; c++)
				if (kChoices[c].contains(_s.mouse()))
					over = c;
			_s.setCursor(over >= 0 ? kCursorButton : kCursorDefault);
			if (after(250)) {
				_s.voice("a13_031c");
				_count = 0;
			}
			if (_aborted) {
				result = 0;
				return true;
			}
			if (over >= 0 && _s.clicked()) {
				_c = over;
				_first.hide();
				_second.play();
				_s.sound("cc_clic1");
				go(5);
			}
			break;
		}
		case 5:
			if (_s.odd()) {
				_second.step();
				// The answers only choose the sound: the object is always accepted.
				if (_second.frame == 15)
					_s.sound(_c == kAnswers[_r] ? "cc_clic2" : "pt_clic3");
			}
			if (_second.playing)
				break;
			if (++_r < 6)
				go(2);
			else
				go(6);
			break;
		case 6:
			if (!_s.down())
				go(7);
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
		_first.draw(dst);
		_second.draw(dst);
	}

private:
	Anim _first, _second;
	uint _r = 0, _c = 0;
};

/** Zone 8, object 14: join the six pairs (0x403a1a). */
class JoinPairs : public SlotRun {
public:
	JoinPairs(Shell &s) : SlotRun(s) {
		// The clean picture (110, 29, 482, 386) is the background's.
		_s.background("a13_032c");
		_s.drawSlots();
		_bank.load("a13_032");
		for (int e = 0; e < 6; e++)
			drawElement(e);
		_s.voice("a13_032");
	}

	bool run(int &result) override {
		switch (_step) {
		case 0:
			if (_s.voiceWait())
				go(1);
			break;
		case 1: {
			if (_aborted) {
				result = 0;
				return true;
			}
			_hint += 2;
			if (_hint > 625) {
				_s.voice("A13_032b");
				_hint = 0;
			}
			if (_waitRelease) {
				_waitRelease = _s.down();
				break;
			}
			if (!_s.clicked())
				break;
			_waitRelease = true;
			int e, h;
			if (!hitTest(e, h))
				break;
			if (e == _sel && h != _selHalf) {
				_s.sound("cc1_clic");
				go(2);
			} else if (e == _sel) {
				_sel = -1;
				drawElement(e);
			} else {
				const int prev = _sel;
				_sel = e;
				_selHalf = h;
				if (prev >= 0)
					drawElement(prev);
				drawElement(e);
			}
			break;
		}
		case 2:
			// 25 ticks: the selected half follows the tick parity.
			_selHalf = _s.odd();
			drawElement(_sel);
			if (!after(25))
				break;
			_s.stopSound("cc1_clic");
			_joined[_sel] = true;
			_sel = -1;
			_s.restore(rectWH(110, 29, 482, 386));
			for (int e = 0; e < 6; e++)
				drawElement(e);
			if (_joined[0] && _joined[1] && _joined[2] && _joined[3] && _joined[4] && _joined[5]) {
				_s.sound("reussit");
				go(3);
			} else {
				go(1);
			}
			break;
		case 3:
			if (after(50)) {
				_s.voice("A13_032c");
				go(4);
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
	static const Common::Point kPos[6][2];
	static const Common::Rect kRects[6][2];

	/** 0x403215: the first half under the cursor of an element not joined. */
	bool hitTest(int &e, int &h) const {
		const Common::Point m = _s.mouse();
		for (e = 0; e < 6; e++) {
			if (_joined[e])
				continue;
			for (h = 0; h < 2; h++) {
				if (!kRects[e][h].contains(m))
					continue;
				if (e == 4 && h == 0 && !_joined[5] && rectWH(120, 206, 77, 135).contains(m))
					e = 5;
				return true;
			}
		}
		return false;
	}

	/** 0x402e76: element e, as selected (half _selHalf) or plain. */
	void drawElement(int e) {
		if (e < 0 || _joined[e])
			return;
		const int s = _selHalf;
		int f1, f2, f3 = -1;
		if (e < 4) {
			f1 = _sel == e ? (1 - s) * 14 + 2 * e : 2 * e;
			f2 = _sel == e ? s * 14 + 2 * e + 1 : 2 * e + 1;
		} else if (e == 4) {
			if (_sel == 4) {
				f1 = (1 - s) * 14 + 8;
				f2 = s * 14 + 9;
				f3 = _joined[5] ? -1 : (1 - s) + 10;   // without × 14, as in the code
			} else if (_sel == 5) {
				f1 = (1 - s) * 14 + 8;
				f2 = s * 14 + 9;
				f3 = (1 - s) * 14 + 10;
			} else {
				f1 = 8;
				f2 = 9;
				f3 = _joined[5] ? -1 : 10;
			}
		} else {
			if (_sel == 5) {
				f1 = _joined[4] ? (1 - s) * 14 + 12 : (1 - s) * 14 + 10;
				f2 = s * 14 + 13;
			} else if (_sel == 4) {
				f1 = (1 - s) + 10;
				f2 = s * 14 + 13;
			} else {
				f1 = _joined[4] ? 12 : 10;
				f2 = 13;
			}
		}
		_bank.draw(_s.page(), f1, kPos[e][0].x, kPos[e][0].y);
		_bank.draw(_s.page(), f2, kPos[e][1].x, kPos[e][1].y);
		if (f3 >= 0)
			_bank.draw(_s.page(), f3, 118, 184);
	}

	SpriteBank _bank;
	bool _joined[6] = { false, false, false, false, false, false };
	int _sel = -1, _selHalf = 0;
	uint _hint = 0;
	bool _waitRelease = false;
};

const Common::Point JoinPairs::kPos[6][2] = {
	{ Common::Point(436, 29), Common::Point(496, 29) }, { Common::Point(454, 103), Common::Point(486, 103) },
	{ Common::Point(364, 146), Common::Point(408, 145) }, { Common::Point(256, 154), Common::Point(263, 152) },
	{ Common::Point(118, 29), Common::Point(554, 29) }, { Common::Point(118, 184), Common::Point(304, 155) }
};

const Common::Rect JoinPairs::kRects[6][2] = {
	{ rectWH(447, 34, 35, 54), rectWH(502, 28, 34, 51) }, { rectWH(459, 106, 20, 35), rectWH(494, 110, 43, 32) },
	{ rectWH(376, 154, 31, 20), rectWH(416, 149, 28, 19) }, { rectWH(253, 153, 9, 18), rectWH(262, 153, 8, 16) },
	{ rectWH(119, 29, 41, 256), rectWH(557, 29, 33, 285) }, { rectWH(120, 206, 77, 135), rectWH(305, 172, 41, 71) }
};

/** Zone 10, object 17: four rects, the fourth is right (0x403f04). */
class FourRects : public SlotRun {
public:
	FourRects(Shell &s) : SlotRun(s) {
		_s.background("a13_052a");
		_s.drawSlots();
		_s.movie(22, false);
	}

	bool run(int &result) override {
		static const Common::Rect kRects[4] = {
			rectWH(255, 76, 112, 39), rectWH(255, 161, 112, 39), rectWH(256, 255, 112, 39), rectWH(255, 346, 112, 39)
		};
		switch (_step) {
		case 0:
			if (_s.moviePlaying())
				break;
			_s.background("a13_052a");
			_s.drawSlots();
			go(1);
			break;
		case 1: {
			int over = -1;
			for (int i = 0; i < 4; i++)
				if (kRects[i].contains(_s.mouse()))
					over = i;
			_s.setCursor(over >= 0 ? kCursorButton : kCursorDefault);
			if (_aborted) {
				_s.stopVoice();
				result = 0;
				return true;
			}
			if (over >= 0 && _s.clicked()) {
				_i = over;
				go(2);
			}
			break;
		}
		case 2:
			if (_s.down())
				break;
			_s.movie(23 + _i, false);
			go(3);
			break;
		case 3:
			if (_s.moviePlaying())
				break;
			if (_i == 3) {
				_s.background("A13_05");
				_s.drawSlots();
				_s.voice("A13_052f");
				go(4);
			} else {
				_s.background("a13_052a");
				_s.drawSlots();
				go(1);
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
	uint _i = 0;
};

/** Zone 15, object 24: the sixteen-piece picture (0x404bde). */
class SixteenPieces : public SlotRun {
public:
	SixteenPieces(Shell &s) : SlotRun(s) {
		_bank.load("A13_061");
		for (uint k = 0; k < 16; k++)
			_stack.push_back(kOrder[k]);
		_s.voice("a13_061");
	}

	bool run(int &result) override {
		switch (_step) {
		case 0:
			if (!_s.voiceWait())
				break;
			_s.movie(55, false);
			go(1);
			break;
		case 1:
			if (_s.moviePlaying())
				break;
			_s.movie(56, false);
			go(2);
			break;
		case 2:
			if (_s.moviePlaying())
				break;
			// The background holds the empty board (156, 29, 328, 400).
			_s.background("a13_061a");
			_s.drawSlots();
			redraw();
			go(3);
			break;
		case 3: {
			const int top = _stack.empty() ? -1 : (int)_stack.back();
			const bool overTop = top >= 0 && spriteRect(_bank, top, kStack[top].x, kStack[top].y).contains(_s.mouse());
			int board = -1;
			for (int k = (int)_board.size() - 1; k >= 0 && board < 0; k--)
				if (spriteRect(_bank, _board[k].p, _board[k].pos.x, _board[k].pos.y).contains(_s.mouse()))
					board = k;
			_s.setCursor(overTop || board >= 0 ? kCursorTake : kCursorDefault);
			if (_aborted) {
				result = 0;
				return true;
			}
			if (!_s.clicked())
				break;
			_grab = _s.mouse();
			if (overTop) {
				_p = top;
				_from = kStack[top];
				_stack.pop_back();
				_fromStack = true;
			} else if (board >= 0) {
				_p = _board[board].p;
				_from = _board[board].pos;
				_board.remove_at(board);
				_fromStack = false;
			} else {
				break;
			}
			redraw();
			_drag = true;
			_s.sound("pa_clic1");
			go(4);
			break;
		}
		case 4: {
			if (_s.down())
				break;
			_drag = false;
			const Common::Point q = dragPos();
			const Common::Point t = kTarget[_p];
			bool snapped = false;
			if (q.x >= t.x - 4 && q.x < t.x + 4 && q.y >= t.y - 4 && q.y < t.y + 4) {
				_board.push_back(Placed(_p, t));
				_s.sound("pa_clic2");
				snapped = true;
			} else if (Common::Rect(156, 29, 484, 429).contains(spriteRect(_bank, _p, q.x, q.y))) {
				_board.push_back(Placed(_p, q));
				if (!_fromStack)
					_s.sound("pa_clic3");
			} else {
				if (_fromStack)
					_stack.push_back(_p);
				else
					_board.push_back(Placed(_p, _from));
				_s.sound("pa_clic3");
			}
			redraw();
			if (snapped && _stack.empty() && allRight()) {
				_s.sound("reussit");
				go(5);
			} else {
				go(3);
			}
			break;
		}
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
		if (_drag) {
			const Common::Point q = dragPos();
			_bank.draw(dst, _p, q.x, q.y);
		}
	}

private:
	struct Placed {
		uint p;
		Common::Point pos;
		Placed() : p(0) {}
		Placed(uint piece, const Common::Point &at) : p(piece), pos(at) {}
	};
	static const uint kOrder[16];
	static const Common::Point kStack[16];
	static const Common::Point kTarget[16];

	Common::Point dragPos() const {
		return Common::Point(_from.x + _s.mouse().x - _grab.x, _from.y + _s.mouse().y - _grab.y);
	}

	bool allRight() const {
		if (_board.size() != 16)
			return false;
		for (const Placed &b : _board)
			if (b.pos != kTarget[b.p])
				return false;
		return true;
	}

	/** The empty board and the stack's places, then the stack bottom up and the board in order. */
	void redraw() {
		_s.restore(rectWH(156, 29, 328, 400));
		for (uint p = 0; p < 16; p++)
			_s.restore(spriteRect(_bank, p, kStack[p].x, kStack[p].y));
		for (uint p : _stack)
			_bank.draw(_s.page(), p, kStack[p].x, kStack[p].y);
		for (const Placed &b : _board)
			_bank.draw(_s.page(), b.p, b.pos.x, b.pos.y);
	}

	SpriteBank _bank;
	Common::Array<uint> _stack;     // bottom first, the top is the last
	Common::Array<Placed> _board;   // in the order placed
	uint _p = 0;
	bool _fromStack = false, _drag = false;
	Common::Point _from, _grab;
};

const uint SixteenPieces::kOrder[16] = { 15, 3, 13, 12, 2, 14, 11, 9, 10, 7, 8, 6, 5, 4, 1, 0 };

const Common::Point SixteenPieces::kStack[16] = {
	Common::Point(575, 348), Common::Point(576, 329), Common::Point(577, 157), Common::Point(577, 118),
	Common::Point(577, 278), Common::Point(577, 262), Common::Point(577, 212), Common::Point(577, 232),
	Common::Point(580, 288), Common::Point(577, 361), Common::Point(576, 355), Common::Point(588, 184),
	Common::Point(567, 345), Common::Point(577, 164), Common::Point(577, 170), Common::Point(577, 137)
};

const Common::Point SixteenPieces::kTarget[16] = {
	Common::Point(205, 87), Common::Point(311, 71), Common::Point(201, 330), Common::Point(340, 369),
	Common::Point(425, 96), Common::Point(214, 207), Common::Point(263, 141), Common::Point(336, 158),
	Common::Point(425, 152), Common::Point(429, 325), Common::Point(253, 384), Common::Point(413, 243),
	Common::Point(188, 384), Common::Point(296, 302), Common::Point(289, 244), Common::Point(429, 378)
};

} // End of anonymous namespace

SlotRun *placeA13(Shell &s, uint zone, uint object) {
	switch (object) {
	case 8:
		return new MovieSlot(s, 21, true);   // zone 5, A13_08
	case 9:
		return new MovieSlot(s, 61, true);   // zone 6, A13_04
	case 13:
		return new SixRounds(s);             // zone 8
	case 14:
		return new JoinPairs(s);             // zone 8
	case 15:
		return new MovieSlot(s, 59, true);   // zone 9, A13_02
	case 16:
		return new MovieSlot(s, 20, true);   // zone 10, A13_051
	case 17:
		return new FourRects(s);             // zone 10
	case 18:
		return new MovieSlot(s, 58, true);   // zone 11, A13_07
	case 22:
		return new MovieSlot(s, 37, true);   // zone 14, A13_011
	case 23:
		return new MovieSlot(s, 38, true);   // zone 14, A13_012
	case 24:
		return new SixteenPieces(s);         // zone 15
	case 25:
		return new MovieSlot(s, 66, true);   // zone 15, A13_062
	case 27:
		return new MovieSlot(s, 63, true);   // zone 17, A13_09
	default:
		return nullptr;
	}
}

} // End of namespace Peintre
