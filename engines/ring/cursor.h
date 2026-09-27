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

#ifndef RING_CURSOR_H
#define RING_CURSOR_H

#include "common/array.h"
#include "common/ptr.h"
#include "common/str.h"

namespace Graphics {
class ManagedSurface;
}

namespace Ring {

struct Image;
class Resources;

/** The cursor handler (spec/cursor.md): cursors by id, one current, drawn last each frame. */
class Cursors {
public:
	/** `CurAdd`: kind 3 is one picture, kind 4 an animation of `frames` at `fps`. */
	void add(int id, const Common::String &name, int kind, int frames = 0, float fps = 0.0f);
	/** Drops the cursor with that id (the drag cursors, spec/cursor.md "Dragging"). */
	void remove(int id);
	/** `CurSetOffset`. */
	void setOffset(int id, int x, int y);
	/** `CurSet`. */
	void set(int id);
	/** Draws the current cursor at the mouse position when it is of kind 3 or 4. */
	void draw(Resources &res, Graphics::ManagedSurface &dst, int x, int y, uint32 now);

private:
	struct Cursor {
		int id, kind;
		Common::String name;
		int offsetX = 0, offsetY = 0;
		int frames = 1;
		uint32 frameMs = 0, lastStep = 0;
		int frame = 0;
		bool started = false;
		Common::Array<Common::SharedPtr<Image> > images;
	};
	Cursor *find(int id);

	Common::Array<Cursor> _cursors;
	int _current = -1;
};

} // End of namespace Ring

#endif // RING_CURSOR_H
