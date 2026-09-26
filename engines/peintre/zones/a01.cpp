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

// Zones 1 and 2 (games/mission-sunlight/docs/a01.md) and zone 3 (a11.md): the chapter
// of sunflower counter 1.

namespace {

/** Zone 2, object 3: the five pieces (0x40d293). */
class FivePieces : public SlotRun {
public:
	FivePieces(Shell &s) : SlotRun(s) {
		_s.background("a01_032a");
		_s.movie(0, false);
		_bank.load("a01_032a");
	}

	bool run(int &result) override {
		switch (_step) {
		case 0:
			if (_s.moviePlaying())
				break;
			_s.background("a01_032a");
			_s.drawSlots();
			_s.voice("a01_032");
			go(1);
			break;
		case 1:
			if (!_s.voiceWait())
				break;
			_s.movie(1, false);
			go(2);
			break;
		case 2:
			if (_s.moviePlaying())
				break;
			_s.background(Common::String::format("a01_032%c", 'a' + _i));
			_s.drawSlots();
			_bank.draw(_s.page(), _i, kPieces[_i].pos.x, kPieces[_i].pos.y);
			go(3);
			break;
		case 3: {
			const bool over = kPieces[_i].pick.contains(_s.mouse());
			_s.setCursor(over ? kCursorTake : kCursorDefault);
			if (!_s.down() && after(250)) {
				_s.voice("a01_032b");
				_count = 0;
			}
			if (_aborted) {
				result = 0;
				return true;
			}
			if (over && _s.clicked()) {
				_s.sound("pt_clic1");
				_s.setCursor(kCursorDrag);
				_s.restore(spriteRect(_bank, _i, kPieces[_i].pos.x, kPieces[_i].pos.y));
				_grab = _s.mouse();
				_drag = true;
				go(4);
			}
			break;
		}
		case 4: {
			if (_s.down())
				break;
			_drag = false;
			const Common::Point ref = dragPos();
			if (kPieces[_i].drop.contains(ref)) {
				_s.setCursor(kCursorBusy);
				_s.background(Common::String::format("a01_032%c", 'b' + _i));
				_s.drawSlots();
				if (_i == 4) {
					_s.sound("reussit");
					go(5);
				} else {
					_i++;
					_s.movie(_i + 1, false);
					_s.sound("pt_clic2");
					go(2);
				}
			} else {
				_bank.draw(_s.page(), _i, kPieces[_i].pos.x, kPieces[_i].pos.y);
				_s.sound("pt_clic3");
				go(3);
			}
			break;
		}
		case 5:
			if (after(60)) {
				_s.movie(6, false);
				go(6);
			}
			break;
		case 6:
			if (_s.moviePlaying())
				break;
			_s.voice("a01_032c");
			go(7);
			break;
		default:
			if (!_s.voiceWait())
				break;
			result = 1;
			return true;
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
		Common::Point pos;
		Common::Rect pick, drop;
	};
	static const Piece kPieces[5];

	/** The piece's position moved with the cursor (its reference point). */
	Common::Point dragPos() const {
		return Common::Point(kPieces[_i].pos.x + _s.mouse().x - _grab.x, kPieces[_i].pos.y + _s.mouse().y - _grab.y);
	}

	SpriteBank _bank;
	uint _i = 0;
	bool _drag = false;
	Common::Point _grab;
};

// Table 0x4a62c0: position, pick rect, drop rect.
const FivePieces::Piece FivePieces::kPieces[5] = {
	{ Common::Point(573, 403), rectWH(532, 361, 84, 85), rectWH(205, 122, 71, 89) },
	{ Common::Point(553, 384), rectWH(489, 295, 130, 177), rectWH(124, 112, 75, 180) },
	{ Common::Point(572, 405), rectWH(549, 356, 47, 97), rectWH(309, 114, 40, 78) },
	{ Common::Point(573, 404), rectWH(532, 337, 83, 132), rectWH(284, 198, 54, 97) },
	{ Common::Point(566, 384), rectWH(527, 299, 103, 173), rectWH(393, 110, 90, 184) }
};

} // End of anonymous namespace

SlotRun *placeA01(Shell &s, uint zone, uint object) {
	switch (object) {
	case 1:
		return new MovieSlot(s, 29, true);   // zone 1, A01_02
	case 2:
		return new VoiceSlot(s, "A01_031");  // zone 2
	case 3:
		return new FivePieces(s);            // zone 2
	case 4:
		return new MovieSlot(s, 60, true);   // zone 3, A11_01
	default:
		return nullptr;
	}
}

} // End of namespace Peintre
