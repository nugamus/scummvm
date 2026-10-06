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

#ifndef GRUMPA_CONSOLE_H
#define GRUMPA_CONSOLE_H

#include "gui/debugger.h"

namespace Grumpa {

class GrumpaEngine;

// The debugger console: commands go through the event VM and the inventory, as the game's own
class Console : public GUI::Debugger {
public:
	explicit Console(GrumpaEngine *vm);

private:
	bool cmdScene(int argc, const char **argv);
	bool cmdOp(int argc, const char **argv);
	bool cmdGive(int argc, const char **argv);
	bool cmdItem(int argc, const char **argv);
	bool cmdActors(int argc, const char **argv);
	bool cmdGlobals(int argc, const char **argv);
	bool cmdVar(int argc, const char **argv);
	bool cmdSave(int argc, const char **argv);
	bool cmdLoad(int argc, const char **argv);

	GrumpaEngine *_vm;
};

} // End of namespace Grumpa

#endif // GRUMPA_CONSOLE_H
