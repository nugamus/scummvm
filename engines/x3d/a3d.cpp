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

#include "x3d/a3d.h"
#include "x3d/o3d.h"

namespace X3D {

static Common::String readName(Common::SeekableReadStream &s) {
	char buf[33];
	s.read(buf, 32);
	buf[32] = 0;
	return Common::String(buf);
}

static void readTrack(Common::SeekableReadStream &s, A3DTrack &t, uint valueFloats) {
	const uint32 count = s.readUint32LE();
	s.readUint32LE(); // flags, 0 in the corpus
	t.frames.resize(count);
	for (uint32 &f : t.frames)
		f = s.readUint32LE();
	t.params.resize(count * 5);
	for (float &p : t.params)
		p = s.readFloatLE();
	t.values.resize(count * valueFloats);
	for (float &v : t.values)
		v = s.readFloatLE();
}

bool A3DFile::load(Common::SeekableReadStream &s) {
	const Common::String signature = readName(s);
	if (signature != "(c) 1998 4X Tech. 0.95 (A)") {
		warning("A3D: unsupported signature '%s'", signature.c_str());
		return false;
	}

	animations.resize(s.readUint32LE());
	for (uint i = 0; i < animations.size(); i++) {
		A3DAnimation &a = animations[i];
		a.name = readName(s);
		if (s.readUint32LE()) {
			const Common::String parent = readName(s);
			for (int j = i - 1; j >= 0 && a.parent < 0; j--)
				if (animations[j].name == parent)
					a.parent = j;
		}
		s.skip(3 * 4); // pivot, the object's own in Monet
		s.readUint32LE(); // number of frames
		a.firstFrame = s.readUint32LE();
		a.lastFrame = s.readUint32LE();
		readTrack(s, a.translation, 3);
		readTrack(s, a.scale, 3);
		readTrack(s, a.rotation, 4);

		// Hide: frames and one u32 per key
		uint32 count = s.readUint32LE();
		s.readUint32LE();
		s.skip(count * 4 * 2);

		// Morph: frames and key parameters, then per key a vertex set and its bounds
		count = s.readUint32LE();
		s.readUint32LE();
		s.skip(count * 4 + count * 5 * 4);
		if (count) {
			const uint32 inner = s.readUint32LE();
			s.skip(count * (inner * 6 * 4 + 10 * 4));
		}
	}
	return !s.err() && s.pos() == s.size();
}

// Keys i and j around frame f, and the parameter between them (animation.md, Key lookup)
static bool lookup(const A3DTrack &t, float f, uint &i, uint &j, float &s) {
	const uint n = t.frames.size();
	if (!n)
		return false;
	if (f < t.frames[0]) {
		i = j = 0;
	} else if (f >= t.frames[n - 1]) {
		i = j = n - 1;
	} else {
		i = 0;
		while (i + 1 < n && t.frames[i + 1] < f)
			i++;
		j = i + 1;
	}
	s = t.frames[j] == t.frames[i] ? 0 : (f - t.frames[i]) / (float)(t.frames[j] - t.frames[i]);
	return true;
}

// TCB tangents of key i (animation.md, Translation spline)
static void tangents(const A3DTrack &t, uint i, float in[3], float out[3]) {
	const uint n = t.frames.size();
	const uint p = i > 0 ? i - 1 : 0, q = MIN(i + 1, n - 1);
	if (t.frames[p] == t.frames[q]) {
		for (int k = 0; k < 3; k++)
			in[k] = out[k] = 0;
		return;
	}
	const float *key = &t.params[i * 5];
	const float tension = key[0], continuity = key[1], bias = key[2];
	const float h = (t.frames[q] - (float)t.frames[p]) / 2;
	const float a = (t.frames[i] - (float)t.frames[p]) / h, b = (t.frames[q] - (float)t.frames[i]) / h;
	const float c = fabsf(continuity);
	const float a2 = a + c - c * a, b2 = b + c - c * b, half = (1 - tension) / 2;
	for (int k = 0; k < 3; k++) {
		const float d0 = t.values[i * 3 + k] - t.values[p * 3 + k];
		const float d1 = t.values[q * 3 + k] - t.values[i * 3 + k];
		in[k] = a2 * half * ((1 - continuity) * (1 + bias) * d0 + (1 + continuity) * (1 - bias) * d1);
		out[k] = b2 * half * ((1 + continuity) * (1 + bias) * d0 + (1 - continuity) * (1 - bias) * d1);
	}
}

void A3DFile::sample(uint index, float f, O3DObject &o) const {
	const A3DAnimation &a = animations[index];
	uint i, j;
	float s;

	if (lookup(a.translation, f, i, j, s)) {
		const A3DTrack &t = a.translation;
		// Ease: key i's ease-out, key j's ease-in
		float eo = t.params[i * 5 + 4], ei = t.params[j * 5 + 3];
		if (eo + ei != 0) {
			if (eo + ei > 1) {
				const float sum = eo + ei;
				eo /= sum;
				ei /= sum;
			}
			const float g = 1 / (2 - eo - ei);
			if (s < eo)
				s = g / eo * s * s;
			else if (s < 1 - ei)
				s = (2 * s - eo) * g;
			else
				s = ei > 0 ? 1 - g / ei * (1 - s) * (1 - s) : 1;
		}
		float inI[3], outI[3], inJ[3], outJ[3];
		tangents(t, i, inI, outI);
		tangents(t, j, inJ, outJ);
		const float s2 = s * s, s3 = s2 * s;
		for (int k = 0; k < 3; k++)
			o.localPosition[k] = (2 * s3 - 3 * s2 + 1) * t.values[i * 3 + k] + (s3 - 2 * s2 + s) * outI[k] +
			                     (-2 * s3 + 3 * s2) * t.values[j * 3 + k] + (s3 - s2) * inJ[k];
	}

	if (lookup(a.scale, f, i, j, s))
		for (int k = 0; k < 3; k++)
			o.localScale[k] = a.scale.values[i * 3 + k] + s * (a.scale.values[j * 3 + k] - a.scale.values[i * 3 + k]);

	if (lookup(a.rotation, f, i, j, s)) {
		// Slerp without the shortest-path sign flip; (w, x, y, z)
		const float *qi = &a.rotation.values[i * 4], *qj = &a.rotation.values[j * 4];
		const float d = qi[0] * qj[0] + qi[1] * qj[1] + qi[2] * qj[2] + qi[3] * qj[3];
		float q[4];
		if (1 + d <= 1e-5f) {
			const float r[4] = { qi[3], -qi[2], qi[1], -qi[0] };
			const float wi = sinf((1 - s) * M_PI / 2), wr = sinf(s * M_PI / 2);
			for (int k = 0; k < 4; k++)
				q[k] = wi * qi[k] + wr * r[k];
		} else if (1 - d <= 1e-5f) {
			for (int k = 0; k < 4; k++)
				q[k] = (1 - s) * qi[k] + s * qj[k];
		} else {
			const float theta = acosf(d), sine = sinf(theta);
			const float wi = sinf((1 - s) * theta) / sine, wj = sinf(s * theta) / sine;
			for (int k = 0; k < 4; k++)
				q[k] = wi * qi[k] + wj * qj[k];
		}

		const float w = q[0], x = q[1], y = q[2], z = q[3];
		const float n = w * w + x * x + y * y + z * z;
		const float k = n > 0 ? 2 / n : 0;
		const float m[16] = {
			1 - k * (y * y + z * z), k * (x * y - w * z), k * (x * z + w * y), 0,
			k * (x * y + w * z), 1 - k * (x * x + z * z), k * (y * z - w * x), 0,
			k * (x * z - w * y), k * (y * z + w * x), 1 - k * (x * x + y * y), 0,
			0, 0, 0, 1
		};
		memcpy(o.matrix, m, sizeof(m));
	}
}

} // End of namespace X3D
