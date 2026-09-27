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

#ifndef RING_SOUND_H
#define RING_SOUND_H

#include "audio/mixer.h"
#include "common/array.h"
#include "common/ptr.h"
#include "common/str.h"

namespace Graphics {
class Font;
class ManagedSurface;
}

namespace Ring {

class RingEngine;

enum SoundType {
	kSoundMusic = 1,
	kSoundAmbientMusic = 2,
	kSoundAmbientEffect = 3,
	kSoundEffect = 4,
	kSoundDialogue = 5
};

/** Sound event reasons (spec/sound.md, "Playing, stopping, events"). */
enum {
	kSoundEnded = 0x1001,
	kSoundRestarted = 0x20,
	kSoundLeft = 0x10
};

/** An ambient or 3D sound of a puzzle or rotation (`aSoundItem`, spec/sound.md). */
struct SoundItem {
	int sound = 0;
	int volume = 100, pan = 0;
	bool active = true;
	int sameMode = 1, leaveMode = 1;
	int fade = 1;         ///< the declared fade less one (+0x19)
	int amplitude = 20;   ///< 3D: the pan's amplitude (+0x1d)
	float offset = 0.0f;  ///< 3D: LR × angle in radians (+0x21)
	// A transition's state (+0x25..+0x3d).
	float vol = 0, volTarget = 0, panNow = 0, panTarget = 0, volInc = 0, panInc = 0, steps = 0;

	/** The 3D pan for a view angle in degrees (0x41a4a0). */
	int pan3D(float alpha, int lr) const;
};

typedef Common::Array<Common::SharedPtr<SoundItem> > SoundItems;

/**
 * The sound list, dialogues and the ambient-sound handler (spec/sound.md). Sounds are
 * decoded whole when played; the mixer gets the original's DirectSound attenuation.
 */
class Sounds {
public:
	explicit Sounds(RingEngine *vm);
	~Sounds();

	/** `SouAdd`. */
	void add(int id, int type, const Common::String &file);
	/** 0x406de0 / `NoiceIdPlay`: once or looping. */
	void play(int id, bool loop);
	/** 0x406e00: stops it, with the event when it was playing. */
	void stop(int id, int reason);
	/** 0x406e40 */
	void stopType(int type, int reason);
	/** 0x406ea0 */
	void stopAll(int reason);
	/** `SouSet_406e20`: the sound's own volume. */
	void setVolume(int id, int volume);
	void setPan(int id, int pan);
	/** The preferences' volumes: `volume` for every type but 5, `dialogue` for 5. */
	void setTypeVolumes(int volume, int dialogue);
	bool playing(int id);
	bool typePlaying(int type);

	/** Once per frame, after drawing: natural ends raise their events (0x468da0). */
	void checkEnds();
	/** Once per frame before the cursor: the first dialogue's subtitles and lip sync (0x427c70). */
	void dialogueFrame(Graphics::ManagedSurface &dst, const Graphics::Font *font, bool subtitles);

	// Ambient and 3D sounds (spec/sound.md, "Changing place").
	/**
	 * `PuzSetAct` / `RotSetAct`'s sound part for the place entered. For a rotation the 3D
	 * items get their pans for view angle `alpha` when the place is started afresh (0x41ee10).
	 */
	void enterPlace(const SoundItems *items, bool start, bool stop, bool rotation = false, float alpha = 0.0f, int lr = -1);
	/** A movability click: the target's items become the new list and a transition is set up. */
	void prepareTransition(const SoundItems *items);
	bool transitionPending() const { return _pending; }
	/** 0x41aee0: the step count of the transition; false abandons it. */
	bool beginTransition(int steps);
	/** 0x41b180 and 0x41b350 for step `k`. */
	void stepTransition(int k);
	/** The ride video's start (`aCinMov::Init`): stop-now items stopped, `frames` steps. */
	void beginRide(int frames);
	/** After the ride video's frame `k` (and with the frame count on Escape). */
	void rideStep(int k) {
		if (_pending)
			stepTransition(k);
	}
	/** The finish of a transition without a ride video (0x41b130, 0x41aee0(2), steps 3). */
	void finishTransition();
	/** Starts / stops an item (0x41a350 / 0x41a3b0), without events. */
	void startItem(const SoundItem &item);
	void stopItem(const SoundItem &item);

private:
	struct Sound {
		int id = 0, type = 0;
		Common::String file;
		int own = 100, typeVolume = 100, pan = 0;
		bool started = false;
		Audio::SoundHandle handle;
	};
	struct Line {
		uint32 time;
		Common::String first, second;
	};
	struct Dialogue {
		int id = 0;
		Common::Array<Line> lines;
		struct Map {
			int level, object, presentation;
		};
		struct Lip {
			uint32 start, end;
			int level;
		};
		Common::Array<Map> maps;
		Common::Array<Lip> lips;
		bool hasLips = false;
		uint32 clock = 0;
		int level = 0;
	};

	Sound *find(int id);
	bool active(Sound &s);
	void apply(Sound &s);
	void startStream(Sound &s, bool loop);
	void stopStream(Sound &s);
	bool readDialogue(Dialogue &d, const Sound &s);
	bool removeDialogue(int id);
	void event(Sound &s, int reason);
	void computeTransition();

	RingEngine *_vm;
	Common::Array<Sound> _sounds;
	Common::Array<Dialogue> _dialogues;

	const SoundItems *_old = nullptr, *_new = nullptr;
	bool _pending = false;
	Common::Array<SoundItem *> _stopNow, _fadeOut, _fade, _start;
};

} // End of namespace Ring

#endif // RING_SOUND_H
