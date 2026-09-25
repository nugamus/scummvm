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

#include "x3d/console.h"
#include "x3d/x3d.h"

namespace X3D {

Console::Console(X3DEngine *vm) : GUI::Debugger(), _vm(vm) {
	static const char *const commands[] = { "where", "goto", "lookat", "click", "hotspots", "give", "hold" };
	for (const char *c : commands)
		registerCmd(c, WRAP_METHOD(Console, cmd));
}

bool Console::cmd(int argc, const char **argv) {
	Common::String line;
	for (int i = 0; i < argc; i++)
		line += Common::String(i ? " " : "") + argv[i];
	debugPrintf("%s\n", _vm->command(line).c_str());
	return true;
}

} // End of namespace X3D
