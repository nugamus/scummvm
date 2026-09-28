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
#include "common/textconsole.h"

#include "audio/audiostream.h"
#include "audio/decoders/wave.h"

#include "x3d/sound.h"

namespace X3D {

Sound::Sound(Audio::Mixer *mixer) : _mixer(mixer) {
	for (int &g : _groupVolume)
		g = 100;
	_emitters[kVoiceEmitter].group = kVoice;
	_emitters[kEffectsEmitter].group = kEffects;
	_emitters[kPhoneEmitter].group = 4;
	_emitters[kUnitEmitter1].group = 5;
	_emitters[kUnitEmitter2].group = 6;
}

Sound::~Sound() {
	stopAll();
}

void Sound::apply(Playing &p) {
	// attenuation (dB) = (v * G - 10000) / 100, as an amplitude for the mixer
	const float gain = powf(10.0f, (p.v * _groupVolume[p.group] - 10000) / 2000.0f);
	_mixer->setChannelVolume(p.handle, (byte)CLIP<float>(gain * Audio::Mixer::kMaxChannelVolume, 0, 255));
}

void Sound::prune() {
	for (uint i = 0; i < _playing.size();) {
		if (_mixer->isSoundHandleActive(_playing[i].handle))
			i++;
		else
			_playing.remove_at(i);
	}
}

bool Sound::play(const Common::Path &path, int group, int v, bool loop) {
	Common::File *f = new Common::File();
	if (!f->open(path)) {
		warning("Missing sound %s", path.toString().c_str());
		delete f;
		return false;
	}
	Audio::RewindableAudioStream *wav = Audio::makeWAVStream(f, DisposeAfterUse::YES);
	if (!wav)
		return false;
	Audio::AudioStream *stream = loop ? Audio::makeLoopingAudioStream(wav, 0) : wav;

	static const Audio::Mixer::SoundType types[] = {
		Audio::Mixer::kPlainSoundType, Audio::Mixer::kMusicSoundType, Audio::Mixer::kSpeechSoundType,
		Audio::Mixer::kSFXSoundType, Audio::Mixer::kSFXSoundType, Audio::Mixer::kSFXSoundType, Audio::Mixer::kSFXSoundType
	};
	prune();
	Playing p;
	p.group = CLIP(group, 1, 6);
	p.v = CLIP(v, 0, 100);
	_mixer->playStream(types[p.group], &p.handle, stream, -1, 0);
	apply(p);
	_playing.push_back(p);
	return true;
}

void Sound::stopGroup(int group) {
	for (Playing &p : _playing)
		if (p.group == group)
			_mixer->stopHandle(p.handle);
	prune();
}

void Sound::stopAll() {
	for (Playing &p : _playing)
		_mixer->stopHandle(p.handle);
	_playing.clear();
}

// "Playing" ends at the audible end; the original's early end for streamed sounds is
// not modelled
bool Sound::isGroupPlaying(int group) {
	prune();
	for (const Playing &p : _playing)
		if (p.group == group)
			return true;
	return false;
}

int Sound::groupVolume(int group) const {
	assert(validGroup(group));
	return _groupVolume[group];
}

void Sound::setGroupVolume(int group, int g) {
	assert(validGroup(group));
	_groupVolume[group] = CLIP(g, 0, 100);
	for (Playing &p : _playing)
		if (p.group == group)
			apply(p);
}

void Sound::setScale(float scale) {
	_emitters[kVoiceEmitter].range = 50 * scale;
	_emitters[kEffectsEmitter].range = 60 * scale;
	_emitters[kPhoneEmitter].range = 50 * scale;
}

void Sound::setEmitter(Emitter e, int group, float range) {
	_emitters[e].group = group;
	_emitters[e].range = range;
}

bool Sound::emit(Emitter e, const Common::Path &path, const Math::Vector3d &position, bool loop) {
	EmitterState &em = _emitters[e];
	const Common::String name = path.baseName();
	if (isGroupPlaying(em.group) && name.equalsIgnoreCase(em.lastName))
		return false;
	em.position = position;
	stopGroup(em.group);
	em.owns = play(path, em.group, 100, loop);
	em.lastName = name;
	if (em.owns)
		em.handle = _playing.back().handle;
	return true;
}

void Sound::stopEmitter(Emitter e) {
	if (_emitters[e].owns)
		_mixer->stopHandle(_emitters[e].handle);
	_emitters[e].owns = false;
	prune();
}

void Sound::detach(Emitter e) {
	_emitters[e].owns = false;
}

void Sound::updateVolumes(const Math::Vector3d &eye) {
	prune();
	for (EmitterState &em : _emitters) {
		if (!em.owns)
			continue;
		for (Playing &p : _playing) {
			if (!(p.handle == em.handle))
				continue;
			const float d = (em.position - eye).getMagnitude();
			p.v = d < em.range ? CLIP((int)(100 * (em.range - d) / em.range), 0, 100) : 0;
			apply(p);
		}
	}
}

} // End of namespace X3D
