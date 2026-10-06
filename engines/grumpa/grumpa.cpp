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

#include "common/archive.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/hashmap.h"
#include "common/ptr.h"
#include "common/compression/installshield_cab.h"
#include "common/system.h"
#include "graphics/font.h"
#include "graphics/pixelformat.h"

#include "engines/util.h"

#include "grumpa/console.h"
#include "grumpa/events.h"
#include "grumpa/grumpa.h"

namespace Grumpa {

GrumpaEngine::GrumpaEngine(OSystem *syst, const ADGameDescription *gameDesc)
	: Engine(syst), _gameDesc(gameDesc) {
	_events = new EventVM(this);
	setDebugger(new Console(this));
	_voices = new Voices(_mixer, &_characters);
	_inventory.attach(&_characters, _events);
	_characters.attach(this, _events);
}

GrumpaEngine::~GrumpaEngine() {
	delete _menuFont;
	delete _voices;
	delete _events;
}

// The CD keeps the game data in its InstallShield cabinet data1.hdr + data*.cab, by file
// group; a language's groups are "Sounds Swedish" there and "Sounds_Swedish" unpacked.
// This names the cabinet's members as unpacked, so every lookup reads both alike (E-1000).
class CabinetArchive : public Common::Archive {
public:
	CabinetArchive(Common::Archive *cab) : _cab(cab) {
		Common::ArchiveMemberList list;
		cab->listMembers(list);
		for (auto &m : list) {
			Common::String s = m->getPathInArchive().toString('/');
			for (uint i = 0; i < s.size() && s[i] != '/'; i++)
				if (s[i] == ' ')
					s.setChar('_', i);
			_names[Common::Path(s)] = m->getPathInArchive();
		}
	}
	bool hasFile(const Common::Path &path) const override { return _names.contains(path); }
	int listMembers(Common::ArchiveMemberList &list) const override {
		for (const auto &n : _names)
			list.push_back(getMember(n._key));
		return _names.size();
	}
	const Common::ArchiveMemberPtr getMember(const Common::Path &path) const override {
		if (!hasFile(path))
			return Common::ArchiveMemberPtr();
		return Common::ArchiveMemberPtr(new Common::GenericArchiveMember(path, *this));
	}
	Common::SeekableReadStream *createReadStreamForMember(const Common::Path &path) const override {
		Common::Path inCab;
		return _names.tryGetVal(path, inCab) ? _cab->createReadStreamForMember(inCab) : nullptr;
	}

private:
	Common::ScopedPtr<Common::Archive> _cab;
	Common::HashMap<Common::Path, Common::Path, Common::Path::IgnoreCase_Hash, Common::Path::IgnoreCase_EqualTo> _names;
};

// Actor 185's fade scales every colour channel by level / 255 (a gamma ramp, E-0700).
static void fadeScreen(Graphics::ManagedSurface &screen, int level) {
	if (level >= 255)
		return;
	const Graphics::PixelFormat &f = screen.format;
	for (int y = 0; y < screen.h; y++) {
		uint16 *p = (uint16 *)screen.getBasePtr(0, y);
		for (int x = 0; x < screen.w; x++, p++) {
			byte r, g, b;
			f.colorToRGB(*p, r, g, b);
			*p = f.RGBToColor(r * level / 255, g * level / 255, b * level / 255);
		}
	}
}

void GrumpaEngine::drawFrame(uint32 now) {
	renderSceneFrame(now);
	_events->score().draw(_screen);  // layer 6, like the panel
	_inventory.draw(_screen);
	fadeScreen(_screen, _events->fadeLevel());
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
	if (Common::Archive *cab = Common::makeInstallShieldArchive("data", true))  // run from the CD
		SearchMan.add("cabinet", new CabinetArchive(cab));
	_events->newGame();  // the global actors of Actors/global.atx (E-0205)
	setGameCursor();

	_screen.clear();
	// Boot sequence: the intro film, then the main menu. Selecting "Nytt Spel" enters the
	// first scene view; "Avsluta Spel" quits.
	playMovie("grumpa_intro");
	_events->startAmbience();  // the boot plays it under the main menu (E-0902)
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
				if (event.type == Common::EVENT_LBUTTONUP)
					_leftHeld = false;  // actor 3 stops (E-0811)
				if (event.type == Common::EVENT_KEYDOWN || event.type == Common::EVENT_KEYUP) {
					const bool down = event.type == Common::EVENT_KEYDOWN;
					if (event.kbd.keycode == Common::KEYCODE_LCTRL || event.kbd.keycode == Common::KEYCODE_RCTRL)
						_ctrlHeld = down;  // the stance (E-1400)
					else if (event.kbd.keycode == Common::KEYCODE_SPACE)
						_spaceHeld = down;  // jump (E-0812)
				}
				if (event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_ESCAPE) {
					if (!_events->fadeIdle())  // not during a fade or on black (E-0700)
						continue;
					state = kMenu;
					_leftHeld = _leftWas = _rightHeld = _ctrlHeld = _spaceHeld = false;  // key-ups go to the menu
					dirty = true;
				} else if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START && event.customType == kActionDismount) {
					_backspace = true;  // leave a mount, else let the companion go (no key repeats)
				} else if (event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_h) {
					_showHotspots = !_showHotspots;  // overlay the clickable trigger polygons
					dirty = true;
				} else if (event.type == Common::EVENT_MOUSEMOVE) {
					updateHoverCursor(event.mouse);
				} else if (event.type == Common::EVENT_LBUTTONDOWN) {
					_leftHeld = true;  // actor 3 walks while it is held (E-0811)
				} else if (event.type == Common::EVENT_RBUTTONDOWN) {
					_rightHeld = true;  // in the stance: block (E-1400)
					if (!_ctrlHeld) {   // else: the inventory (E-0810)
						_inventory.command(Inventory::kPanelId, 19, 0, 0);
						dirty = true;
					}
				} else if (event.type == Common::EVENT_RBUTTONUP) {
					_rightHeld = false;
				} else if (event.type == Common::EVENT_LBUTTONUP && _inventory.click(event.mouse)) {
					updateHoverCursor(event.mouse);
					int request = _inventory.takeRequest();  // the panel's buttons (E-0901)
					if (request == 60)
						state = kMenu, _leftHeld = _leftWas = _rightHeld = _ctrlHeld = _spaceHeld = false;
					else if (request == 61)
						saveGameDialog();
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
					_events->newGame();
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
			// The event VM: one update per 20 ms, at most one when far behind (E-0202).
			if (now - _lastUpdate > 1000)
				_lastUpdate = now - EventVM::kUpdateMs;
			while (now - _lastUpdate >= EventVM::kUpdateMs && _nextScene < 0) {
				_lastUpdate += EventVM::kUpdateMs;
				_events->update();
				dirty = true;
			}
			int request = _events->takeRequest();  // actor 1: 60 the main menu, 61 the saves
			if (request == 60)
				state = kMenu, _leftHeld = _leftWas = false, dirty = true;
			else if (request == 61)
				saveGameDialog(), dirty = true;
			if (_nextScene >= 0) {  // 185 op 31 (E-0206)
				enterScene(_nextScene);
				_nextScene = -1;
				_cursorName = "";
				_lastUpdate = g_system->getMillis();
			}
			if (dirty || now - lastSceneDraw >= 66) {
				drawFrame(now);
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
