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

#ifndef GRUMPA_CHARACTER_H
#define GRUMPA_CHARACTER_H

#include "common/array.h"
#include "common/str.h"

#include "grumpa/mesh.h"

namespace Grumpa {

// One CFXCharacter form from Actors/Characters.abi (docs/spec/characters.md).
struct Character {
	struct Attachment {
		Common::String anb, tga;
	};

	uint32 id = 0;
	bool active = false, visible = false;
	int32 home = -1;                 // the scene it is in (-1: none)
	Vec3 pos;
	float yaw = 0.0f;                // radians about +Y
	Common::Array<Common::String> anims, sounds, textures;  // anims[0] is the idle
	int texture = 0;
	Common::Array<Attachment> attachments;
	int kind = 0;                    // 0 creature, 1 mount, 2 rider form of parts[0] + parts[1]
	int parts[2] = { 0, 0 };
	Common::Array<uint32> pairs;     // (other id, form id) pairs, flattened
	bool latched = false;            // disabled by 0xd until 0x34
	bool talking = false;            // the speaker of a playing voice line (Q-0401)
};

/** The character database, loaded once and kept for the whole game. */
class Characters {
public:
	/** Read Actors/Characters.abi (the boot load, E-0402). */
	bool load();
	Character *find(int id);
	/** Deliver a command (`id` -1: every character); returns false if `id` is no character. */
	bool command(int id, int op, int arg1, int arg2);
	/** Drawn in the current scene: visible and at home there (E-0403). */
	bool present(const Character &c) const { return c.visible && c.home == _scene; }
	int scene() const { return _scene; }
	const Common::Array<Character> &all() const { return _chars; }

private:
	void apply(Character &c, int op, int arg1);

	Common::Array<Character> _chars;
	int _scene = -1;                 // +0x448, set by the scene-entry broadcast 0x17
};

} // End of namespace Grumpa

#endif // GRUMPA_CHARACTER_H
