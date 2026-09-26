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

#ifndef PEINTRE_BFG_H
#define PEINTRE_BFG_H

#include "common/array.h"
#include "common/path.h"
#include "common/str.h"

namespace Peintre {

/**
 * A scene bundle (Data/Scenes_3D/<scene>.BFG): a 100-slot directory of named, packed
 * entries (docs/formats/README.md, bfg.ksy).
 */
class Bfg {
public:
	bool open(const Common::Path &path);
	bool isOpen() const { return !_data.empty(); }
	bool hasEntry(const Common::String &name) const;
	/** Unpacks an entry, object header included. False when the name is not found. */
	bool readEntry(const Common::String &name, Common::Array<byte> &out) const;
	uint entryCount() const { return _entries.size(); }
	const Common::String &entryName(uint i) const { return _entries[i].name; }

	/** The BFG entry packing: method byte 1 = stored, else LZ (bfg.ksy packed_entry). */
	static bool unpack(const byte *src, uint32 size, Common::Array<byte> &out);

private:
	struct Entry {
		Common::String name;
		uint32 offset;
		uint32 size;
	};
	Common::Array<byte> _data;
	Common::Array<Entry> _entries;
};

/** Object types in the 20-byte header of every entry (bfg.ksy object_header). */
enum ObjectType {
	kObjScene = 1,
	kObjTexture = 3,
	kObjAnim = 4,
	kObjBoxes = 5
};

const uint32 kObjectHeaderSize = 20;

} // End of namespace Peintre

#endif // PEINTRE_BFG_H
