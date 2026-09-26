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

// Zones 0, 20, 21 and 24 (games/mission-sunlight/docs/a14.md).

namespace {

const Common::Rect kPotArea = rectWH(590, 400, 45, 80);

/** Zone 0, object 0: the magnifier's two views, then the sunflower (0x411a28). */
class FirstZone : public SlotRun {
public:
	FirstZone(Shell &s) : SlotRun(s) {
		_s.movie(32, false);
	}

	bool run(int &result) override {
		switch (_step) {
		case 0:
			if (_s.moviePlaying())
				break;
			show("a14_031b");
			_s.drawMagnifierOpen();
			_s.setCursor(kCursorDefault);
			go(1);
			break;
		case 1:
			_s.setCursor(_s.overViewA() || _s.overViewB() ? kCursorButton : kCursorDefault);
			if (_s.clicked() && (_s.overViewA() || _s.overViewB())) {
				_view = _s.overViewA() ? 0 : 1;
				_s.sound("clic_1");
				_s.startView(_view == 0 ? "A14_03a" : "A14_03b", _view == 1);
				go(2);
			} else if ((_seen[0] && _seen[1]) || after(250)) {
				_s.movie(33, false);
				go(4);
			}
			break;
		case 2:
			if (!_s.viewStep())
				break;
			show("a14_031b");
			_s.drawMagnifierOpen();
			_s.setCursor(kCursorDefault);
			_seen[_view] = true;
			go(3);
			break;
		case 3:
			if (!_s.down())
				go(1);
			break;
		case 4:
			if (_s.moviePlaying())
				break;
			show("a14_031c");
			_bank.load("A14_031a");
			_tourn.load("TOURN1");
			go(5);
			break;
		case 5: {
			const bool over = rectWH(111, 244, 24, 32).contains(_s.mouse());
			_s.setCursor(over ? kCursorTake : kCursorDefault);
			if (over && _s.clicked()) {
				_bank.draw(_s.page(), 1, 31, 198);
				_grab = _s.mouse();
				_drag = true;
				_s.sound("tourneso");
				_s.setCursor(kCursorDrag);
				go(6);
			}
			break;
		}
		case 6:
			if (_s.down())
				break;
			_drag = false;
			_s.setCursor(kCursorDefault);
			if (kPotArea.contains(Common::Point(132 + _s.mouse().x - _grab.x, 295 + _s.mouse().y - _grab.y))) {
				_s.counter(0) = 1;
				_s.movie(34, true);
				go(7);
			} else {
				_bank.draw(_s.page(), 0, 31, 198);
				_s.sound("cf_clic3");
				go(5);
			}
			break;
		case 7:
			if (_s.moviePlaying())
				break;
			show("a14_031d");
			go(8);
			break;
		default:
			if (after(40)) {
				result = 1;
				return true;
			}
			break;
		}
		return false;
	}

	void draw(Graphics::Surface &dst) override {
		// TOURN1 centred on (132, 295) moved with the cursor (E-0442).
		if (_drag)
			_tourn.draw(dst, 0, 132 + _s.mouse().x - _grab.x, 295 + _s.mouse().y - _grab.y);
	}

private:
	void show(const char *background) {
		_s.background(background);
		_s.drawSlots();
	}

	SpriteBank _bank, _tourn;
	bool _seen[2] = { false, false };
	uint _view = 0;
	bool _drag = false;
	Common::Point _grab;
};

/** Zone 20, object 31: three wheels (0x40770c). */
class ThreeWheels : public SlotRun {
public:
	ThreeWheels(Shell &s) : SlotRun(s) {
		// N[w][s] (0x4a5808).
		static const char kLetters[3][3] = { { 'h', 'i', 'j' }, { 'b', 'c', 'd' }, { 'e', 'f', 'g' } };
		for (uint w = 0; w < 3; w++)
			for (uint k = 0; k < 3; k++)
				_banks[w][k].load(Common::String::format("a14_01%c", kLetters[w][k]));
		_s.movie(54, false);
	}

	bool run(int &result) override {
		static const Common::Rect kWheels[3] = {
			rectWH(153, 29, 335, 140), rectWH(153, 170, 335, 185), rectWH(153, 356, 335, 73)
		};
		switch (_step) {
		case 0:
			if (_s.moviePlaying())
				break;
			_s.background("a14_010a");
			_s.drawSlots();
			_s.voice("a14_01b");
			go(1);
			break;
		case 1:
			if (_s.voiceWait())
				go(2);
			break;
		case 2: {
			int over = -1;
			for (int w = 0; w < 3; w++)
				if (kWheels[w].contains(_s.mouse()))
					over = w;
			_s.setCursor(over >= 0 ? kCursorButton : kCursorDefault);
			if (_aborted) {
				result = 0;
				return true;
			}
			if (over >= 0 && _s.clicked()) {
				_w = over;
				_frame = 0;
				_s.sound("a14_01e");
				_s.setCursor(kCursorBusy);
				go(3);
			}
			break;
		}
		case 3: {
			// N[w][s] plays once at the wheel's (x, y), one frame per odd tick; raw frames
			// are opaque, so they go straight on the page.
			const SpriteBank &bank = _banks[_w][_state[_w]];
			if (_frame < (int)bank.frameCount()) {
				if (_s.odd())
					bank.draw(_s.page(), _frame++, kWheels[_w].left, kWheels[_w].top);
				break;
			}
			_state[_w] = (_state[_w] + 1) % 3;
			if (!_state[0] && !_state[1] && !_state[2]) {
				_s.sound("reussit");
				go(4);
			} else {
				go(7);
			}
			break;
		}
		case 7:
			if (!_s.down())
				go(2);
			break;
		case 4:
			if (after(51)) {
				_s.background("a14_01");
				_s.drawSlots();
				_s.voice("a14_01c");
				_step = 5;
			}
			break;
		case 5:
			if (_s.voiceWait())
				_step = 6;
			break;
		default:
			// The same count runs on to 60.
			if (after(60)) {
				result = 1;
				return true;
			}
			break;
		}
		return false;
	}

private:
	SpriteBank _banks[3][3];
	uint _state[3] = { 2, 2, 1 };
	uint _w = 0;
	int _frame = 0;
};

/** Zone 24, object 34: uncover the picture, then five pieces (0x408118). */
class Uncover : public SlotRun {
public:
	Uncover(Shell &s) : SlotRun(s) {
		// The mask: 479 × 387 bytes and 3 marked ones, 46,344 u32 words.
		_mask.resize(479 * 387 + 3);
		memset(_mask.data(), 0, _mask.size());
		memset(_mask.data() + 479 * 387, 0xFF, 3);
		_loose.load("a14_021a");
		_placedBank.load("a14_021b");
		SpriteBank cursors;
		cursors.load("Curseurs");
		cursors.frameSize(kCursorBrush, _brushW, _brushH);
		_s.movie(28, true);
	}

	~Uncover() override {
		_hidden.free();
	}

	bool run(int &result) override {
		static const Common::Rect kArea(112, 29, 591, 416);
		switch (_step) {
		case 0:
			if (_s.moviePlaying())
				break;
			if (!loadTgp("a14_020a", _hidden))
				_hidden.create(640, 480, _s.page().format);
			_s.background("a14_02");
			_s.drawSlots();
			go(1);
			break;
		case 1:
			if (!_s.down())
				go(2);
			break;
		case 2: {
			const Common::Point m = _s.mouse();
			const bool inside = kArea.contains(m);
			_s.setCursor(inside ? kCursorBrush : kCursorDefault);
			if (_aborted) {
				_s.stopSound("p_clic1");
				result = 0;
				return true;
			}
			if (!inside || !_s.down()) {
				_s.stopSound("p_clic1");
				_last = m;
				break;
			}
			if (m != _last) {
				if (!_s.soundPlaying("p_clic1"))
					_s.sound("p_clic1", true);
			} else {
				_s.stopSound("p_clic1");
			}
			_last = m;
			brush(m);
			if (unmarkedWords() < 9251) {
				_s.stopSound("p_clic1");
				_s.sound("p_clic2");
				go(3);
			}
			break;
		}
		case 3:
			if (_s.down())
				break;
			_s.movie(57, false);
			go(4);
			break;
		case 4:
			if (_s.moviePlaying())
				break;
			_s.background("a14_020a");
			_s.drawSlots();
			for (uint i = 0; i < 5; i++)
				_loose.draw(_s.page(), i, kPieces[i].pick.left, kPieces[i].pick.top);
			_s.voice("a14_02c");
			go(5);
			break;
		case 5:
			if (_s.voiceWait()) {
				go(6);
				_count = 125;
			}
			break;
		case 6: {
			int over = -1;
			for (int i = 0; i < 5; i++)
				if (!_placed[i] && kPieces[i].pick.contains(_s.mouse()))
					over = i;
			_s.setCursor(over >= 0 ? kCursorTake : kCursorDefault);
			if (_aborted) {
				result = 0;
				return true;
			}
			if (++_count > 500) {
				_s.voice("a14_02d");
				_count = 0;
			}
			if (over >= 0 && _s.clicked()) {
				_i = over;
				_s.restore(kPieces[_i].pick);
				_grab = _s.mouse();
				_drag = true;
				_s.sound("p_clic3");
				_s.setCursor(kCursorDrag);
				_step = 7;
			}
			break;
		}
		case 7: {
			if (_s.down())
				break;
			_drag = false;
			_s.setCursor(kCursorDefault);
			const Common::Point p = dragPos();
			const Common::Point q(p.x + 32, p.y + 32);
			int hit = -1;
			for (int j = 0; j < 5 && hit < 0; j++)
				if (kPieces[j].drop.contains(q))
					hit = j;
			if (hit == (int)_i) {
				_s.sound("p_clic4");
				_placedBank.draw(_s.page(), _i, kPieces[_i].placed.x, kPieces[_i].placed.y);
				_placed[_i] = true;
				_count = 125;
				_step = 6;
				if (_placed[0] && _placed[1] && _placed[2] && _placed[3] && _placed[4])
					go(8);
			} else {
				_loose.draw(_s.page(), _i, kPieces[_i].pick.left, kPieces[_i].pick.top);
				_s.sound("cf_clic3");
				_count = 0;
				_step = 6;
			}
			break;
		}
		case 8:
			if (after(25)) {
				_s.sound("reussit");
				go(9);
			}
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
		if (_drag) {
			const Common::Point p = dragPos();
			_loose.draw(dst, _i, p.x, p.y);
		}
	}

private:
	struct Piece {
		Common::Rect pick, drop;
		Common::Point placed;
	};
	static const Piece kPieces[5];

	Common::Point dragPos() const {
		return Common::Point(kPieces[_i].pick.left + _s.mouse().x - _grab.x, kPieces[_i].pick.top + _s.mouse().y - _grab.y);
	}

	void brush(const Common::Point &m) {
		// The brush rectangle centred on the cursor, clipped to the area, copied from the
		// hidden picture and marked in the mask (row stride 479, origin (112, 29)).
		Common::Rect r = rectWH(m.x - _brushW / 2, m.y - _brushH / 2, _brushW, _brushH);
		r.clip(Common::Rect(112, 29, 591, 416));
		if (r.isEmpty())
			return;
		_s.page().copyRectToSurface(_hidden, r.left, r.top, r);
		for (int y = r.top; y < r.bottom; y++)
			memset(_mask.data() + (y - 29) * 479 + (r.left - 112), 1, r.width());
	}

	uint unmarkedWords() const {
		uint n = 0;
		for (uint w = 0; w + 4 <= _mask.size(); w += 4)
			if (!_mask[w] || !_mask[w + 1] || !_mask[w + 2] || !_mask[w + 3])
				n++;
		return n;
	}

	Common::Array<byte> _mask;
	Graphics::Surface _hidden;
	SpriteBank _loose, _placedBank;
	int _brushW = 0, _brushH = 0;
	Common::Point _last, _grab;
	bool _placed[5] = { false, false, false, false, false };
	uint _i = 0;
	bool _drag = false;
};

const Uncover::Piece Uncover::kPieces[5] = {
	{ rectWH(569, 15, 65, 65), rectWH(460, 356, 65, 65), Common::Point(475, 365) },
	{ rectWH(569, 95, 65, 65), rectWH(125, 169, 65, 65), Common::Point(181, 211) },
	{ rectWH(569, 175, 65, 65), rectWH(469, 234, 65, 65), Common::Point(492, 258) },
	{ rectWH(569, 255, 65, 65), rectWH(114, 32, 65, 65), Common::Point(183, 92) },
	{ rectWH(569, 335, 65, 65), rectWH(147, 326, 65, 65), Common::Point(184, 347) }
};

/** Zone 21: the ending (0x412065, state 0x24). */
class Ending : public SlotRun {
public:
	Ending(Shell &s) : SlotRun(s) {
		_s.background("a14_032a");
		_s.movie(64, false);
	}

	bool run(int &result) override {
		result = 1;
		switch (_step) {
		case 0:
			if (_s.moviePlaying())
				break;
			_s.background("a14_032b");
			_s.setCursor(kCursorBusy);
			go(1);
			break;
		case 1:
			if (!_s.down())
				go(2);
			break;
		case 2: {
			const bool over = rectWH(121, 325, 41, 45).contains(_s.mouse());
			_s.setCursor(over ? kCursorButton : kCursorDefault);
			if (over && _s.clicked()) {
				_s.movie(65, false);
				go(3);
			}
			break;
		}
		case 3:
			if (_s.moviePlaying())
				break;
			_s.setCursor(kCursorBusy);
			go(4);
			break;
		case 4:
			if (!_s.down())
				go(5);
			break;
		default:
			return after(60);
		}
		return false;
	}
};

} // End of anonymous namespace

SlotRun *placeA14(Shell &s, uint zone, uint object) {
	switch (object) {
	case 0:
		return new FirstZone(s);     // zone 0
	case 31:
		return new ThreeWheels(s);   // zone 20
	case 34:
		return new Uncover(s);       // zone 24
	default:
		return nullptr;
	}
}

SlotRun *createEnding(Shell &s) {
	return new Ending(s);
}

} // End of namespace Peintre
