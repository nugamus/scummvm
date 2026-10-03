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

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/system.h"
#include "graphics/font.h"
#include "graphics/pixelformat.h"

#include "engines/util.h"

#include "grumpa/grumpa.h"

namespace Grumpa {

GrumpaEngine::GrumpaEngine(OSystem *syst, const ADGameDescription *gameDesc)
	: Engine(syst), _gameDesc(gameDesc) {
}

GrumpaEngine::~GrumpaEngine() {
	delete _menuFont;
}

Common::Error GrumpaEngine::run() {
	// The original draws to one 800x600 16-bit (RGB555) page; use that format (E-0010).
	// Scene rendering (2D background + .fxi depth + 3D actors) comes once the scene formats
	// are specced (games/grumpa docs, Q-0005..Q-0007).
	Graphics::PixelFormat format = Graphics::PixelFormat(2, 5, 5, 5, 0, 10, 5, 0, 0);
	initGraphics(kScreenWidth, kScreenHeight, &format);
	_screen.create(kScreenWidth, kScreenHeight, g_system->getScreenFormat());

	debug(1, "Grumpa: booted, %dx%d %d-bit surface", kScreenWidth, kScreenHeight,
		  _screen.format.bytesPerPixel * 8);

	// The game data sits in subdirectories (Bitmaps, Actors, Scenes, ...) of the game dir.
	SearchMan.addDirectory("gamedir", ConfMan.getPath("path"), 0, 2);
	setGameCursor();

	_screen.clear();
	// Boot sequence: the intro film, then the main menu. Selecting "Nytt Spel" enters the
	// first scene view; "Avsluta Spel" quits.
	if (!(ConfMan.hasKey("dev_skip_intro") && ConfMan.getBool("dev_skip_intro")))
		playMovie("grumpa_intro");
	Common::Array<Common::U32String> items;
	loadMenuText(items);
	int sel = 0;
	int count = (int)items.size();
	// Menu items (Nordic Text.txt order): 0 New Game, 1 Load, 2 Continue, 3 Settings,
	// 4 Help, 5 Credits, 6 Quit.
	enum { kNewGame = 0, kHelp = 4, kCredits = 5, kQuit = 6 };
	enum { kMenu, kScene } state = kMenu;
	bool dirty = true;
	uint32 lastSceneDraw = 0;
	if (ConfMan.hasKey("save_slot"))  // started from the launcher's Load
		loadGameState(ConfMan.getInt("save_slot"));
	while (!shouldQuit()) {
		if (_restoring && _nextScene >= 0) {  // a loaded save (saveload.cpp)
			enterScene(_nextScene);
			_nextScene = -1;
			_restoring = false;
			_cursorName = "";
			state = kScene;
			dirty = true;
		}
		Common::Event event;
		while (g_system->getEventManager()->pollEvent(event)) {
			if (count == 0)
				continue;
			if (state == kScene) {
				if (event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_ESCAPE) {
					state = kMenu;
					dirty = true;
				} else if (event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_h) {
					_showHotspots = !_showHotspots;  // overlay the clickable trigger polygons
					dirty = true;
				} else if (event.type == Common::EVENT_MOUSEMOVE) {
					if (!_inventory.showHeldCursor())  // the held item is the cursor
						updateHoverCursor(event.mouse);  // hand over a clickable exit/interaction
				} else if (event.type == Common::EVENT_RBUTTONUP) {
					_inventory.command(Inventory::kPanelId, 19, 0, 0);  // right click: the inventory
					dirty = true;
				} else if (event.type == Common::EVENT_LBUTTONUP && _inventory.click(event.mouse)) {
					if (!_inventory.showHeldCursor())
						_cursorName = "", updateHoverCursor(event.mouse);
					dirty = true;
				} else if (event.type == Common::EVENT_LBUTTONUP) {
					if (handleSceneClick(event.mouse)) {
						if (_nextScene >= 0) {  // a go-to-scene trigger (E-0116)
							enterScene(_nextScene);
							_nextScene = -1;
							_cursorName = "";  // refresh hover for the new scene's triggers
						}
						dirty = true;  // redraw with the new scene / actor state
					}
				}
				continue;
			}
			int activate = -1;
			if (event.type == Common::EVENT_KEYDOWN) {
				if (event.kbd.keycode == Common::KEYCODE_UP) {
					sel = (sel + count - 1) % count;
					dirty = true;
				} else if (event.kbd.keycode == Common::KEYCODE_DOWN) {
					sel = (sel + 1) % count;
					dirty = true;
				} else if (event.kbd.keycode == Common::KEYCODE_RETURN
						   || event.kbd.keycode == Common::KEYCODE_KP_ENTER) {
					activate = sel;
				}
			} else if (event.type == Common::EVENT_MOUSEMOVE) {
				int hit = menuItemAt(event.mouse);
				if (hit >= 0 && hit != sel) {
					sel = hit;
					dirty = true;
				}
			} else if (event.type == Common::EVENT_LBUTTONUP) {
				activate = menuItemAt(event.mouse);
			}
			if (activate >= 0) {
				if (activate == kQuit)
					return Common::kNoError;
				else if (activate == kNewGame) {
					// The first scene is the tutorial hut (scene 1); render it live with its
					// animated sprite props (docs/spec/scene.md). Scene navigation and the
					// game logic are the next layers.
					_inventory.load();
					enterScene(1);
					state = kScene;
					dirty = true;
				} else if (activate == kHelp) {
					showTextScreen("UI/001_Menu/Help.txt");
					dirty = true;
				} else if (activate == kCredits) {
					showTextScreen("UI/001_Menu/Credits.txt");
					dirty = true;
				}
			}
		}
		if (dirty && state == kMenu) {
			drawMenu(sel);
			dirty = false;
		}
		if (state == kScene) {
			// Redraw the scene at ~15 fps for the sprite animation (the original ran a 10 ms
			// timer; the props cycle at a few fps).
			uint32 now = g_system->getMillis();
			if (dirty || now - lastSceneDraw >= 66) {
				renderSceneFrame(now);
				_inventory.draw(_screen);
				lastSceneDraw = now;
				dirty = false;
			}
		}
		g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0,
								   kScreenWidth, kScreenHeight);
		g_system->updateScreen();
		g_system->delayMillis(10);
	}
	return Common::kNoError;
}

} // End of namespace Grumpa
