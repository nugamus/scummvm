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

#include "common/endian.h"

#include "ring/codec.h"

namespace Ring {

// Four bytes from byte pos >> 3, big-endian; zero past the buffer.
static inline uint32 window(const byte *buf, uint32 size, uint32 pos) {
	uint32 o = pos >> 3, w = 0;
	for (uint i = 0; i < 4; i++)
		w = (w << 8) | (o + i < size ? buf[o + i] : 0);
	return w;
}

Common::Array<uint16> decodeBits(const byte *buf, uint32 size, uint valueBits, uint32 pos, uint32 end,
								  uint32 maxCodes, uint shift) {
	uint16 value[64] = {};
	int32 stamp[64] = {};
	byte repl = 0, last = 0;
	Common::Array<uint16> out;

	auto oldest = [&]() {
		byte best = 0;
		for (byte i = 1; i < 64; i++)
			if (stamp[i] < stamp[best])
				best = i;
		return best;
	};

	while (pos < end && out.size() < maxCodes) {
		uint32 w = window(buf, size, pos);
		int sh = 31 - (int)(pos & 7);
		if (!((w >> sh) & 1)) {
			// 0 + literal: stored in the replacement slot, no stamp
			uint16 v = (uint16)(((w << (32 - sh)) >> (32 - valueBits)) << shift);
			out.push_back(v);
			last = repl;
			value[repl] = v;
			pos += valueBits + 1;
		} else if (!((w >> (sh - 1)) & 1)) {
			// 10 + 6-bit slot
			byte i = (byte)((w << (33 - sh)) >> 26);
			last = i;
			out.push_back(value[i]);
			pos += 8;
			stamp[i] = (int32)pos;
			if (i == repl)
				repl = oldest();
		} else {
			// 11: the previous value again
			pos += 2;
			out.push_back(out.empty() ? 0 : out.back());
			stamp[last] = (int32)pos;
			if (repl == last)
				repl = oldest();
		}
	}
	return out;
}

void decodeDpcmChunk(const byte *buf, uint32 size, DpcmState &state, int16 *out) {
	uint32 pos = 3; // three unused bits
	for (uint n = 0; n < 256; n++) {
		uint32 w = window(buf, size, pos);
		int sh = 31 - (int)(pos & 7);
		if (!((w >> sh) & 1)) {
			int16 d = (int16)((w << (32 - sh)) >> 22);
			if (d > 0x1ff)
				d = (int16)(0x200 - d);
			state.delta = (int16)(d * 0x40);
			pos += 11;
		} else {
			pos += 1;
		}
		state.sample = (int16)(state.sample + state.delta);
		out[n] = state.sample;
	}
}

HbrDecoder::HbrDecoder() {
	memset(_ring, 0, sizeof(_ring));
}

int32 HbrDecoder::decode(const byte *data, uint32 pos, uint32 end, const byte *runs, uint32 runsLen,
						 const uint16 *tiles, uint32 tileCount, uint16 *out, uint32 cap, bool useBack) {
	for (uint i = 0; i < ARRAYSIZE(_memo); i++)
		_memo[i].kind = 0;
	if (useBack) {
		for (uint k = 0; k < _segStart.size() && 0x780 + k < ARRAYSIZE(_memo); k++) {
			_memo[0x780 + k].kind = 3;
			_memo[0x780 + k].start = _segStart[k];
			_memo[0x780 + k].len = _segLen[k];
		}
	}

	uint32 n = 0;
	uint rp = 0; // the ring's write position restarts every chunk, its contents persist
	bool half = false;
	auto copy = [&](const Memo &m) -> bool {
		if (!m.kind || n + m.len > cap || (m.kind == 3 && m.start + m.len > _back.size()))
			return false;
		const uint16 *src = m.kind == 1 ? tiles : m.kind == 2 ? out : _back.data();
		for (uint32 i = 0; i < m.len; i++)
			out[n + i] = src[m.start + i];
		n += m.len;
		return true;
	};

	while (pos < end) {
		byte b = data[pos], nx = data[pos + 1]; // the run list follows the stream in the chunk
		uint32 code;
		bool isNew;
		if (half) {
			byte lo = b & 15;
			isNew = lo < 8;
			code = isNew ? lo * 256u + nx : _ring[lo * 16 + (nx >> 4) - 0x80];
			pos += isNew ? 2 : 1;
			half = !isNew;
		} else {
			isNew = b < 0x80;
			code = isNew ? b * 16u + (nx >> 4) : _ring[b - 0x80];
			pos += 1;
			half = isNew;
		}
		if (code >= ARRAYSIZE(_memo))
			return -1;
		if (!isNew) {
			if (!copy(_memo[code]))
				return -1;
			continue;
		}
		_ring[rp] = code;
		rp = (rp + 1) & 127;
		if (code > tileCount && code < 0x780) {
			// run number (code - tileCount) of the run list: u8 byte count, u16 tile indices
			uint32 p = 0;
			for (uint32 k = 0; k + 1 < code - tileCount; k++) {
				if (p >= runsLen)
					return -1;
				p += runs[p] + 1u;
			}
			if (p >= runsLen || p + 1 + runs[p] > runsLen)
				return -1;
			uint cnt = runs[p] >> 1;
			if (n + cnt * 4 > cap)
				return -1;
			_memo[code].kind = 2;
			_memo[code].start = n;
			_memo[code].len = cnt * 4;
			for (uint k = 0; k < cnt; k++) {
				uint t = READ_LE_UINT16(runs + p + 1 + 2 * k);
				if (t >= tileCount)
					return -1;
				for (uint q = 0; q < 4; q++)
					out[n++] = tiles[t * 4 + q];
			}
		} else if (code >= 0x780 || _memo[code].kind) {
			if (!copy(_memo[code]))
				return -1;
		} else {
			if (code >= tileCount)
				return -1;
			_memo[code].kind = 1;
			_memo[code].start = code * 4;
			_memo[code].len = 4;
			if (!copy(_memo[code]))
				return -1;
		}
	}
	return (int32)n;
}

static Common::Array<uint16> readTiles(const byte *data, uint16 count, uint16 words) {
	Common::Array<uint16> tiles(count * words);
	for (uint i = 0; i < tiles.size(); i++)
		tiles[i] = READ_LE_UINT16(data + 2 * i);
	return tiles;
}

bool HbrDecoder::decodeTiles(const byte *data, uint32 size, uint32 runsSize, uint16 tileCount, uint16 tileWords,
							 const uint16 *segments, uint segmentCount) {
	if (tileWords != 4 || runsSize > size || tileCount * 8u > size - runsSize)
		return false;
	Common::Array<uint16> tiles = readTiles(data, tileCount, tileWords);
	uint32 total = 0;
	_segStart.clear();
	_segLen.clear();
	for (uint k = 0; k < segmentCount; k++) {
		_segStart.push_back(total);
		_segLen.push_back(segments[k] * 4u);
		total += segments[k] * 4u;
	}
	_back.resize(1200000 / 2); // the original's back buffer limit
	return decode(data, tileCount * 8, size - runsSize, data + size - runsSize, runsSize, tiles.data(),
				  tileCount, _back.data(), _back.size(), false) >= 0;
}

bool HbrDecoder::decodeFrame(const byte *data, uint32 size, uint32 runsSize, uint16 tileCount, uint16 tileWords,
							 uint16 *picture, uint32 pixels) {
	if (tileWords != 4 || runsSize > size || tileCount * 8u > size - runsSize)
		return false;
	Common::Array<uint16> tiles = readTiles(data, tileCount, tileWords);
	// The original checks for at most 0x8cfff bytes of output; the extra lies past the picture.
	Common::Array<uint16> out(0x8d000 / 2);
	memcpy(out.data(), picture, MIN<uint32>(pixels, out.size()) * 2);
	int32 n = decode(data, tileCount * 8, size - runsSize, data + size - runsSize, runsSize, tiles.data(),
					 tileCount, out.data(), out.size(), true);
	if (n < 0)
		return false;
	memcpy(picture, out.data(), MIN<uint32>(n, pixels) * 2);
	return true;
}

} // End of namespace Ring
