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

#ifndef RING_WORLD_H
#define RING_WORLD_H

#include "common/array.h"
#include "common/hashmap.h"
#include "common/ptr.h"
#include "common/rect.h"
#include "common/str.h"

#include "ring/api.h"
#include "ring/rotation.h"
#include "ring/sound.h"

namespace Graphics {
class Font;
class ManagedSurface;
}

namespace Ring {

struct Image;
class Resources;

/** A hot spot (spec/api.md, "Hot spot"): contains (x, y) when x1 <= x < x2, y1 <= y < y2. */
struct HotSpot {
	Common::Rect rect;
	bool enabled = true;
	int cursor = 0;
	int value = 0; ///< `unk_19`, passed to the zone's handlers
	int key = -1;

	bool contains(int x, int y) const { return enabled && rect.contains(x, y); }
};

struct Accessibility {
	int object = 0;
	HotSpot hotSpot;
};

/** A text on a puzzle (`ObjPreAddTxtToPuz`, spec/text.md). */
struct PuzzleText {
	int object = 0, presentation = 0;
	int x = 0, y = 0;
	int font = 0;
	byte color[3] = {};
	bool opaque = false; ///< false when the background colour is -1, -1, -1
	byte background[3] = {};
	Common::String text;
};

/** `aAnimation` (spec/animation.md); frames count from 0, events report frame + 1. */
struct Animation {
	int id = 0;
	int frames = 1, start = 0;
	int mode = 4;            ///< 4 forward, 8 backward, 0x10 / 0x20 ping-pong (forward / backward first)
	bool stopAtWrap = false; ///< flag bit 1
	bool restart = true;     ///< back to the start frame when started
	int frame = 0;
	bool backward = false; ///< ping-pong's current direction
	bool active = false, paused = false, justStarted = true;
	uint32 frameTime = 0, lastStep = 0;
	int lastReported = -5;
	int baseMode = 4;                            ///< +0x18: the mode chosen at Init
	int pauseFrame = 0, pauseState = 0;          ///< +0x42, +0x4a: 0 none, 1 armed, 2 holding
	uint32 pauseMs = 0, holdStart = 0;           ///< +0x46, the hold's start
	bool stepped = false;                        ///< +0x60: a step since arming
	int holdEvent = 0;                           ///< 0x40c910 to raise: 1 the hold starts, 2 it ends

	/** `aAnimation::PauseExactOnFrame` 0x416af0 (spec/animation.md, "Pausing on a frame"). */
	void pauseExactOnFrame(int frame, uint32 ms, int direction);

	/** `aAnimation::Init` 0x416450, start frame 1. */
	void init(int count, float fps, int flags);
	/** 0x416670 */
	void begin(uint32 time);
	/** 0x416710 */
	void end() {
		active = false;
		justStarted = true;
	}
	/** 0x416870: at most one step; true when frame + 1 is to be reported (the animation event). */
	bool advance(uint32 time);
};

/** A rotation layer a presentation shows (`ObjPreAddImgToRot`, `ObjPreAddAniToRot`). */
struct LayerRef {
	int rotation = 0, layer = 0;
};

struct Presentation {
	bool shown = false;
	Common::Array<Common::SharedPtr<PuzzleText> > texts;
	Common::Array<LayerRef> layers;
	Common::Array<Common::SharedPtr<Animation> > animations;       ///< its rotation layers' animations
	Common::Array<Common::SharedPtr<Animation> > puzzleAnimations; ///< `ObjPreAddAniToPuz`
};

/** A picture of a presentation on a puzzle (`ObjPreAddImgToPuz`, spec/drawing.md). */
struct PuzzleImage {
	int object = 0, presentation = 0;
	int zone = 0;
	Common::String file;
	int x = 0, y = 0;
	int originX = 0, originY = 0; ///< as declared (`ObjPreSetImgOriCooOnPuz`)
	bool active = true;
	byte drawType = 1;
	int priority = 0;
	Common::ScopedPtr<Image> image;
	/** A puzzle animation (spec/animation.md): frames `ANI/<file>/<file>.0001.<ext>`, loaded when first drawn. */
	Common::SharedPtr<Animation> animation;
	Common::String ext;
	Common::Array<Common::SharedPtr<Image> > frames;
};

/** A way out of a puzzle or rotation (`*AddMovTo*`, spec/api.md "Movability"). */
struct Movability {
	/** The transition (0x423730): the turn before the ride and the angles after it. */
	float alpha1 = 0, beta1 = 0, ran1 = 85.0f;
	byte turn = 2; ///< 0 animated, 1 at once, 2 none (spec/rotation.md)
	float alpha2 = 0, beta2 = 0, ran2 = 85.0f;
	HotSpot hotSpot;
	int target = 0;
	int kind = 0;        ///< 0 rotation → rotation, 1 rotation → puzzle, 2 puzzle → rotation, 3 puzzle → puzzle
	Common::String ride; ///< the video played on the way
};

struct Puzzle {
	int id = 0;
	int zone = 0;
	Common::String background;
	int bgX = 0, bgY = 0;
	Common::ScopedPtr<Image> bgImage;
	int mode = 1, modeObject = 0;                            ///< `PuzSetMod`
	Common::Array<Common::SharedPtr<Accessibility> > accessibilities;
	Common::Array<Common::SharedPtr<PuzzleImage> > images; ///< ascending priority
	Common::Array<Common::SharedPtr<PuzzleText> > texts;
	Common::Array<Movability> movabilities;
	SoundItems sounds; ///< ambient and 3D sounds (spec/sound.md)
	Common::Array<Common::SharedPtr<Animation> > animations; ///< its presentations' animations, advanced when drawn
};

/** A panorama node (`AddRot`, spec/rotation.md). */
struct Rotation {
	int id = 0;
	int zone = 0;
	Common::String name;
	/** A layer's state (rotation +0x2d, spec/rotation.md "Layers"); its pictures are the panorama's. */
	struct Layer {
		bool shown = false, dirty = true;
		int frame = 0;
		Common::SharedPtr<Animation> animation; ///< `ObjPreAddAniToRot`
	};
	Common::Array<Layer> layers;
	bool paused = false; ///< +0x28: not drawn and not tracked while set
	bool frozen = false; ///< +0x67: no looking around with the mouse (set while the bag is open)
	/** The juggle (`RotSetJugOn`, spec/rotation.md "Juggle"): flag, amplitude, speed. */
	bool juggle = false;
	float jugAmplitude = 30.0f, jugSpeed = 0.0f;
	float strength = 0.0f;              ///< +0x31: the effects' strength, 0 at load, up to 1
	uint32 loadTick = 0;                ///< 0x495708
	Common::Array<float> jugWeights;    ///< 32 x 32, filled at the first load
	// ponytail: the constructor leaves alpha, beta and ran unset (E-0046); the zones set them first
	float alpha = 0, beta = 0, ran = 85.3f;
	Common::Array<Common::SharedPtr<Accessibility> > accessibilities;
	Common::Array<Movability> movabilities;
	SoundItems sounds;
	Common::ScopedPtr<Panorama> panorama;

	/** 0x410170: all three at once. */
	void setAngles(float a, float b, float r) {
		setAlpha(a);
		beta = b;
		ran = r;
	}

	/** `RotSetAlp` (0x405920): stored 135 degrees less. */
	void setAlpha(float a) {
		alpha = a - 135.0f;
		if (alpha < 0.0f)
			alpha += 360.0f;
	}
};

/** A drag cursor of an object (`ObjSetPasDraCur` / `ObjSetActDraCur`, spec/cursor.md "Dragging"). */
struct DragCursor {
	int offsetX = 0, offsetY = 0, frames = 0, kind = 0;
	float fps = 0.0f;
	int flags = 0, imageKind = 0; ///< the in-hand cursors' (`ObjSetPasCur` / `ObjSetActCur`); 4: `LSTICON`
};

struct Object {
	int id = 0;
	/** `AddObj`'s last argument: bit 0 clicks reach the zone, bit 1 button-down events, bit 2 drags. */
	byte flags = 0;
	Common::String name; ///< `AddObj`'s, or the language's line of aObj.ini
	Common::String icon;
	DragCursor dragCursors[2]; ///< passive (cursor 3), active (cursor 4)
	DragCursor handCursors[2]; ///< in hand: passive (cursor 1), active (cursor 2) (spec/bag.md)
	/** `ObjAddBagAni`: the bag animation, none when 0 frames. */
	int bagFrames = 0, bagFlags = 4;
	float bagFps = 12.5f;
	Common::Array<Common::SharedPtr<Accessibility> > accessibilities;
	Common::Array<Presentation> presentations;
};

/**
 * The puzzles and objects the zone set-ups declare (spec/api.md), and what the engine
 * does with them: drawing puzzles (spec/drawing.md) and finding hot spots
 * (spec/cursor.md).
 */
class World {
public:
	/**
	 * Runs every zone's set-up once, as 0x431040 does (spec/boot.md); sounds go to
	 * `sounds`, 3D offsets take the stereo preference `lr`.
	 */
	void setUp(Sounds *sounds, int lr);

	Puzzle *puzzle(int id);
	Rotation *rotation(int id);
	Object *object(int id);

	/** aObj.ini (formats README): the objects' names for the language (the first three letters of `lan`). */
	void loadNames(const Common::String &lan);

	/** Font 1 (spec/text.md); texts are not drawn without it. */
	void setFont(const Graphics::Font *font) { _font = font; }

	bool shown(int object, int presentation);
	/**
	 * `ObjPreSho` / `ObjPreHid` (spec/animation.md): also starts / stops the presentation's
	 * animations at `time` and shows / hides its rotation layers.
	 */
	void showPresentation(int object, int presentation, bool shown, uint32 time = 0);
	/** `ObjPrePauAni` / `ObjPreUnPauAni`. */
	void pauseAnimations(int object, int presentation, bool paused);
	/** `ObjPreAniSetActFra` (0x4039b0 .. 0x416aa0): every animation of the presentation goes to `frame` (1-based). */
	void setAnimationFrame(int object, int presentation, int frame);
	/** `ObjPreSetAniIdeOnPuz` / `...OnRot`: the id of the presentation's `index`-th puzzle / rotation animation. */
	void setAnimationId(int object, int presentation, int index, int id, bool onRotation);
	/** `ObjPrePauFraAni`: every animation of the presentation pauses `ms` on `frame` (1-based). */
	void pauseOnFrame(int object, int presentation, int frame, uint32 ms, int direction);
	/** `ObjPreSetTxtToPuz` / `ObjPreSetTxtCooToPuz`: the presentation's `index`-th text. */
	PuzzleText *text(int object, int presentation, int index);
	/** `ObjPreSetImgCooOnPuz`: the `index`-th picture of the presentation, on any puzzle. */
	PuzzleImage *image(int object, int presentation, int index);
	/** `ObjPreSetImgCooOnPuz` (0x4037c0): every picture of the presentation to (x, y); `ObjPreSetImgOriCooOnPuz` (0x403810) back. */
	void movePictures(int object, int presentation, int x, int y);
	void restorePictures(int object, int presentation);
	/** `ObjPreHidDeaPuz`: hides the presentation (all when negative) and frees its pictures. */
	void hideAndFree(int object, int presentation = -1);
	/** `PuzSetMovOnOrOff` / `RotSetMovOnOrOff`: movabilities `from`..`to` (all when negative) of a puzzle or rotation. */
	void setMovabilities(int place, bool on, int from = -1, int to = -1);
	/** `RotSetMovRidNam` (0x405870): the ride video of movability `index` of the rotation. */
	void setRide(int rotation, int index, const Common::String &name);
	/** `PuzAddBgrImg` while playing: the puzzle's background becomes `file`. */
	void setBackground(int puzzle, const Common::String &file);
	/** `ObjSetAccOnOrOff` over all (from < 0) or `from`..`to` of the object's accessibilities. */
	void setAccessibilities(int object, bool on, int from = -1, int to = -1);

	/** Draws a puzzle: background with type 1, its shown pictures (spec/drawing.md), its texts (spec/text.md). */
	void draw(Puzzle &p, Resources &res, Graphics::ManagedSurface &dst);

	/**
	 * The first enabled accessibility of the puzzle under (x, y), or null. In mode 2 an
	 * accessibility of another object than the puzzle's ends the search (spec/cursor.md).
	 */
	const Accessibility *hit(const Puzzle &p, int x, int y) const;
	static const Accessibility *hit(const Common::Array<Common::SharedPtr<Accessibility> > &list, int x, int y);
	static const Movability *hit(const Common::Array<Movability> &list, int x, int y);

	/** `VarGet*` / `VarSet*` (spec/api.md, "Variables"): an unknown id is reported and reads 0. */
	enum VarType { kVarByte, kVarWord, kVarDword };
	int var(VarType type, int id) const;
	void setVar(VarType type, int id, int value);
	int varByte(int id) const { return var(kVarByte, id); }
	void setVarByte(int id, int value) { setVar(kVarByte, id, value); }
	float varFloat(int id) const;
	void setVarFloat(int id, float value);
	/** `VarGetStrg` (0x4062e0) / `VarSetStrg` (0x4062b0). */
	Common::String varString(int id) const;
	void setVarString(int id, const Common::String &value);

private:
	void apply(int zone, const SetupCall &c);

	Common::Array<Common::SharedPtr<Puzzle> > _puzzles;
	Common::Array<Common::SharedPtr<Rotation> > _rotations;
	Common::Array<Common::SharedPtr<Object> > _objects;
	const Graphics::Font *_font = nullptr;
	Sounds *_sounds = nullptr;
	int _lr = -1;
	Common::HashMap<int, int32> _ints[3]; ///< by VarType, the values truncated to the type
	Common::HashMap<int, Common::String> _strings;
	Common::HashMap<int, float> _floats;
};

} // End of namespace Ring

#endif // RING_WORLD_H
