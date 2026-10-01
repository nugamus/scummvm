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

#ifndef GILBERT_DIALOG_H
#define GILBERT_DIALOG_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

namespace Gilbert {

class GilbertEngine;

/** The dialogue box, an overlay of the room, close-up and book screens (screens.md). */
class DialogBox {
public:
	explicit DialogBox(GilbertEngine *vm) : _vm(vm) {}

	bool isOpen() const { return _open; }
	/** Call-back 6: the current dialogue of the game rules. */
	void open();
	/** The book's print message (screens.md "Opening"). */
	void openPrintMessage();
	void close() { _open = false; }

	/** dialog::HandleMouse: takes the press. */
	void handleMouse();
	/** dialog::Layout and dialog::Draw. */
	void draw();

private:
	void layout();

	GilbertEngine *_vm;
	bool _open = false;
	bool _print = false;
	uint32 _id = 0; ///< the dialogue's ID, for the mark_choices option
	Common::String _title, _text;
	Common::Array<Common::String> _choices;
	Common::Array<Common::String> _lines;
	Common::Rect _box;
	int _off = 0;
	Common::Array<Common::Rect> _rows;
	int _hover = -1, _pressed = -1;
};

} // End of namespace Gilbert

#endif // GILBERT_DIALOG_H
