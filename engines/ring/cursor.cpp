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

#include "ring/cursor.h"
#include "ring/resources.h"

namespace Ring {

void Cursors::add(int id, const Common::String &name, int kind, int frames, float fps, const char *folder) {
	if (find(id))
		return; // "id already exist"
	Cursor c;
	c.id = id;
	c.kind = kind;
	c.name = name;
	c.folder = folder;
	if (kind == 4) {
		c.frames = MAX(frames, 1);
		c.frameMs = (uint32)(1000.0f / fps); // truncated, as __ftol does
	}
	_cursors.push_back(c);
}

Cursors::Cursor *Cursors::find(int id) {
	for (auto &c : _cursors)
		if (c.id == id)
			return &c;
	return nullptr;
}

void Cursors::remove(int id) {
	for (uint i = 0; i < _cursors.size(); i++) {
		if (_cursors[i].id != id)
			continue;
		_cursors.remove_at(i);
		if (_current == (int)i)
			_current = -1;
		else if (_current > (int)i)
			_current--;
		return;
	}
}

void Cursors::setOffset(int id, int x, int y) {
	if (Cursor *c = find(id)) {
		c->offsetX = x;
		c->offsetY = y;
	}
}

void Cursors::set(int id) {
	for (uint i = 0; i < _cursors.size(); i++)
		if (_cursors[i].id == id)
			_current = i;
}

void Cursors::draw(Resources &res, Graphics::ManagedSurface &dst, int x, int y, uint32 now) {
	if (_current < 0)
		set(0x32);
	if (_current < 0)
		return;
	Cursor &c = _cursors[_current];
	if (c.kind != 3 && c.kind != 4)
		return; // kinds 1 and 2 are Windows cursors; the game's are all 3 or 4
	if (c.images.empty()) {
		// `\cursor\<name>.tga`, or `\cursor\<name>\<name>.NNNN.tga` from frame 1 (spec/cursor.md).
		for (int f = 0; f < (c.kind == 4 ? c.frames : 1); f++) {
			Common::String file = c.kind == 4 ? Common::String::format("%s\\%s.%04d.tga", c.name.c_str(), c.name.c_str(), f + 1)
											  : c.name + ".tga";
			c.images.push_back(Common::SharedPtr<Image>(res.loadImage(kZoneSY, file, true, c.folder)));
		}
	}
	if (c.kind == 4) {
		// The first draw only starts the clock; then one frame per elapsed frame time.
		if (!c.started) {
			c.started = true;
			c.lastStep = now;
		} else if (now - c.lastStep > c.frameMs) {
			c.lastStep = now;
			c.frame = (c.frame + 1) % c.frames;
		}
	}
	const Common::SharedPtr<Image> &img = c.images[c.kind == 4 ? c.frame : 0];
	if (img)
		img->draw(dst, x - c.offsetX, y - c.offsetY, 3);
}

} // End of namespace Ring
