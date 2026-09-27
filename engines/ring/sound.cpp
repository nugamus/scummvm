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
#include "audio/decoders/wave.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "graphics/font.h"
#include "graphics/managed_surface.h"

#include "ring/codec.h"
#include "ring/detection.h"
#include "ring/resources.h"
#include "ring/ring.h"
#include "ring/sound.h"
#include "ring/world.h"

namespace Ring {

int SoundItem::pan3D(float alpha, int lr) const {
	return (int)(sinf(alpha * 0.0174532889f + offset) * amplitude) * (lr == 1 ? 1 : -1);
}

Sounds::Sounds(RingEngine *vm) : _vm(vm) {
}

Sounds::~Sounds() {
	for (auto &s : _sounds)
		g_system->getMixer()->stopHandle(s.handle);
}

Sounds::Sound *Sounds::find(int id) {
	for (auto &s : _sounds)
		if (s.id == id)
			return &s;
	return nullptr;
}

void Sounds::add(int id, int type, const Common::String &file) {
	if (find(id) || file.size() < 4)
		return;
	Sound s;
	s.id = id;
	s.type = type;
	s.file = file;
	_sounds.push_back(s);
}

bool Sounds::active(Sound &s) {
	return g_system->getMixer()->isSoundHandleActive(s.handle);
}

void Sounds::apply(Sound &s) {
	// DirectSound gets -10000 * (1 - own * type / 10000) hundredths of a dB and a pan of
	// 100 * pan (0x468100); the mixer takes the matching linear gain and a balance.
	float v = s.own / 100.0f * (s.typeVolume / 100.0f);
	int mb = -10000 - (int)(v * -10000.0f);
	float gain = powf(10.0f, mb / 2000.0f);
	Audio::Mixer *mixer = g_system->getMixer();
	mixer->setChannelVolume(s.handle, (byte)CLIP<int>((int)(gain * Audio::Mixer::kMaxChannelVolume), 0, 255));
	// ponytail: the balance is linear where DirectSound attenuates the other side in dB
	mixer->setChannelBalance(s.handle, (int8)CLIP(s.pan * 127 / 100, -127, 127));
}

// A whole .wav, .wac (mono DPCM) or .was (stereo bit stream) as a seekable stream (formats README).
static Audio::SeekableAudioStream *openSound(const Common::Path &path) {
	Common::File *f = new Common::File();
	if (!f->open(path)) {
		delete f;
		return nullptr;
	}
	Common::String name = path.baseName();
	name.toLowercase();
	if (name.hasSuffix(".wav"))
		return Audio::makeWAVStream(f, DisposeAfterUse::YES);
	uint32 size = f->size();
	Common::Array<byte> data(size + 8, 0);
	f->read(data.data(), size);
	delete f;
	if (size < 52)
		return nullptr;
	uint32 count = READ_LE_UINT32(&data[0]), wavSize = READ_LE_UINT32(&data[4]);
	uint16 channels = READ_LE_UINT16(&data[8 + 22]);
	uint32 rate = READ_LE_UINT32(&data[8 + 24]);
	if (wavSize < 44 || (channels != 1 && channels != 2))
		return nullptr;
	uint32 samples = (wavSize - 44) / 2;
	int16 *out = (int16 *)calloc(samples + 256, 2);
	uint32 pos = 52, n = 0;
	if (channels == 1) {
		DpcmState state;
		for (uint32 i = 0; i < count && pos + 2 <= size && n + 256 <= samples + 256; i++) {
			uint16 len = READ_LE_UINT16(&data[pos]);
			pos += 2;
			if (pos + len > size)
				break;
			decodeDpcmChunk(&data[pos], len, state, out + n);
			n += 256;
			pos += len;
		}
	} else {
		for (uint32 i = 0; i < count && pos + 8 <= size; i++) {
			uint32 packed = READ_LE_UINT32(&data[pos]), unpacked = READ_LE_UINT32(&data[pos + 4]);
			pos += 8;
			if (pos + packed > size)
				break;
			Common::Array<uint16> codes = decodeBits(&data[pos], packed, 12, 0, packed * 8, unpacked / 2, 4);
			for (uint32 k = 0; k < unpacked / 2 && k < codes.size() && n < samples; k++)
				out[n++] = (int16)codes[k];
			pos += packed;
		}
	}
	n = MIN(n, samples);
	byte flags = Audio::FLAG_16BITS | (channels == 2 ? Audio::FLAG_STEREO : 0);
#ifdef SCUMM_LITTLE_ENDIAN
	flags |= Audio::FLAG_LITTLE_ENDIAN;
#endif
	return Audio::makeRawStream((byte *)out, n * 2, rate, flags, DisposeAfterUse::YES);
}

void Sounds::startStream(Sound &s, bool loop) {
	// <prefix>DATA\<zone>\SOUND\[<language>\]<file>, the zone current at the play (0x468ae0).
	Common::Path path = Common::Path("DATA").appendComponent(zoneFolder(_vm->zone())).appendComponent("SOUND");
	if (s.type == kSoundDialogue)
		path = path.appendComponent(_vm->languageFolder());
	path = path.appendComponent(s.file);
	Audio::SeekableAudioStream *stream = openSound(path);
	if (!stream) {
		warning("Ring: cannot play sound %d (%s)", s.id, path.toString().c_str());
		return;
	}
	Audio::AudioStream *as = loop ? Audio::makeLoopingAudioStream(stream, 0) : (Audio::AudioStream *)stream;
	Audio::Mixer::SoundType type = s.type == kSoundDialogue ? Audio::Mixer::kSpeechSoundType
		: s.type <= kSoundAmbientMusic ? Audio::Mixer::kMusicSoundType : Audio::Mixer::kSFXSoundType;
	g_system->getMixer()->playStream(type, &s.handle, as);
	apply(s);
	debugC(1, kDebugSound, "sound %d (%s) %s, volume %d x %d, pan %d", s.id, s.file.c_str(), loop ? "looping" : "once", s.own, s.typeVolume, s.pan);
}

void Sounds::stopStream(Sound &s) {
	g_system->getMixer()->stopHandle(s.handle);
	s.started = false;
}

void Sounds::event(Sound &s, int reason) {
	debugC(1, kDebugSound, "sound event %d type %d reason %x", s.id, s.type, reason);
	_vm->soundEvent(s.id, s.type, reason);
}

void Sounds::play(int id, bool loop) {
	while (_vm->escapePressed() && !_vm->shouldQuit())
		_vm->pollEvents(10);
	Sound *s = find(id);
	if (!s)
		return;
	if (s->type == kSoundDialogue) {
		stopType(kSoundDialogue, kSoundRestarted);
	} else if (active(*s)) {
		stopStream(*s);
		event(*s, kSoundRestarted);
	}
	if (s->type == kSoundDialogue) {
		Dialogue d;
		if (!readDialogue(d, *s)) {
			event(*s, kSoundRestarted);
			return;
		}
		_dialogues.push_back(d);
	}
	startStream(*s, loop);
	s->started = true;
	if (s->type == kSoundDialogue)
		_dialogues.back().clock = g_system->getMillis();
}

void Sounds::stop(int id, int reason) {
	Sound *s = find(id);
	if (!s)
		return;
	if (s->type == kSoundDialogue) {
		if (active(*s))
			stopStream(*s);
		if (removeDialogue(id))
			event(*s, reason);
		return;
	}
	if (!active(*s))
		return;
	stopStream(*s);
	event(*s, reason);
}

void Sounds::stopType(int type, int reason) {
	// Only the first dialogue is removed; later type-5 sounds are left alone (0x469150).
	bool first = true;
	for (uint i = 0; i < _sounds.size(); i++) {
		Sound &s = _sounds[i];
		if (s.type != type && type != 0)
			continue;
		if (s.type == kSoundDialogue) {
			if (!first)
				continue;
			if (active(s))
				stopStream(s);
			if (removeDialogue(s.id)) {
				event(s, reason);
				first = false;
			}
		} else if (active(s)) {
			stopStream(s);
			event(s, reason);
		}
	}
}

void Sounds::stopAll(int reason) {
	stopType(0, reason);
}

void Sounds::setVolume(int id, int volume) {
	if (Sound *s = find(id)) {
		s->own = CLIP(volume, 0, 100);
		apply(*s);
	}
}

void Sounds::setPan(int id, int pan) {
	if (Sound *s = find(id)) {
		s->pan = CLIP(pan, -100, 100);
		apply(*s);
	}
}

void Sounds::setTypeVolumes(int volume, int dialogue) {
	for (auto &s : _sounds) {
		s.typeVolume = CLIP(s.type == kSoundDialogue ? dialogue : volume, 0, 100);
		apply(s);
	}
}

bool Sounds::playing(int id) {
	Sound *s = find(id);
	if (!s)
		return false;
	if (s->type == kSoundDialogue) {
		for (auto &d : _dialogues)
			if (d.id == id)
				return true;
		return false;
	}
	return active(*s);
}

bool Sounds::typePlaying(int type) {
	if (type == kSoundDialogue)
		return !_dialogues.empty();
	for (auto &s : _sounds)
		if (s.type == type && active(s))
			return true;
	return false;
}

void Sounds::checkEnds() {
	for (uint i = 0; i < _sounds.size(); i++) {
		Sound &s = _sounds[i];
		if (!s.started || active(s))
			continue;
		stopStream(s);
		if (s.type != kSoundDialogue)
			event(s, kSoundEnded);
	}
}

// .dia: the first 0x1000 bytes; each complete line is spaces, a time in ms, an optional
// ",m:s.f" stamp, spaces and the text, '#' splitting it (formats README, "Dialog text").
static bool parseLine(const char *p, uint32 &time, Common::String &first, Common::String &second, bool greek) {
	while (*p && Common::isSpace(*p))
		p++;
	if (!Common::isDigit(*p))
		return false;
	time = 0;
	while (Common::isDigit(*p))
		time = time * 10 + (*p++ - '0');
	if (*p == ',') {
		p++;
		while (Common::isDigit(*p))
			p++;
		if (*p != ':')
			return false;
		p++;
		while (Common::isDigit(*p))
			p++;
		if (*p != '.')
			return false;
		p++;
		while (Common::isDigit(*p))
			p++;
	}
	while (*p && Common::isSpace(*p))
		p++;
	Common::String text(p);
	if (greek)
		for (uint i = 0; i < text.size(); i++)
			if ((byte)text[i] == 0xa0)
				text.setChar(' ', i);
	size_t hash = text.findFirstOf('#');
	first = hash == Common::String::npos ? text : text.substr(0, hash);
	second = hash == Common::String::npos ? Common::String() : text.substr(hash + 1);
	return true;
}

bool Sounds::readDialogue(Dialogue &d, const Sound &s) {
	d.id = s.id;
	Common::String stem = s.file.substr(0, s.file.size() - 3);
	Common::Path dir = Common::Path("DATA").appendComponent(zoneFolder(_vm->zone())).appendComponent("DIA").appendComponent(_vm->languageFolder());
	Common::File f;
	if (!f.open(dir.appendComponent(stem + "dia"))) {
		warning("Ring: no dialogue text for sound %d", s.id);
		return false;
	}
	char buf[0x1001];
	uint32 len = f.read(buf, 0x1000);
	buf[len] = 0;
	f.close();
	uint32 lineStart = 0;
	for (uint32 i = 0; i < len; i++) {
		if (buf[i] != '\n')
			continue;
		buf[i] = 0;
		if (i > 0)
			buf[i - 1] = 0;
		Line line;
		if (!parseLine(buf + lineStart, line.time, line.first, line.second, _vm->languageFolder() == "GRE")) {
			warning("Ring: bad dialogue line for sound %d", s.id);
			return false;
		}
		d.lines.push_back(line);
		lineStart = i + 1;
	}

	// .dan: N, N × (level, object, presentation), then (start, end, level) to the end.
	if (!f.open(dir.appendComponent(stem + "dan")))
		return true;
	Common::String text = f.readString(0, f.size());
	const char *p = text.c_str();
	auto next = [&](int &v) {
		char *end;
		long n = strtol(p, &end, 10);
		if (end == p)
			return false;
		p = end;
		v = (int)n;
		return true;
	};
	int count;
	if (!next(count))
		return true;
	for (int i = 0; i < count; i++) {
		Dialogue::Map m;
		if (!next(m.level) || !next(m.object) || !next(m.presentation))
			return true;
		d.maps.push_back(m);
	}
	int a, b, c;
	while (next(a) && next(b) && next(c))
		d.lips.push_back(Dialogue::Lip{ (uint32)a, (uint32)b, c });
	d.hasLips = true;
	return true;
}

bool Sounds::removeDialogue(int id) {
	for (uint i = 0; i < _dialogues.size(); i++) {
		if (_dialogues[i].id != id)
			continue;
		if (_dialogues[i].hasLips)
			for (auto &m : _dialogues[i].maps)
				_vm->world().showPresentation(m.object, m.presentation, false);
		_dialogues.remove_at(i);
		return true;
	}
	return false;
}

void Sounds::dialogueFrame(Graphics::ManagedSurface &dst, const Graphics::Font *font, bool subtitles) {
	if (_dialogues.empty())
		return;
	Dialogue &d = _dialogues[0];
	uint32 t = g_system->getMillis() - d.clock;
	int index = -1;
	for (int i = 0; i + 1 < (int)d.lines.size(); i++) {
		if (d.lines[i].time <= t && t <= d.lines[i + 1].time) {
			index = i;
			break;
		}
	}
	if (index < 0) {
		int id = d.id;
		removeDialogue(id);
		if (Sound *s = find(id))
			event(*s, kSoundEnded);
		return;
	}
	if (subtitles && font) {
		// Font 1, colour (200, 200, 30) on black, centred above y 461 (0x427c70).
		const Line &l = d.lines[index];
		int h = font->getFontHeight();
		uint32 fg = dst.format.RGBToColor(200, 200, 30), bg = dst.format.RGBToColor(0, 0, 0);
		auto draw = [&](const Common::String &s, int y) {
			int w = font->getStringWidth(s);
			int x = 320 - w / 2;
			dst.fillRect(Common::Rect(x, y, x + w, y + h).findIntersectingRect(Common::Rect(0, 0, dst.w, dst.h)), bg);
			font->drawString(&dst, s, x, y, w, fg);
		};
		if (l.second.empty()) {
			draw(l.first, 461 - h);
		} else {
			draw(l.first, 461 - h - 3 - h);
			draw(l.second, 461 - h);
		}
	}
	if (d.hasLips) {
		int level = 0;
		for (auto &lip : d.lips) {
			if (lip.start <= t && t <= lip.end) {
				level = lip.level;
				break;
			}
		}
		if (level != d.level) {
			for (auto &m : d.maps)
				_vm->world().showPresentation(m.object, m.presentation, false);
			if (level) {
				for (auto &m : d.maps) {
					if (m.level == level) {
						_vm->world().showPresentation(m.object, m.presentation, true);
						break;
					}
				}
			}
			d.level = level;
		}
	}
}

void Sounds::startItem(const SoundItem &item) {
	Sound *s = find(item.sound);
	if (!s)
		return;
	if (active(*s))
		stopStream(*s);
	s->own = CLIP(item.volume, 0, 100);
	s->pan = CLIP(item.pan, -100, 100);
	startStream(*s, true);
	s->started = true;
}

void Sounds::stopItem(const SoundItem &item) {
	Sound *s = find(item.sound);
	if (s && active(*s))
		stopStream(*s);
}

static SoundItem *findItem(const SoundItems *list, int sound) {
	if (list)
		for (auto &i : *list)
			if (i->sound == sound)
				return i.get();
	return nullptr;
}

void Sounds::computeTransition() {
	// 0x41aa00: stop now, fade out (to 40), fade to the new values, start.
	_stopNow.clear();
	_fadeOut.clear();
	_fade.clear();
	_start.clear();
	for (auto &op : *_old) {
		SoundItem *o = op.get();
		SoundItem *n = findItem(_new, o->sound);
		int mode = 0; // 0 leaving, 1 staying, 2 nothing
		if (n) {
			mode = 1;
			if (!n->active)
				mode = o->active ? 0 : 2;
		}
		auto fadeOut = [&]() {
			o->vol = o->volume;
			o->volTarget = 40;
			o->panNow = o->panTarget = o->pan;
			_fadeOut.push_back(o);
		};
		if (mode == 0) {
			if (o->leaveMode == 1)
				fadeOut();
			else if (o->leaveMode == 2)
				_stopNow.push_back(o);
		} else if (mode == 1) {
			if (o->sameMode == 1) {
				o->vol = o->volume;
				o->volTarget = n->volume;
				o->panNow = o->pan;
				o->panTarget = n->pan;
				_fade.push_back(o);
			} else if (o->sameMode == 2) {
				fadeOut();
				_start.push_back(n);
			} else if (o->sameMode == 3) {
				_stopNow.push_back(o);
				_start.push_back(n);
			}
		}
	}
	for (auto &np : *_new) {
		SoundItem *o = findItem(_old, np->sound);
		if (!o || (!o->active && np->active))
			_start.push_back(np.get());
	}
}

bool Sounds::beginTransition(int n) {
	for (SoundItem *i : _fadeOut) {
		int steps = n < i->fade ? n - 1 : i->fade;
		if (steps <= 0)
			return false;
		i->steps = steps;
		i->volInc = -(i->vol - i->volTarget) / steps;
		i->panInc = -(i->panNow - i->panTarget) / steps;
	}
	for (SoundItem *i : _fade) {
		if (n - 1 <= 0)
			return false;
		i->steps = n - 1;
		i->volInc = -(i->vol - i->volTarget) / (n - 1);
		i->panInc = -(i->panNow - i->panTarget) / (n - 1);
	}
	return true;
}

void Sounds::stepTransition(int k) {
	for (uint j = 0; j < _fadeOut.size();) {
		SoundItem *i = _fadeOut[j];
		if (i->steps <= k) {
			stopItem(*i);
			_fadeOut.remove_at(j);
			continue;
		}
		i->panNow += i->panInc;
		setPan(i->sound, (int)i->panNow);
		i->vol += i->volInc;
		setVolume(i->sound, (int)i->vol);
		j++;
	}
	for (SoundItem *i : _fade) {
		if (i->steps <= k) {
			setVolume(i->sound, (int)i->volTarget);
			setPan(i->sound, (int)i->panTarget);
			continue;
		}
		i->panNow += i->panInc;
		setPan(i->sound, (int)i->panNow);
		i->vol += i->volInc;
		setVolume(i->sound, (int)i->vol);
	}
}

void Sounds::prepareTransition(const SoundItems *items) {
	_new = items;
	_pending = true;
	if (!_old || !_new) {
		_pending = false;
		if (_old)
			for (auto &i : *_old)
				stopItem(*i);
		return;
	}
	computeTransition();
}

void Sounds::finishTransition() {
	if (!_pending)
		return;
	for (SoundItem *i : _stopNow)
		stopItem(*i);
	if (!beginTransition(2)) {
		for (auto &i : *_old)
			stopItem(*i);
		_pending = false;
		return;
	}
	stepTransition(3);
}

void Sounds::beginRide(int frames) {
	if (!_pending)
		return;
	for (SoundItem *i : _stopNow)
		stopItem(*i);
	if (!beginTransition(frames)) {
		for (auto &i : *_old)
			stopItem(*i);
		_pending = false;
	}
}

void Sounds::enterPlace(const SoundItems *items, bool start, bool stop, bool rotation, float alpha, int lr) {
	auto startStep = [&]() {
		for (SoundItem *i : _start) {
			Sound *s = find(i->sound);
			if (i->active && s && !active(*s))
				startItem(*i);
		}
	};
	if (_pending) {
		startStep();
	} else {
		_new = items;
		bool done = false;
		if (_old && _new) {
			computeTransition();
			for (SoundItem *i : _stopNow)
				stopItem(*i);
			if (beginTransition(2)) {
				stepTransition(3);
				startStep();
				done = true;
			}
		}
		if (!done) {
			if (stop && _old)
				for (auto &i : *_old)
					stopItem(*i);
			if (rotation && items)
				for (auto &i : *items)
					if (Sound *s = find(i->sound))
						if (s->type == kSoundAmbientEffect)
							i->pan = i->pan3D(alpha, lr);
			if (start && items) {
				for (auto &i : *items) {
					Sound *s = find(i->sound);
					if (!i->active)
						stopItem(*i);
					else if (s && !active(*s))
						startItem(*i);
				}
			}
		}
	}
	_stopNow.clear();
	_fadeOut.clear();
	_fade.clear();
	_start.clear();
	_pending = false;
	_new = nullptr;
	_old = items;
}

} // End of namespace Ring
