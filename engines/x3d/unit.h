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
	virtual void draw() {}                           // 2D drawn over the frame
	virtual bool gameStarted() const { return true; } // false: Escape opens the Option menu
};

} // End of namespace X3D

#endif // X3D_UNIT_H
