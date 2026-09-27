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

#include "audio/audiostream.h"
#include "audio/decoders/raw.h"
#include "audio/mixer.h"
#include "common/array.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/system.h"

#include "ring/codec.h"
#include "ring/detection.h"
#include "ring/movie.h"
#include "ring/ring.h"
#include "ring/sound.h"

namespace Ring {

bool playMovie(RingEngine *vm, const Common::Path &path, int soundChannel, float rate) {
	Common::File f;
	if (!f.open(path)) {
		warning("Ring: cannot open video %s", path.toString().c_str());
		return false;
	}
	byte hdr[0x40];
	if (f.read(hdr, sizeof(hdr)) != sizeof(hdr) || memcmp(hdr, "CNM HBR", 8)) {
		warning("Ring: %s is not an HBR video", path.toString().c_str());
		return false;
	}
	uint16 channels = hdr[8], bits = hdr[9];
	uint32 sampleRate = READ_LE_UINT32(hdr + 0x0a), frames = READ_LE_UINT32(hdr + 0x0e);
	uint32 timing = READ_LE_UINT32(hdr + 0x12);
	uint32 width = READ_LE_UINT32(hdr + 0x17), height = READ_LE_UINT32(hdr + 0x1b);
	float frameMs = rate != 0.0f ? 1000.0f / rate : 1000.0f / (timing * 0.01f);
	debugC(1, kDebugVideo, "%s: %u frames %ux%u, %.1f ms each", path.toString().c_str(), frames, width, height, frameMs);

	// Sound chunk per channel: 'Z' for 0 and 1, 'A' for 2, 'B' for 3 (E-0029).
	byte soundTag = soundChannel == 2 ? 'A' : soundChannel == 3 ? 'B' : 'Z';
	byte flags = (bits == 16 ? Audio::FLAG_16BITS | Audio::FLAG_LITTLE_ENDIAN : Audio::FLAG_UNSIGNED) |
				 (channels == 2 ? Audio::FLAG_STEREO : 0);
	Audio::QueuingAudioStream *audio = nullptr;
	Audio::SoundHandle handle;

	HbrDecoder decoder;
	Common::Array<uint16> picture(width * height, 0);
	Common::Array<uint16> segments;
	uint32 shown = 0, start = 0;
	bool started = false;
	Graphics::ManagedSurface &screen = vm->screen();
	// A pending ambient transition runs over the ride's frames (aCinMov::Init / Play, spec/sound.md).
	vm->sounds().beginRide(frames);

	while (shown < frames && !f.eos() && !vm->shouldQuit()) {
		if (vm->escapePressed()) {
			vm->sounds().rideStep(frames);
			break;
		}
		byte tag = f.readByte();
		if (f.eos())
			break;
		if (tag == 'A' || tag == 'B' || tag == 'Z') {
			uint32 size = f.readUint32LE();
			if (tag != soundTag || !size) {
				f.skip(size);
				continue;
			}
			byte *pcm = (byte *)malloc(size);
			f.read(pcm, size);
			if (!audio) {
				audio = Audio::makeQueuingAudioStream(sampleRate, channels == 2);
				g_system->getMixer()->playStream(Audio::Mixer::kSFXSoundType, &handle, audio);
			}
			audio->queueBuffer(pcm, size, DisposeAfterUse::YES, flags);
		} else if (tag == 'T') {
			uint32 size = f.readUint32LE(), runs = f.readUint32LE();
			uint16 count = f.readUint16LE(), words = f.readUint16LE();
			byte n = f.readByte();
			segments.resize(n + 1);
			for (uint i = 0; i <= n; i++)
				segments[i] = f.readUint16LE();
			Common::Array<byte> body(size + 4, 0);
			f.read(body.data(), size);
			if (!decoder.decodeTiles(body.data(), size, runs, count, words, segments.data(), segments.size()))
				warning("Ring: bad tile chunk in %s", path.toString().c_str());
		} else if (tag == 'S') {
			uint32 size = f.readUint32LE(), runs = f.readUint32LE();
			uint16 count = f.readUint16LE(), words = f.readUint16LE();
			f.readUint32LE(); // width
			f.readUint32LE(); // height
			uint32 now = g_system->getMillis();
			uint32 due = started ? start + (uint32)(shown * frameMs) : now;
			if (started && now > due) {
				f.skip(size); // late: skipped unread, as aCin::Decompress does
			} else {
				Common::Array<byte> body(size + 4, 0);
				f.read(body.data(), size);
				while (started && g_system->getMillis() + 50 < due && !vm->shouldQuit())
					vm->pollEvents(10);
				if (!started) {
					started = true;
					start = g_system->getMillis();
				}
				if (!decoder.decodeFrame(body.data(), size, runs, count, words, picture.data(), picture.size()))
					warning("Ring: bad frame %u in %s", shown, path.toString().c_str());
				for (uint32 y = 0; y < height && 16 + y < (uint32)screen.h; y++) {
					uint16 *row = (uint16 *)screen.getBasePtr(0, 16 + height - 1 - y);
					for (uint32 x = 0; x < width && x < (uint32)screen.w; x++) {
						uint16 p = picture[y * width + x];
						row[x] = screen.format.RGBToColor(((p >> 10) & 31) << 3, ((p >> 5) & 31) << 3, (p & 31) << 3);
					}
				}
				vm->present();
			}
			vm->sounds().rideStep(shown);
			shown++;
		} else {
			warning("Ring: unknown chunk %02x in %s", tag, path.toString().c_str());
			break;
		}
	}

	if (audio)
		g_system->getMixer()->stopHandle(handle); // the player's clean-up stops its stream
	return true;
}

} // End of namespace Ring
