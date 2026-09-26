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

#include "peintre/peintre.h"

namespace Peintre {

// The 2D shell (ui.md). Not implemented yet: every zone returns at once.

int PeintreEngine::enterZone(uint zone) {
	warning("enterZone(%d): the 2D shell is not implemented yet", zone);
	return -1;
}

int PeintreEngine::runOptionMenu(bool from3D) {
	return -1;
}

void PeintreEngine::runEndCredits() {
}

} // End of namespace Peintre
