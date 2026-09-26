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

// Zone SY: the main menu and the dialogues (games/ring/docs/sy.md).

#include "common/textconsole.h"

#include "ring/cursor.h"
#include "ring/resources.h"
#include "ring/ring.h"
#include "ring/world.h"
#include "ring/ring/zones.h"

namespace Ring {
namespace SY {

enum {
	kObjExit = 2, kObjWarning = 3, kObjQuestion = 4,
	kObjNewGame = 90000, kObjPreferences, kObjLoad, kObjSave, kObjContinue, kObjStatus, kObjQuit
};

// Presentations lit while the mouse is on an entry; all of them go out on nothing (0x433bc0).
static const struct { int object, presentation; } kLit[] = {
	{ 90000, 0 }, { 90001, 0 }, { 90002, 0 }, { 90003, 0 }, { 90004, 0 }, { 90006, 0 },
	{ 90101, 0 }, { 90102, 0 }, { 90104, 2 }, { 90208, 0 }, { 90207, 0 }, { 90309, 0 },
	{ 90310, 0 }, { 90005, 0 }, { 90401, 0 }, { 2, 1 }, { 2, 2 }, { 4, 1 }, { 4, 2 }, { 3, 1 },
	{ 90912, 0 }
};

void onAccessibility(RingEngine *vm, int object, int value) {
	World &w = vm->world();
	if (object >= kObjNewGame && object <= kObjQuit) {
		for (int o = kObjNewGame; o <= kObjQuit; o++)
			if (object != kObjNewGame || o != kObjStatus) // 90000's list leaves 90005 lit
				w.showPresentation(o, 0, false);
		w.showPresentation(object, 0, true);
	} else if (object == kObjExit) {
		// unk_19 0 is "no" (presentation 2), 1 is "yes" (presentation 1)
		if (value == 0 || value == 1) {
			w.showPresentation(kObjExit, value ? 2 : 1, false);
			w.showPresentation(kObjExit, value ? 1 : 2, true);
		}
	} else if (object == kObjWarning) {
		w.showPresentation(kObjWarning, 1, true);
	} else if (object == kObjQuestion) {
		// odd unk_19: cancel (presentation 2) lit, even: OK (presentation 1)
		w.showPresentation(kObjQuestion, value & 1 ? 1 : 2, false);
		w.showPresentation(kObjQuestion, value & 1 ? 2 : 1, true);
	}
	// ponytail: the preferences, load, save and status screens light their own pictures (sy.md, to come)
}

void onNothing(RingEngine *vm) {
	for (const auto &l : kLit)
		vm->world().showPresentation(l.object, l.presentation, false);
	vm->cursors().set(0x38); // CUR_MenuIdle
}

static void closeQuestion(RingEngine *vm, int kind) {
	vm->world().hideAndFree(kObjQuestion);
	vm->world().setAccessibilities(kObjQuestion, false, kind, kind + 1);
	vm->puzSetMod(1, 1, 0);
}

// The message's title and text as the dialogue's two lines (0x40e090, spec/text.md).
static void setLines(RingEngine *vm, int object) {
	World &w = vm->world();
	const Common::String *lines[2] = { &vm->messageTitle(), &vm->messageText() };
	for (int i = 0; i < 2; i++) {
		if (PuzzleText *t = w.text(object, 0, i)) {
			t->text = *lines[i];
			t->x = 225;
			t->y = i ? 213 : 193;
		}
	}
}

static void askQuestion(RingEngine *vm, int kind, const char *key) {
	vm->message(key);
	if (!vm->puzSetMod(1, 2, kObjQuestion))
		return;
	setLines(vm, kObjQuestion);
	vm->world().showPresentation(kObjQuestion, 0, true);
	vm->world().setAccessibilities(kObjQuestion, true, kind, kind + 1);
}

void onClick(RingEngine *vm, int object, int value) {
	switch (object) {
	case kObjNewGame:
		askQuestion(vm, 2, "DoYouWantToStartNewGame");
		break;
	case kObjPreferences:
	case kObjLoad:
	case kObjSave:
		vm->puzSetAct(object);
		break;
	case kObjStatus:
		vm->puzSetAct(90004);
		break;
	case kObjContinue:
		warning("Ring: continuing a game is not implemented yet");
		break;
	case kObjQuit:
		vm->requestClose();
		break;
	case kObjExit:
		if (value == 1) {
			vm->quitGame();
		} else if (value == 0) {
			vm->world().hideAndFree(kObjExit);
			vm->world().setAccessibilities(kObjExit, false);
			vm->puzSetMod(1, 1, 0);
		}
		break;
	case kObjQuestion:
		if (value <= 3)
			closeQuestion(vm, value >= 2 ? 2 : 0);
		if (value == 2) // 0x431140: a new game starts in zone AS
			vm->setZone(kZoneAS, 999);
		break;
	default:
		break;
	}
}

} // End of namespace SY
} // End of namespace Ring
