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

#ifndef GILBERT_GILBERT_H
#define GILBERT_GILBERT_H

#include "common/array.h"
#include "common/hashmap.h"
#include "common/keyboard.h"
#include "common/rect.h"
#include "common/str.h"
#include "common/ustr.h"

#include "engines/advancedDetector.h"
#include "engines/engine.h"

#include "graphics/managed_surface.h"

#include "gilbert/collection.h"
#include "gilbert/logic.h"

namespace Graphics {
class Font;
}

namespace Gilbert {

class Menu;
class Room;
class CloseUp;
class DialogBox;
class Sound;

/** Colours as the original's TColors give them (boot.md "Conventions"), RGB. */
enum : uint32 {
	kColourTan = 0xDDBD8E,
	kColourYellow = 0xFFFF00,
	kColourWhite = 0xFFFFFF,
	kColourBlack = 0x000000
};

/** Settings (boot.md "Settings"), kept in the game's configuration domain. */
struct Settings {
	bool fullscreenVideo = false;
	int installationType = -1;
	int soundVolume = 4;
	int musicVolume = 5;
};

class GilbertEngine : public Engine, public LogicListener {
public:
	GilbertEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~GilbertEngine() override;

	Common::Error run() override;

	// Screen: one 640x480 RGB565 page, drawn and then shown (boot.md "Conventions").
	Graphics::ManagedSurface &screen() { return _screen; }
	void clear();
	void present();
	/** Draws a picture with its top-left corner at (x, y), clipped, and records the place. */
	void drawPicture(Picture *pic, int x, int y);
	/** The same for pattern k of a picture. */
	void drawPattern(Picture *pic, int k, int x, int y);
	/** Pattern k stretched into `dst`, blended over the screen at alpha 0..255, clipped (Q-0202). */
	void blendPattern(Picture *pic, int k, const Common::Rect &dst, int alpha);
	/** A one-pixel outline inside `r`, like GDI's Rectangle with a hollow brush. */
	void frameRect(const Common::Rect &r, uint32 rgb);
	/** Draws part of an off-screen surface with black transparent, clipped. */
	void drawSurface(const Graphics::ManagedSurface &src, const Common::Rect &srcRect, int x, int y);
	/** Fills a rectangle with a colour at alpha 0..255 over what is there (Q-0202). */
	void fillAlpha(const Common::Rect &r, uint32 rgb, int alpha);

	// Text: Arial of the given point size (boot.md "Conventions", Q-0205).
	const Graphics::Font *font(int size, bool bold = false);
	int textWidth(const Common::U32String &text, int size, bool bold = false);
	void drawText(Graphics::ManagedSurface &dst, const Common::U32String &text, int x, int y, int size, uint32 rgb, bool bold = false);
	/** Line n of Data/misc/language.txt. */
	Common::U32String languageLine(uint n) const;
	static Common::U32String fromWindows1252(const Common::String &s);

	// Pictures (boot.md "Conventions").
	PictureCollection &interface1() { return _interface1; }
	PictureCollection &interface2() { return _interface2; }
	PictureCollection &cursors() { return _cursors; }

	Sound *sound() { return _sound; }
	Settings &settings() { return _settings; }
	void applyVolumes();
	void saveSettings();

	// Input, gathered by pollEvents().
	void pollEvents();
	Common::Point mouse() const { return _mouse; }
	/** A left press not yet taken; taking it clears it (boot.md "Mouse"). */
	bool takeLeftPress();
	/** Keys typed since the last call. */
	Common::Array<Common::KeyState> takeKeys();
	void warpMouse(int x, int y);

	/** movie::Play: a film of Data/mpg (boot.md "Films"). */
	void playFilm(const Common::String &name, bool fromIntro = false);
	/** boot::Exit: logo3 and the end of the program (boot.md "Exit"). */
	void exitGame();
	/** New game (boot.md "Button actions"): the database from default.dat, then event 1. */
	bool newGame();
	/** The Save and Load buttons (boot.md "Save slots"), slots 1..50. */
	bool saveSlot(int n, const Common::String &name);
	bool loadSlot(int n);

	bool hasFeature(EngineFeature f) const override {
		return f == kSupportsLoadingDuringRuntime || f == kSupportsSavingDuringRuntime || f == kSupportsReturnToLauncher;
	}
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override;
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override;
	Common::Error saveGameState(int slot, const Common::String &desc, bool isAutosave = false) override;
	Common::Error loadGameState(int slot) override;
	int getAutosaveSlot() const override { return -1; }

	/** Save slots 1..50 from gilbert.ini (boot.md "Save slots"). */
	void readSlotNames();
	const Common::String &slotName(int slot) const { return _slotNames[CLIP(slot, 1, 50)]; }

	// The main loop's modes (boot.md "Main loop").
	enum Mode {
		kModeMenu = 0,
		kModeRoom = 1,
		kModeCua = 2,
		kModeBook = 4,
		kModeLoading = 0x99
	};
	void setMode(int mode) { _mode = mode; }
	int mode() const { return _mode; }

	Logic *logic() { return _logic; }
	Menu *menu() { return _menu; }
	Room *room() { return _room; }
	CloseUp *closeUp() { return _cua; }
	DialogBox *dialog() { return _dialog; }
	PictureCollection &inventoryPictures() { return _inventory; }
	PictureCollection &gilbert() { return _gilbert; }
	bool leftHeld() const { return _leftHeld; }
	bool ctrlHeld() const;
	/** boot.md's DrawCursor: the mouse kept inside x 72..568, y 58..422, `cur[n]` at it. */
	void drawCursor(int n);
	/** Shows the page at a brightness 0..256 (the room fades, rooms.md "Fades"). */
	void present(int brightness);
	int brightness() const { return _brightness; }

	// LogicListener: the call-backs of the game rules (logic.md, rooms.md). Close-ups,
	// the inventory, dialogues and books come with the screens.
	void gotoWalkmap(uint32 id, int x, int y, int direction) override;
	void refreshWalkmap() override;
	void gotoCua(uint32 id) override;
	void refreshCua() override;
	void refreshInventory() override;
	void showDialog() override;
	void playWave(int list, int index, bool loop) override;
	void stopWave(int list, int index) override {}
	void playStream(const Common::String &name, bool loop, int kind) override;
	void startFilm(const Common::String &name) override { playFilm(name, false); }
	Common::Point gilbertPosition() override;
	int mapWidth() override;
	int mapHeight() override;
	int mapCell(int x, int y) override;
	void walk(int direction) override;
	void newTopic(bool shown) override;

private:
	void boot();
	void loadingStep(int step, uint line);
	void checkDatabase();
	void resetState();
	void loadLanguage();
	void loadSettings();

	const ADGameDescription *_gameDesc;
	Graphics::ManagedSurface _screen;
	Common::Rect _clip;
	PictureCollection _interface1, _interface2, _map, _cursors, _inventory, _gilbert;
	Common::HashMap<int, Graphics::Font *> _fonts;
	Common::Array<Common::String> _language;
	Common::String _slotNames[51];
	Settings _settings;
	Sound *_sound = nullptr;
	Logic *_logic = nullptr;
	Room *_room = nullptr;
	CloseUp *_cua = nullptr;
	DialogBox *_dialog = nullptr;
	int _mode = kModeMenu;
	int _brightness = 0;
	bool _leftHeld = false;
	Menu *_menu = nullptr;

	Common::Point _mouse;
	bool _leftPress = false;
	bool _escHeld = false;
	Common::Array<Common::KeyState> _keys;
};

} // End of namespace Gilbert

#endif // GILBERT_GILBERT_H
