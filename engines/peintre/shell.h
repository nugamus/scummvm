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

#ifndef PEINTRE_SHELL_H
#define PEINTRE_SHELL_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

#include "graphics/surface.h"

#include "peintre/gfx.h"

namespace Peintre {

class PeintreEngine;
class Shell;

// The 2D shell (ui.md). Zone puzzles live in zones/*.cpp and talk to the shell below.

/** A row of the zone table 0x4a6b18 (ui.md "Zone table"). */
struct ZoneDef {
	const char *background;
	uint count;
	int objects[3];
	int16 magX, magY;
	uint tourn;     ///< TOURN<n>.SPR and its rects
	uint counter;   ///< sunflower counter index
	bool vanGogh;   ///< PA<background><a|b|c>.SPR exist
};

extern const ZoneDef kZones[];

/** Frames of Curseurs.SPR (ui.md "Cursors"). */
enum {
	kCursorDefault = 0,
	kCursorBusy = 9,
	kCursorBrush = 10,
	kCursorButton = 11,
	kCursorTake = 12,
	kCursorDrag = 13
};

inline Common::Rect rectWH(int x, int y, int w, int h) {
	return Common::Rect(x, y, x + w, y + h);
}

/** The screen rectangle a sprite frame covers when drawn at (x, y). */
inline Common::Rect spriteRect(const SpriteBank &bank, uint frame, int x, int y) {
	int a, b;
	bank.frameSize(frame, a, b);
	switch (bank.kind()) {
	case SpriteBank::kRle:
		return rectWH(x - a / 2, y - b / 2, a, b);
	case SpriteBank::kBand:
		return rectWH(0, a, 640, b);
	default:
		return rectWH(x, y, a, b);
	}
}

/** Draws cursor n of `bank` (Curseurs) with its hot-spot offset. */
void drawCursor(Graphics::Surface &dst, const SpriteBank &bank, uint n, const Common::Point &pos);

/** A sprite animation: one frame per call to step(), once or looping. */
struct Anim {
	SpriteBank bank;
	int frame = -1;    ///< the frame shown, -1 = nothing
	int next = 0;      ///< the next frame step() shows
	bool playing = false, loop = false;
	Common::Point pos;

	bool load(const Common::String &name, int x = 0, int y = 0) {
		pos = Common::Point(x, y);
		frame = -1;
		playing = false;
		return bank.load(name);
	}
	void play(bool looping = false) {
		next = 0;
		playing = true;
		loop = looping;
	}
	void show(int f) {
		frame = f;
		playing = false;
	}
	void hide() {
		frame = -1;
		playing = false;
	}
	void step() {
		if (!playing)
			return;
		frame = next++;
		if (next >= (int)bank.frameCount()) {
			if (loop)
				next = 0;
			else
				playing = false;
		}
	}
	int last() const { return (int)bank.frameCount() - 1; }
	void draw(Graphics::Surface &dst) const {
		if (frame >= 0)
			bank.draw(dst, frame, pos.x, pos.y);
	}
};

/**
 * A running slot (ui.md "Running a slot"): the zone's onPlace builds it, run() is its
 * slotRun, called once per tick until it returns true with the result.
 */
class SlotRun {
public:
	SlotRun(Shell &shell) : _s(shell) {}
	virtual ~SlotRun() {}
	virtual bool run(int &result) = 0;
	/** The zone's onAbort (Backspace during the run). */
	virtual void onAbort() { _aborted = true; }
	/** Sprites the slot animates over the page, drawn every tick. */
	virtual void draw(Graphics::Surface &dst) {}

protected:
	/** Goes to step n with the tick count at 0. */
	void go(uint n) {
		_step = n;
		_count = 0;
	}
	/** Counts this tick; true once `ticks` ticks have been counted. */
	bool after(uint ticks) { return ++_count >= ticks; }

	Shell &_s;
	bool _aborted = false;   // latched by onAbort, tested by the puzzle steps that end on Backspace (E-0444)
	uint _step = 0;
	uint _count = 0;
};

/** The generic movie slot 0x414dcc: result 1 when the movie ends. */
class MovieSlot : public SlotRun {
public:
	MovieSlot(Shell &shell, uint movie, bool skippable);
	bool run(int &result) override;
};

/** The generic voice slot 0x414dec: result 1 when the voice ends or on a click. */
class VoiceSlot : public SlotRun {
public:
	VoiceSlot(Shell &shell, const char *voice);
	bool run(int &result) override;
};

// onPlace of each zone group: builds the slot run for `object` (zones/*.cpp).
SlotRun *placeA01(Shell &shell, uint zone, uint object);
SlotRun *placeA03(Shell &shell, uint zone, uint object);
SlotRun *placeA04(Shell &shell, uint zone, uint object);
SlotRun *placeA13(Shell &shell, uint zone, uint object);
SlotRun *placeA14(Shell &shell, uint zone, uint object);
/** Zone 21's ending, state 0x24 (a14.md). */
SlotRun *createEnding(Shell &shell);

class Shell {
public:
	Shell(PeintreEngine *vm);
	~Shell();

	/** Runs the zone until it is left; returns the code of ui.md "Leaving a zone". */
	int run(uint zone);

	// Services for the zone puzzles.
	PeintreEngine *vm() { return _vm; }
	uint zone() const { return _zone; }
	bool odd() const { return _tick & 1; }
	Common::Point mouse() const { return _mouse; }
	/** The left button is down on this tick (ui.md "click"). */
	bool down() const { return _down; }
	/** The first tick the button is down. */
	bool clicked() const { return _down && !_wasDown; }
	void setCursor(uint n) { _cursor = n; }

	/** The page: what is on screen under the bar, the dragged sprites and the cursor. */
	Graphics::Surface &page() { return _page; }
	/** Draws GFX\<name>.TGP on the page and keeps it to restore from. */
	bool background(const Common::String &name);
	/** Copies a rectangle of the last background back to the page. */
	void restore(const Common::Rect &r);
	/** Draws the zone's object slots on the page (0x41170e). */
	void drawSlots();
	/** Draws the magnifier open (the last frame of LoupeOut) and marks it open. */
	void drawMagnifierOpen();
	bool overViewA() const;
	bool overViewB() const;

	/** Movie_Open(n, !skippable): stepped by the shell every tick into the page. */
	void movie(uint n, bool skippable);
	bool moviePlaying() const;
	void voice(const Common::String &name);
	/** True once the voice has ended or on a click, which stops it (0x414dec). */
	bool voiceWait();
	void stopVoice();
	void sound(const Common::String &name, bool loop = false);
	void stopSound(const Common::String &name);
	bool soundPlaying(const Common::String &name) const;

	/** Starts view A (animation) or view B (panorama) of `name`; viewStep runs it. */
	void startView(const Common::String &name, bool panorama);
	/** One tick of the view; true when it has ended. */
	bool viewStep();

	uint32 &counter(uint i);

private:
	enum State {
		kInit = 0, kViewBRun = 1, kViewB = 2, kViewARun = 3, kViewA = 4, kIdle = 5,
		kBarClose = 6, kBarOpen = 7, kDragOwn = 8, kPlacing = 10, kReplay = 0xB,
		kCapsClose = 0xC, kRun = 0xD, kPlace = 0xE, kResult = 0xF, kFlyWait = 0x10, kFlyBack = 0x11,
		kBackIdle = 0x12, kDragOther = 0x15, kSunWalk = 0x16, kSunGrab = 0x17,
		kSunDrag = 0x18, kSunLeave = 0x19, kSunStart = 0x1A, kSunFree = 0x1B,
		kSunPot = 0x1C, kMenu = 0x1F, kLeave = 0x20, kSave = 0x21, kMenuWait = 0x22,
		kLeaveWait = 0x23, kEnding = 0x24
	};
	enum { kMagClosing = 0, kMagOpening = 1, kMagOpen = 2, kMagClosed = 3 };

	void init();
	void step();
	void stepIdle();
	void stepSunflower();
	void compose();
	void redrawZone();

	void openMagnifier();
	void closeMagnifier();
	void drawMagnifier();
	void openBar();
	void closeBar();
	bool barOpen() const { return _barStep == 30; }
	bool barClosed() const { return _barStep == -1; }
	bool barMoving() const { return !barOpen() && !barClosed(); }
	int barY() const;
	void moveBar();
	void drawBar(Graphics::Surface &dst);
	int barHit() const;
	void syncHeld();
	int slotOf(uint object) const;
	/** Places the object at `index` of the list in its slot. */
	void place(uint index);
	/** rate: 0 every tick, 1 odd ticks, 2 even ticks. */
	void startCaps(uint slot, const SpriteBank *bank, int rate);
	void startRetour();
	bool capsDone() const { return !_capsBank || _capsFrame >= (int)_capsBank->frameCount(); }
	void leave(int code);

	PeintreEngine *_vm;
	uint _zone = 0;
	const ZoneDef *_def = nullptr;
	int _state = kInit;
	int _code = -1;
	uint _tick = 0;       // 0x4e25d0, +1 mod 4 per tick
	uint _ticks = 0;      // every tick (the bar arrows' scroll clock)

	Common::Point _mouse;
	bool _down = false, _wasDown = false;
	uint _cursor = kCursorDefault;
	SpriteBank _cursors;

	Graphics::Surface _page, _bg;

	// Zone objects: slot banks and the inventory list.
	SpriteBank _capsE[3], _capsOP[3], _capsAO[3], _capsAC[3];
	const SpriteBank *_capsBank = nullptr;   // the slot animation playing, drawn on the page
	int _capsFrame = 0;
	uint _capsSlot = 0;
	int _capsRate = 0;
	int _runSlot = -1;      // the slot being run (CapsAC frame 0)
	Common::Array<uint> _list;
	uint _first = 0;

	// Magnifier, Retour buttons, bar.
	SpriteBank _loupeOut, _loupeIn, _retour, _retourM;
	int _mag = kMagClosed;
	int _magFrame = 0;
	bool _retourOn = false;
	uint _retourFrame = 0;
	SpriteBank _invent, _opi, _op, _count;
	int _barStep = -1;      // -1 closed .. 30 open
	int _barReq = 0;        // 1 opening, -1 closing
	int _arrowHeld = -1;

	// Drag and fly-back.
	int _dragObject = -1;
	int _dragIndex = -1;    // its index in the list
	Common::Point _dragOrigin, _grab;
	bool _flying = false;
	uint _flyN = 0;
	Common::Point _flyFrom, _flyTo;

	// The slot run.
	uint _object = 0;
	uint _slot = 0;
	uint _placedIndex = 0;  // its index in the list, to put it back
	bool _justPlaced = false;
	SlotRun *_run = nullptr;
	int _result = 0;

	// Movie skip flag and view state.
	bool _movieSkippable = false;
	bool _movieOn = false;
	bool _viewing = false, _viewWait = false, _viewPanorama = false;
	Graphics::Surface _panorama;
	Common::Point _viewPos, _viewSpeed;
	Anim _viewAnim;

	// Sunflower.
	Anim _pa, _pot;
	SpriteBank _tourn;
	bool _tournDrag = false;
	Common::Point _tournPos;
};

} // End of namespace Peintre

#endif // PEINTRE_SHELL_H
