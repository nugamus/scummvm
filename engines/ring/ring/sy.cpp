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
	kObjNewGame = 90000, kObjPreferences, kObjLoad, kObjSave, kObjContinue, kObjStatus, kObjQuit,
	kObjPrefCancel = 90101, kObjPrefOK, kObjSubtitles, kObjStereo, kObjVolume, kObjDialogueVolume, kObjCredits
};

// The preferences screen's working values (sy.md, "Preferences").
static int s_volume, s_dialogue, s_subtitles;
static bool s_swapped;
static int s_sliderX, s_delta; // the slider's x between drags; the last move's signed distance

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
	} else if (object == kObjPrefCancel || object == kObjPrefOK) {
		w.showPresentation(kObjPrefCancel, 0, object == kObjPrefCancel);
		w.showPresentation(kObjPrefOK, 0, object == kObjPrefOK);
	} else if (object == kObjStereo) {
		w.showPresentation(kObjStereo, 2, true);
	}
	// ponytail: the load, save and status screens light their own pictures (sy.md, to come)
}

void onNothing(RingEngine *vm) {
	for (const auto &l : kLit)
		vm->world().showPresentation(l.object, l.presentation, false);
	vm->cursors().set(0x38); // CUR_MenuIdle
}

static void moveSlider(RingEngine *vm, int object, int x) {
	if (PuzzleImage *img = vm->world().image(object, 0, 0))
		img->x = x;
}

static void showSubtitles(RingEngine *vm) {
	vm->world().showPresentation(kObjSubtitles, 0, s_subtitles == 1);
	vm->world().showPresentation(kObjSubtitles, 1, s_subtitles != 1);
}

static void showStereo(RingEngine *vm) {
	vm->world().showPresentation(kObjStereo, -1, false);
	vm->world().showPresentation(kObjStereo, s_swapped, true);
}

static void openPreferences(RingEngine *vm) {
	vm->puzSetAct(kObjPreferences);
	const int *p = vm->preferences();
	s_volume = p[0];
	s_dialogue = p[1];
	s_swapped = p[2] == 1;
	s_subtitles = p[3];
	// ScummVM always has a mixer, so the subtitles switch is never forced on and disabled (0x406ee0).
	showSubtitles(vm);
	moveSlider(vm, kObjVolume, s_volume * 5 + 84);
	moveSlider(vm, kObjDialogueVolume, s_dialogue * 5 + 84);
	showStereo(vm);
}

void onDrag(RingEngine *vm, int object, int phase) {
	if (object != kObjVolume && object != kObjDialogueVolume)
		return;
	Drag &d = vm->drag();
	int step;
	switch (phase) {
	case 1:
		d.mode = 2;
		d.limit = object == kObjVolume ? Common::Rect(310, 140, 600, 180) : Common::Rect(310, 197, 600, 237);
		step = CLIP((d.current.x - 314) / 5, 0, 54);
		break;
	case 3:
		s_delta = d.current.x - d.press.x;
		moveSlider(vm, object, s_sliderX + s_delta);
		return;
	case 2:
		// s_delta is the last move's; a press and release without a move reuses the previous drag's.
		step = CLIP((s_sliderX + s_delta - 314) / 5, 0, 54);
		(object == kObjVolume ? s_volume : s_dialogue) = step + 46;
		break;
	default:
		return;
	}
	s_sliderX = step * 5 + 314;
	moveSlider(vm, object, s_sliderX);
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
		openPreferences(vm);
		break;
	case kObjPrefCancel:
		vm->puzSetAct(kObjNewGame);
		break;
	case kObjPrefOK:
		vm->puzSetAct(kObjNewGame);
		vm->savePreferences(s_volume, s_dialogue, s_swapped ? 1 : -1, s_subtitles);
		break;
	case kObjSubtitles:
		s_subtitles = value == 0;
		showSubtitles(vm);
		break;
	case kObjStereo:
		s_swapped = !s_swapped;
		showStereo(vm);
		break;
	case kObjCredits:
		warning("Ring: the credits (ScrollImage) are not implemented yet");
		break;
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

void onSound(RingEngine *vm, int id, int type, int reason, int ended) {
	// The end of Isha's last words (sound 90001): her picture goes, the credits, the menu.
	if (ended && id == kObjPreferences) {
		vm->world().hideAndFree(7);
		warning("Ring: the credits (ScrollImage) are not implemented yet");
		vm->startMenu(false);
	}
}

} // End of namespace SY
} // End of namespace Ring
