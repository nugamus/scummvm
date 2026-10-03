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

#ifndef GRUMPA_DIALOGUE_H
#define GRUMPA_DIALOGUE_H

#include "audio/mixer.h"
#include "common/array.h"
#include "common/str.h"

#include "grumpa/mesh.h"

namespace Grumpa {

class Characters;

// A CFXSound actor (types 0x18 / 0x2a) of a scene: an effect, music or a voice line
// (docs/spec/dialogue.md, E-0405).
// SceneSound lives in mesh.h (SceneData::sounds).

/** Read a sound record's body (after `u32 type, u32 id`) at `d[o]`, `n` bytes in all;
 *  advances `o` past it. The on-end commands keep their `when` and conditions. */
bool readSceneSound(const byte *d, uint32 n, uint32 &o, uint32 id, SceneSound &s);

/** Plays the current scene's sound actors (voice lines included). */
class Voices {
public:
	Voices(Audio::Mixer *mixer, Characters *chars) : _mixer(mixer), _chars(chars) {}
	~Voices() { stopAll(); }

	/** A new scene: stop everything, then take its sounds (owned by the scene data). */
	void enterScene(Common::Array<SceneSound> *sounds);
	/** Deliver a command (`id` -1: every sound). Commands a stop runs are appended to `out`.
	 *  Returns false if `id` is no sound of the scene. */
	bool command(int id, int op, int arg1, Common::Array<SceneCommand> &out);
	/** Once per update: start deferred plays, stop the sounds that ended and append their
	 *  command lists to `out`. */
	void update(Common::Array<SceneCommand> &out);
	void stopAll();
	bool playing(int id) const;

private:
	struct Run {
		Audio::SoundHandle handle;
		bool playing = false, pending = false, latched = false;
	};

	void apply(uint idx, int op, Common::Array<SceneCommand> &out);
	void play(uint idx);
	void stop(uint idx, bool runList, Common::Array<SceneCommand> &out);
	void setTalking(int speaker, bool on);

	Audio::Mixer *_mixer;
	Characters *_chars;
	Common::Array<SceneSound> *_sounds = nullptr;
	Common::Array<Run> _run;
};

} // End of namespace Grumpa

#endif // GRUMPA_DIALOGUE_H
