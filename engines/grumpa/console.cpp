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

#include "grumpa/console.h"
#include "grumpa/events.h"
#include "grumpa/grumpa.h"

namespace Grumpa {

Console::Console(GrumpaEngine *vm) : GUI::Debugger(), _vm(vm) {
	registerCmd("scene", WRAP_METHOD(Console, cmdScene));     // scene <n>: 185 op 31 (E-0116)
	registerCmd("op", WRAP_METHOD(Console, cmdOp));           // op <id> <op> [a1 [a2]]
	registerCmd("give", WRAP_METHOD(Console, cmdGive));       // give <item>: op 42
	registerCmd("item", WRAP_METHOD(Console, cmdItem));       // item <id> [state]: op 16
	registerCmd("actors", WRAP_METHOD(Console, cmdActors));   // the scene's actors
	registerCmd("globals", WRAP_METHOD(Console, cmdGlobals)); // the global.atx actors
	registerCmd("var", WRAP_METHOD(Console, cmdVar));         // var <id> [slot [value]]
	registerCmd("save", WRAP_METHOD(Console, cmdSave));       // save <slot>
	registerCmd("load", WRAP_METHOD(Console, cmdLoad));       // load <slot>
}

bool Console::cmdScene(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("scene <n>\n");
		return true;
	}
	_vm->_events->deliver(185, 31, atoi(argv[1]), -1);  // arg2 -1: black at once (E-0700)
	return false;
}

bool Console::cmdOp(int argc, const char **argv) {
	if (argc < 3 || argc > 5) {
		debugPrintf("op <id> <op> [a1 [a2]]   (id -1: every actor)\n");
		return true;
	}
	_vm->_events->deliver(atoi(argv[1]), atoi(argv[2]), argc > 3 ? atoi(argv[3]) : 0,
						  argc > 4 ? atoi(argv[4]) : 0);
	return true;
}

bool Console::cmdGive(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("give <item>   (%d..%d)\n", Inventory::kFirstItem, Inventory::kLastItem);
		return true;
	}
	_vm->_events->deliver(atoi(argv[1]), 42, 0, 0);
	return true;
}

bool Console::cmdItem(int argc, const char **argv) {
	if (argc < 2 || argc > 3) {
		debugPrintf("item <id> [state]   (1 gone, 3 carried, 4 in a scene, 6 on the cursor)\n");
		return true;
	}
	int id = atoi(argv[1]);
	if (argc == 3)
		_vm->_events->deliver(id, 16, atoi(argv[2]), 0);
	const Common::Array<Inventory::Item> &items = _vm->_inventory.items();
	for (uint i = 0; i < items.size(); i++)
		if (items[i].id == id)
			debugPrintf("item %d: state %d, scene %d, %s\n", id, items[i].state, items[i].scene,
						items[i].mesh.c_str());
	return true;
}

bool Console::cmdActors(int argc, const char **argv) {
	const SceneData &sc = _vm->_sceneData;
	debugPrintf("scene %d, view %d\n", _vm->_sceneNum, sc.view);
	for (uint i = 0; i < sc.sprites.size(); i++)
		debugPrintf("  sprite  %4u %c%c %s\n", sc.sprites[i].id, sc.sprites[i].active ? 'a' : '-',
					sc.sprites[i].visible ? 'v' : '-', sc.sprites[i].name.c_str());
	for (uint i = 0; i < sc.meshes.size(); i++)
		debugPrintf("  mesh    %4u %c%c %s\n", sc.meshes[i].id, sc.meshes[i].active ? 'a' : '-',
					sc.meshes[i].visible ? 'v' : '-', sc.meshes[i].anb.c_str());
	for (uint i = 0; i < sc.triggers.size(); i++)
		debugPrintf("  trigger %4u %c%c\n", sc.triggers[i].id, sc.triggers[i].active ? 'a' : '-',
					sc.triggers[i].visible ? 'v' : '-');
	for (uint i = 0; i < sc.logic.size(); i++)
		debugPrintf("  logic   %4u %c  type 0x%02x state %d count %d\n", sc.logic[i].id,
					sc.logic[i].active ? 'a' : '-', sc.logic[i].type,
					sc.logic[i].state.empty() ? 0 : sc.logic[i].state[0], sc.logic[i].count);
	return true;
}

bool Console::cmdGlobals(int argc, const char **argv) {
	EventVM &vm = *_vm->_events;
	for (uint i = 0; i < vm._globalIds.size(); i++) {
		const SceneLogic &g = vm._globals[vm._globalIds[i]];
		Common::String slots;
		for (uint k = 0; k < g.state.size(); k++)
			slots += Common::String::format(" %d", g.state[k]);
		debugPrintf("  %4u %c type 0x%02x count %d slots%s\n", g.id, g.active ? 'a' : '-', g.type,
					g.count, slots.c_str());
	}
	return true;
}

bool Console::cmdVar(int argc, const char **argv) {
	if (argc < 2 || argc > 4) {
		debugPrintf("var <global id> [slot [value]]\n");
		return true;
	}
	EventVM &vm = *_vm->_events;
	uint32 id = atoi(argv[1]);
	if (!vm._globals.contains(id)) {
		debugPrintf("no global %u\n", id);
		return true;
	}
	SceneLogic &g = vm._globals[id];
	uint slot = argc > 2 ? atoi(argv[2]) : 0;
	if (slot >= g.state.size()) {
		debugPrintf("global %u has %u slots\n", id, g.state.size());
		return true;
	}
	if (argc == 4)
		g.state[slot] = atoi(argv[3]);
	debugPrintf("global %u slot %u = %d\n", id, slot, g.state[slot]);
	return true;
}

bool Console::cmdSave(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("save <slot>\n");
		return true;
	}
	if (!_vm->canSaveGameStateCurrently()) {
		debugPrintf("cannot save now (not in a scene)\n");
		return true;
	}
	debugPrintf("%s\n", _vm->saveGameState(atoi(argv[1]), "console").getCode() == Common::kNoError
							? "saved" : "save failed");
	return true;
}

bool Console::cmdLoad(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("load <slot>\n");
		return true;
	}
	if (_vm->loadGameState(atoi(argv[1])).getCode() != Common::kNoError) {
		debugPrintf("load failed\n");
		return true;
	}
	return false;  // the main loop enters the loaded scene
}

} // End of namespace Grumpa
