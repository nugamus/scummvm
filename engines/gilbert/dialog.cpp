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

#include "graphics/font.h"

#include "gilbert/dialog.h"
#include "gilbert/gilbert.h"
#include "gilbert/logic.h"

namespace Gilbert {

// Titles and texts shorter than 3 characters become two spaces (screens.md "Opening").
static Common::String atLeast3(const Common::String &s) {
	return s.size() < 3 ? Common::String("  ") : s;
}

void DialogBox::open() {
	Logic *logic = _vm->logic();
	_title = atLeast3(logic->dialogTitle());
	_text = atLeast3(logic->dialogText());
	_choices.clear();
	for (uint i = 0; i < logic->dialogChoiceCount(); i++)
		_choices.push_back(logic->dialogChoice(i));
	_print = false;
	_open = true;
	_hover = _pressed = -1;
}

// The book's Udskriv message: language lines 20..24 and one choice, line 25.
void DialogBox::openPrintMessage() {
	_title = "   ";
	_text.clear();
	for (uint n = 20; n <= 24; n++) {
		const Common::String line = _vm->languageLine(n).encode(Common::kWindows1252);
		if (n == 20 || line.size() > 2)
			_text += line + "\n";
	}
	_choices.clear();
	_choices.push_back(_vm->languageLine(25).encode(Common::kWindows1252));
	_print = true;
	_open = true;
	_hover = _pressed = -1;
}

// dialog::Layout, every tick (screens.md "Layout").
void DialogBox::layout() {
	int w = 0, h = 0;
	for (const Common::String &c : _choices)
		w = MAX(w, _vm->textWidth(GilbertEngine::fromWindows1252(c), 8));
	h = 18 * _choices.size();
	w = MAX(w, _vm->textWidth(GilbertEngine::fromWindows1252(_title), 9, true));
	h += _vm->font(9, true)->getFontHeight();
	_lines.clear();
	int lineFeeds = 0;
	Common::String line;
	for (uint i = 0; i <= _text.size(); i++) {
		if (i == _text.size() || _text[i] == '\n') {
			line.trim();
			_lines.push_back(line);
			line.clear();
			if (i < _text.size())
				lineFeeds++;
		} else {
			line += _text[i];
		}
	}
	_off = _vm->font(9, true)->getFontHeight() * lineFeeds;
	for (const Common::String &l : _lines) {
		w = MAX(w, _vm->textWidth(GilbertEngine::fromWindows1252(l), 8));
		h += _vm->font(8)->getFontHeight();
	}
	_box = Common::Rect(310 - w / 2, 240 - h / 2, 330 + w / 2, 240 + h / 2);
	_rows.clear();
	for (uint k = 0; k < _choices.size(); k++)
		_rows.push_back(Common::Rect(_box.left + 5, _box.top + 25 + 18 * k + _off, _box.right - 5, _box.top + 41 + 18 * k + _off));
}

// dialog::HandleMouse (screens.md "Choosing").
void DialogBox::handleMouse() {
	if (!_open)
		return;
	layout();
	const Common::Point m = _vm->mouse();
	const Common::Rect mr(m.x - 3, m.y - 3, m.x + 3, m.y + 3);
	int hit = -1;
	for (uint k = 0; k < _rows.size(); k++) {
		const int width = _vm->textWidth(GilbertEngine::fromWindows1252(_choices[k]), 8);
		if (Common::Rect(_rows[k].left, _rows[k].top, _rows[k].left + width + 10, _rows[k].bottom).intersects(mr))
			hit = k;
	}
	_hover = hit;
	if (_vm->takeLeftPress()) {
		_pressed = hit;
		if (hit >= 0) {
			_open = false;
			if (!_print)
				_vm->logic()->dialogEnd(hit);
		}
	}
}

// dialog::Draw (screens.md "Drawing").
void DialogBox::draw() {
	if (!_open)
		return;
	layout();
	const Common::Rect &b = _box;
	PictureCollection &i2 = _vm->interface2();
	_vm->fillAlpha(Common::Rect(b.left, b.top - 16, b.right, b.bottom), kColourBlack, 180);
	_vm->drawPicture(i2[0x9f], b.left - 2, b.top - 17);
	_vm->drawPicture(i2[0xa0], b.right - 8, b.top - 17);
	_vm->drawPicture(i2[0xa1], b.left - 2, b.bottom - 8);
	_vm->drawPicture(i2[0xa2], b.right - 8, b.bottom - 8);
	_vm->blendPattern(i2[0xa3], 0, Common::Rect(b.left + 2, b.top - 17, b.right - 2, b.top - 14), 255);
	_vm->blendPattern(i2[0xa3], 0, Common::Rect(b.left + 2, b.bottom - 1, b.right - 2, b.bottom + 2), 255);
	_vm->blendPattern(i2[0xa4], 0, Common::Rect(b.left - 2, b.top - 14, b.left + 1, b.bottom - 8), 255);
	_vm->blendPattern(i2[0xa4], 0, Common::Rect(b.right - 1, b.top - 14, b.right + 2, b.bottom - 8), 255);
	for (uint i = 0; i < _lines.size(); i++)
		_vm->drawText(_vm->screen(), GilbertEngine::fromWindows1252(_lines[i]), b.left + 10, b.top - 6 + 12 * i, 8, kColourTan);
	for (uint k = 0; k < _choices.size(); k++)
		_vm->drawText(_vm->screen(), GilbertEngine::fromWindows1252(_choices[k]), b.left + 10, b.top + 20 + 18 * k + _off, 8,
		              (int)k == _hover || (int)k == _pressed ? kColourTan : 0x7F7F7F);
}

} // End of namespace Gilbert
