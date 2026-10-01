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

#ifndef GILBERT_BOOK_H
#define GILBERT_BOOK_H

#include "common/array.h"
#include "common/rect.h"

#include "graphics/managed_surface.h"

#include "gilbert/collection.h"
#include "gilbert/gilbert.h"

namespace Gilbert {

class GilbertEngine;

/** The books, mode 4 (screens.md "Books"). */
class Book {
public:
	explicit Book(GilbertEngine *vm) : _vm(vm) {}

	/** The room's book button: Ordliste, the list from topic 0. */
	void open();
	void tick();

private:
	struct Link {
		Common::Rect rect;
		int book = 0;
		uint32 topic = 0;
	};

	void buildList(int book, int first);
	void buildPage(int book, uint32 topic);
	void draw();
	void handleMouse();
	void tabAction(int item);
	void arrowAction(int item);
	int tabFor(int book) const;

	GilbertEngine *_vm;
	PictureCollection _images;
	TextPage _surface; ///< the list or the page (the original's off-screen surface)
	bool _page = false;
	int _book = 1;
	int _first = 0;
	int _scroll = 0;
	Common::Array<Link> _links; ///< list titles (list view) or links (page view)
	int _tab = 0x4e;
	int _pressedButton = -1;
	int _hoverTab = -1, _hoverArrow = -1, _pressedArrow = -1;
	int _cursor = 0;
	bool _flip = false;
};

} // End of namespace Gilbert

#endif // GILBERT_BOOK_H
