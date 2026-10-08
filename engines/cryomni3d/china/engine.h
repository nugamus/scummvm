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

enum MenuChoice {
	kMenuNone = 0,
	kMenuNewGame = 1,
	kMenuLoad = 2,
	kMenuResume = 3,
	kMenuSave = 4,
	kMenuVisit = 5,
	kMenuQuit = 6
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
	void initializePath(const Common::FSNode &gamePath) override;

protected:
	Common::Error run() override;

private:
	// Files: every name is relative to the game's DATA folder (`WARP/PNE140.HNM`).
	bool loadStill(const Common::Path &path, Graphics::ManagedSurface &dst);
	bool loadSprite(const Common::Path &path, Sprite &sprite);
	void loadLabels();
	Common::String label(const char *key) const;
	/** Converts one of the game's X1R5G5B5 colours to the screen format. */
	uint32 gameColor(uint16 color) const;
	/** Converts one of the code's R5G6B5 text colours (0x7020, 0x9a73) to the screen format. */
	uint32 textColor(uint16 color) const;

	// Start-up (spec/china-boot.md)
	void playIntroduction();

	// Main menu
	MenuChoice mainMenu();
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
	void image(const char *name);
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
	void interfaceScreen();
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

	Graphics::PixelFormat _format;
	Graphics::ManagedSurface _screen;

	Common::HashMap<Common::String, Common::String, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _labels;

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
	uint32 _nextFrame;
};

} // End of namespace China
} // End of namespace CryOmni3D

#endif
