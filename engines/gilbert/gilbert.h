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
#include "common/events.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/keyboard.h"
#include "common/rect.h"
#include "common/str.h"
#include "common/ustr.h"

#include "engines/advancedDetector.h"
#include "engines/engine.h"

#include "graphics/hotspot_renderer.h"
#include "graphics/managed_surface.h"

#include "gilbert/collection.h"
#include "gilbert/detection.h"
#include "gilbert/logic.h"

namespace Graphics {
class Font;
}

namespace Gilbert {

class Menu;
class Room;
class CloseUp;
class Book;
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

/**
 * Text, lines and pictures laid out once and drawn each frame through a window: the book's
 * pages, the help and the credits, which the original draws into off-screen surfaces. Kept
 * as a list so that the text is drawn at the screen's resolution, over what lies under it.
 */
struct TextPage {
	struct Item {
		enum Type { kText, kPicture } type = kText;
		int x = 0, y = 0;
		int underline = -1; ///< the underline's y, -1 none
		int underlineW = 0;  ///< its length in the page's layout
		Common::U32String text;
		int size = 8;
		uint32 rgb = 0;
		bool bold = false;
		Picture *picture = nullptr;
	};
	Common::Array<Item> items;

	void clear() { items.clear(); }
	void text(const Common::U32String &s, int x, int y, int size, uint32 rgb, bool bold = false);
	/** Underlines the last text at y, w long in the layout (as long as the drawn text at a larger scale). */
	void underlineLast(int y, int w);
	/** A picture, fuchsia transparent. */
	void picture(Picture *pic, int x, int y);
};

/** The launcher's game options, the enhancements (metaengine.cpp). */
struct Options {
	bool alwaysRun = false;
	bool shortcuts = false;
	bool wheel = false;
	bool markChoices = false;
	bool newTopics = false;
	bool autosave = false;
	bool fullscreenFilms = false;
	bool smoothText = false;
	bool highResText = false;
};

class GilbertEngine : public Engine, public LogicListener {
public:
	GilbertEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~GilbertEngine() override;

	Common::Error run() override;

	// Screen: one 640x480 RGB565 page, drawn and then shown (boot.md "Conventions"). With
	// the high_res_text option the page is kept at twice the size: everything is drawn in
	// the original's coordinates, pictures doubled and text at the page's resolution.
	Graphics::ManagedSurface &screen() { return _screen; }
	int scale() const { return _scale; }
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
	/** Draws the part `src` of a page with its top-left corner at (x, y), clipped. */
	void drawPage(const TextPage &page, const Common::Rect &src, int x, int y);
	/** Fills a rectangle with a colour at alpha 0..255 over what is there (Q-0202). */
	void fillAlpha(const Common::Rect &r, uint32 rgb, int alpha);

	// Text: Arial of the given point size (boot.md "Conventions", Q-0205).
	const Graphics::Font *font(int size, bool bold = false);
	int textWidth(const Common::U32String &text, int size, bool bold = false);
	/** Text on the page; not clipped (boot.md "Conventions"). */
	void drawText(const Common::U32String &text, int x, int y, int size, uint32 rgb, bool bold = false);
	/** Line n of Data/misc/language.txt. */
	Common::U32String languageLine(uint n) const;
	static Common::U32String fromWindows1252(const Common::String &s);

	// Pictures (boot.md "Conventions").
	PictureCollection &interface1() { return _interface1; }
	PictureCollection &interface2() { return _interface2; }
	PictureCollection &cursors() { return _cursors; }

	Sound *sound() { return _sound; }
	Settings &settings() { return _settings; }
	const Options &options() const { return _options; }

	/**
	 * The keyboard shortcuts and the mouse wheel (options): the action of this tick
	 * (GilbertAction), kActionNone if none; a screen that acts on it takes it.
	 */
	int shortcut() const { return _shortcut; }
	void takeShortcut() { _shortcut = kActionNone; }

	/**
	 * What the player has seen, for the options that mark it: dialogue choices picked and
	 * book topics opened. Kept beside each save in `<target>.game<n>.dat.seen`.
	 */
	static Common::String choiceKey(uint32 dialog, const Common::String &text);
	static Common::String topicKey(int book, uint32 topic);
	bool isSeen(const Common::String &key) const { return _seen.contains(key); }
	void markSeen(const Common::String &key) { _seen[key] = true; }

	// ScummVM's hotspot overlay (H): the room's areas, the close-up's objects.
	void getHotspotPositions(Common::Array<Graphics::HotspotInfo> &hotspots) override;
	void drawHotspots() override;
	bool hotspotsShown() const { return _showHotspots; }
	void applyVolumes();
	void saveSettings();

	// Input, gathered by pollEvents().
	void pollEvents();
	Common::Point mouse() const { return _mouse; }
	/** The screens that watch the button state, each with its own last-seen state. */
	enum Screen {
		kScreenMenu,
		kScreenRoom,
		kScreenCua,
		kScreenDialog,
		kScreenBook,
		kScreenCount
	};
	/**
	 * The button state (boot.md "Mouse", screens.md "Conventions"): -1, or 1 (left) / 2
	 * (right), set on a mouse down while -1 (and on a move with that button held), -1 on any
	 * mouse up. A screen sees a press when the state changed to 1 since it last looked;
	 * `changed` tells whether it changed at all.
	 */
	bool press(Screen screen, bool *changed = nullptr);
	/** A screen shown now starts from the present button state: the click that showed it is not its press. */
	void syncPress(Screen screen);
	/** Films, fades and waits pump events: the ScummVM menu must not save or load then. */
	void setBusy(bool busy) { _busy += busy ? 1 : -1; }
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
	void setMode(int mode);
	int mode() const { return _mode; }

	Logic *logic() { return _logic; }
	Menu *menu() { return _menu; }
	Room *room() { return _room; }
	CloseUp *closeUp() { return _cua; }
	Book *book() { return _book; }
	DialogBox *dialog() { return _dialog; }
	PictureCollection &inventoryPictures() { return _inventory; }
	PictureCollection &gilbert() { return _gilbert; }
	bool leftHeld() const { return _buttonState == 1; }
	bool ctrlHeld() const;
	/** boot.md's DrawCursor: the mouse kept inside x 72..568, y 58..422, `cur[n]` at it. */
	void drawCursor(int n);
	/** Shows the page at a brightness 0..256 (the room fades, rooms.md "Fades"). */
	void present(int brightness);
	/** The fades' work ramp: 0 at start-up, so the first room fades in from black. */
	int fadeLevel() const { return _fadeLevel; }
	void setFadeLevel(int level) { _fadeLevel = level; }

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
	void playFilmBody(const Common::String &name);
	void checkDatabase();
	void resetState();
	void loadLanguage();
	void loadSettings();
	void loadOptions();
	/** The font text is drawn with: the layout font, or the smooth one at the page's scale. */
	const Graphics::Font *drawingFont(int size, bool bold);
	void renderText(Graphics::ManagedSurface &dst, const Common::U32String &text, int x, int y, int size, uint32 rgb, bool bold);
	Common::Rect scaled(const Common::Rect &r) const {
		return Common::Rect(r.left * _scale, r.top * _scale, r.right * _scale, r.bottom * _scale);
	}
	void toggleHotspots();
	void enableKeymaps();
	void seedTopics();
	void autosave();

	const ADGameDescription *_gameDesc;
	Graphics::ManagedSurface _screen;
	Common::Rect _clip;
	PictureCollection _interface1, _interface2, _map, _cursors, _inventory, _gilbert;
	Common::HashMap<int, Graphics::Font *> _fonts, _drawingFonts;
	int _scale = 1;
	Common::Array<Common::String> _language;
	Common::String _slotNames[51];
	Settings _settings;
	Options _options;
	int _shortcut = kActionNone;
	Common::HashMap<Common::String, bool> _seen;
	Common::Array<Graphics::HotspotInfo> _shownHotspots;
	uint32 _autosaved = 0; ///< the room of the last autosave
	Sound *_sound = nullptr;
	Logic *_logic = nullptr;
	Room *_room = nullptr;
	CloseUp *_cua = nullptr;
	Book *_book = nullptr;
	DialogBox *_dialog = nullptr;
	int _mode = kModeMenu;
	int _brightness = 256;
	int _fadeLevel = 0;
	int _buttonState = -1;
	uint32 _downs = 0;
	int _seenState[kScreenCount] = { -1, -1, -1, -1, -1 };
	uint32 _seenDowns[kScreenCount] = { 0, 0, 0, 0, 0 };
	/** Films, fades and waits: the ScummVM menu does not save or load then. */
	int _busy = 0;
	Menu *_menu = nullptr;

	Common::Point _mouse;
	bool _escHeld = false;
	bool _runHeld = false;
	Common::Array<Common::KeyState> _keys;
	void setButtonState(int state);
};

} // End of namespace Gilbert

#endif // GILBERT_GILBERT_H
