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

#ifndef RING_ROTATION_H
#define RING_ROTATION_H

#include "common/array.h"
#include "common/rect.h"

namespace Common {
class SeekableReadStream;
}

namespace Graphics {
class ManagedSurface;
}

namespace Ring {

struct Rotation;

/** A panorama node in memory (spec/rotation.md, "Node in memory"); 2048 columns. */
class Panorama {
public:
	/** Reads the node and its first `layerCount` sections (spec/rotation.md, "Layers"). */
	bool load(Common::SeekableReadStream &s, uint layerCount);

	/** RGB565, as the table stores it. */
	uint16 pixel(int x, int y) const { return _table[4 * _index[(y * 2048 + x) >> 2] + (x & 3)]; }

	/**
	 * 0x4126b0: writes the layer's entry `frame` into the index array, or the backup taken
	 * under the first entry when `frame` is negative.
	 */
	void patch(uint layer, int frame);
	/** True when the layer's section has a frame rate (an animated layer). */
	bool animated(uint layer) const { return layer < _layers.size() && _layers[layer].animated; }

	int height = 0;
	float hRange = 0, hMid = 0, vRange = 0, vMid = 0;

private:
	struct Patch {
		uint32 x0 = 0, x1 = 0, y0 = 0, y1 = 0;
		Common::Array<uint16> index;
	};
	struct Layer {
		bool animated = false;
		Common::Array<Patch> entries;
		Patch backup;
	};
	Common::Array<Layer> _layers;
	Common::Array<uint16> _table, _index;
};

/** The view of a rotation: 40 × 28 blocks of 16 pixels (spec/rotation.md, "Camera", "Drawing"). */
class RotationView {
public:
	enum { kWidth = 640, kHeight = 448, kCols = 40, kRows = 28 };

	/** 0x40f9e0: clamps the rotation's angles and computes the grid for its camera. */
	void update(Rotation &r, const Panorama &p);
	/** The juggle's ripple (spec/rotation.md "Juggle"), after update(), `t` in seconds since the load. */
	void juggle(const Rotation &r, float t);
	/** Draws the view with its top at row `top` of `dst` (16-bit). */
	void draw(const Panorama &p, Graphics::ManagedSurface &dst, int top) const;
	/** A window position to panorama coordinates in tenths of a degree (0x4119e0, 0x412360). */
	Common::Point toPanorama(const Panorama &p, int x, int y) const;

private:
	int32 _u[kRows + 1][kCols + 1] = {}; ///< 16.16 panorama column
	int32 _v[kRows + 1][kCols + 1] = {}; ///< 16.16 panorama row
};

} // End of namespace Ring

#endif // RING_ROTATION_H
