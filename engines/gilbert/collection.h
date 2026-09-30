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

#ifndef GILBERT_COLLECTION_H
#define GILBERT_COLLECTION_H

#include "common/array.h"
#include "common/path.h"
#include "common/rect.h"
#include "common/str.h"

#include "graphics/managed_surface.h"

namespace Gilbert {

/**
 * One picture of a DelphiX picture collection (`.wxi`), converted to the screen's RGB565.
 * A transparent picture keeps its key colour converted the same way, as a 16-bit
 * DirectDraw colour key does.
 */
struct Picture {
	Common::String name;
	Graphics::ManagedSurface surface;
	bool transparent = false;
	uint16 key = 0;
	/** Where the picture was last drawn: the menu's hit tests use it. */
	Common::Rect last;
};

/** One wave of a DelphiX wave collection (`.wxs`): a RIFF WAVE file in memory. */
struct Wave {
	Common::String name;
	bool looped = false;
	Common::Array<byte> data;
};

class PictureCollection {
public:
	~PictureCollection() { clear(); }
	bool load(const Common::Path &path);
	void clear();
	uint size() const { return _items.size(); }
	Picture *operator[](uint i) const { return i < _items.size() ? _items[i] : nullptr; }

private:
	Common::Array<Picture *> _items;
};

/** Reads a `.wxs` wave collection. */
bool loadWaves(const Common::Path &path, Common::Array<Wave> &waves);

} // End of namespace Gilbert

#endif // GILBERT_COLLECTION_H
