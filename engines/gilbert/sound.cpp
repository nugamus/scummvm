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

#include "common/debug.h"
#include "common/file.h"
#include "common/memstream.h"
#include "common/util.h"

#include "gilbert/detection.h"
#include "gilbert/sound.h"

namespace Gilbert {

// Volume level 1..6 -> DirectSound attenuation in hundredths of a dB (boot.md "Settings").
static byte levelToVolume(int level) {
	static const int attenuation[] = { -5000, -4000, -3000, -2000, -1000, 0 };
	const int cb = attenuation[CLIP(level, 1, 6) - 1];
	return (byte)(Audio::Mixer::kMaxChannelVolume * pow(10.0, cb / 2000.0) + 0.5);
}

Sound::Sound(Audio::Mixer *mixer) : _mixer(mixer) {
	for (int i = 0; i < kStreamCount; i++)
		_pending[i] = nullptr;
}

Sound::~Sound() {
	stopAll();
}

void Sound::loadList(int list, const char *file) {
	loadWaves(Common::Path("Sounds/misc/").appendComponent(file), _lists[list]);
	debugC(1, kDebugSound, "Sound: list %d = %s, %d waves", list, file, _lists[list].size());
}

void Sound::playWave(int list, int index, bool looped) {
	if (list < 1 || list > 2 || index < 0 || index >= (int)_lists[list].size())
		return;
	const Wave &w = _lists[list][index];
	Common::SeekableReadStream *mem = new Common::MemoryReadStream(w.data.data(), w.data.size());
	Audio::SeekableAudioStream *s = Audio::makeWAVStream(mem, DisposeAfterUse::YES);
	if (!s)
		return;
	_mixer->playStream(Audio::Mixer::kSFXSoundType, &_wave,
	                   looped ? (Audio::AudioStream *)Audio::makeLoopingAudioStream(s, 0) : s,
	                   -1, _soundVolume);
}

bool Sound::isWavePlaying() const {
	return _mixer->isSoundHandleActive(_wave);
}

void Sound::openStream(Stream stream, const Common::String &name, bool looped, bool start) {
	_mixer->stopHandle(_streams[stream]);
	delete _pending[stream];
	_pending[stream] = nullptr;
	Common::File *f = new Common::File();
	if (!f->open(Common::Path("Sounds/MUSIC/").appendComponent(name + ".wav"))) {
		warning("Gilbert: no music %s", name.c_str());
		delete f;
		return;
	}
	Audio::SeekableAudioStream *s = Audio::makeWAVStream(f, DisposeAfterUse::YES);
	if (!s)
		return;
	_pending[stream] = looped ? (Audio::AudioStream *)Audio::makeLoopingAudioStream(s, 0) : s;
	if (start)
		startStream(stream);
}

void Sound::startStream(Stream stream) {
	if (!_pending[stream])
		return;
	_mixer->playStream(Audio::Mixer::kMusicSoundType, &_streams[stream], _pending[stream], -1, _musicVolume);
	_pending[stream] = nullptr;
}

void Sound::stopAll() {
	_mixer->stopAll();
	for (int i = 0; i < kStreamCount; i++) {
		delete _pending[i];
		_pending[i] = nullptr;
	}
}

void Sound::setVolumes(int music, int sound) {
	_musicVolume = levelToVolume(music);
	_soundVolume = levelToVolume(sound);
	for (int i = 0; i < kStreamCount; i++)
		_mixer->setChannelVolume(_streams[i], _musicVolume);
	_mixer->setChannelVolume(_wave, _soundVolume);
}

} // End of namespace Gilbert
