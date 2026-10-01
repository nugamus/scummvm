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
#include "common/stream.h"
#include "common/util.h"

#include "graphics/managed_surface.h"

#include "ring/codec.h"
#include "ring/rotation.h"
#include "ring/world.h"

namespace Ring {

// Node: u32 index size, 13-word header, index stream (13-bit), u32 table size, table
// stream (16-bit); then one section per layer: u32 count, rate, last frame, and `count`
// entries (u32 size, 13-word header, index stream) (formats README, aqc.ksy).
bool Panorama::load(Common::SeekableReadStream &s, uint layerCount) {
	uint32 size = s.size();
	Common::Array<byte> d(size);
	if (size < 60 || s.read(d.data(), size) != size)
		return false;
	uint32 indexSize = READ_LE_UINT32(&d[0]);
	const byte *h = &d[4];
	uint32 width = READ_LE_UINT32(h), dataSize = READ_LE_UINT32(h + 44);
	height = READ_LE_UINT32(h + 4);
	// unk_3 .. unk_6 read as floats (unk_3 is the integer 0, so 0.0)
	float h0 = READ_LE_FLOAT32(h + 12), h1 = READ_LE_FLOAT32(h + 16);
	float v0 = READ_LE_FLOAT32(h + 20), v1 = READ_LE_FLOAT32(h + 24);
	hRange = h1 - h0;
	hMid = (h0 + h1) * 0.5f;
	vRange = v1 - v0;
	vMid = (v0 + v1) * 0.5f;
	uint32 pos = 56;
	if (width != 2048 || height < 2 || dataSize != width * height * 2 || pos + indexSize + 4 > size)
		return false;
	_index = decodeBits(d.data(), size, 13, pos * 8, (pos + indexSize) * 8, dataSize / 8);
	pos += indexSize;
	uint32 tableSize = READ_LE_UINT32(&d[pos]);
	pos += 4;
	if (pos + tableSize > size)
		return false;
	_table = decodeBits(d.data(), size, 16, pos * 8, (pos + tableSize) * 8, 1 << 17);
	if (_index.size() != dataSize / 8)
		return false;
	for (uint16 i : _index)
		if (4u * i + 3 >= _table.size())
			return false;

	// 0x4111d0, 0x411150, 0x410d70; then the backup under the first entry (0x410e50).
	pos += tableSize;
	_layers.resize(layerCount);
	for (Layer &l : _layers) {
		if (pos + 12 > size)
			return false;
		uint32 count = READ_LE_UINT32(&d[pos]);
		l.animated = READ_LE_FLOAT32(&d[pos + 4]) != 0.0f;
		pos += 12;
		for (uint32 e = 0; e < count; e++) {
			if (pos + 56 > size)
				return false;
			uint32 streamSize = READ_LE_UINT32(&d[pos]);
			const byte *eh = &d[pos + 4];
			Patch p;
			p.x0 = READ_LE_UINT32(eh + 28);
			p.x1 = READ_LE_UINT32(eh + 32);
			p.y0 = READ_LE_UINT32(eh + 36);
			p.y1 = READ_LE_UINT32(eh + 40);
			uint32 entrySize = READ_LE_UINT32(eh + 44);
			pos += 56;
			if (pos + streamSize > size || p.x0 >= p.x1 || p.x1 > 2048 || (p.x0 & 3) || (p.x1 & 3) ||
				p.y0 >= p.y1 || p.y1 > (uint32)height || entrySize != (p.x1 - p.x0) * (p.y1 - p.y0) * 2)
				return false;
			p.index = decodeBits(d.data(), size, 13, pos * 8, (pos + streamSize) * 8, entrySize / 8);
			pos += streamSize;
			if (p.index.size() != entrySize / 8)
				return false;
			for (uint16 i : p.index)
				if (4u * i + 3 >= _table.size())
					return false;
			l.entries.push_back(p);
		}
		if (!l.entries.empty()) {
			l.backup = l.entries[0];
			l.backup.index.clear();
			for (uint32 y = l.backup.y0; y < l.backup.y1; y++)
				for (uint32 x = l.backup.x0 / 4; x < l.backup.x1 / 4; x++)
					l.backup.index.push_back(_index[y * 512 + x]);
		}
	}
	return true;
}

void Panorama::patch(uint layer, int frame) {
	if (layer >= _layers.size() || _layers[layer].entries.empty())
		return;
	const Layer &l = _layers[layer];
	const Patch &p = frame < 0 ? l.backup : l.entries[MIN<uint>(frame, l.entries.size() - 1)];
	uint32 w = (p.x1 - p.x0) / 4;
	for (uint32 y = p.y0, i = 0; y < p.y1; y++, i += w)
		memcpy(&_index[y * 512 + p.x0 / 4], &p.index[i], w * 2);
}

void RotationView::update(Rotation &r, const Panorama &p) {
	r.ran = CLIP(r.ran, 30.0f, 87.0f);
	double half = r.ran * M_PI / 360.0;
	double s = sin(half), c = cos(half), t = s * kHeight / kWidth;
	double fovV = 2 * asin(t) * 180.0 / M_PI;
	while (r.alpha > 360.0f)
		r.alpha -= 360.0f;
	while (r.alpha < 0.0f)
		r.alpha += 360.0f;
	float margin = (float)((p.vRange - fovV) * 0.5 - 5.0);
	if (r.beta > p.vMid + margin)
		r.beta = p.vMid + margin;
	if (r.beta < p.vMid - margin)
		r.beta = p.vMid - margin;

	// Forward, up and right (0x410a20); screen y grows downwards with world y.
	double a = r.alpha * M_PI / 180.0, b = r.beta * M_PI / 180.0;
	double f[3] = { sin(a) * cos(b), sin(b), cos(a) * cos(b) };
	double fl = sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
	for (double &x : f)
		x /= fl;
	double up[3] = { -f[1] * f[0], 1 - f[1] * f[1], -f[1] * f[2] };
	double ul = sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
	for (double &x : up)
		x /= ul;
	double rt[3] = { up[1] * f[2] - up[2] * f[1], up[2] * f[0] - up[0] * f[2], up[0] * f[1] - up[1] * f[0] };

	for (int j = 0; j <= kRows; j++) {
		double y = -t + 2 * t * j / kRows;
		for (int i = 0; i <= kCols; i++) {
			double x = -s + 2 * s * i / kCols;
			double d[3];
			for (int k = 0; k < 3; k++)
				d[k] = x * rt[k] + y * up[k] + c * f[k];
			double len = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
			double lat = asin(CLIP(d[1] / len, -1.0, 1.0));
			double lon = asin(CLIP(d[0] / len / cos(lat), -1.0, 1.0));
			if (d[2] < 0)
				lon = lon > 0 ? M_PI - lon : -M_PI - lon;
			double u = (lon * 180.0 / M_PI + p.hMid) / p.hRange * 2048.0;
			double v = (lat * 180.0 / M_PI - p.vMid) / p.vRange * p.height + p.height * 0.5;
			_u[j][i] = (int32)(u * 65536.0);
			_v[j][i] = (int32)(v * 65536.0);
		}
	}
}

// Brings `u` within half a panorama of `ref` (0x411810); `full` is 2048 in u's units.
template<typename T>
static T unwrap(T u, T ref, T full) {
	if (u - ref > full / 2)
		return u - full;
	if (ref - u > full / 2)
		return u + full;
	return u;
}

void RotationView::draw(const Panorama &p, Graphics::ManagedSurface &dst, int top) const {
	const int32 full = 2048 << 16, vMax = (p.height - 2) << 16;
	auto v = [&](int j, int i) { return CLIP<int32>(_v[j][i], 0, vMax); }; // 0x412180
	for (int by = 0; by < kRows; by++) {
		for (int bx = 0; bx < kCols; bx++) {
			int32 u0 = _u[by][bx];
			int32 lu = u0, ru = unwrap(_u[by][bx + 1], u0, full);
			int32 lv = v(by, bx), rv = v(by, bx + 1);
			int32 dlu = (unwrap(_u[by + 1][bx], u0, full) - lu) >> 4, dlv = (v(by + 1, bx) - lv) >> 4;
			int32 dru = (unwrap(_u[by + 1][bx + 1], u0, full) - ru) >> 4, drv = (v(by + 1, bx + 1) - rv) >> 4;
			for (int row = 0; row < 16; row++) {
				uint16 *out = (uint16 *)dst.getBasePtr(bx * 16, top + by * 16 + row);
				int32 su = (ru - lu) >> 4, sv = (rv - lv) >> 4, u = lu, vv = lv;
				for (int col = 0; col < 16; col++, u += su, vv += sv)
					*out++ = p.pixel((u >> 16) & 2047, vv >> 16);
				lu += dlu;
				lv += dlv;
				ru += dru;
				rv += drv;
			}
		}
	}
}

Common::Point RotationView::toPanorama(const Panorama &p, int x, int y) const {
	x = CLIP(x, 0, kWidth - 1);
	y = CLIP(y - 16, 0, kHeight - 1);
	int cx = x >> 4, cy = y >> 4, fx = x & 15, fy = y & 15;
	auto lerp = [&](double a, double b, double c, double d) {
		return (((16 - fx) * a + fx * b) * (16 - fy) + ((16 - fx) * c + fx * d) * fy) / 256.0;
	};
	double u0 = _u[cy][cx] / 65536.0;
	int col = (int)lerp(u0, unwrap(_u[cy][cx + 1] / 65536.0, u0, 2048.0),
						unwrap(_u[cy + 1][cx] / 65536.0, u0, 2048.0), unwrap(_u[cy + 1][cx + 1] / 65536.0, u0, 2048.0));
	int row = (int)lerp(_v[cy][cx] / 65536.0, _v[cy][cx + 1] / 65536.0, _v[cy + 1][cx] / 65536.0, _v[cy + 1][cx + 1] / 65536.0);
	col %= 2048;
	row %= p.height;
	return Common::Point((int16)(col / 2048.0 * p.hRange * 10.0),
						 (int16)(((row - p.height * 0.5) / p.height * p.vRange + p.vMid) * 10.0));
}

} // End of namespace Ring
