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

// Zone SY: the main menu, the dialogues and the system screens (games/ring/docs/sy.md).

#include "common/hashmap.h"
#include "common/hash-str.h"
#include "common/textconsole.h"

#include "graphics/fonts/winfont.h"

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
	kObjPrefCancel = 90101, kObjPrefOK, kObjSubtitles, kObjStereo, kObjVolume, kObjDialogueVolume, kObjCredits,
	kObjLoadCancel = 90207, kObjLoadOK = 90208, kObjSaveOK = 90309, kObjSaveCancel = 90310, kObjSaveName = 90313,
	kObjStatusOK = 90401, kObjScores = 90402,
	kPuzLoad = 90002, kPuzSave = 90003, kPuzStatus = 90004
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
	} else if (object == kObjLoadCancel || object == kObjLoadOK) {
		w.showPresentation(kObjLoadCancel, 0, object == kObjLoadCancel);
		w.showPresentation(kObjLoadOK, 0, object == kObjLoadOK);
	} else if (object == kObjSaveOK || object == kObjSaveCancel) {
		w.showPresentation(kObjSaveOK, 0, object == kObjSaveOK);
		w.showPresentation(kObjSaveCancel, 0, object == kObjSaveCancel);
	} else if (object == kObjStatusOK) {
		w.showPresentation(kObjStatusOK, 0, true);
	}
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

// The warning (0x40dfd0) and its closing (0x40e060).
static void showWarning(RingEngine *vm, const char *key) {
	vm->message(key);
	if (!vm->puzSetMod(1, 2, kObjWarning))
		return;
	setLines(vm, kObjWarning);
	vm->world().showPresentation(kObjWarning, 0, true);
	vm->world().setAccessibilities(kObjWarning, true);
}

static void closeWarning(RingEngine *vm) {
	vm->world().hideAndFree(kObjWarning);
	vm->world().setAccessibilities(kObjWarning, false);
	vm->puzSetMod(1, 1, 0);
}

// The load screen's list: visual object 1 of puzzle 90002 with the set-up's values (sy.md,
// "Load"; E-0263, E-0266). Its entries are ScummVM's slots, newest on top.
struct SaveEntry {
	int slot;
	Common::String description, name;
};
// The screens' objects sit behind a pointer: ScummVM allows no global constructors.
// Made on first use; shutdown() frees them and resets the rest when the engine ends.
struct Screens {
	Common::Array<SaveEntry> saves;
	Common::ScopedPtr<Image> picture; ///< the selected entry's
	Common::String name, description; ///< the save screen's
};
static Screens *s_screens;
static Screens &screens() {
	if (!s_screens)
		s_screens = new Screens();
	return *s_screens;
}
static int s_first, s_selected = -1;
static int s_hover; ///< 1 up, 2 down: the mouse is on that usable arrow
static int s_bars[4]; ///< the game status bars' lengths

enum { kRows = 4, kRowStep = 45 };
static const Common::Rect kUp(Common::Point(320, 339), 40, 40), kDown(Common::Point(320, 370), 40, 40);

void shutdown() {
	delete s_screens;
	s_screens = nullptr;
	s_first = s_hover = 0;
	s_selected = -1;
	for (int &b : s_bars)
		b = 0;
}

static int rowCentre(int r) {
	return 127 + kRowStep / 2 + kRowStep * r;
}

static Common::Rect rowRect(int r) {
	int top = rowCentre(r) - 35 / 2;
	return Common::Rect(335, top, 635, top + 35);
}

static bool canUp() {
	return s_first > 0;
}

static bool canDown() {
	return s_first + kRows < (int)screens().saves.size();
}

static Image *visual(RingEngine *vm, const char *name) {
	// Loose files in DATA\SY\VISUAL (load-from 'e'), drawn with draw type 3.
	static Common::HashMap<Common::String, Common::SharedPtr<Image> > cache;
	if (!cache.contains(name))
		cache[name].reset(vm->resources().loadImage(kZoneSY, name, false, "VISUAL"));
	return cache[name].get();
}

static void openLoad(RingEngine *vm) {
	screens().saves.clear();
	for (int slot : vm->saveSlots()) {
		SaveEntry e;
		e.slot = slot;
		if (vm->readSave(slot, e.description, e.name, nullptr))
			screens().saves.insert_at(0, e); // aList::Add puts each at the top
	}
	s_first = 0;
	s_selected = -1;
	screens().picture.reset();
	vm->puzSetAct(kPuzLoad);
}

static void select(RingEngine *vm, int entry) {
	s_selected = entry;
	Image *picture = nullptr;
	Common::String description, name;
	vm->readSave(screens().saves[entry].slot, description, name, &picture);
	screens().picture.reset(picture);
}

bool listTrack(RingEngine *vm, int x, int y) {
	// 0x46bd80: a usable arrow or a row takes the cursor 57; a usable arrow shows its _gur picture.
	s_hover = 0;
	if (vm->currentPlace() != kPuzLoad)
		return false;
	if (canUp() && kUp.contains(x, y))
		s_hover = 1;
	else if (canDown() && kDown.contains(x, y))
		s_hover = 2;
	bool row = false;
	for (int r = 0; r < kRows && s_first + r < (int)screens().saves.size(); r++)
		row = row || rowRect(r).contains(x, y);
	if (!s_hover && !row)
		return false;
	vm->cursors().set(57);
	return true;
}

bool listClick(RingEngine *vm, int x, int y) {
	// 0x46bc50; the event it raises (0x40d1f0) no zone handles.
	if (vm->currentPlace() != kPuzLoad)
		return false;
	if (canUp() && kUp.contains(x, y)) {
		s_first--;
		return true;
	}
	if (canDown() && kDown.contains(x, y)) {
		s_first++;
		return true;
	}
	for (int r = 0; r < kRows && s_first + r < (int)screens().saves.size(); r++) {
		if (rowRect(r).contains(x, y)) {
			select(vm, s_first + r);
			return true;
		}
	}
	return false;
}

void draw(RingEngine *vm, Graphics::ManagedSurface &dst) {
	int puzzle = vm->currentPlace();
	if (puzzle == kPuzStatus) {
		// 0x46ec60: GDI rectangles, filled RGB(255, 150, 0), outlined with the default (black) pen.
		static const int ys[4] = { 343, 343 + 28 + 1, 343 + 56 + 1, 343 + 84 - 1 };
		for (int k = 0; k < 4; k++) {
			if (s_bars[k] <= 0)
				continue;
			Common::Rect r(295, ys[k], 295 + s_bars[k], ys[k] + 4);
			dst.fillRect(r, dst.format.RGBToColor(255, 150, 0));
			dst.frameRect(r, dst.format.RGBToColor(0, 0, 0));
		}
		return;
	}
	if (puzzle != kPuzLoad)
		return;
	// 0x46bf90: the arrows, the rows (icon and two lines), the selected entry's picture.
	Image *up = visual(vm, !canUp() ? "up_gun.tga" : s_hover == 1 ? "up_gur.tga" : "up_gua.tga");
	Image *down = visual(vm, !canDown() ? "down_gun.tga" : s_hover == 2 ? "down_gur.tga" : "down_gua.tga");
	if (up)
		up->draw(dst, 330, 349, 3);
	if (down)
		down->draw(dst, 330, 380, 3);
	Graphics::WinFont *font = vm->font();
	Image *selectedIcon = visual(vm, "load_gua.tga");
	int iconHeight = selectedIcon ? selectedIcon->surface.h : 0;
	for (int r = 0; r < kRows && s_first + r < (int)screens().saves.size(); r++) {
		int i = s_first + r;
		bool selected = i == s_selected;
		if (Image *icon = selected ? selectedIcon : visual(vm, "load_gun.tga"))
			icon->draw(dst, 311, 137 + kRowStep / 2 + kRowStep * r - iconHeight / 2, 3);
		if (!font)
			continue;
		uint32 colour = selected ? dst.format.RGBToColor(245, 235, 50) : dst.format.RGBToColor(255, 95, 0);
		int h = font->getFontHeight();
		int y = rowCentre(r) - h / 2;
		font->drawString(&dst, screens().saves[i].description, 335, y, dst.w - 335, colour);
		font->drawString(&dst, screens().saves[i].name, 335, y + h + 3, dst.w - 335, colour);
	}
	if (screens().picture)
		screens().picture->draw(dst, 0, 0, 1);
}

// The save screen's name and caret (E-0262).
static void setName(RingEngine *vm) {
	World &w = vm->world();
	if (PuzzleText *t = w.text(kObjSaveName, 0, 0)) {
		t->text = screens().name;
		t->x = 344;
		t->y = 181;
	}
	if (PuzzleImage *caret = w.image(kObjSaveName, 0, 0)) {
		caret->x = (vm->font() ? vm->font()->getStringWidth(screens().name) : 0) + 346;
		caret->y = 181;
	}
}

static void openSave(RingEngine *vm) {
	screens().name.clear();
	setName(vm);
	screens().description = vm->describeSave();
	if (PuzzleText *t = vm->world().text(kObjSaveName, 0, 1)) {
		t->text = screens().description;
		t->x = 344;
		t->y = 155;
	}
	// osc.bmp: the F12 screen, 260 x 480; without one nothing is drawn.
	if (PuzzleImage *osc = vm->world().image(kObjSaveName, 0, 1)) {
		osc->image.reset(vm->savePicture());
		osc->active = osc->image != nullptr;
	}
	vm->puzSetAct(kPuzSave);
}

static void openStatus(RingEngine *vm) {
	// 0x46ed40: the four characters' scores, SY floats 90005..90008, clamped to 0..100.
	for (int k = 0; k < 4; k++) {
		float v = CLIP(vm->world().varFloat(90005 + k), 0.0f, 100.0f);
		if (PuzzleText *t = vm->world().text(kObjScores, 0, k))
			t->text = Common::String::format("%3.1f", v);
		s_bars[k] = (int)ceil(300 * v * 0.01);
	}
	vm->puzSetAct(kPuzStatus);
}

void onKey(RingEngine *vm, int code) {
	// 0x433d30: Delete on the load screen, typing on the save screen.
	int puzzle = vm->currentRotation() ? 0 : vm->currentPlace();
	if (puzzle == kPuzLoad && code == 0x2e) {
		askQuestion(vm, 4, "DoYouWantToDeleteSavedGame");
		return;
	}
	if (puzzle != kPuzSave || code == 13)
		return;
	if (code == 8) {
		if (!screens().name.empty())
			screens().name.deleteLastChar();
	} else if (code == 27) {
		screens().name.clear();
	} else if (vm->font() && vm->font()->getStringWidth(screens().name) >= 280) {
		return;
	} else {
		screens().name += (char)code;
	}
	setName(vm);
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
		vm->credits();
		break;
	case kObjLoad:
		openLoad(vm);
		break;
	case kObjSave:
		openSave(vm);
		break;
	case kObjStatus:
		openStatus(vm);
		break;
	case kObjContinue:
		// The game F12 left (`LoadSave("SaveGame", 1)`).
		if (!vm->continueGame())
			showWarning(vm, "CanNotCountineGame");
		break;
	case kObjLoadOK:
		if (s_selected < 0) {
			showWarning(vm, "SelectGame");
			break;
		}
		// ponytail: a failed load keeps the load screen with the warning; the original returns to the F12 game first
		if (vm->loadGameState(screens().saves[s_selected].slot).getCode() != Common::kNoError) {
			showWarning(vm, "CanNotLoadGame");
			break;
		}
		screens().saves.clear();
		screens().picture.reset();
		break;
	case kObjLoadCancel:
		vm->puzSetAct(kObjNewGame);
		screens().saves.clear();
		screens().picture.reset();
		break;
	case kObjSaveOK:
		// The F12 game in a new slot, then that game goes on.
		if (vm->saveToFreeSlot(screens().description, screens().name))
			vm->continueGame();
		else
			showWarning(vm, "CanNotSaveGame");
		break;
	case kObjSaveCancel:
	case kObjStatusOK:
		vm->puzSetAct(kObjNewGame);
		break;
	case kObjWarning:
		closeWarning(vm);
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
		if (value == 2) { // 0x408bc0, 0x431040: the set-ups again, then 0x431140: a new game in zone AS
			vm->resetWorld();
			vm->goZone(kZoneAS, vm->edition() == kEditionISO ? 998 : 999); // the ISO's plays the intro
		}
		if (value == 4) { // delete the selected save
			closeQuestion(vm, 4);
			if (s_selected < 0) {
				showWarning(vm, "SelectGame");
				break;
			}
			vm->deleteSave(screens().saves[s_selected].slot);
			screens().saves.remove_at(s_selected);
			s_selected = -1;
			screens().picture.reset();
			s_first = MAX(0, MIN(s_first, (int)screens().saves.size() - kRows));
		} else if (value == 5) {
			closeQuestion(vm, 4);
		}
		break;
	default:
		break;
	}
}

void onSound(RingEngine *vm, int id, int type, int reason, int ended) {
	// The end of Isha's last words (sound 90001): her picture goes, the credits, the menu.
	if (ended && id == kObjPreferences) {
		vm->world().hideAndFree(7);
		vm->credits();
		vm->startMenu(false);
	}
}

} // End of namespace SY
} // End of namespace Ring
