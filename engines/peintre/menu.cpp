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

#include "peintre/gfx.h"
#include "peintre/peintre.h"
#include "peintre/shell.h"
#include "peintre/sound.h"

namespace Peintre {

// The option menu (ui.md "Option menu", E-0415) and the credits after the end (E-0418).

static Common::Rect rect(int x, int y, int w, int h) {
	return Common::Rect(x, y, x + w, y + h);
}

static void drawBackground(Graphics::Surface &dst, const char *name) {
	Graphics::Surface s;
	if (!loadTgp(name, s))
		return;
	dst.copyRectToSurface(s, 0, 0, Common::Rect(MIN<int>(s.w, 640), MIN<int>(s.h, 480)));
	s.free();
}

int PeintreEngine::runOptionMenu(bool from3D) {
	enum Page { kMain, kVolume, kSize, kKeyboard, kCredits, kQuit, kLoad };
	// Buttons 0..5 (0x4a65c0); `options` frame b is drawn at the button's top-left while
	// pressed, except frames 1..3 (the whole Options panel), Q-0354.
	static const Common::Rect kButtons[6] = {
		rect(186, 66, 268, 85), rect(325, 166, 128, 19), rect(325, 186, 128, 19),
		rect(325, 206, 128, 19), rect(187, 243, 268, 85), rect(187, 331, 268, 83)
	};
	static const Common::Point kPanel(187, 154);
	static const Common::Rect kVolumeBar = rect(205, 199, 227, 9);
	static const Common::Rect kSizes[4] = {
		rect(284, 174, 69, 15), rect(284, 238, 60, 15), rect(284, 301, 71, 15), rect(283, 357, 66, 15)
	};
	static const Common::Point kSizeMarks[4] = {
		Common::Point(246, 174), Common::Point(244, 237), Common::Point(246, 300), Common::Point(246, 356)
	};
	static const int kRowY[4] = { 168, 239, 309, 379 };
	static const int kRowClickY[4] = { 140, 211, 281, 351 };
	static const Common::Rect kUp = rect(436, 135, 13, 12), kDown = rect(436, 401, 13, 12);
	static const Common::Rect kYes = rect(255, 281, 33, 17), kNo = rect(358, 281, 36, 17);

	Graphics::Surface saved, bg;
	saved.copyFrom(_screen);
	bg.create(640, 480, _screen.format);
	SpriteBank options, cursopt, lcaps, cursors;
	Font font;
	options.load("options");
	cursopt.load("cursopt");
	lcaps.load("lcaps");
	cursors.load("Curseurs");
	font.load("trobo12");

	PlayerRecord &player = _players[_player];
	// v = volume % × 211 / 100; the record holds (percent - 100) × 50.
	int v = CLIP<int>((player.volume / 50 + 100) * 211 / 100, 0, 210);
	uint viewSize = player.viewSize;
	// Q-0355: the saved games, slots 1..34, in slot order (the original sorts by file time).
	Common::Array<uint> games;
	for (uint s = 1; s < kNumObjects; s++)
		if (gameExists(_player, s))
			games.push_back(s);

	Page page = kMain;
	drawBackground(bg, "option");
	int pressed = -1;
	bool knob = false, wasDown = buttonDown();
	int result = -3;   // none yet
	uint first = 0, credit = 0, creditTicks = 0;
	auto setPage = [&](Page p, const char *background) {
		page = p;
		if (background)
			drawBackground(bg, background);
	};

	while (result == -3) {
		if (shouldQuit()) {
			result = -2;
			break;
		}
		pollInput();
		const Common::Point m = mouse();
		const bool down = buttonDown(), clicked = down && !wasDown, released = !down && wasDown;
		wasDown = down;
		uint cursor = kCursorDefault;

		if (keyFired(Common::KEYCODE_ESCAPE)) {
			// Esc goes back to the first page, and closes the menu from there.
			if (page == kMain)
				result = -1;
			else
				setPage(kMain, page == kVolume ? nullptr : "option");
			pressed = -1;
			knob = false;
			continue;
		}

		switch (page) {
		case kMain:
		case kVolume:
			for (int b = 0; b < 6; b++) {
				if (page == kVolume && b >= 1 && b <= 3)
					continue; // the volume row covers them
				if (!kButtons[b].contains(m))
					continue;
				cursor = kCursorButton;
				if (clicked)
					pressed = b;
			}
			if (page == kVolume) {
				if (clicked && (kVolumeBar.contains(m) || rect(214 + v - 6, 197, 15, 10).contains(m)))
					knob = true;
				if (knob && down)
					v = CLIP<int>(m.x - 214, 0, 210);
				if (!down)
					knob = false;
			}
			if (released && pressed >= 0) {
				// Q-0354: a button acts when released over it.
				const int b = pressed;
				pressed = -1;
				if (!kButtons[b].contains(m))
					break;
				switch (b) {
				case 0:
					first = 0;
					setPage(kLoad, "load");
					break;
				case 1:
					setPage(kVolume, nullptr);
					break;
				case 2:
					setPage(kSize, "scrsize");
					break;
				case 3:
					setPage(kKeyboard, "keyboard");
					break;
				case 4:
					credit = 0;
					creditTicks = 0;
					setPage(kCredits, "Credit00");
					break;
				default:
					setPage(kQuit, "quit");
					break;
				}
			}
			break;

		case kSize:
			for (uint i = 0; i < 4; i++) {
				if (!kSizes[i].contains(m))
					continue;
				cursor = kCursorButton;
				if (clicked)
					viewSize = i;
			}
			break;

		case kKeyboard:
			break;

		case kCredits:
			// Credit00..15, each for 250 ticks or until a click or Space.
			if (++creditTicks > 250 || clicked || keyFired(Common::KEYCODE_SPACE)) {
				creditTicks = 0;
				if (++credit >= 16)
					setPage(kMain, "option");
				else
					drawBackground(bg, Common::String::format("Credit%02u", credit).c_str());
			}
			break;

		case kQuit:
			if (kYes.contains(m) || kNo.contains(m))
				cursor = kCursorButton;
			if (clicked && kNo.contains(m)) {
				setPage(kMain, "option");
			} else if (clicked && kYes.contains(m)) {
				pressed = 11;   // shows Yes for the last frame
				player.volume = (v * 100 / 211 - 100) * 50;
				player.viewSize = viewSize;
				writeResume(from3D ? 0 : 1);
				savePlayers();
				result = -2;
			}
			break;

		case kLoad:
			for (uint i = 0; i < 4 && first + i < games.size(); i++) {
				if (!rect(194, kRowClickY[i], 57, 57).contains(m))
					continue;
				cursor = kCursorButton;
				if (clicked)
					result = _player * 100 + games[first + i];
			}
			if (kUp.contains(m) || kDown.contains(m))
				cursor = kCursorButton;
			if (clicked && kUp.contains(m) && first > 0)
				first--;
			if (clicked && kDown.contains(m) && first + 4 < games.size())
				first++;
			break;
		}

		// Draw.
		_screen.copyRectToSurface(bg, 0, 0, Common::Rect(640, 480));
		switch (page) {
		case kMain:
		case kVolume:
			if (page == kVolume) {
				options.draw(_screen, 6, 188, 162);
				cursopt.draw(_screen, 0, 214 + v, 201);
			}
			if (pressed >= 0) {
				const Common::Point at = pressed >= 1 && pressed <= 3 ? kPanel
					: Common::Point(kButtons[pressed].left, kButtons[pressed].top);
				options.draw(_screen, pressed, at.x, at.y);
			}
			break;
		case kSize:
			options.draw(_screen, 7 + viewSize, kSizeMarks[viewSize].x, kSizeMarks[viewSize].y);
			break;
		case kQuit:
			// No is preselected (frame 12).
			if (pressed == 11)
				options.draw(_screen, 11, 255, 281);
			else
				options.draw(_screen, 12, 357, 281);
			break;
		case kLoad:
			for (uint i = 0; i < 4 && first + i < games.size(); i++) {
				lcaps.draw(_screen, games[first + i], 222, kRowY[i]);
				font.draw(_screen, Common::String::format("Game %u", first + i + 1), 304, kRowY[i] + 5, 0xCE59);
			}
			break;
		default:
			break;
		}
		drawCursor(_screen, cursors, cursor, m);
		present();
		waitTick(40);
	}

	// Closing restores the screen and applies volume and view size.
	player.volume = (v * 100 / 211 - 100) * 50;
	player.viewSize = viewSize;
	_sound->setVolume(player.volume);
	_screen.copyFrom(saved);
	present();
	saved.free();
	bg.free();
	return result;
}

void PeintreEngine::runEndCredits() {
	// Sound `Credits` looping, Credit00..15 for 250 ticks each; a click or Space ends them;
	// then wait for the button to be up and quit (E-0418).
	_sound->playStatic("Credits", true);
	bool wasDown = buttonDown();
	for (uint credit = 0; credit < 16 && !shouldQuit(); credit++) {
		drawBackground(_screen, Common::String::format("Credit%02u", credit).c_str());
		present();
		bool end = false;
		for (uint t = 0; t <= 250 && !shouldQuit(); t++) {
			pollInput();
			const bool clicked = buttonDown() && !wasDown;
			wasDown = buttonDown();
			if (clicked || keyFired(Common::KEYCODE_SPACE)) {
				end = true;
				break;
			}
			waitTick(40);
		}
		if (end)
			break;
	}
	while (buttonDown() && !shouldQuit()) {
		pollInput();
		waitTick(40);
	}
	_sound->stopStatic("Credits");
	quitGame();
}

} // End of namespace Peintre
