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
#include "common/endian.h"
#include "common/file.h"
#include "common/memstream.h"

#include "image/codecs/hlz.h"

#include "peintre/detection.h"
#include "peintre/gfx.h"

namespace Peintre {

static const Graphics::PixelFormat kFormat565(2, 5, 6, 5, 0, 11, 5, 0, 0);

static bool readAll(const Common::Path &path, Common::Array<byte> &out) {
	Common::File f;
	if (!f.open(path))
		return false;
	out.resize(f.size());
	return f.read(out.data(), out.size()) == out.size();
}

bool loadTgp(const Common::String &name, Graphics::Surface &out) {
	// u32 width, u32 height; "LZWCRYO" at 12 = single form, else chunked (E-0100, E-0101).
	Common::Array<byte> d;
	if (!readAll(Common::Path("GFX/" + name + ".TGP"), d) || d.size() < 16) {
		warning("loadTgp: cannot read %s", name.c_str());
		return false;
	}
	const uint32 w = READ_LE_UINT32(d.data()), h = READ_LE_UINT32(d.data() + 4);
	if (w == 0 || h == 0 || w > 4096 || h > 4096)
		return false;
	out.create(w, h, kFormat565);
	byte *dst = (byte *)out.getPixels();
	if (d.size() >= 0x2C && !memcmp(d.data() + 12, "LZWCRYO", 7)) {
		// Single: the packed size at 0x28, one HLZ stream from 0x2C.
		const uint32 packed = READ_LE_UINT32(d.data() + 0x28);
		if (0x2C + packed > d.size())
			return false;
		Common::MemoryReadStream s(d.data() + 0x2C, packed);
		Image::HLZDecoder::decodeFrameInPlace(s, packed, dst);
	} else {
		// Chunked: u32 count, {packed, unpacked, HLZ} unpacked back to back.
		const uint32 count = READ_LE_UINT32(d.data() + 8);
		uint32 pos = 12, outPos = 0;
		for (uint32 i = 0; i < count; i++) {
			if (pos + 8 > d.size())
				return false;
			const uint32 packed = READ_LE_UINT32(d.data() + pos);
			const uint32 unpacked = READ_LE_UINT32(d.data() + pos + 4);
			pos += 8;
			if (pos + packed > d.size() || outPos + unpacked > w * h * 2)
				return false;
			Common::MemoryReadStream s(d.data() + pos, packed);
			Image::HLZDecoder::decodeFrameInPlace(s, packed, dst + outPos);
			pos += packed;
			outPos += unpacked;
		}
	}
	return true;
}

bool loadTga(const Common::String &name, Graphics::Surface &out) {
	Common::Array<byte> d;
	if (!readAll(Common::Path("Graphs_2D/" + name + ".TGA"), d) || d.size() < 18)
		return false;
	const int w = READ_LE_UINT16(d.data() + 12), h = READ_LE_UINT16(d.data() + 14);
	if (18 + (uint32)w * h * 2 > d.size())
		return false;
	out.create(w, h, kFormat565);
	for (int y = 0; y < h; y++) {
		const byte *src = d.data() + 18 + (h - 1 - y) * w * 2;
		uint16 *dst = (uint16 *)out.getBasePtr(0, y);
		for (int x = 0; x < w; x++) {
			const uint16 c = READ_LE_UINT16(src + 2 * x);
			// X1R5G5B5 to RGB565: green gets one more bit (E-0104).
			dst[x] = ((c & 0x7C00) << 1) | ((c & 0x03E0) << 1) | (c & 0x1F);
		}
	}
	return true;
}

void blitKeyed(Graphics::Surface &dst, const Graphics::Surface &src, int x, int y) {
	for (int sy = 0; sy < src.h; sy++) {
		const int dy = y + sy;
		if (dy < 0 || dy >= dst.h)
			continue;
		const uint16 *s = (const uint16 *)src.getBasePtr(0, sy);
		uint16 *d = (uint16 *)dst.getBasePtr(0, dy);
		for (int sx = 0; sx < src.w; sx++) {
			const int dx = x + sx;
			if (dx >= 0 && dx < dst.w && s[sx] != kTgaKey)
				d[dx] = s[sx];
		}
	}
}

bool SpriteBank::load(const Common::String &name) {
	_name = name;
	_bank.clear();
	_offsets.clear();
	Common::Array<byte> d;
	if (!readAll(Common::Path("SPRITES/" + name + ".SPR"), d) || d.size() < 8) {
		warning("SpriteBank: cannot read %s", name.c_str());
		return false;
	}
	// u32 head: bit 31 raw, bit 30 band, bits 0..29 the palette end (E-0102).
	const uint32 head = READ_LE_UINT32(d.data());
	const uint32 palEnd = head & 0x3FFFFFFF;
	const bool raw = head & 0x80000000, band = (head & 0x40000000) && !raw;
	const bool hasPalette = palEnd > 4;
	if (palEnd > d.size())
		return false;
	for (int i = 0; i < 256; i++) {
		if (hasPalette && (uint32)(4 + 3 * i + 2) < palEnd) {
			const byte *p = d.data() + 4 + 3 * i;
			_palette[i] = ((p[0] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[2] >> 3);
		} else {
			_palette[i] = 0;
		}
	}
	_kind = raw ? (hasPalette ? kRaw8 : kRaw16) : (band ? kBand : kRle);
	_bank.resize(d.size() - palEnd);
	memcpy(_bank.data(), d.data() + palEnd, _bank.size());
	if (_bank.size() < 4)
		return false;
	const uint32 n = READ_LE_UINT32(_bank.data()) / 4;
	for (uint32 i = 0; i < n && 4 * i + 4 <= _bank.size(); i++)
		_offsets.push_back(READ_LE_UINT32(_bank.data() + 4 * i));
	debugC(2, kDebugLoad, "SpriteBank: %s, kind %d, %d frames", name.c_str(), _kind, n);
	return true;
}

void SpriteBank::frameSize(uint frame, int &a, int &b) const {
	a = b = 0;
	if (frame >= _offsets.size() || _offsets[frame] + 4 > _bank.size())
		return;
	a = READ_LE_UINT16(_bank.data() + _offsets[frame]);
	b = READ_LE_UINT16(_bank.data() + _offsets[frame] + 2);
}

void SpriteBank::drawRle(Graphics::Surface &dst, const byte *p, const byte *end, int x0, int y0, int rows, int width) const {
	// Row: 0x80 ends it; c < 0x80 skips c pixels; c > 0x80: c & 0x7F palette indices follow.
	for (int r = 0; r < rows && p < end; r++) {
		int x = x0;
		const int y = y0 + r;
		while (p < end) {
			const byte c = *p++;
			if (c == 0x80)
				break;
			if (c < 0x80) {
				x += c;
				continue;
			}
			const int n = c & 0x7F;
			for (int i = 0; i < n && p < end; i++, x++) {
				const byte idx = *p++;
				if (y >= 0 && y < dst.h && x >= 0 && x < dst.w && x < x0 + width)
					*(uint16 *)dst.getBasePtr(x, y) = _palette[idx];
			}
		}
	}
}

void SpriteBank::draw(Graphics::Surface &dst, uint frame, int x, int y) const {
	if (frame >= _offsets.size())
		return;
	const uint32 start = _offsets[frame];
	const uint32 stop = frame + 1 < _offsets.size() ? _offsets[frame + 1] : _bank.size();
	if (start + 4 > stop || stop > _bank.size())
		return;
	const byte *p = _bank.data() + start;
	const int a = READ_LE_UINT16(p), b = READ_LE_UINT16(p + 2);
	const byte *data = p + 4, *end = _bank.data() + stop;
	switch (_kind) {
	case kRle:
		drawRle(dst, data, end, x - a / 2, y - b / 2, b, a);
		break;
	case kBand:
		drawRle(dst, data, end, 0, a, b, 640);
		break;
	case kRaw8:
	case kRaw16:
		for (int r = 0; r < b; r++) {
			const int dy = y + r;
			if (dy < 0 || dy >= dst.h)
				continue;
			for (int c = 0; c < a; c++) {
				const int dx = x + c;
				if (dx < 0 || dx >= dst.w)
					continue;
				uint16 colour;
				if (_kind == kRaw8) {
					if (data + r * a + c >= end)
						return;
					colour = _palette[data[r * a + c]];
				} else {
					if (data + 2 * (r * a + c) + 2 > end)
						return;
					colour = READ_LE_UINT16(data + 2 * (r * a + c));
				}
				*(uint16 *)dst.getBasePtr(dx, dy) = colour;
			}
		}
		break;
	}
}

bool SpriteBank::hit(uint frame, int x, int y, int px, int py) const {
	int a, b;
	frameSize(frame, a, b);
	switch (_kind) {
	case kRle:
		return px >= x - a / 2 && px < x - a / 2 + a && py >= y - b / 2 && py < y - b / 2 + b;
	case kBand:
		return py >= a && py < a + b;
	default:
		return px >= x && px < x + a && py >= y && py < y + b;
	}
}

bool Font::load(const Common::String &name) {
	Common::Array<byte> d;
	if (!readAll(Common::Path("FONTS/" + name + ".AWF"), d) || d.size() < 38) {
		warning("Font: cannot read %s", name.c_str());
		return false;
	}
	// 38-byte header, offsets into the data after it (E-0105).
	_glyphs = READ_LE_UINT32(d.data() + 8);
	_table = READ_LE_UINT32(d.data() + 12);
	_widths = READ_LE_UINT32(d.data() + 16);
	_spaceA = READ_LE_UINT32(d.data() + 20);
	_spaceB = READ_LE_UINT32(d.data() + 24);
	_first = d[28];
	_count = d[29];
	_baseline = d[30];
	_proportional = READ_LE_UINT16(d.data() + 32) & 1;
	_fixedWidth = READ_LE_UINT16(d.data() + 34);
	_height = READ_LE_UINT16(d.data() + 36);
	_data.resize(d.size() - 38);
	memcpy(_data.data(), d.data() + 38, _data.size());
	return true;
}

int Font::advance(byte c) const {
	const int i = c - _first;
	if (c < _first || i >= _count)
		return 0;
	if (!_proportional)
		return _fixedWidth;
	if (_spaceB + i >= _data.size())
		return 0;
	return _data[_widths + i] + _data[_spaceA + i] + (int8)_data[_spaceB + i];
}

int Font::width(const Common::String &text) const {
	int w = 0;
	for (uint k = 0; k < text.size(); k++)
		w += advance(text[k]);
	return w;
}

void Font::draw(Graphics::Surface &dst, const Common::String &text, int x, int y, uint16 colour, bool shadow) const {
	int pen = x;
	const int top = y - _baseline;
	for (uint k = 0; k < text.size(); k++) {
		const byte c = text[k];
		if (c < _first || c - _first >= _count)
			continue;
		const int i = c - _first;
		const int w = _proportional ? _data[_widths + i] : _fixedWidth;
		const int stride = (w + 7) / 8;
		if (_table + 4 * i + 4 > _data.size())
			continue;
		const uint32 g = _glyphs + READ_LE_UINT32(_data.data() + _table + 4 * i);
		for (int r = 0; r < _height; r++) {
			for (int col = 0; col < w; col++) {
				const uint32 o = g + r * stride + col / 8;
				if (o >= _data.size() || !(_data[o] & (0x80 >> (col & 7))))
					continue;
				const int px = pen + col, py = top + r;
				if (shadow && px + 1 < dst.w && py + 1 < dst.h && px + 1 >= 0 && py + 1 >= 0)
					*(uint16 *)dst.getBasePtr(px + 1, py + 1) = 0;
				if (px >= 0 && px < dst.w && py >= 0 && py < dst.h)
					*(uint16 *)dst.getBasePtr(px, py) = colour;
			}
		}
		pen += advance(c);
	}
}

} // End of namespace Peintre
