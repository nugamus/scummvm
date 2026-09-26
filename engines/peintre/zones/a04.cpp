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

#include "peintre/shell.h"

namespace Peintre {

// Zones 19, 22, 23 (games/mission-sunlight/docs/a04.md).

SlotRun *placeA04(Shell &s, uint zone, uint object) {
	switch (object) {
	case 29:
		return new VoiceSlot(s, "A04_01");   // zone 19
	case 30:
		return new MovieSlot(s, 67, true);   // zone 19, A04_012
	case 32:
		return new MovieSlot(s, 35, true);   // zone 22, A04_02
	case 33:
		return new MovieSlot(s, 36, true);   // zone 23, A04_03
	default:
		return nullptr;
	}
}

} // End of namespace Peintre
