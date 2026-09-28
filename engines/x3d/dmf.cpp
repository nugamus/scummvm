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

#include "common/stream.h"
#include "common/textconsole.h"

#include "graphics/surface.h"

#include "x3d/dmf.h"

namespace X3D {

Graphics::Surface *loadDMF(Common::SeekableReadStream &s) {
	if (s.readUint16LE() != 0xfb00)
		return nullptr;
	const uint32 total = s.readUint32LE();

	uint16 bpp = 0, width = 0, height = 0;
	byte palette[256][4] = {};
	bool keyed = false;
	byte key[3] = {};
	Graphics::Surface *surface = nullptr;

	while (!s.err() && !s.eos() && (uint32)s.pos() < total) {
		const uint16 id = s.readUint16LE();
		const uint32 size = s.readUint32LE();
		// A chunk shorter than its own header, or one past the end, would never advance
		if (size < 6 || s.eos() || s.pos() - 6 + size > s.size()) {
			if (surface) {
				surface->free();
				delete surface;
			}
			return nullptr;
		}
		const int64 next = s.pos() + size - 6;

		switch (id) {
		case 0xfb10:
			bpp = s.readUint16LE();
			width = s.readUint16LE();
			height = s.readUint16LE();
			break;
		case 0xfb20:
			s.read(palette, sizeof(palette)); // B, G, R, x
			break;
		case 0xfb21:
			s.read(key, 3); // R, G, B
			keyed = true;
			break;
		case 0xfb30: {
			if (surface || !width || !height || (bpp != 8 && bpp != 15 && bpp != 16))
				break;
			surface = new Graphics::Surface();
			surface->create(width, height, Graphics::PixelFormat::createFormatRGBA32());
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					byte r, g, b;
					bool transparent;
					if (bpp == 8) {
						const byte *e = palette[s.readByte()];
						r = e[2];
						g = e[1];
						b = e[0];
						transparent = keyed && r == key[0] && g == key[1] && b == key[2];
					} else {
						const uint16 p = s.readUint16LE();
						if (bpp == 15) {
							r = (p >> 10) & 0x1f;
							g = (p >> 5) & 0x1f;
							b = p & 0x1f;
							transparent = keyed && r == key[0] >> 3 && g == key[1] >> 3 && b == key[2] >> 3;
							r = (r << 3) | (r >> 2);
							g = (g << 3) | (g >> 2);
						} else {
							r = (p >> 11) & 0x1f;
							g = (p >> 5) & 0x3f;
							b = p & 0x1f;
							transparent = keyed && r == key[0] >> 3 && g == key[1] >> 2 && b == key[2] >> 3;
							r = (r << 3) | (r >> 2);
							g = (g << 2) | (g >> 4);
						}
						b = (b << 3) | (b >> 2);
					}
					surface->setPixel(x, y, surface->format.ARGBToColor(transparent ? 0 : 255, r, g, b));
				}
			}
			break;
		}
		default:
			// 0xfb22/0xfb23 are opaque; 0xfb31 (vector quantised) does not occur in the corpus
			if (id == 0xfb31)
				warning("DMF: vector-quantised pixels are not supported");
			break;
		}
		s.seek(next);
	}

	if (s.err() && surface) {
		surface->free();
		delete surface;
		surface = nullptr;
	}
	return surface;
}

} // End of namespace X3D
