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

#include "common/file.h"
#include "common/debug.h"

#include "peintre/bfg.h"
#include "peintre/detection.h"

namespace Peintre {

// Directory: u32 count, 100 slots of {name[28], offset, size}, entries from 0xE18.
static const uint32 kDataBase = 0xE18;
static const uint32 kMaxEntries = 100;
static const uint32 kSlotSize = 36;

bool Bfg::open(const Common::Path &path) {
	_data.clear();
	_entries.clear();
	Common::File f;
	if (!f.open(path))
		return false;
	const uint32 size = f.size();
	if (size < kDataBase)
		return false;
	_data.resize(size);
	if (f.read(_data.data(), size) != size) {
		_data.clear();
		return false;
	}
	const uint32 count = READ_LE_UINT32(_data.data());
	if (count == 0 || count > kMaxEntries) {
		_data.clear();
		return false;
	}
	for (uint32 i = 0; i < count; i++) {
		const byte *slot = _data.data() + 4 + i * kSlotSize;
		Entry e;
		// The name ends at its NUL; later bytes are leftovers of the authoring tool.
		uint32 len = 0;
		while (len < 28 && slot[len])
			len++;
		e.name = Common::String((const char *)slot, len);
		e.offset = READ_LE_UINT32(slot + 28);
		e.size = READ_LE_UINT32(slot + 32);
		if (kDataBase + e.offset + e.size > size) {
			warning("Bfg: entry %s past the end of %s", e.name.c_str(), path.toString().c_str());
			_data.clear();
			_entries.clear();
			return false;
		}
		_entries.push_back(e);
	}
	debugC(1, kDebugLoad, "Bfg: %s, %d entries", path.toString().c_str(), count);
	return true;
}

bool Bfg::hasEntry(const Common::String &name) const {
	for (const Entry &e : _entries)
		if (e.name == name)
			return true;
	return false;
}

bool Bfg::readEntry(const Common::String &name, Common::Array<byte> &out) const {
	// The original compares names exactly (strcmp), first match wins.
	for (const Entry &e : _entries) {
		if (e.name == name)
			return unpack(_data.data() + kDataBase + e.offset, e.size, out);
	}
	return false;
}

bool Bfg::unpack(const byte *src, uint32 size, Common::Array<byte> &out) {
	out.clear();
	if (size < 4)
		return false;
	if (src[0] == 1) {
		out.resize(size - 4);
		memcpy(out.data(), src + 4, size - 4);
		return true;
	}
	// LZ: u16 flags (LSB first) per 16 items; 0 = literal, 1 = (b0, b1) copy
	// (b0 & 15) + 1 bytes from ((b0 & 0xF0) << 4) + b1 back.
	out.reserve(size * 3);
	uint32 i = 4, count = 0, flags = 0;
	while (i != size) {
		if (count == 0) {
			if (i + 2 > size)
				return false;
			flags = READ_LE_UINT16(src + i);
			i += 2;
			count = 16;
		}
		if (i >= size)
			return false;
		if (!(flags & 1)) {
			out.push_back(src[i++]);
		} else {
			if (i + 2 > size)
				return false;
			const byte b0 = src[i], b1 = src[i + 1];
			i += 2;
			const uint32 dist = ((b0 & 0xF0) << 4) + b1;
			const uint32 len = (b0 & 0x0F) + 1;
			if (dist == 0 || dist > out.size())
				return false;
			for (uint32 k = 0; k < len; k++)
				out.push_back(out[out.size() - dist]);
		}
		flags >>= 1;
		count--;
	}
	return true;
}

} // End of namespace Peintre
