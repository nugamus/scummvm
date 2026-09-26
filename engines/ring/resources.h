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

#ifndef RING_RESOURCES_H
#define RING_RESOURCES_H

#include "common/array.h"
#include "common/hashmap.h"
#include "common/hash-str.h"
#include "common/path.h"
#include "common/ptr.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

namespace Common {
class SeekableReadStream;
}

namespace Ring {

enum Zone {
	kZoneSY = 1,
	kZoneNI = 2,
	kZoneRH = 3,
	kZoneFO = 4,
	kZoneRO = 5,
	kZoneWA = 6,
	kZoneAS = 7,
	kZoneN2 = 8
};

/** Zone folder names, 0x402010 (spec/boot.md). */
const char *zoneFolder(int zone);

/** One `.at2` archive (formats README, at2.ksy). Member names as stored, e.g. "\image\end.bmp". */
class ArtArchive {
public:
	bool open(const Common::Path &path);
	/** The member's packed bytes, or nullptr when there is no such member. */
	Common::SeekableReadStream *member(const Common::String &name) const;

private:
	struct Entry {
		uint32 offset, size;
	};
	Common::Path _path;
	Common::HashMap<Common::String, Entry, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _entries;
};

/**
 * A decoded picture in the screen's 16-bit format. 24- and 32-bit sources keep a per
 * pixel alpha (spec/drawing.md): 0 for a pure black 24-bit pixel (draw type 2 skips it),
 * the TGA alpha for 32-bit ones, whose colours are premultiplied as the original does.
 */
struct Image {
	Graphics::ManagedSurface surface;
	Common::Array<byte> alpha; ///< empty for 8/16-bit sources
	bool trueColor = false;

	void draw(Graphics::ManagedSurface &dst, int x, int y, byte type) const;
};

class Resources {
public:
	explicit Resources(const Graphics::PixelFormat &format) : _format(format) {}

	void setLanguageFolder(const Common::String &folder) { _language = folder; }

	/** Opens `DATA/<zone>.at2` (SY: the language's `DATA/<LAN>/SY.AT2`). */
	bool openArchive(int zone);

	/**
	 * Loads an image by name as a handle of `zone` would (spec/resources.md): from the
	 * zone's archive (`\<folder>\<name>`) when `fromArchive`, else
	 * `DATA/<zone>/<folder>/<name>`. Falls back to the loose file when the archive has no
	 * such member (Q-0007). `name` may hold `\` separators (animation frames).
	 */
	Image *loadImage(int zone, const Common::String &name, bool fromArchive, const char *folder = "IMAGE");

	/** Decodes image bytes by the name's extension and source (the table in spec/resources.md). */
	Image *decodeImage(Common::SeekableReadStream &stream, const Common::String &name, bool fromArchive);

	const Graphics::PixelFormat &format() const { return _format; }

private:
	Image *fromRgb555(const Common::Array<uint16> &pixels, int w, int h);
	Image *decodeBma(Common::SeekableReadStream &s);
	Image *decodeTgc(Common::SeekableReadStream &s);
	Image *decodeBmp(Common::SeekableReadStream &s);
	Image *decodeTga(Common::SeekableReadStream &s);
	Image *fromTga(const byte *data, uint32 size);

	Graphics::PixelFormat _format;
	Common::String _language;
	Common::HashMap<int, Common::SharedPtr<ArtArchive> > _archives;
};

} // End of namespace Ring

#endif // RING_RESOURCES_H
