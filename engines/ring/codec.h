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

#ifndef RING_CODEC_H
#define RING_CODEC_H

#include "common/array.h"
#include "common/scummsys.h"

namespace Ring {

/**
 * The engine's packed bit stream (formats README, "Packed bit stream"). Decodes codes
 * from bit `pos` while below bit `end`, at most `maxCodes`. Literals are `valueBits`
 * wide and shifted left by `shift` (the stereo sound uses 12 and 4), cache indices 6 bits.
 */
Common::Array<uint16> decodeBits(const byte *buf, uint32 size, uint valueBits, uint32 pos, uint32 end,
								  uint32 maxCodes, uint shift = 0);

/** Mono sound DPCM (formats README, wac.ksy): 256 samples per chunk, state kept across chunks. */
struct DpcmState {
	int16 sample = 0;
	int16 delta = 0;
};
void decodeDpcmChunk(const byte *buf, uint32 size, DpcmState &state, int16 *out);

/** The HBR video codec (formats README, "HBR video codec"). One per video. */
class HbrDecoder {
public:
	HbrDecoder();

	/** A 'T' chunk: `segments` precede its payload; the payload decodes into the back buffer. */
	bool decodeTiles(const byte *data, uint32 size, uint32 runsSize, uint16 tileCount, uint16 tileWords,
					 const uint16 *segments, uint segmentCount);
	/** An 'S' chunk into `picture` (RGB555, rows bottom-up); pixels past a short stream are kept. */
	bool decodeFrame(const byte *data, uint32 size, uint32 runsSize, uint16 tileCount, uint16 tileWords,
					 uint16 *picture, uint32 pixels);

private:
	struct Memo {
		byte kind = 0; // 0 unset, 1 tile table, 2 this chunk's output, 3 back buffer
		uint32 start = 0, len = 0;
	};

	int32 decode(const byte *data, uint32 pos, uint32 end, const byte *runs, uint32 runsLen,
				 const uint16 *tiles, uint32 tileCount, uint16 *out, uint32 cap, bool useBack);

	uint32 _ring[128];
	Memo _memo[0x800];
	Common::Array<uint16> _back;
	Common::Array<uint32> _segStart, _segLen;
};

} // End of namespace Ring

#endif // RING_CODEC_H
