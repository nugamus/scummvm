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

#ifndef X3D_A3D_H
#define X3D_A3D_H

#include "common/array.h"
#include "common/str.h"

namespace Common {
class SeekableReadStream;
}

namespace X3D {

struct O3DObject;

// .A3D keyframe animation (docs/formats/a3d.ksy, engines/x3d/docs/spec/animation.md)

struct A3DTrack {
	Common::Array<uint32> frames;
	Common::Array<float> params; // tension, continuity, bias, ease in, ease out per key
	Common::Array<float> values; // value floats per key
};

struct A3DAnimation {
	Common::String name;
	int parent = -1; // index into A3DFile::animations
	uint32 firstFrame, lastFrame;
	A3DTrack translation, scale, rotation; // hide and morph have no keys in the corpus
};

struct A3DFile {
	Common::Array<A3DAnimation> animations;

	bool load(Common::SeekableReadStream &s);

	// Writes animation a's pose at frame f into o's live position, scale and matrix
	void sample(uint a, float f, O3DObject &o) const;
};

} // End of namespace X3D

#endif // X3D_A3D_H
