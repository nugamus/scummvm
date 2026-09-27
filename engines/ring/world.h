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
#include "common/ptr.h"
#include "common/rect.h"
#include "common/str.h"

#include "ring/api.h"
#include "ring/rotation.h"

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

struct Presentation {
	bool shown = false;
	Common::Array<Common::SharedPtr<PuzzleText> > texts;
};

/** A picture of a presentation on a puzzle (`ObjPreAddImgToPuz`, spec/drawing.md). */
struct PuzzleImage {
	int object = 0, presentation = 0;
	int zone = 0;
	Common::String file;
	int x = 0, y = 0;
	bool active = true;
	byte drawType = 1;
	int priority = 0;
	Common::ScopedPtr<Image> image;
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
};

/** A panorama node (`AddRot`, spec/rotation.md). */
struct Rotation {
	int id = 0;
	int zone = 0;
	Common::String name;
	int layers = 0;
	bool paused = false; ///< +0x28: not drawn and not tracked while set
	// ponytail: the constructor leaves alpha, beta and ran unset (E-0046); the zones set them first
	float alpha = 0, beta = 0, ran = 85.3f;
	Common::Array<Common::SharedPtr<Accessibility> > accessibilities;
	Common::Array<Movability> movabilities;
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

struct Object {
	int id = 0;
	byte flags = 0; ///< `AddObj`'s last argument: bit 0 = clicks reach the zone
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
	/** Runs every zone's set-up once, as 0x431040 does (spec/boot.md). */
	void setUp();

	Puzzle *puzzle(int id);
	Rotation *rotation(int id);
	Object *object(int id);

	/** Font 1 (spec/text.md); texts are not drawn without it. */
	void setFont(const Graphics::Font *font) { _font = font; }

	bool shown(int object, int presentation);
	void showPresentation(int object, int presentation, bool shown);
	/** `ObjPreSetTxtToPuz` / `ObjPreSetTxtCooToPuz`: the presentation's `index`-th text. */
	PuzzleText *text(int object, int presentation, int index);
	/** `ObjPreHidDeaPuz`: hides every presentation of the object and frees its pictures. */
	void hideAndFree(int object);
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

private:
	void apply(int zone, const SetupCall &c);

	Common::Array<Common::SharedPtr<Puzzle> > _puzzles;
	Common::Array<Common::SharedPtr<Rotation> > _rotations;
	Common::Array<Common::SharedPtr<Object> > _objects;
	const Graphics::Font *_font = nullptr;
};

} // End of namespace Ring

#endif // RING_WORLD_H
