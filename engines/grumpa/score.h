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

#ifndef GRUMPA_SCORE_H
#define GRUMPA_SCORE_H

#include "audio/mixer.h"
#include "common/array.h"
#include "common/rect.h"
#include "common/serializer.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

namespace Grumpa {

class Characters;

/** Actor 8, the score display: coins, life, air and the current form (docs/spec/score.md). */
class Score {
public:
	enum { kId = 8 };

	/** A new game: UI/008_Score/008_Score.atx, its pictures, the starting values. */
	bool load();
	/** DoCommand; `chars` holds the form characters' Life (slot 1). */
	void command(int op, int arg1, int arg2, Characters &chars);
	/** Slot `slot` (0 State, 1 Coins, 2 Life, 3 Air) for a condition. */
	bool stateOf(int slot, int32 &value) const;
	/** The 20 ms update: the stars' animation. */
	void update();
	/** Draw it over the scene (layer 6). */
	void draw(Graphics::ManagedSurface &screen) const;
	void syncState(Common::Serializer &s);

private:
	enum { kCoins = 1, kLife = 2, kAir = 3, kElements = 13, kFont = 5 };
	struct Element {
		Common::Array<Graphics::ManagedSurface> frames;
		uint32 key = 0;           // frame 0's pixel (0, 0)
		Common::Point pos;
		bool visible = true, active = true;
		int frame = 0;
		bool running = false;     // a star playing forward and back
		int dir = 0;
		int width() const { return frames.empty() ? 0 : frames[0].w; }
	};

	void add(int slot, int n);
	void sub(int slot, int n);
	void setForm(int charId, Characters &chars);
	void barFrame(int slot, bool added);
	void playStar(int i);
	void sound(int i);
	int formIcon() const;

	Element _el[kElements];
	Common::String _sounds[6];
	Audio::SoundHandle _handle[6];
	int32 _slot[4] = { 0, 0, 99, 99 };
	bool _active = true, _visible = true;
	int _form = 0;                // index into the form map (Grumpa)
	int _digitGap = 10, _digitSpacing = 2;
};

/** Actor 180, the ambience: looping sounds cross-faded by opcode 73 (docs/spec/score.md). */
class Ambience {
public:
	enum { kId = 180 };

	~Ambience() { stop(); }

	/** A new game: the names from Actors/global2.atx; the start sound plays with the first
	 *  scene. */
	bool load();
	void command(int op, int arg1);
	/** Entering a scene: play the start sound (new game) or the saved one (load). */
	void enterScene();
	void update();
	void stop();
	void syncState(Common::Serializer &s);

private:
	struct Sound {
		Audio::SoundHandle handle;
		int32 volume = 0;           // hundredths of a dB
		bool fadeIn = false, fadeOut = false;
	};
	void play(int i);
	void setVolume(Sound &snd, int32 v);

	Common::Array<Common::String> _names;
	Common::Array<Sound> _snd;
	int _start = 0, _cur = -1, _prev = -1;
	int _pending = -1;              // played on the next scene entry
};

} // End of namespace Grumpa

#endif // GRUMPA_SCORE_H
