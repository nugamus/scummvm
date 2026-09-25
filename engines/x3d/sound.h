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

#ifndef X3D_SOUND_H
#define X3D_SOUND_H

#include "common/array.h"
#include "common/path.h"
#include "common/str.h"

#include "audio/mixer.h"

#include "math/vector3d.h"

namespace X3D {

// The sound manager (docs/engine-spec/sound.md): groups with volumes, sounds with their
// own volume, and the scene's two positional emitters
class Sound {
public:
	enum Group { kAmbient = 1, kVoice = 2, kEffects = 3 };
	// The phone emitter is U01's third one (u01.md, ClicTel)
	enum Emitter { kVoiceEmitter, kEffectsEmitter, kPhoneEmitter };

	explicit Sound(Audio::Mixer *mixer);
	~Sound();

	// Starts a WAV file; v is 0..100
	void play(const Common::Path &path, int group, int v, bool loop);
	void stopGroup(int group);
	void stopAll();
	bool isGroupPlaying(int group);
	void setGroupVolume(int group, int g); // 0..100, applied at once

	// Emitters: ranges from the scene scale, then play at a position; false when the same
	// file is still playing on it
	void setScale(float scale);
	bool emit(Emitter e, const Common::Path &path, const Math::Vector3d &position, bool loop);
	void detach(Emitter e); // op 101: the emitter no longer owns its sound
	void updateVolumes(const Math::Vector3d &eye);
	void setEmitterPosition(Emitter e, const Math::Vector3d &position) { _emitters[e].position = position; }
	void stopEmitter(Emitter e);

private:
	struct Playing {
		Audio::SoundHandle handle;
		int group, v;
	};

	void apply(Playing &p);
	void prune();

	Audio::Mixer *_mixer;
	Common::Array<Playing> _playing;
	int _groupVolume[7];

	struct EmitterState {
		int group;
		float range = 1;
		Math::Vector3d position;
		Common::String lastName;
		Audio::SoundHandle handle;
		bool owns = false;
	} _emitters[3];
};

} // End of namespace X3D

#endif // X3D_SOUND_H
