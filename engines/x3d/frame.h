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

#ifndef X3D_FRAME_H
#define X3D_FRAME_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

namespace Graphics {
struct Surface;
}

namespace X3D {

class Renderer;

// A 2D screen loaded from Data/2DFRA/<name>.fra (docs/engine-spec/ui.md, Frames;
// docs/formats/fra.ksy). Coordinates are absolute 640x480 frame pixels.
class Frame {
public:
	~Frame();
	bool load(const Common::String &name);

	// The view under p (the last visible one in file order that contains it), or -1
	int viewAt(const Common::Point &p) const;
	int cursorAt(int view) const;                    // ucg@, -1 when none
	const Common::String &commandAt(int view) const; // RCS@, empty when none

	// Text edits (dEU#, dES#, idE#): the first one takes typed characters
	void type(char c);
	void backspace();
	Common::String text() const;

	// Draws the views, the hover highlight of the view under the mouse and edit text
	void draw(Renderer &r, int xOffset, int hovered);

private:
	struct View {
		Common::String tag;
		int id, x, y, w, h, parent;
		bool visible;
		Graphics::Surface *bitmap = nullptr;
		int bitmapDx = 0, bitmapDy = 0;
		Graphics::Surface *hover = nullptr; // LIH@ (GIH@ starts disabled: not drawn)
		int hoverDx = 0, hoverDy = 0;
		int cursor = -1;
		Common::String command;
		bool edit = false;
		Common::String text;
		uint maxLength = 30;
		bool placeholder = false; // the text is the default one, replaced when typing
	};

	int editView() const;

	Common::Array<View> _views;
};

} // End of namespace X3D

#endif // X3D_FRAME_H
