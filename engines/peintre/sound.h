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

#ifndef PEINTRE_SOUND_H
#define PEINTRE_SOUND_H

#include "common/hashmap.h"
#include "common/hash-str.h"
#include "common/str.h"

#include "audio/mixer.h"

namespace Peintre {

/**
 * Sounds (formats README ".APC", ".WAV"; E-0209): static sounds are SOUND\<name>.WAV kept
 * in memory and played by name; the one stream plays SOUND\<name>.APC (voices) or a WAV.
 */
class Sound {
public:
	Sound(Audio::Mixer *mixer) : _mixer(mixer) {}
	~Sound();

	/** Plays the static sound SOUND\<name>.WAV; loop repeats it until stopStatic. */
	void playStatic(const Common::String &name, bool loop = false);
	void stopStatic(const Common::String &name);
	bool isStaticPlaying(const Common::String &name) const;
	void stopAllStatic();

	/** Starts the stream: SOUND\<name>.APC, else SOUND\<name>.WAV. Stops the previous one. */
	bool playStream(const Common::String &name);
	void stopStream();
	bool isStreamPlaying() const;
	/** Pauses or resumes the stream (the option menu from 3D, 0x416961 / 0x416987). */
	void pauseStream(bool pause);

	/** The player's volume, DirectSound attenuation (0 = full, -5000 = silent). */
	void setVolume(int32 attenuation);

private:
	Audio::Mixer *_mixer;
	Common::HashMap<Common::String, Audio::SoundHandle, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _statics;
	Audio::SoundHandle _stream;
};

} // End of namespace Peintre

#endif // PEINTRE_SOUND_H
