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

#ifndef GILBERT_SOUND_H
#define GILBERT_SOUND_H

#include "audio/mixer.h"

#include "common/array.h"
#include "common/str.h"

#include "gilbert/collection.h"

namespace Gilbert {

/**
 * The wave lists (`Data/Sounds/misc/<list>.wxs`) and the streamed music
 * (`Data/Sounds/MUSIC/<name>.wav`), boot.md "Sounds in the menu".
 */
class Sound {
public:
	enum Stream {
		kMusic,   ///< the menu or room music
		kCredits, ///< credit.wav on the About page
		kDialog,  ///< a dialogue voice
		kOther,   ///< another streamed sound (misc)
		kStreamCount
	};

	explicit Sound(Audio::Mixer *mixer);
	~Sound();

	/** Loads `Data/Sounds/misc/<file>` as wave list `list` (1 or 2). */
	void loadList(int list, const char *file);
	/** PlayWave(list, index): a wave of a list at the sound volume. */
	void playWave(int list, int index, bool looped = false);
	/**
	 * Call-back 7: a list other than 1 and than the last one loaded first loads
	 * `Data/Sounds/misc/<list>.wxs` into wave list 3; then item `index` of wave list `list`.
	 */
	void playListWave(int list, int index, bool looped);
	/** Whether the last wave started is still playing. */
	bool isWavePlaying() const;

	/**
	 * Opens `Data/Sounds/MUSIC/<name>.wav` on a stream at the music volume; it plays at
	 * once unless `start` is false (then startStream() plays it).
	 */
	void openStream(Stream stream, const Common::String &name, bool looped, bool start = true);
	void startStream(Stream stream);
	/** Streams a WAV file below Data; `sfx` plays it at the sound volume. */
	void openFile(Stream stream, const Common::Path &path, bool looped, bool sfx);
	/** Stop-all: every stream (the waves play on). */
	void stopAll();

	/** Levels 1..6 (boot.md "Settings"). */
	void setVolumes(int music, int sound);

private:
	Audio::Mixer *_mixer;
	Common::Array<Wave> _lists[5];
	int _lastLoaded = 0;
	Audio::SoundHandle _wave;
	Audio::SoundHandle _streams[kStreamCount];
	Audio::AudioStream *_pending[kStreamCount];
	byte _musicVolume = 255, _soundVolume = 255;
};

} // End of namespace Gilbert

#endif // GILBERT_SOUND_H
