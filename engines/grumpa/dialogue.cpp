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
#include "audio/decoders/wave.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/file.h"

#include "grumpa/character.h"
#include "grumpa/dialogue.h"

namespace Grumpa {

// The original opens <data>\Sounds\<name>; the cabinet splits that folder into the voices of
// a language and the common sounds (dialogue.md, Q-0400). Names with non-ASCII letters were
// extracted with '_' in their place.
Common::SeekableReadStream *openSound(const Common::String &name) {
	static const char *const dirs[] = { "Sounds_Swedish/", "Sounds_Danish/", "Sounds_Norwegian/",
										"Sounds_Finnish/", "Sounds_/", "Sounds/" };
	Common::String ascii = name;
	for (uint i = 0; i < ascii.size(); i++)
		if ((byte)ascii[i] >= 0x80)
			ascii.setChar('_', i);
	for (uint i = 0; i < ARRAYSIZE(dirs); i++) {
		Common::File *f = new Common::File();
		if (f->open(Common::Path(Common::String(dirs[i]) + name)) ||
			f->open(Common::Path(Common::String(dirs[i]) + ascii)))
			return f;
		delete f;
	}
	return nullptr;
}

bool readSceneSound(const byte *d, uint32 n, uint32 &o, uint32 id, SceneSound &s) {
	uint32 p = o;
	bool ok = true;
	auto u32 = [&]() -> uint32 {
		if (!ok || n - p < 4 || p > n) { ok = false; return 0; }
		uint32 v = READ_LE_UINT32(d + p);
		p += 4;
		return v;
	};
	auto count = [&]() -> uint32 { uint32 c = u32(); if (c > 0x100000) ok = false; return ok ? c : 0; };
	s.id = id;
	s.active = u32() != 0;             // header: active, visible, n x u32 (E-0400)
	s.visible = u32() != 0;
	for (uint32 k = count(); k > 0 && ok; k--)
		u32();
	uint32 f[10];                      // E-0405
	for (int i = 0; i < 10; i++)
		f[i] = u32();
	s.volSet = f[0] == 1;
	s.volume = (int32)f[1];
	s.panSet = f[2] == 1;
	s.pan = (int32)f[3];
	s.loop = f[6] == 1;
	s.onEntry = f[7] == 1 || f[8] == 1;  // "playing" or "play on entry": both start on 0x17
	s.speaker = (int)f[9];
	p += 56;                           // the timer sub-object
	uint32 len = count();
	if (ok && p <= n && len <= n - p) {
		s.name = Common::String((const char *)d + p, len);
		uint z = s.name.findFirstOf('\0');
		if (z != Common::String::npos)
			s.name = Common::String(s.name.c_str(), z);
		p += len;
	} else {
		ok = false;
	}
	for (uint32 k = count(); k > 0 && ok; k--) {   // CC: when, target, opcode, arg1, arg2 + conditions
		SceneCommand c;
		c.when = (int32)u32();
		c.targetId = (int32)u32();
		c.opcode = (int32)u32();
		c.arg1 = (int32)u32();
		c.arg2 = (int32)u32();
		for (uint32 j = count(); j > 0 && ok; j--) {
			SceneCond e;               // EC: 5 u32 (E-0101)
			e.id = (int32)u32();
			e.slot = (int32)u32();
			e.value = (int32)u32();
			e.mode = (int32)u32();
			e.link = (int32)u32();
			c.conds.push_back(e);
		}
		s.onEnd.push_back(c);
	}
	if (ok)
		o = p;
	return ok;
}

void Voices::enterScene(Common::Array<SceneSound> *sounds) {
	stopAll();
	_sounds = sounds;
	_run.clear();
	if (_sounds)
		_run.resize(_sounds->size());
}

void Voices::stopAll() {
	for (uint i = 0; i < _run.size(); i++) {
		_mixer->stopHandle(_run[i].handle);
		if (_run[i].playing && _sounds && i < _sounds->size())
			setTalking((*_sounds)[i].speaker, false);
		_run[i].playing = _run[i].pending = false;
	}
}

bool Voices::playing(int id) const {
	if (!_sounds)
		return false;
	for (uint i = 0; i < _sounds->size(); i++)
		if ((int)(*_sounds)[i].id == id)
			return _run[i].playing;
	return false;
}

bool Voices::command(int id, int op, int arg1, Common::Array<SceneCommand> &out) {
	if (!_sounds)
		return false;
	bool found = false;
	for (uint i = 0; i < _sounds->size(); i++) {
		if (id != -1 && (int)(*_sounds)[i].id != id)
			continue;
		apply(i, op, out);
		found = true;
	}
	return found;
}

// CFXSound::DoCommand (E-0405). A play waits for the next update, as the original's first
// update after loading does.
void Voices::apply(uint idx, int op, Common::Array<SceneCommand> &out) {
	SceneSound &s = (*_sounds)[idx];
	Run &r = _run[idx];
	if (r.latched)
		return;
	switch (op) {
	case 0:
	case 500:
		s.active = true;
		r.pending = true;
		break;
	case 1:
		stop(idx, true, out);
		break;
	case 501:
		s.active = false;
		stop(idx, true, out);
		break;
	case 0xb:
		s.active = true;
		break;
	case 0xc:
		s.active = false;
		break;
	case 0xd:
		stop(idx, false, out);
		s.active = s.visible = false;
		r.latched = true;
		break;
	case 0x17:
		if (s.onEntry)
			r.pending = true;
		break;
	default:
		break;
	}
}

void Voices::update(Common::Array<SceneCommand> &out) {
	if (!_sounds)
		return;
	for (uint i = 0; i < _sounds->size(); i++) {
		Run &r = _run[i];
		if (!(*_sounds)[i].active)
			continue;
		if (r.pending) {
			r.pending = false;
			play(i);
		} else if (r.playing && !_mixer->isSoundHandleActive(r.handle)) {
			stop(i, true, out);      // a line that ended stops itself and runs its list
		}
	}
}

void Voices::play(uint idx) {
	SceneSound &s = (*_sounds)[idx];
	Run &r = _run[idx];
	_mixer->stopHandle(r.handle);
	Common::SeekableReadStream *f = openSound(s.name);
	Audio::RewindableAudioStream *wav = f ? Audio::makeWAVStream(f, DisposeAfterUse::YES) : nullptr;
	if (!wav) {
		debug(1, "Grumpa: sound %u %s not found", s.id, s.name.c_str());
		return;
	}
	// DirectSound volume and pan are in hundredths of a dB of attenuation.
	byte vol = s.volSet ? (byte)CLIP<double>(255.0 * pow(10.0, s.volume / 2000.0), 0, 255) : 255;
	int8 bal = 0;
	if (s.panSet && s.pan != 0) {
		double att = pow(10.0, -ABS(s.pan) / 2000.0);
		bal = (int8)((s.pan < 0 ? -1 : 1) * (1.0 - att) * 127);
	}
	Audio::Mixer::SoundType type = s.speaker ? Audio::Mixer::kSpeechSoundType : Audio::Mixer::kSFXSoundType;
	Audio::AudioStream *stream = s.loop ? (Audio::AudioStream *)Audio::makeLoopingAudioStream(wav, 0) : wav;
	_mixer->playStream(type, &r.handle, stream, -1, vol, bal);
	r.playing = true;
	if (s.speaker > 0)
		_chars->flushSpeech(s.speaker);
	setTalking(s.speaker, true);
	debug(1, "Grumpa: sound %u %s%s (speaker %d)", s.id, s.name.c_str(), s.loop ? " looping" : "", s.speaker);
}

void Voices::stop(uint idx, bool runList, Common::Array<SceneCommand> &out) {
	SceneSound &s = (*_sounds)[idx];
	Run &r = _run[idx];
	_mixer->stopHandle(r.handle);
	r.pending = false;
	setTalking(s.speaker, false);
	r.playing = false;
	if (runList) {
		for (uint i = 0; i < s.onEnd.size(); i++)
			out.push_back(s.onEnd[i]);
		if (!s.onEnd.empty())
			debug(1, "Grumpa: sound %u ended, %u commands", s.id, (uint)s.onEnd.size());
	}
}

// "Talking" is the speaker's state slot 5, read only by conditions: the companion's hints wait
// for it to be 0 (E-1620).
void Voices::setTalking(int speaker, bool on) {
	Character *c = speaker > 0 ? _chars->find(speaker) : nullptr;
	if (c && c->state.size() > 5)
		c->state[5] = on ? 1 : 0;
}

} // End of namespace Grumpa
