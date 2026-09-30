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

#include "common/debug.h"
#include "common/file.h"
#include "common/system.h"

#include "gilbert/detection.h"
#include "gilbert/gilbert.h"
#include "gilbert/menu.h"
#include "gilbert/room.h"
#include "gilbert/sound.h"

namespace Gilbert {

namespace {

// The column at x 384 (boot.md "Frame" step 3): item, y, hover, pressed, disabled.
const struct ColumnButton {
	int item, y, hover, pressed, disabled;
} kColumn[] = {
	{ 0x0b, 70, 0x13, 0x1b, 0x24 },  // Fortsæt spil (Continue)
	{ 0x0c, 104, 0x14, 0x1c, -1 },   // Nyt spil (New game)
	{ 0x0d, 138, 0x15, 0x1d, -1 },   // Åbn spil (Load)
	{ 0x0e, 172, 0x16, 0x1e, 0x25 }, // Gem spil (Save)
	{ 0x0f, 206, 0x17, 0x1f, -1 },   // Indstillinger (Settings)
	{ 0x10, 240, 0x18, 0x20, -1 },   // Hjælp (Help)
	{ 0x11, 274, 0x19, 0x21, -1 },   // Om Gilbert (About)
	{ 0x12, 308, 0x1a, 0x22, -1 },   // Intro
	{ 0x33, 380, 0x70, 0x72, -1 },   // Afslut (Quit)
};

enum {
	kContinue = 0x0b, kNewGame = 0x0c, kLoad = 0x0d, kSave = 0x0e, kSettings = 0x0f,
	kHelp = 0x10, kAbout = 0x11, kIntro = 0x12, kExitButton = 0x33, kBack = 0x73,
	kSaveButton = 0x44, kLoadButton = 0x45, kUp = 0x63, kDown = 0x66,
	kHelpUp = 0x84, kHelpDown = 0x87, kMusic1 = 0x8a, kSound1 = 0x90, kVideo = 0x96
};

// The row fill (boot.md "Pages", Q-0202).
const uint32 kRowFill = 0xC42600;
const int kRowAlpha = 50;

} // End of anonymous namespace

Picture *Menu::i2(int item) const {
	return _vm->interface2()[item];
}

Common::Rect Menu::mouseRect() const {
	const Common::Point m = _vm->mouse();
	return Common::Rect(m.x - 3, m.y - 3, m.x + 3, m.y + 3);
}

int Menu::firstHit(const int *items, int count) const {
	const Common::Rect m = mouseRect();
	for (int i = 0; i < count; i++) {
		Picture *p = i2(items[i]);
		if (p && p->last.intersects(m))
			return items[i];
	}
	return -1;
}

// Load row k (1..6) and save row k (1..5) (boot.md "Mouse").
int Menu::rowHit(int count) const {
	const Common::Rect m = mouseRect();
	for (int k = 1; k <= count; k++)
		if (Common::Rect(111, 107 + 30 * (k - 1), 298, 134 + 30 * (k - 1)).intersects(m))
			return k;
	return 0;
}

// TgMain.DXTimer1Timer, mode 0 (boot.md "Main loop").
Menu::Result Menu::tick() {
	draw();
	handleMouse();
	handleTyping();
	// The cursor, and the mouse kept inside x 72..568, y 58..422.
	Common::Point m = _vm->mouse();
	if (m.x < 72 || m.x > 568 || m.y < 58 || m.y > 422) {
		m.x = CLIP<int16>(m.x, 72, 568);
		m.y = CLIP<int16>(m.y, 58, 422);
		_vm->warpMouse(m.x, m.y);
	}
	_vm->drawPicture(_vm->cursors()[0], m.x - 16, m.y - 16);
	_vm->present();
	if (_counter == 10)
		_vm->sound()->startStream(Sound::kMusic);
	return _result;
}

void Menu::enterFromGame() {
	_vm->sound()->playWave(1, 2);
	_vm->sound()->stopAll();
	_counter = 0;
	_vm->sound()->openStream(Sound::kMusic, "menu1", true, false);
	_vm->setMode(GilbertEngine::kModeMenu);
}

void Menu::gameOver() {
	_running = _canSave = false;
	_counter = 0;
	_vm->setMode(GilbertEngine::kModeMenu);
}

void Menu::musicAfterFilm() {
	_vm->sound()->openStream(Sound::kMusic, "menu1", true, false);
	_counter = 9;
}

// gmenu::Draw (boot.md "Frame").
void Menu::draw() {
	_vm->clear();
	if (_credits) {
		_vm->drawPicture(i2(0x03), 64, 50);
	} else {
		_vm->drawPicture(i2(0x00), 64, 50);
		_vm->drawPicture(i2(0x02), 352, 56);
	}
	_vm->drawPicture(_vm->interface1()[0], 64, 50);
	_vm->drawPicture(i2(0x32), 64, 337);

	if (!_help && !_credits)
		drawColumn();

	if (_credits) {
		_vm->drawPicture(i2(0x77), 509, 336);
		_vm->drawPicture(i2(kBack), 513, 340);
	}
	if (_help || _credits) {
		if (_hover == kBack)
			_vm->drawPicture(i2(0x74), 513, 340);
		if (_pressed == kBack) {
			_vm->drawPicture(i2(0x75), 513, 340);
			action(kBack);
		}
	}

	drawPage();
	if (_counter < 20)
		_counter++;
}

void Menu::drawColumn() {
	for (const ColumnButton &b : kColumn) {
		const bool disabled = (b.item == kContinue && !_running) || (b.item == kSave && !_canSave);
		_vm->drawPicture(i2(disabled ? b.disabled : b.item), 384, b.y);
	}
	for (const ColumnButton &b : kColumn) {
		const bool disabled = (b.item == kContinue && !_running) || (b.item == kSave && !_canSave);
		if (disabled)
			continue;
		if (_hover == b.item)
			_vm->drawPicture(i2(b.hover), 384, b.y);
		if (_pressed == b.item) {
			_vm->drawPicture(i2(b.pressed), 384, b.y);
			action(b.item);
		}
	}
}

// gmenu::HandleMouse (boot.md "Mouse").
void Menu::handleMouse() {
	static const int rightItems[] = { kContinue, kNewGame, kLoad, kSave, kSettings, kHelp, kAbout, kIntro, kBack, kExitButton };
	static const int leftItems[] = {
		kSaveButton, kLoadButton, kUp, kDown, kBack, kHelpUp, kHelpDown,
		0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, kVideo
	};
	const bool press = _vm->takeLeftPress();
	const Common::Rect m = mouseRect();
	const bool open = _help || _credits;
	// Continue counts only once a room has been shown since the last new game or load.
	const int *right = _shown ? rightItems : rightItems + 1;
	const int rightCount = _shown ? ARRAYSIZE(rightItems) : ARRAYSIZE(rightItems) - 1;

	if (open || m.intersects(Common::Rect(320, 50, 512, 440))) {
		_hover = firstHit(right, rightCount);
		if (press)
			_pressed = firstHit(right, rightCount);
	}
	if (open || m.intersects(Common::Rect(0, 0, 320, 440))) {
		_pageHover = firstHit(leftItems, 7);
		_loadHover = rowHit(6);
		_saveHover = rowHit(5);
		if (press) {
			_pagePress = firstHit(leftItems, ARRAYSIZE(leftItems));
			if (rowHit(6))
				_loadPicked = rowHit(6);
			if (rowHit(5))
				_savePicked = rowHit(5);
		}
	}
	if (press && !open && _pressed != -1)
		_page = _pressed - 10;
	if (press)
		debugC(1, kDebugScript, "Menu: press at (%d, %d): pressed %d, page press %d, rows %d/%d",
		       _vm->mouse().x, _vm->mouse().y, _pressed, _pagePress, _loadPicked, _savePicked);
}

// Typing a save name (boot.md "Typing").
void Menu::handleTyping() {
	for (const Common::KeyState &k : _vm->takeKeys()) {
		if (_page != 4 || !_editing || !_savePicked)
			continue;
		if (k.keycode == Common::KEYCODE_BACKSPACE) {
			if (!_name.empty())
				_name.deleteLastChar();
			continue;
		}
		char c = 0;
		if (k.keycode >= Common::KEYCODE_a && k.keycode <= Common::KEYCODE_z)
			c = 'A' + (k.keycode - Common::KEYCODE_a);
		else if (k.keycode >= Common::KEYCODE_0 && k.keycode <= Common::KEYCODE_9)
			c = '0' + (k.keycode - Common::KEYCODE_0);
		else if (k.keycode == Common::KEYCODE_SPACE)
			c = ' ';
		else if (k.ascii == ':' || k.ascii == '@' || k.ascii == '[' || k.ascii == '\\')
			c = (char)k.ascii;
		else if (k.ascii == 0xE4 || k.ascii == 0xC4)
			c = (char)0xC4; // Ä
		else if (k.ascii == 0xE5 || k.ascii == 0xC5)
			c = (char)0xC5; // Å
		else if (k.ascii == 0xF6 || k.ascii == 0xD6)
			c = (char)0xD6; // Ö
		if (c && _name.size() < 20)
			_name += c;
	}
}

void Menu::closeMenuState() {
	_page = _hover = _pressed = -1;
	_help = _credits = false;
}

void Menu::clickAndWait(int wave) {
	_vm->sound()->playWave(1, wave);
	while (_vm->sound()->isWavePlaying() && !_vm->shouldQuit()) {
		_vm->pollEvents();
		g_system->delayMillis(10);
	}
}

// gmenu::Action for the column and the back button (boot.md "Button actions").
void Menu::action(int item) {
	const bool first = item != _lastAction;
	_lastAction = item;
	Sound *snd = _vm->sound();
	switch (item) {
	case kContinue:
		if (first) {
			snd->playWave(1, 4);
			snd->stopAll();
			_vm->room()->restartMusic();
		}
		closeMenuState();
		// Back to the close-up ge.dll still has open (the original shows the room, Q-0501).
		_vm->setMode(_vm->logic()->cuaId() ? GilbertEngine::kModeCua : GilbertEngine::kModeRoom);
		break;
	case kNewGame:
		if (first) {
			snd->playWave(1, 4);
			snd->stopAll();
			_shown = false;
			if (_vm->newGame()) {
				_canSave = _vm->settings().installationType != -1;
				_running = true;
			}
		}
		closeMenuState();
		break;
	case kLoad:
	case kSave:
		if (first) {
			snd->playWave(1, 5);
			_vm->readSlotNames();
			_loadPicked = _savePicked = _saveNamed = 0;
		}
		break;
	case kSettings:
		if (first)
			snd->playWave(1, 5);
		break;
	case kHelp:
		if (first) {
			snd->playWave(1, 5);
			buildHelp();
		}
		_help = true;
		break;
	case kAbout:
		if (first) {
			snd->playWave(1, 5);
			snd->stopAll();
			buildCredits();
			snd->openStream(Sound::kCredits, "credit", true);
			_scroll = 0;
		}
		_credits = true;
		break;
	case kIntro:
		if (first)
			_vm->playFilm("intro.mpg", true);
		_pressed = _page = -1;
		_help = _credits = false;
		break;
	case kExitButton:
		snd->stopAll();
		if (first)
			clickAndWait(3);
		closeMenuState();
		_result = kQuit;
		break;
	case kBack:
		clickAndWait(4);
		if (_credits) {
			snd->stopAll();
			musicAfterFilm();
		}
		closeMenuState();
		break;
	default:
		break;
	}
}

// Page items (boot.md "Button actions"); they also clear the page hover and press.
void Menu::pageAction(int item) {
	Sound *snd = _vm->sound();
	Settings &s = _vm->settings();
	if (item >= kMusic1 && item < kMusic1 + 6) {
		snd->playWave(1, 0);
		s.musicVolume = item - kMusic1 + 1;
		_vm->applyVolumes();
	} else if (item >= kSound1 && item < kSound1 + 6) {
		s.soundVolume = item - kSound1 + 1;
		_vm->applyVolumes();
		snd->playWave(1, 0);
	} else if (item == kVideo) {
		snd->playWave(1, 0);
		s.fullscreenVideo = !s.fullscreenVideo;
	} else if (item == kUp || item == kDown) {
		snd->playWave(1, 1);
		const int d = item == kUp ? -1 : 1;
		if (_page == 3)
			_loadTop = CLIP(_loadTop + d, 0, 44);
		else
			_saveTop = CLIP(_saveTop + d, 0, 45);
		_editing = false;
	} else if (item == kLoadButton) {
		snd->playWave(1, 0);
		if (_loadPicked) {
			snd->stopAll();
			gameLoaded(_vm->loadSlot(_loadTop + _loadPicked));
		}
	} else if (item == kSaveButton) {
		snd->playWave(1, 0);
		if (_savePicked && !_vm->saveSlot(_saveTop + _savePicked, _name))
			warning("Gilbert: could not save slot %d", _saveTop + _savePicked);
		closeMenuState();
	}
	_pageHover = _pagePress = -1;
}

bool Menu::pageButton(int item, int x, int y, int hover, int pressed) {
	_vm->drawPicture(i2(item), x, y);
	if (_pageHover == item && hover >= 0)
		_vm->drawPicture(i2(hover), x, y);
	if (_pagePress == item) {
		if (pressed >= 0)
			_vm->drawPicture(i2(pressed), x, y);
		return true;
	}
	return false;
}

// gmenu::DrawPage (boot.md "Pages").
void Menu::drawPage() {
	if (_help) {
		drawHelpPage();
		return;
	}
	if (_credits) {
		drawAboutPage();
		return;
	}
	switch (_page) {
	case 3:
		drawLoadPage();
		break;
	case 4:
		if (_canSave)
			drawSavePage();
		break;
	case 5:
		drawSettingsPage();
		break;
	default:
		break;
	}
}

void Menu::drawRows(int count, int top, int hover, int picked) {
	if (hover)
		_vm->fillAlpha(Common::Rect(111, 107 + 30 * (hover - 1), 298, 134 + 30 * (hover - 1)), kRowFill, kRowAlpha);
	for (int k = 1; k <= count; k++) {
		const int n = top + k;
		const Common::String text = Common::String::format("%d: ", n) + _vm->slotName(n);
		_vm->drawText(_vm->screen(), GilbertEngine::fromWindows1252(text), 119, 85 + 30 * k, 8,
		              k == picked ? kColourYellow : kColourTan);
	}
}

void Menu::drawLoadPage() {
	_vm->drawPicture(i2(0x23), 102, 63);
	_vm->drawPicture(i2(0x3f), 102, 63);
	_vm->drawPicture(i2(0x38), 104, 100);
	drawRows(6, _loadTop, _loadHover, _loadPicked);
	if (pageButton(kLoadButton, 258, 302, 0x49, 0x4d))
		pageAction(kLoadButton);
	else if (pageButton(kUp, 301, 107, 0x64, 0x65))
		pageAction(kUp);
	else if (pageButton(kDown, 301, 272, 0x67, 0x68))
		pageAction(kDown);
}

void Menu::drawSavePage() {
	_vm->drawPicture(i2(0x23), 102, 63);
	_vm->drawPicture(i2(0x40), 102, 63);
	_vm->drawPicture(i2(0x39), 104, 100);
	drawRows(5, _saveTop, _saveHover, _savePicked);
	if (_savePicked && _savePicked != _saveNamed) {
		// A row picked for the first time, or another row: its name, and editing starts.
		_name = _vm->slotName(_saveTop + _savePicked);
		_editing = true;
		_saveNamed = _savePicked;
	}
	if (_savePicked) {
		_vm->fillAlpha(Common::Rect(111, 269, 298, 292), kRowFill, _fieldAlpha);
		_fieldAlpha += _fieldStep;
		if (_fieldAlpha <= 0) {
			_fieldAlpha = 0;
			_fieldStep = 5;
		} else if (_fieldAlpha >= 80) {
			_fieldAlpha = 80;
			_fieldStep = -2;
		}
		_vm->drawText(_vm->screen(), GilbertEngine::fromWindows1252(_name), 119, 274, 8, kColourTan);
	}
	if (pageButton(kSaveButton, 258, 302, 0x48, 0x4c))
		pageAction(kSaveButton);
	else if (pageButton(kUp, 301, 107, 0x64, 0x65))
		pageAction(kUp);
	else if (pageButton(kDown, 301, 241, 0x67, 0x68))
		pageAction(kDown);
}

void Menu::drawSettingsPage() {
	const Settings &s = _vm->settings();
	_vm->drawPicture(i2(0x23), 102, 63);
	_vm->drawPicture(i2(0x37), 102, 63);
	_vm->drawPicture(i2(0x3b), 108, 97);
	_vm->drawPicture(i2(0x3e), 108, 192);
	int act = -1;
	for (int j = 0; j < 6; j++) {
		const int x = 114 + 33 * j;
		_vm->drawPicture(i2(j == s.musicVolume - 1 ? 0x71 : 0x3c), x, 122);
		if (pageButton(kMusic1 + j, x, 153, -1, -1))
			act = kMusic1 + j;
		_vm->drawPicture(i2(j == s.soundVolume - 1 ? 0x71 : 0x3c), x, 216);
		if (pageButton(kSound1 + j, x, 247, -1, -1))
			act = kSound1 + j;
	}
	_vm->drawPicture(i2(0x7d), 112, 288);
	_vm->drawPicture(i2(s.fullscreenVideo ? 0x71 : 0x3c), 279, 293);
	if (pageButton(kVideo, 246, 294, -1, -1))
		act = kVideo;
	if (act != -1)
		pageAction(act);
}

void Menu::drawHelpPage() {
	_vm->drawPicture(i2(0x00), 64, 50);
	_vm->drawPicture(i2(0x6f), 64, 50);
	_vm->drawSurface(_helpText, Common::Rect(0, 0, 355, 320), 135, 95);
	_vm->drawPicture(_vm->interface1()[0], 64, 50);
	_vm->drawPicture(i2(0x32), 64, 337);
	_vm->drawPicture(i2(0x01), 192, 56);
	_vm->drawPicture(i2(0x77), 509, 336);
	_vm->drawPicture(i2(kBack), 513, 340);
	if (_hover == kBack)
		_vm->drawPicture(i2(0x74), 513, 340);
	if (_pressed == kBack)
		_vm->drawPicture(i2(0x75), 513, 320);
	// The arrows do nothing: the help text never scrolls.
	if (pageButton(kHelpUp, 503, 93, 0x85, 0x86) || pageButton(kHelpDown, 503, 313, 0x88, 0x89))
		_pageHover = _pagePress = -1;
}

void Menu::drawAboutPage() {
	const int t = (int)_scroll;
	_vm->drawSurface(_creditsText, Common::Rect(0, t, 440, MIN(t + 280, 3000)), 102, 55);
	_scroll += 0.5;
	if (_scroll >= 3000)
		_scroll = 0;
}

// Data/misc/help.txt into a 440x1400 surface (boot.md "Help").
void Menu::buildHelp() {
	_helpText.create(440, 1400, _vm->screen().format);
	_helpText.clear(0);
	Common::File f;
	if (!f.open("misc/help.txt"))
		return;
	bool bold = false;
	Common::String line;
	int y = 0;
	while (!f.eos()) {
		const char c = f.readByte();
		if (f.eos())
			break;
		if (c == '#' || c == '%') {
			bold = false;
		} else if (c == '$') {
			bold = true;
		} else if (c == '\r' || c == '\n') {
			_vm->drawText(_helpText, GilbertEngine::fromWindows1252(line), 0, y, 8, 0x804040, bold);
			line.clear();
			y += 10;
		} else {
			line += c;
		}
	}
}

// Data/misc/credits.txt into a 440x3000 surface (boot.md "About").
void Menu::buildCredits() {
	_creditsText.create(440, 3000, _vm->screen().format);
	_creditsText.clear(0);
	Common::File f;
	if (!f.open("misc/credits.txt"))
		return;
	int size = 9;
	uint32 colour = kColourTan;
	Common::String line;
	int y = 300;
	while (!f.eos()) {
		const char c = f.readByte();
		if (f.eos())
			break;
		if (c == '#') {
			size = 9;
			colour = kColourTan;
		} else if (c == '$') {
			size = 9;
			colour = kColourYellow;
		} else if (c == '%') {
			size = 11;
			colour = kColourWhite;
		} else if (c == '\r' || c == '\n') {
			const Common::U32String text = GilbertEngine::fromWindows1252(line);
			const int w = _vm->textWidth(text, size);
			_vm->drawText(_creditsText, text, 221 - w / 2, y + 1, size, 0x031602);
			_vm->drawText(_creditsText, text, 220 - w / 2, y, size, colour);
			line.clear();
			y += 10;
		} else {
			line += c;
		}
	}
}

} // End of namespace Gilbert
