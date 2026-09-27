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

// Zone AS (spec/rotation.md for the entry of a new game).

#include "common/textconsole.h"

#include "ring/ring.h"
#include "ring/world.h"
#include "ring/ring/zones.h"

namespace Ring {
namespace AS {

void enter(RingEngine *vm, int entry) {
	if (entry == 999) { // a new game (0x437ba0, E-0046)
		Rotation *r = vm->world().rotation(80001);
		if (!r)
			return;
		r->setAlpha(90.0f);
		r->ran = 85.3f;
		vm->rotSetAct(80001);
		// ponytail: timers 2, 3, 4 (100, 220, 150 s) come with the zone's handlers
		return;
	}
	warning("Ring: AS entry %d is not implemented yet", entry);
}

void onSound(RingEngine *vm, int id, int type, int reason, int ended) {
	// ponytail: the zone's sound chains (0x437190) come with its handlers
}

} // End of namespace AS
} // End of namespace Ring
