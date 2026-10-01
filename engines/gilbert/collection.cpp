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
#include "common/hash-str.h"
#include "common/hashmap.h"

#include "gilbert/collection.h"
#include "gilbert/detection.h"

namespace Gilbert {

namespace {

// Delphi's TValueType, the value kinds of a binary component stream.
enum {
	kVaNull = 0, kVaList = 1, kVaInt8 = 2, kVaInt16 = 3, kVaInt32 = 4, kVaExtended = 5,
	kVaString = 6, kVaIdent = 7, kVaFalse = 8, kVaTrue = 9, kVaBinary = 10, kVaSet = 11,
	kVaLString = 12, kVaNil = 13, kVaCollection = 14
};

/** A property value; numbers, booleans (0/1), strings, identifiers and binaries. */
struct Value {
	int32 number = 0;
	Common::String text;
	Common::Array<byte> binary;
};

typedef Common::HashMap<Common::String, Value, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> Properties;

class StreamReader {
public:
	explicit StreamReader(Common::SeekableReadStream &s) : _s(s) {}

	Common::String shortString() {
		const byte n = _s.readByte();
		char buf[256];
		_s.read(buf, n);
		return Common::String(buf, n);
	}

	/** Properties up to the empty name; the items of a collection value go to `items`. */
	bool properties(Properties &props, Common::Array<Properties> *items) {
		for (;;) {
			if (_s.err() || _s.eos())
				return false;
			const Common::String name = shortString();
			if (name.empty())
				return true;
			Value v;
			if (!value(v, items))
				return false;
			props[name] = v;
		}
	}

	bool value(Value &v, Common::Array<Properties> *items) {
		const byte type = _s.readByte();
		switch (type) {
		case kVaInt8:
			v.number = (int8)_s.readByte();
			return true;
		case kVaInt16:
			v.number = _s.readSint16LE();
			return true;
		case kVaInt32:
			v.number = _s.readSint32LE();
			return true;
		case kVaString:
		case kVaIdent:
			v.text = shortString();
			return true;
		case kVaLString:
			v.text = _s.readString(0, _s.readUint32LE());
			return true;
		case kVaFalse:
		case kVaNull:
		case kVaNil:
			return true;
		case kVaTrue:
			v.number = 1;
			return true;
		case kVaExtended:
			_s.skip(10);
			return true;
		case kVaBinary: {
			const uint32 n = _s.readUint32LE();
			v.binary.resize(n);
			return n == 0 || _s.read(v.binary.data(), n) == n;
		}
		case kVaSet:
			while (!_s.eos() && !shortString().empty()) {
			}
			return true;
		case kVaList:
			while (!_s.eos() && _s.readByte() != kVaNull) {
				_s.seek(-1, SEEK_CUR);
				Value element;
				if (!value(element, nullptr))
					return false;
			}
			return true;
		case kVaCollection:
			while (!_s.eos() && _s.readByte() == kVaList) {
				Properties item;
				if (!properties(item, nullptr))
					return false;
				if (items)
					items->push_back(item);
			}
			return true;
		default:
			warning("Gilbert: unknown Delphi value type %d at %d", type, (int)_s.pos() - 1);
			return false;
		}
	}

private:
	Common::SeekableReadStream &_s;
};

/** Skips the resource header and reads the component's collection items. */
bool readCollection(const Common::Path &path, Common::Array<Properties> &items) {
	Common::File f;
	if (!f.open(path)) {
		warning("Gilbert: cannot open %s", path.toString().c_str());
		return false;
	}
	if (f.readByte() != 0xFF || f.readUint16LE() != 10)
		return false;
	while (!f.eos() && f.readByte() != 0) {
	}
	f.readUint16LE(); // flags
	f.readUint32LE(); // size
	if (f.readUint32BE() != MKTAG('T', 'P', 'F', '0'))
		return false;
	StreamReader r(f);
	r.shortString(); // class name
	r.shortString(); // object name
	Properties props;
	return r.properties(props, &items);
}

/** The Delphi colour names (TransparentColor), as RGB. */
uint32 delphiColour(const Common::String &name) {
	static const struct {
		const char *name;
		uint32 rgb;
	} colours[] = {
		{ "clBlack", 0x000000 }, { "clWhite", 0xFFFFFF }, { "clFuchsia", 0xFF00FF },
		{ "clRed", 0xFF0000 }, { "clLime", 0x00FF00 }, { "clBlue", 0x0000FF },
		{ "clAqua", 0x00FFFF }, { "clYellow", 0xFFFF00 }, { "clGray", 0x808080 },
		{ "clSilver", 0xC0C0C0 }, { "clMaroon", 0x800000 }, { "clGreen", 0x008000 },
		{ "clNavy", 0x000080 }, { "clOlive", 0x808000 }, { "clPurple", 0x800080 },
		{ "clTeal", 0x008080 },
	};
	for (const auto &c : colours)
		if (name.equalsIgnoreCase(c.name))
			return c.rgb;
	warning("Gilbert: unknown colour %s", name.c_str());
	return 0xFF00FF;
}

uint16 rgb565(byte r, byte g, byte b) {
	return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

/** `Picture.Data`: "TDIB", a BITMAPINFOHEADER, the palette, bottom-up rows. */
bool decodeDib(const Common::Array<byte> &blob, Graphics::ManagedSurface &out) {
	if (blob.size() < 5 + 40 || blob[0] != 4 || memcmp(&blob[1], "TDIB", 4) != 0)
		return false;
	const byte *h = &blob[5];
	const int32 w = (int32)READ_LE_UINT32(h + 4), hgt = (int32)READ_LE_UINT32(h + 8);
	const uint16 bpp = READ_LE_UINT16(h + 14);
	const uint32 used = READ_LE_UINT32(h + 32);
	if (READ_LE_UINT32(h + 16) != 0 || w <= 0 || hgt == 0)
		return false;
	const uint32 colours = bpp <= 8 ? (used ? used : 1u << bpp) : 0;
	const byte *pal = h + 40;
	const byte *pixels = pal + 4 * colours;
	const uint32 stride = (w * bpp + 31) / 32 * 4;
	const int rows = ABS(hgt);
	if (pixels + stride * rows > blob.data() + blob.size())
		return false;
	out.create(w, rows, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));
	for (int y = 0; y < rows; y++) {
		const byte *src = pixels + stride * (hgt > 0 ? rows - 1 - y : y);
		uint16 *dst = (uint16 *)out.getBasePtr(0, y);
		for (int x = 0; x < w; x++) {
			uint32 index;
			switch (bpp) {
			case 24:
				dst[x] = rgb565(src[3 * x + 2], src[3 * x + 1], src[3 * x]);
				continue;
			case 8:
				index = src[x];
				break;
			case 4:
				index = (src[x >> 1] >> ((x & 1) ? 0 : 4)) & 15;
				break;
			case 1:
				index = (src[x >> 3] >> (7 - (x & 7))) & 1;
				break;
			default:
				return false;
			}
			const byte *c = pal + 4 * MIN<uint32>(index, colours - 1);
			dst[x] = rgb565(c[2], c[1], c[0]);
		}
	}
	return true;
}

} // End of anonymous namespace

// A pattern size of 0 is the whole picture's (formats README "Picture items").
int Picture::patternCount() const {
	const int w = patternW > 0 ? patternW : surface.w, h = patternH > 0 ? patternH : surface.h;
	return w && h ? (surface.w / w) * (surface.h / h) : 0;
}

Common::Rect Picture::pattern(int k) const {
	const int w = patternW > 0 ? patternW : surface.w, h = patternH > 0 ? patternH : surface.h;
	if (k < 0 || k >= patternCount())
		return Common::Rect();
	const int cols = surface.w / w;
	const int x = (k % cols) * w, y = (k / cols) * h;
	return Common::Rect(x, y, x + w, y + h);
}

bool PictureCollection::load(const Common::Path &path) {
	clear();
	Common::Array<Properties> items;
	if (!readCollection(path, items)) {
		warning("Gilbert: bad picture collection %s", path.toString().c_str());
		return false;
	}
	for (const Properties &p : items) {
		Picture *pic = new Picture();
		if (p.contains("Name"))
			pic->name = p["Name"].text;
		if (p.contains("Picture.Data") && !decodeDib(p["Picture.Data"].binary, pic->surface))
			warning("Gilbert: bad picture %s in %s", pic->name.c_str(), path.toString().c_str());
		pic->transparent = p.contains("Transparent") && p["Transparent"].number;
		const uint32 rgb = delphiColour(p.contains("TransparentColor") ? p["TransparentColor"].text : "clBlack");
		pic->key = rgb565(rgb >> 16, (rgb >> 8) & 0xFF, rgb & 0xFF);
		pic->patternW = p.contains("PatternWidth") ? p["PatternWidth"].number : 0;
		pic->patternH = p.contains("PatternHeight") ? p["PatternHeight"].number : 0;
		pic->last = Common::Rect(pic->surface.w, pic->surface.h);
		_items.push_back(pic);
	}
	debugC(1, kDebugLoad, "PictureCollection: %s, %d pictures", path.toString().c_str(), _items.size());
	return true;
}

void PictureCollection::clear() {
	for (Picture *p : _items)
		delete p;
	_items.clear();
}

bool loadWaves(const Common::Path &path, Common::Array<Wave> &waves) {
	waves.clear();
	Common::Array<Properties> items;
	if (!readCollection(path, items)) {
		warning("Gilbert: bad wave collection %s", path.toString().c_str());
		return false;
	}
	for (const Properties &p : items) {
		Wave w;
		if (p.contains("Name"))
			w.name = p["Name"].text;
		w.looped = p.contains("Looped") && p["Looped"].number;
		if (p.contains("Wave.WAVE"))
			w.data = p["Wave.WAVE"].binary;
		waves.push_back(w);
	}
	return true;
}

} // End of namespace Gilbert
