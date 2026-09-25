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

#ifndef X3D_TALK_H
#define X3D_TALK_H

#include "common/array.h"
#include "common/random.h"
#include "common/str.h"

namespace X3D {

class Scene;
class Sound;

// Characters' voices with lip sync (docs/engine-spec/sound.md, Character talk)
class Talk {
public:
	Talk(Scene &scene, Sound &sound, const Common::String &unitDir);

	// A speaking character and its face object, e.g. U01_01 / $$$DUMMY.*01SParle
	// clipDir: the mouth clips' folder, Anim/<character>/ when empty
	void addTalker(const Common::String &character, const Common::String &face, const Common::String &clipDir = "");

	// Plays Sound/<name> as the character; false when the character is not a talker
	bool say(const Common::String &character, const Common::String &name);

	// One logic step at logic time now (ms)
	void tick(uint32 now);

	bool talking() const { return _current >= 0; }

private:
	struct Talker {
		Common::String character, face;
		int slots[9]; // mouth clip nodes: 1 Yeux, 2 Ch, 3 Ch_yeux, 4 B, 5 E, 6 F, 7 O, 8 A
	};

	void select(int slot);
	void stop();

	Scene &_scene;
	Sound &_sound;
	Common::String _unitDir;
	Common::Array<Talker> _talkers;
	Common::RandomSource _random;

	int _current = -1, _selected = 0;
	Common::Array<uint32> _times;
	Common::Array<uint16> _shapes;
	uint32 _start = 0, _lastChange = 0, _nextRandom = 0, _now = 0;
};

} // End of namespace X3D

#endif // X3D_TALK_H
