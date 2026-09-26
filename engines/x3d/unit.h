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

#ifndef X3D_UNIT_H
#define X3D_UNIT_H

#include "common/str.h"

#include "x3d/renderer.h"

namespace Common {
class Serializer;
}

namespace X3D {

// A unit's own code on top of the generic scene (U##.cpp in the original): U00, U01, ...
class Unit {
public:
	virtual ~Unit() {}

	virtual void afterLoad() {}                      // renames and hides, before the hotspots
	virtual void start(bool newGame, bool video) = 0; // the start hook
	virtual bool input(float dt) { return false; }    // the input hook; false: normal camera keys
	virtual bool handle(const Common::String &action) { return false; } // a queued click action
	virtual void afterFrame() {}                     // per-frame checks after rendering
	virtual void afterAnimate() {}                   // each logic step, before the pose
	virtual void afterClick(const Common::String &hotspot) {} // after a click's queued actions
	virtual void draw() {}                           // 2D drawn over the frame
	virtual bool gameStarted() const { return true; } // false: Escape opens the Option menu
	virtual void syncState(Common::Serializer &s) {}  // the unit's save chunk (save.md)
};

// The timer gauge (u01.md): a grey frame and a red bar that shrinks as the elapsed
// fraction p grows, in 640x480 frame pixels
inline void drawGauge(Renderer *r, float p) {
	const int x = (r->width() - 640) / 2;
	r->fillRect(x + 9, 9, x + 111, 21, 0x80, 0x80, 0x80);
	r->fillRect(x + 10, 10, x + 110 - (int)(100 * p), 20, 0xff, 0, 0);
}

} // End of namespace X3D

#endif // X3D_UNIT_H
