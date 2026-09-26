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
#include "common/file.h"
#include "common/memstream.h"
#include "common/substream.h"
#include "common/textconsole.h"

#include "ring/codec.h"
#include "ring/detection.h"
#include "ring/resources.h"

namespace Ring {

const char *zoneFolder(int zone) {
	static const char *const folders[] = { "sy", "ni", "rh", "fo", "ro", "wa", "as", "n2" };
	return zone >= 1 && zone <= 8 ? folders[zone - 1] : "";
}

bool ArtArchive::open(const Common::Path &path) {
	Common::File f;
	if (!f.open(path))
		return false;
	byte magic[8];
	f.read(magic, 8);
	if (memcmp(magic, "AT_II", 5) && memcmp(magic, "ATIII", 5))
		return false;
	f.readUint32LE(); // directory size
	uint32 count = f.readUint32LE();
	f.seek(32);
	for (uint32 i = 0; i < count; i++) {
		char name[244] = {};
		f.read(name, 243);
		Entry e;
		e.offset = f.readUint32LE();
		e.size = f.readUint32LE();
		f.readUint32LE(); // unpacked size
		_entries[name] = e;
	}
	_path = path;
	debugC(1, kDebugLoad, "Opened %s: %u members", path.toString().c_str(), count);
	return !f.err();
}

Common::SeekableReadStream *ArtArchive::member(const Common::String &name) const {
	if (!_entries.contains(name))
		return nullptr;
	const Entry &e = _entries[name];
	Common::File *f = new Common::File();
	if (!f->open(_path)) {
		delete f;
		return nullptr;
	}
	return new Common::SeekableSubReadStream(f, e.offset, e.offset + e.size, DisposeAfterUse::YES);
}

bool Resources::openArchive(int zone) {
	if (_archives.contains(zone))
		return true;
	Common::Path path = zone == kZoneSY ? Common::Path("DATA").appendComponent(_language).appendComponent("SY.AT2")
										: Common::Path("DATA").appendComponent(Common::String(zoneFolder(zone)) + ".at2");
	Common::SharedPtr<ArtArchive> art(new ArtArchive());
	if (!art->open(path)) {
		warning("Ring: cannot open %s", path.toString().c_str());
		return false;
	}
	_archives[zone] = art;
	return true;
}

Image *Resources::loadImage(int zone, const Common::String &name, bool fromArchive) {
	if (fromArchive && openArchive(zone)) {
		Common::ScopedPtr<Common::SeekableReadStream> s(_archives[zone]->member("\\IMAGE\\" + name));
		if (s)
			return decodeImage(*s, name, true);
		debugC(1, kDebugLoad, "%s is not in the %s archive, trying the loose file", name.c_str(), zoneFolder(zone));
	}
	Common::File f;
	Common::Path path = Common::Path("DATA").appendComponent(zoneFolder(zone)).appendComponent("IMAGE").appendComponent(name);
	if (!f.open(path)) {
		warning("Ring: cannot open image %s", path.toString().c_str());
		return nullptr;
	}
	return decodeImage(f, name, false);
}

Image *Resources::decodeImage(Common::SeekableReadStream &stream, const Common::String &name, bool fromArchive) {
	Common::String ext = name.size() >= 3 ? name.substr(name.size() - 3) : "";
	ext.toLowercase();
	if (ext == "tga" || ext == "tgc")
		return fromArchive || ext == "tgc" ? decodeTgc(stream) : decodeTga(stream);
	if (ext == "bma" || fromArchive)
		return decodeBma(stream);
	return decodeBmp(stream);
}

Image *Resources::fromRgb555(const Common::Array<uint16> &pixels, int w, int h) {
	Image *img = new Image();
	img->surface.create(w, h, _format);
	for (int y = 0; y < h; y++) {
		uint16 *row = (uint16 *)img->surface.getBasePtr(0, h - 1 - y); // rows are stored bottom-up
		for (int x = 0; x < w; x++) {
			uint16 p = pixels[y * w + x];
			row[x] = _format.RGBToColor(((p >> 10) & 31) << 3, ((p >> 5) & 31) << 3, (p & 31) << 3);
		}
	}
	return img;
}

// Packed 16-bit image (bma.ksy): a colour table of 3-pixel entries and one index per entry.
Image *Resources::decodeBma(Common::SeekableReadStream &s) {
	uint32 size = s.size();
	Common::Array<byte> data(size);
	s.read(data.data(), size);
	if (size < 0x50)
		return nullptr;
	const byte *d = data.data();
	uint32 seqSize = READ_LE_UINT32(d);
	uint16 coreCount = READ_LE_UINT16(d + 4), perEntry = READ_LE_UINT16(d + 6);
	uint32 w = READ_LE_UINT32(d + 8), h = READ_LE_UINT32(d + 12);
	if (perEntry != 3 || 0x50 + seqSize > size || w > 4096 || h > 4096)
		return nullptr;
	uint32 coreSize = READ_LE_UINT32(d + 0x4c + seqSize);
	uint32 coreStart = 0x50 + seqSize;
	if (coreStart + coreSize > size)
		return nullptr;
	uint32 need = w * h * 2 / 3 / 2;
	uint bits = 0;
	for (uint16 c = coreCount; c; c >>= 1)
		bits++;
	Common::Array<uint16> seq = decodeBits(d, size, bits, 0x260, 0x260 + seqSize * 8, need + 1);
	Common::Array<uint16> core = decodeBits(d, size, 16, coreStart * 8, (coreStart + coreSize) * 8, coreCount * 3u + 1);
	Common::Array<uint16> pixels(w * h, 0);
	for (uint32 i = 0; i < need && i < seq.size(); i++) {
		uint16 e = seq[i];
		for (uint k = 0; k < 3 && e * 3u + k < core.size() && i * 3 + k < pixels.size(); k++)
			pixels[i * 3 + k] = core[e * 3 + k];
	}
	for (uint k = 0; k < 3 && k < pixels.size(); k++)
		pixels[pixels.size() - 3 + k] = READ_LE_UINT16(d + 0x10 + 2 * k);
	return fromRgb555(pixels, w, h);
}

// Packed TGA (tgc.ksy): chunks of bit stream that concatenate to a TGA file.
Image *Resources::decodeTgc(Common::SeekableReadStream &s) {
	uint32 count = s.readUint32LE(), total = s.readUint32LE();
	if (total > 10000000)
		return nullptr;
	Common::Array<byte> tga;
	for (uint32 i = 0; i < count && !s.eos(); i++) {
		uint32 packed = s.readUint32LE(), unpacked = s.readUint32LE();
		Common::Array<byte> chunk(packed);
		s.read(chunk.data(), packed);
		Common::Array<uint16> codes = decodeBits(chunk.data(), packed, 16, 0, packed * 8, unpacked / 2);
		for (uint16 c : codes) {
			tga.push_back(c & 0xff);
			tga.push_back(c >> 8);
		}
	}
	return fromTga(tga.data(), tga.size());
}

Image *Resources::decodeTga(Common::SeekableReadStream &s) {
	Common::Array<byte> data(s.size());
	s.read(data.data(), data.size());
	return fromTga(data.data(), data.size());
}

// 24- or 32-bit TGA, rows filled from the last one up (aImageFileTgc::ReadImage);
// 32-bit colours premultiplied as 0x42c0a0 does (spec/drawing.md).
Image *Resources::fromTga(const byte *data, uint32 size) {
	if (size < 18 || data[2] != 2)
		return nullptr;
	uint w = READ_LE_UINT16(data + 12), h = READ_LE_UINT16(data + 14), bpp = data[16];
	if ((bpp != 24 && bpp != 32) || 18 + w * h * (bpp / 8) > size)
		return nullptr;
	Image *img = new Image();
	img->trueColor = true;
	img->surface.create(w, h, _format);
	img->alpha.resize(w * h);
	const byte *p = data + 18 + data[0];
	for (uint y = 0; y < h; y++) {
		uint16 *row = (uint16 *)img->surface.getBasePtr(0, h - 1 - y);
		for (uint x = 0; x < w; x++, p += bpp / 8) {
			byte b = p[0], g = p[1], r = p[2], a = bpp == 32 ? p[3] : ((r | g | b) ? 255 : 0);
			if (bpp == 32) {
				float f = a * (1.0f / 255.0f);
				r = (byte)(r * f);
				g = (byte)(g * f);
				b = (byte)(b * f);
			}
			row[x] = _format.RGBToColor(r, g, b);
			img->alpha[(h - 1 - y) * w + x] = a;
		}
	}
	return img;
}

// Plain 24-bit BMP from disk (bmp.py): rows bottom-up, padded to 4 bytes.
Image *Resources::decodeBmp(Common::SeekableReadStream &s) {
	if (s.readUint16BE() != MKTAG16('B', 'M'))
		return nullptr;
	s.skip(8);
	uint32 offset = s.readUint32LE();
	s.readUint32LE();
	int32 w = s.readSint32LE(), h = s.readSint32LE();
	s.readUint16LE();
	uint16 bpp = s.readUint16LE();
	if (bpp != 24 || w <= 0 || h <= 0)
		return nullptr;
	Image *img = new Image();
	img->trueColor = true;
	img->surface.create(w, h, _format);
	img->alpha.resize(w * h);
	uint32 stride = (w * 3 + 3) & ~3;
	Common::Array<byte> row(stride);
	for (int y = 0; y < h; y++) {
		s.seek(offset + y * stride);
		s.read(row.data(), stride);
		uint16 *dst = (uint16 *)img->surface.getBasePtr(0, h - 1 - y);
		for (int x = 0; x < w; x++) {
			byte b = row[x * 3], g = row[x * 3 + 1], r = row[x * 3 + 2];
			dst[x] = _format.RGBToColor(r, g, b);
			img->alpha[(h - 1 - y) * w + x] = (r | g | b) ? 255 : 0;
		}
	}
	return img;
}

void Image::draw(Graphics::ManagedSurface &dst, int x, int y, byte type) const {
	Common::Rect r(x, y, x + surface.w, y + surface.h);
	r.clip(Common::Rect(dst.w, dst.h));
	if (r.isEmpty())
		return;
	const Graphics::PixelFormat &f = dst.format;
	uint32 masks[3] = { (uint32)f.rMax() << f.rShift, (uint32)f.gMax() << f.gShift, (uint32)f.bMax() << f.bShift };
	for (int yy = r.top; yy < r.bottom; yy++) {
		const uint16 *src = (const uint16 *)surface.getBasePtr(r.left - x, yy - y);
		const byte *a = alpha.empty() ? nullptr : &alpha[(yy - y) * surface.w + (r.left - x)];
		uint16 *out = (uint16 *)dst.getBasePtr(r.left, yy);
		for (int xx = r.left; xx < r.right; xx++, src++, out++) {
			if (!trueColor || type == 1 || !a) {
				*out = *src;
			} else if (type == 2) {
				if (a[xx - r.left])
					*out = *src;
			} else if (type == 3) {
				byte al = a[xx - r.left];
				if (!al)
					continue;
				if (al == 255) {
					*out = *src;
					continue;
				}
				uint32 v = 0;
				for (uint32 m : masks)
					v |= ((*src & m) + (((*out & m) * (uint32)(255 - al)) >> 8)) & m;
				*out = (uint16)v;
			} else {
				*out = *src;
			}
		}
	}
}

} // End of namespace Ring
