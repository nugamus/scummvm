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

#ifndef CRYOMNI3D_CHINA_ENGINE_H
#define CRYOMNI3D_CHINA_ENGINE_H

#include "common/hashmap.h"
#include "common/array.h"
#include "common/hash-str.h"
#include "common/random.h"
#include "common/path.h"
#include "common/serializer.h"
#include "common/str.h"
#include "common/str-array.h"

#include "audio/mixer.h"

#include "graphics/managed_surface.h"
#include "graphics/pixelformat.h"

#include "cryomni3d/cryomni3d.h"
#include "cryomni3d/omni3d.h"

namespace CryOmni3D {
namespace China {

/** A China sprite (`.SPR`): one 16-bit picture with a key colour and a screen position. */
struct Sprite {
	Graphics::ManagedSurface surface;
	uint32 keyColor = 0;
	Common::Point pos;
};

inline Common::Rect spriteRect(const Sprite &s) {
	return Common::Rect(s.pos.x, s.pos.y, s.pos.x + s.surface.w, s.pos.y + s.surface.h);
}

enum MenuChoice {
	kMenuNone = 0,
	kMenuNewGame = 1,
	kMenuLoad = 2,
	kMenuResume = 3,
	kMenuSave = 4,
	kMenuVisit = 5,
	kMenuQuit = 6,
	kMenuDocumentation = 7,
	kMenuOptions = 8
};

class CryOmni3DEngine_China : public CryOmni3DEngine {
public:
	CryOmni3DEngine_China(OSystem *syst, const CryOmni3DGameDescription *gamedesc);
	~CryOmni3DEngine_China() override;

	bool displayToolbar(const Graphics::Surface *original) override { return false; }
	bool hasPlaceDocumentation() override { return false; }
	bool displayPlaceDocumentation() override { return false; }
	uint displayOptions() override { return 0; }
	void makeTranslucent(Graphics::Surface &dst, const Graphics::Surface &src) const override {}
	void setupPalette(const byte *colors, uint start, uint num) override {}
	bool shouldSkipVideo() override;
	/** Blocking screens also end when a load from the menu is waiting (applied by the play loop). */
	bool shouldAbort() override { return shouldQuit() || _pendingLoad >= 0; }
	bool hasFeature(EngineFeature f) const override;
	Common::Error saveGameState(int slot, const Common::String &desc, bool isAutosave = false) override;
	Common::Error loadGameState(int slot) override;
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override;
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override;
	bool keyEvent(const Common::Event &event) override;
	void initializePath(const Common::FSNode &gamePath) override;

protected:
	Common::Error run() override;

private:
	// Files: every name is relative to the game's DATA folder (`WARP/PNE140.HNM`).
	bool loadStill(const Common::Path &path, Graphics::ManagedSurface &dst);
	bool loadSprite(const Common::Path &path, Sprite &sprite);
	bool loadTga(const Common::Path &path, Graphics::ManagedSurface &dst);
	typedef Common::HashMap<Common::String, Common::String, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> TextMap;
	void loadTextFile(const char *path, TextMap &dst);
	Common::String label(const char *key) const;
	/** Converts one of the game's X1R5G5B5 colours to the screen format. */
	uint32 gameColor(uint16 color) const;
	/** Converts one of the code's R5G6B5 text colours (0x7020, 0x9a73) to the screen format. */
	uint32 textColor(uint16 color) const;

	// Start-up (spec/china-boot.md)
	void playIntroduction();

	// Main menu
	MenuChoice mainMenu();
	void optionsScreen();
	void drawMenu(int hovered);
	Common::Rect menuButtonRect(uint button);
	bool menuButtonEnabled(uint button);

	// Play (logic.cpp; spec/china-zones.md, games/china/docs/places.md)
	void newGame();
	void playLoop();
	void tick();
	void drawFrame();
	void waitFrame();
	void scrollByCursor();
	void turnToPoint(const Common::Point &topLeft);
	void crossFade();
	void fadeTo(const Graphics::Surface *target);
	void updateCursor();
	void setCursorSprite(int id);
	Common::Point cursorTopLeft();
	int zoneAt(const Common::Point &hot);
	void drawLabel();
	void drawLabelBox(const Common::String &text, const Common::Point &pos);

public:
	// The place API (spec/china-zones.md "Place API"), called by the place procedures
	void zonesReset() {
		_zones.clear();
		_hoveredZone = -1;
	}
	void zoneGo(int top, int left, int bottom, int right, bool disabled, const char *target, int arg = 0,
	            double alpha = -1., double beta = -1.);
	void zoneLook(int top, int left, int bottom, int right, bool disabled, const char *target, int arg = 0);
	void zoneTake(int top, int left, int bottom, int right, bool disabled, const char *target);
	void zoneUse(int top, int left, int bottom, int right, bool disabled);
	void zoneLabel(int top, int left, int bottom, int right, bool disabled, const char *key);
	void zoneDoc(int top, int left, int bottom, int right, bool disabled, const char *key);
	void zoneTalk(int top, int left, int bottom, int right, bool disabled);
	void zoneEnable(uint i);
	void zoneDisable(uint i);
	bool zoneHandler();
	int clickedZone() const { return _clickedZone; }

	void warp(const char *name);
	bool image(const char *name);
	void video(const char *name);
	void setAngles(double alpha, double beta);
	void gotoPlace(const char *name);

	uint32 var(uint id) const { return id < kVarCount ? _vars[id] : 0; }
	void setVar(uint id, uint32 value) { if (id < kVarCount) _vars[id] = value; }
	bool visitMode() const { return _vars[0] != 0; }

	uint objectState(uint id) const { return id < kObjectCount ? _objects[id].state : 0; }
	void objectToInventory(uint id);
	void objectToCursor(uint id);
	void objectDestroy(uint id);

	uint heldObject() const { return _heldObject; }

	void minutesAdd(const char *key);
	void voice(const char *line);
	void soundQueue(const char *name);
	void soundPlayWait(const char *name);
	void soundStop();
	void playMusic(const char *name);
	void musicForPlace(const char *place);
	void screenEffect();
	/** Opens the interface bar (spec/china-interface.md); 0 when the exit spiral was clicked, else 1. */
	int32 interfaceScreen();
	int32 puzzle(int32 number, int32 arg);
	uint32 timeMs() const;
	void dialogue(const char *line, const char *stemA, const char *stemB, bool noSubtitles = false);
	// What a few places read or set directly (E-0954)
	void clearDisplay() { _display = kDisplayNone; }
	void endPlay() { _endOfPlay = true; }
	void clearClickedZone() { _clickedZone = -1; }
	void skipNextAutosave() {}
	void setPuzzleMode(int32 on) { _puzzleMode = on != 0; }
	void setFightStart(uint32 ms) { _fightStart = ms; }
	uint32 fightStart() const { return _fightStart; }
	bool rightButtonDown() { return getCurrentMouseButton() == 2; }
	bool rightButtonLatched() const { return _rightLatch; }
	int32 unknownCall(const char *name);
	bool keyDown(int32 scanCode);
	void epilogue();
	void credits();
	bool waitOrEscape(uint32 ms);
	void objectSetLabel(uint id, const char *key);
	void objectSetExamine(uint id, const char *place);

	static const uint kVarCount = 227;
	static const uint kObjectCount = 36;
	static const uint kNoObject = 36;

private:
	struct Zone {
		Common::Rect rect; // inclusive bounds, stored half-open
		bool disabled;
		byte type;
		Common::String target;
		Common::String key;
		double alpha, beta;
	};
	enum ZoneType {
		kZoneGo = 0,
		kZoneLook = 2,
		kZoneTake = 4,
		kZoneUse = 6,
		kZoneLabel = 7,
		kZoneDoc = 8,
		kZoneTalk = 9
	};
	void addZone(byte type, int top, int left, int bottom, int right, bool disabled, const char *target,
	             const char *key, double alpha, double beta);

	struct Object {
		uint state;
		int slot;
	};

public:
	// Place procedures (places.cpp), in the game's list order
	typedef void (*PlaceProc)(CryOmni3DEngine_China &g, bool entry);
	struct PlaceDef {
		const char *name;
		PlaceProc proc;
	};
	static const PlaceDef kPlaces[];

private:
	const PlaceDef *findPlace(const Common::String &name) const;
	bool syncGame(Common::Serializer &s);
	void resetPlayState();
	static const char *objectStem(uint id);
	void applyLoad();

	bool _loadedGame;
	bool _inPlay;
	bool _inMenu; // the menu's own save is running

	Graphics::PixelFormat _format;
	Graphics::ManagedSurface _screen;

	TextMap _labels;
	TextMap _minuteTexts;

	Graphics::ManagedSurface _menuBackground;
	Sprite _menuNormal[6];
	Sprite _menuHover[6];

	Omni3DManager _omni3D;
	Graphics::ManagedSurface _warpImage;
	Graphics::ManagedSurface _still;
	enum Display { kDisplayNone, kDisplayWarp, kDisplayStill } _display;
	bool _fadePending;
	double _alphaSpeed, _betaSpeed;
	uint _panoramaSpeed;

	static const uint kCursorCount = 18;
	Sprite _cursors[kCursorCount];
	int _cursorId;
	static const int kCursorHeld = 100; // the held object's r_ sprite
	Sprite _heldCursor;
	Common::Point _cursorHot;
	Common::Array<Common::String> _minutes;
	Common::String _labelText;
	Common::Point _labelPos;

	Common::Array<Zone> _zones;
	int _hoveredZone;
	int _clickedZone;
	bool _pressLatch;
	bool _pressed;

	const PlaceDef *_place;
	bool _entryPending;

	bool _inPlaceCall;
	int _pendingLoad;
	bool _puzzleMode;
	bool _rightLatch;
	uint32 _fightStart;

	// The interface bar (interface.cpp; spec/china-interface.md)
	enum { kBarSpiral, kBarEye, kBarFrames, kBarNote, kBarCompass, kBarSpriteCount };
	static const uint kSlotCount = 10;
	static const int kCursorEye = 101; // the held document's i_ sprite over the eye
	void barLoad();
	void barObjectSprites(uint id);
	void barPresent();
	void barRebuildSlots();
	int barFirstFreeSlot() const;
	void barHold(uint id);
	void barDraw(int visibleRows, int lower, int hoveredSlot);
	void barSlide(bool in);
	int barSlotAt(const Common::Point &p) const;
	bool barVisible(int sprite) const;
	Common::Rect barRect(int sprite) const;
	void barDocument();
	void barNotebook();
	void wrapText(const Common::String &text, int width, Common::Array<Common::String> &lines);
	Common::String objectLabelKey(uint id) const;
	Common::String objectDocKey(uint id) const;
	const Sprite &cursorSprite(int id) const;

	Graphics::ManagedSurface _barSaved;
	Graphics::ManagedSurface _barBand;
	Sprite _barSprites[kBarSpriteCount];
	Sprite _objSlot[kObjectCount];
	Sprite _objEye[kObjectCount];
	uint _slots[kSlotCount];
	bool _barLoaded;
	bool _barWarp; // the display was a warp when the bar opened (the compass shows)
	bool _skipFade;
	Common::String _objLabelKey[kObjectCount];
	Common::String _objDocKey[kObjectCount];

	// Puzzles (puzzles.cpp; games/china/docs/puzzles.md)
	struct PuzzleInput {
		bool latch = false, rightLatch = false; // the press latches (E-1200)
		bool held = false, press = false;       // left button down / went down this frame
		bool rightPress = false;                // right button went down this frame
		bool escape = false, space = false;
	};
	void puzzleStart(PuzzleInput &in);
	void puzzlePoll(PuzzleInput &in);
	void puzzleFlip(const Common::String &label = Common::String());
	void puzzleReturnHeld();
	Common::Path puzzlePath(const Common::String &file) const;
	bool puzzleBackground(const Common::String &name);
	bool puzzleSprite(const Common::String &name, Sprite &sprite);
	bool puzzleMask(const Common::String &name, Common::Array<byte> &mask);
	void puzzleSound(const char *file, uint channel);
	Audio::SoundHandle _puzzleChannels[9]; // the original's sound channels 0..8
	int32 puzzlePenjing();
	int32 puzzleBouddha();
	int32 puzzleSceaux(int32 variant);
	int32 puzzleGo();
	int32 puzzleRings();
	int32 puzzleHorloge();
	int32 puzzleBoutons();
	int32 puzzleBombe();
	Common::String _puzzleDir;
	int32 _puzzleResult = 0;

	// The documentation base (documentation.cpp; spec/china-documentation.md)
	struct DocTableRow {
		Common::String a, b;
		Common::StringArray links;
	};
	struct DocFiche {
		Common::String label, title, picture, caption, text;
		Common::StringArray links;
		Common::Array<DocTableRow> rows;
	};
	struct DocTheme {
		Common::String title;
		Common::Array<DocFiche> fiches;
	};
	struct DocIndexRow {
		Common::String text, label; // shown text, fiche label (empty for a header)
	};
	struct DocLink {
		Common::Rect rect;
		uint number;
	};
	struct DocWord {
		Common::String text;
		int link;
		int width;
	};
	static const uint kDocHistorySize = 50;
	static const uint kDocSpriteCount = 29;
	void documentation();
	int32 documentFiche(const char *key);

	// The map (map.cpp)
	bool map();
	Common::Point mapBuildings();
	void mapPresent();
	void docLoad();
	void docLoadSprites();
	bool docFind(const Common::String &label, int &theme, int &fiche) const;
	void docHistoryPush(const Common::String &label);
	void docDarken(const Common::Rect &area, int shift);
	int docIndexPanel(int shift, int &scroll, const Common::Point &mouse);
	void docBlit(int sprite);
	Common::Rect docRect(int sprite) const;
	bool docLoadPicture(const Common::String &name, Graphics::ManagedSurface &dst);
	void docDrawText(const Common::String &text, int x, int y, int width, int bottom, bool justify, bool hardBreaks,
	                 Common::Array<DocLink> *links);
	void docDrawFiche(const DocFiche &fiche, const Graphics::ManagedSurface *pic, int tableSel, Common::Array<DocLink> &links);
	Common::Array<DocTheme> _docThemes;
	Common::Array<DocIndexRow> _docIndex;
	int _docIndexWidth;
	bool _docLoaded;
	Sprite _docSprites[kDocSpriteCount];
	Common::String _docHistory[kDocHistorySize]; // E-1302
	int _docCur, _docEnd;
	bool _docWrapped;

	// Dialogues and sounds (dialogue.cpp)
	struct DialogueBlock {
		Common::String text;
		Common::String next;
	};
	Common::HashMap<Common::String, DialogueBlock> _dialogues;
	struct Face {
		Common::Array<Graphics::ManagedSurface> loops[4];
	};
	void loadDialogues();
	void drawSubtitle(const Common::String &text);
	bool playVoice(const Common::String &id, Common::Array<int16> &samples);
	bool loadFaces(const char *stem, Face &face);
	void waitSoundChannel();
	void playSound(const Common::String &name);
	void drawView();
	Audio::SoundHandle _voiceHandle;
	Audio::SoundHandle _soundHandle;
	uint _voiceRate;
	Common::RandomSource _rnd;
	Common::String _musicName;
	Audio::SoundHandle _musicHandle;
	uint32 _vars[kVarCount];
	Object _objects[kObjectCount];
	uint _heldObject;

	bool _gameRunning;
	bool _endOfPlay;
	bool _spacePressed;
	// Space released since last looked at; armed when the frame loop took the press, so only
	// that press's release opens the bar
	bool _spaceUp, _spaceArmed;
	uint32 _nextFrame;
};

} // End of namespace China
} // End of namespace CryOmni3D

#endif
