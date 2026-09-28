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
	// All commands go to X3DEngine::command. Arguments:
	//   where                          camera position, angles and the ground below
	//   goto <x> <y> <z> [yaw [pitch]] move the camera there (a cut)
	//   lookat <object> [ms]           turn the camera to an object's centre
	//   click [<x> <y> | <object>]     click at the mouse, a screen point or an object
	//   hotspots                       the unit's hotspots and their positions
	//   give <item>                    add an item to the inventory bar
	//   hold <item | ->                put an item on the cursor ("-": none)
	//   bar                            show or hide the inventory bar, as Space
	//   probe <x> <y> <z>              the ground below a point
	//   objs <text>                    every object whose name contains the text
	//   node <name>                    an animation node's frame
	//   pos <object>                   an object's origin and centre
	//   act <id>                       run action Mnn, bypassing the click
	//   exhaust <id>                   mark action Mnn exhausted
	//   gauge <ms>                     end the running gauge after ms
	//   save <slot>, load <slot>       save or load a ScummVM slot
	//   page <painting | credits | settings | gallery | loupe <painting>>   show a 2D page
	//   savemenu, loadmenu             the game's save and load screens
	//   view3d <painting>              the painting's 3D gallery scene
	//   overlay                        the hotspot overlay on or off, as its key
	static const char *const commands[] = {
		"where", "goto", "lookat", "click", "hotspots", "give", "hold", "bar", "probe", "objs",
		"node", "pos", "act", "exhaust", "gauge", "save", "load", "page", "savemenu", "loadmenu",
		"view3d", "overlay"
	};
	for (const char *c : commands)
		registerCmd(c, WRAP_METHOD(Console, cmd));
}

bool Console::cmd(int argc, const char **argv) {
	Common::String line;
	for (int i = 0; i < argc; i++)
		line += Common::String(i ? " " : "") + argv[i];
	// Commands that run frames or menus of their own run from the main loop once the
	// console is closed, not inside its event handling
	const Common::String c = argv[0];
	if (c == "lookat" || c == "page" || c == "savemenu" || c == "loadmenu" || c == "load") {
		_vm->queueCommand(line);
		return false;
	}
	debugPrintf("%s\n", _vm->command(line).c_str());
	return true;
}

} // End of namespace X3D
