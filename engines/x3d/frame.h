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

	// The edit's text; a placeholder shows until the player types
	void setText(const Common::String &text, bool placeholder = false);

	// List views (AOL#, VAS#, cSU#; save.md "Lists"): rows 32 px high, one selected
	void setList(const Common::Array<Common::String> &rows, int selected);
	int listRowAt(const Common::Point &p) const; // -1 outside the rows
	void selectRow(int row) { _selected = row; }

	// Views by their file id
	int indexOf(int id) const;
	void setBitmap(int id, const Common::String &name); // 2dbit/<name>.bmp, "" for none
	void setVisible(int id, bool visible);
	const Common::String &bitmapName(int view) const { return _views[view].bitmapName; }
	int idAt(int view) const { return view >= 0 ? _views[view].id : -1; }
	bool hasEdit() const { return editView() >= 0; }

	// Sliders (loV#, AoV#, BoV#; ui.md Settings): position 0..max
	int sliderValue(int id) const;
	int sliderMax(int id) const;
	void setSliderValue(int id, int value);
	bool press(const Common::Point &p); // true when a slider took it
	void drag(const Common::Point &p);
	void release() { _dragging = -1; _scrollDrag = false; }

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
		bool list = false;
		Graphics::Surface *scrollBar = nullptr, *scrollThumb = nullptr; // name_a, name_b
		Graphics::Surface *caption = nullptr; // GIH@: drawn at an absolute place on hover
		int captionX = 0, captionY = 0;
		Common::String bitmapName;
		bool slider = false;
		int margin = 0, value = 0;
		Graphics::Surface *knob = nullptr;
	};
	int _dragging = -1;

	int listView() const;
	Common::Array<Common::String> _rows;
	int _selected = -1;
	int _scroll = 0; // first shown row (save.md Lists, E-0600)
	bool _scrollDrag = false;
	int scrollMax() const;
	bool pressScroll(const Common::Point &p);

	int editView() const;

	Common::Array<View> _views;
};

} // End of namespace X3D

#endif // X3D_FRAME_H
