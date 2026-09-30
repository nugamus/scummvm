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

#ifndef GILBERT_MENU_H
#define GILBERT_MENU_H

#include "common/rect.h"
#include "common/str.h"

#include "graphics/managed_surface.h"

namespace Gilbert {

class GilbertEngine;
struct Picture;

/** The main menu and its pages (boot.md "Main menu"). */
class Menu {
public:
	enum Result {
		kStay,
		kQuit
	};

	explicit Menu(GilbertEngine *vm) : _vm(vm) {}

	/** One tick of mode 0: draw, mouse, cursor, flip, music. */
	Result tick();
	/** After the Intro film: menu1 again, starting on the next frame. */
	void musicAfterFilm();
	/** The room's first frame was shown (Continue becomes clickable). */
	void roomShown() { _shown = true; }
	/** The game's Menu button (boot.md "Button actions"): click 2, menu1, mode 0. */
	void enterFromGame();
	/** Variable 198 set: back to the menu with Continue and Save disabled. */
	void gameOver();
	/** After a load: Continue and Save on (or both off when it failed). */
	void gameLoaded(bool ok) {
		_running = _canSave = ok;
		_shown = false;
	}
	bool canSave() const { return _canSave; }

private:
	Picture *i2(int item) const;
	void draw();
	void drawColumn();
	void drawPage();
	void drawLoadPage();
	void drawSavePage();
	void drawSettingsPage();
	void drawHelpPage();
	void drawAboutPage();
	void drawRows(int count, int top, int hover, int picked);
	/** Draws a page button (plain, hover, pressed). */
	void pageButton(int item, int x, int y, int hover, int pressed);
	void dispatchPagePress();
	void handleMouse();
	void handleTyping();
	void action(int item);
	void pageAction(int item);
	void closeMenuState();
	void clickAndWait(int wave);
	void buildHelp();
	void buildCredits();
	int firstHit(const int *items, int count) const;
	int rowHit(int count) const;
	Common::Rect mouseRect() const;

	GilbertEngine *_vm;
	Result _result = kStay;

	bool _running = false;
	bool _canSave = false;
	bool _shown = false;
	int _hover = -1;
	int _pressed = -1;
	int _page = -1;
	bool _help = false;
	bool _credits = false;
	int _pageHover = -1;
	int _pagePress = -1;
	int _loadHover = 0, _loadPicked = 0;
	int _saveHover = 0, _savePicked = 0, _saveNamed = 0;
	int _loadTop = 0, _saveTop = 0;
	Common::String _name;
	bool _editing = false;
	int _lastAction = -1;
	int _counter = 0;
	int _fieldAlpha = 80;
	int _fieldStep = -2;
	double _scroll = 0;
	Graphics::ManagedSurface _helpText, _creditsText;
};

} // End of namespace Gilbert

#endif // GILBERT_MENU_H
