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

#ifndef PEINTRE_GFX_H
#define PEINTRE_GFX_H

#include "common/array.h"
#include "common/path.h"
#include "common/str.h"

#include "graphics/surface.h"

namespace Peintre {

// 2D formats (docs/formats/README.md: .TGP, .SPR, .TGA, .AWF). Everything is RGB565.

/** Loads GFX/<name>.TGP, single or chunked form. */
bool loadTgp(const Common::String &name, Graphics::Surface &out);

/** Loads Graphs_2D/<name>.TGA (X1R5G5B5, bottom row first) as RGB565. */
bool loadTga(const Common::String &name, Graphics::Surface &out);

/** The TGA key colour, pure green, in RGB565. */
const uint16 kTgaKey = 0x07C0;

/** Draws a TGA surface, skipping the key colour. */
void blitKeyed(Graphics::Surface &dst, const Graphics::Surface &src, int x, int y);

/** A sprite bank, SPRITES/<name>.SPR. */
class SpriteBank {
public:
	enum Kind {
		kRle,     ///< centred at (x - w/2, y - h/2)
		kBand,    ///< full-width rows from screen row a, x/y ignored
		kRaw8,    ///< opaque palette indices at (x, y)
		kRaw16    ///< opaque RGB565 at (x, y)
	};

	bool load(const Common::String &name);
	bool isLoaded() const { return !_bank.empty(); }
	const Common::String &name() const { return _name; }
	uint frameCount() const { return _offsets.size(); }
	Kind kind() const { return _kind; }
	/** Frame header words: width/height, or first row/row count for band frames. */
	void frameSize(uint frame, int &a, int &b) const;
	void draw(Graphics::Surface &dst, uint frame, int x, int y) const;
	/** True when (px, py) hits an opaque pixel of the frame drawn at (x, y). */
	bool hit(uint frame, int x, int y, int px, int py) const;

private:
	void drawRle(Graphics::Surface &dst, const byte *p, const byte *end, int x0, int y0, int rows, int width) const;

	Common::String _name;
	Kind _kind = kRle;
	uint16 _palette[256];
	Common::Array<byte> _bank;
	Common::Array<uint32> _offsets;
};

/** An AWF bitmap font, FONTS/<name>.AWF. */
class Font {
public:
	bool load(const Common::String &name);
	void draw(Graphics::Surface &dst, const Common::String &text, int x, int y, uint16 colour, bool shadow = false) const;
	int width(const Common::String &text) const;
	int height() const { return _height; }

private:
	int advance(byte c) const;

	Common::Array<byte> _data;   ///< from the end of the 38-byte header
	uint32 _glyphs = 0, _table = 0, _widths = 0, _spaceA = 0, _spaceB = 0;
	byte _first = 0, _count = 0, _baseline = 0;
	bool _proportional = false;
	int _fixedWidth = 0, _height = 0;
};

} // End of namespace Peintre

#endif // PEINTRE_GFX_H
