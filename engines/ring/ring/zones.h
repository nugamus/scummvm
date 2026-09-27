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

#ifndef RING_RING_ZONES_H
#define RING_RING_ZONES_H

namespace Ring {

class RingEngine;

/**
 * Ring's zone code: the handlers the engine's events reach (spec/events.md). Each zone's
 * logic is specified in the research repository's games/ring/docs/<zone>.md.
 */
namespace SY {
void onAccessibility(RingEngine *vm, int object, int value);
void onNothing(RingEngine *vm);
void onClick(RingEngine *vm, int object, int value);
/** The drag event (0x4331b0): phase 1 press, 2 release, 3 move. */
void onDrag(RingEngine *vm, int object, int phase);
/** The sound event (0x433cf0). */
void onSound(RingEngine *vm, int id, int type, int reason, int ended);
}

namespace AS {
/** GameSetZoneAS (0x437ba0): entering the zone at `entry` (999: a new game). */
void enter(RingEngine *vm, int entry);
/** The sound event (0x437190). */
void onSound(RingEngine *vm, int id, int type, int reason, int ended);
}

} // End of namespace Ring

#endif // RING_RING_ZONES_H
