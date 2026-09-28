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

#include "common/file.h"

#include "audio/audiostream.h"
#include "audio/decoders/apc.h"
#include "audio/decoders/wave.h"

#include "peintre/sound.h"

namespace Peintre {

Sound::~Sound() {
	stopAllStatic();
	stopStream();
}

void Sound::playStatic(const Common::String &name, bool loop) {
	stopStatic(name);
	Common::File *f = new Common::File();
	if (!f->open(Common::Path("SOUND/" + name + ".WAV"))) {
		delete f;
		warning("Sound: cannot open %s.WAV", name.c_str());
		return;
	}
	Audio::SeekableAudioStream *wav = Audio::makeWAVStream(f, DisposeAfterUse::YES);
	if (!wav)
		return;
	Audio::AudioStream *s = loop ? (Audio::AudioStream *)Audio::makeLoopingAudioStream(wav, 0) : wav;
	_mixer->playStream(Audio::Mixer::kSFXSoundType, &_statics[name], s);
}

void Sound::stopStatic(const Common::String &name) {
	if (_statics.contains(name)) {
		_mixer->stopHandle(_statics[name]);
		_statics.erase(name);
	}
}

bool Sound::isStaticPlaying(const Common::String &name) const {
	return _statics.contains(name) && _mixer->isSoundHandleActive(_statics.getVal(name));
}

void Sound::stopAllStatic() {
	for (auto &it : _statics)
		_mixer->stopHandle(it._value);
	_statics.clear();
}

bool Sound::playStream(const Common::String &name) {
	stopStream();
	Common::File *f = new Common::File();
	Audio::AudioStream *s = nullptr;
	if (f->open(Common::Path("SOUND/" + name + ".APC"))) {
		// The 32-byte header, then the ADPCM data as one packet.
		Audio::PacketizedAudioStream *apc = Audio::makeAPCStream(*f);
		if (apc) {
			apc->queuePacket(f->readStream(f->size() - f->pos()));
			apc->finish();
		}
		delete f;
		s = apc;
	} else if (f->open(Common::Path("SOUND/" + name + ".WAV"))) {
		s = Audio::makeWAVStream(f, DisposeAfterUse::YES);
	} else {
		delete f;
		warning("Sound: cannot open stream %s", name.c_str());
		return false;
	}
	if (!s)
		return false;
	_mixer->playStream(Audio::Mixer::kSpeechSoundType, &_stream, s);
	return true;
}

void Sound::stopStream() {
	_mixer->stopHandle(_stream);
}

bool Sound::isStreamPlaying() const {
	return _mixer->isSoundHandleActive(_stream);
}

void Sound::pauseStream(bool pause) {
	if (_mixer->isSoundHandleActive(_stream))
		_mixer->pauseHandle(_stream, pause);
}

void Sound::setVolume(int32 attenuation) {
	// percent = attenuation / 50 + 100 (ui.md "Option menu").
	const int percent = CLIP<int>(attenuation / 50 + 100, 0, 100);
	const int v = percent * Audio::Mixer::kMaxMixerVolume / 100;
	_mixer->setVolumeForSoundType(Audio::Mixer::kSFXSoundType, v);
	_mixer->setVolumeForSoundType(Audio::Mixer::kSpeechSoundType, v);
}

} // End of namespace Peintre
