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

#include "common/debug.h"
#include "common/events.h"
#include "common/file.h"
#include "common/memstream.h"
#include "common/system.h"

#include "graphics/surface.h"

#include "image/codecs/hlz.h"

#include "video/hnm_decoder.h"

#include "peintre/detection.h"
#include "peintre/movie.h"
#include "peintre/peintre.h"

namespace Peintre {

// The movie table in Data/mission.___: 68 records of 0x24 bytes at VA 0x4a7a08, i.e. file
// offset 0xa6a08 (.data starts at file 0xa3000 for VA 0x4a4000). E-0204.
static const uint32 kMovieTableOffset = 0xA6A08;
static const uint32 kMovieTableCount = 68;
static const uint32 kMovieRecordSize = 0x24;
// Silent movies advance every 80 ms in the original (E-0206).
static const uint32 kSilentFrameDelay = 80;
// The CVY mask colour, RGB565 (E-0205).
static const uint16 kMaskColour = 0x116A;

bool MoviePlayer::loadTable() {
	_table.clear();
	Common::File exe;
	if (!exe.open("mission.___") || exe.size() < kMovieTableOffset + kMovieTableCount * kMovieRecordSize)
		return false;
	exe.seek(kMovieTableOffset);
	for (uint32 i = 0; i < kMovieTableCount; i++) {
		byte rec[kMovieRecordSize];
		exe.read(rec, sizeof(rec));
		MovieEntry e;
		uint32 len = 0;
		while (len < 12 && rec[len])
			len++;
		e.name = Common::String((const char *)rec, len);
		e.hasCvy = READ_LE_UINT32(rec + 12) != 0;
		const int16 x = READ_LE_UINT32(rec + 16), y = READ_LE_UINT32(rec + 20);
		const int16 w = READ_LE_UINT32(rec + 24), h = READ_LE_UINT32(rec + 28);
		e.rect = Common::Rect(x, y, x + w, y + h);
		e.soundOn = READ_LE_UINT32(rec + 32) != 0;
		_table.push_back(e);
	}
	debugC(1, kDebugLoad, "MoviePlayer: %d table entries", _table.size());
	return true;
}

const MovieEntry *MoviePlayer::findEntry(const Common::String &name) const {
	for (const MovieEntry &e : _table)
		if (e.name.equalsIgnoreCase(name))
			return &e;
	return nullptr;
}

bool MoviePlayer::loadMasks(const Common::String &name, Common::Array<Common::Array<byte> > &masks) {
	// u32 frame_count, u32 buffer_size, u32 offsets[frame_count], HLZ streams (E-0204).
	Common::File f;
	if (!f.open(Common::Path("MOVIES/" + name + ".CVY")))
		return false;
	const uint32 count = f.readUint32LE();
	const uint32 bufSize = f.readUint32LE();
	Common::Array<uint32> offsets;
	for (uint32 i = 0; i < count; i++)
		offsets.push_back(f.readUint32LE());
	masks.resize(count);
	for (uint32 i = 0; i < count; i++) {
		const uint32 end = i + 1 < count ? offsets[i + 1] : f.size();
		if (offsets[i] >= end || end > f.size())
			return false;
		f.seek(offsets[i]);
		masks[i].resize(bufSize);
		memset(masks[i].data(), 0, bufSize);
		Image::HLZDecoder::decodeFrameInPlace(f, end - offsets[i], masks[i].data());
	}
	return true;
}

void MoviePlayer::paintMask(Graphics::Surface &dst, const Common::Array<byte> &mask, const Common::Point &origin) {
	// Line-run mask (E-0205): 1..0x7F skips lines, 0 repeats the previous line,
	// 0x80 | n starts a line of n run bytes; a run 0x80 | k paints k + 1 pixel pairs,
	// another byte k skips k + 1 pairs.
	uint32 pos = 0, prev = 0, prevLen = 0;
	int y = 0;
	auto line = [&](uint32 start, uint32 len) {
		int x = 0;
		const int sy = origin.y + y;
		for (uint32 k = 0; k < len; k++) {
			const byte r = mask[start + k];
			const int n = ((r & 0x7F) + 1) * 2;
			if (r & 0x80) {
				for (int i = 0; i < n; i++) {
					const int sx = origin.x + x + i;
					if (sx >= 0 && sx < dst.w && sy >= 0 && sy < dst.h)
						*(uint16 *)dst.getBasePtr(sx, sy) = kMaskColour;
				}
			}
			x += n;
		}
	};
	while (pos < mask.size() && y < 480) {
		const byte b = mask[pos++];
		if (b == 0) {
			line(prev, prevLen);
			y++;
		} else if (b < 0x80) {
			y += b;
		} else {
			const uint32 n = b & 0x7F;
			if (pos + n > mask.size())
				break;
			prev = pos;
			prevLen = n;
			line(pos, n);
			pos += n;
			y++;
		}
	}
}

bool MoviePlayer::play(const Common::String &name, bool skippable) {
	Common::File *f = new Common::File();
	if (!f->open(Common::Path("MOVIES/" + name + ".HNM"))) {
		delete f;
		warning("MoviePlayer: cannot open %s.HNM", name.c_str());
		return false;
	}
	// Header byte 6: audio flags, bit 0 = has sound (formats README, .HNM).
	f->seek(6);
	const bool hasSound = f->readByte() & 1;
	f->seek(0);

	const Graphics::PixelFormat format = g_system->getScreenFormat();
	Video::HNMDecoder decoder(format);
	if (!hasSound)
		decoder.setRegularFrameDelay(kSilentFrameDelay);
	if (!decoder.loadStream(f))
		return false;

	const MovieEntry *entry = findEntry(name);
	Common::Array<Common::Array<byte> > masks;
	if (entry && entry->hasCvy && !loadMasks(name, masks))
		warning("MoviePlayer: cannot read %s.CVY", name.c_str());

	debugC(1, kDebugLoad, "MoviePlayer: %s, %d frames, sound %d, table %d", name.c_str(),
		   decoder.getFrameCount(), hasSound, entry != nullptr);
	decoder.start();
	bool skipped = false;
	while (!_vm->shouldQuit() && !decoder.endOfVideo() && !skipped) {
		if (decoder.needsUpdate()) {
			const Graphics::Surface *frame = decoder.decodeNextFrame();
			if (frame) {
				Graphics::Surface *screen = g_system->lockScreen();
				if (entry) {
					// The top-left w x h of the frame goes to (x, y); the mask is painted over it.
					const int w = MIN<int>(entry->rect.width(), frame->w);
					const int h = MIN<int>(entry->rect.height(), frame->h);
					screen->copyRectToSurface(*frame, entry->rect.left, entry->rect.top, Common::Rect(0, 0, w, h));
					const int n = decoder.getCurFrame();
					if (n >= 0 && (uint)n < masks.size())
						paintMask(*screen, masks[n], Common::Point(entry->rect.left, entry->rect.top));
				} else {
					screen->copyRectToSurface(*frame, 0, 0, Common::Rect(0, 0, MIN<int>(frame->w, 640), MIN<int>(frame->h, 480)));
				}
				g_system->unlockScreen();
				g_system->updateScreen();
			}
		}
		Common::Event event;
		while (g_system->getEventManager()->pollEvent(event)) {
			// A left click skips a skippable movie (boot.md step 8; 0x40e4ef is the left button).
			if (skippable && event.type == Common::EVENT_LBUTTONDOWN)
				skipped = true;
		}
		g_system->delayMillis(5);
	}
	decoder.close();
	return true;
}

} // End of namespace Peintre
