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
#include "common/serializer.h"
#include "common/str.h"

#include "grumpa/mesh.h"
#include "grumpa/walk.h"

namespace Grumpa {

class GrumpaEngine;

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
	// One animation slot (E-0815): the .anb and its .amb per-frame root motion, on first use.
	struct Clip {
		bool loaded = false;
		Mesh mesh;
		Common::Array<Vec3> motion;
	};
	Common::Array<Clip> clips;       // +0x2dc: 44 slots, by the number the .anb name starts with
	int clip = 0;                    // +0x434: the slot playing
	Common::Array<int> queue;        // +0x404: the slots to play next
	int turnSteps = 0;               // +0x4ac: yaw steps left
	float turnStep = 0.0f;           // +0x4b8
	float radius = 0.0f;             // [0x28c]: kept this far off the walk mesh's walls
	float sphere = 0.0f;             // [0x290]: the proximity sphere's radius (E-0705)
	int face = -1;                   // +0x44c: the walk-mesh face under it
	int floorType = -1;              // +0x450
	int platform = -1;               // +0x454: the platform it stands on (E-1600)
	// Drawing (scene.cpp): the texture in use, loaded on first draw.
	Graphics::Surface skin;          // ARGB8888, like SceneMesh::texture
	bool alpha = false;
	int skinIndex = -1;              // the texture index `skin` was loaded for
	float clock = 0.0f;              // +0x4a4: the animation clock (E-0603)
	int frame = 0;                   // +0x494: the frame of the clip shown
	Common::Array<int32> state;      // the state slots (E-0201, E-0407): 6 per character
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
	Common::Array<Character> &list() { return _chars; }
	/** The 20 ms update: present characters step their clips and move on the walk mesh. */
	void update();
	/** A movement request (E-0813): 0 walk, 1 run, 2 stop, 5 reset; any request (-1 only
	 *  that) also aims the yaw `turn` radians round over the next 10 animation ticks. */
	void request(Character &c, int req, float turn);
	/** The mesh of the clip `c` plays (loaded on first use); nullptr if it has none. */
	const Mesh *mesh(Character &c);
	/** The player's character (actor 3's, E-0811), present or not; nullptr if none. */
	Character *player() { return find(kPlayer); }
	/** The sphere test (E-0705): `c`'s sphere and (centre, r) overlap. */
	static bool touches(const Character &c, const Vec3 &centre, float r);
	const Floor &floor() const { return _floor; }
	/** A load: the next scene entry is a first one (no entry placement), or, with
	 *  `firstEntry`, puts the player at the scene's first entry, at home there. */
	void forgetScene(bool firstEntry) { _scene = -1; _firstEntry = firstEntry; }
	/** Saves (version 4): each character's position, yaw, home, active and visible. */
	void syncState(Common::Serializer &s);
	/** The engine, for loading meshes. */
	void attach(GrumpaEngine *vm) { _vm = vm; }

	// ponytail: Grumpa; the original takes whoever character opcode 0x2c names (E-0816), the
	// mounts' rider forms (11, 12, 13, 88) come with them (Q-0403).
	enum { kPlayer = 10, kFloor = 600 };
	/** State slot `slot` of character `id` for a condition (E-0201); false if `id` is no character. */
	bool stateOf(int id, int slot, int32 &value);

private:
	void apply(Character &c, int op, int arg1);
	Character::Clip *clip(Character &c, int slot);
	void enter(int scene);

	Common::Array<Character> _chars;
	int _scene = -1;                 // +0x448, set by the scene-entry broadcast 0x17
	Floor _floor;                    // the scene's walk mesh (walk.cpp)
	bool _firstEntry = false;        // the next entry places the player at the first entry
	GrumpaEngine *_vm = nullptr;
};

} // End of namespace Grumpa

#endif // GRUMPA_CHARACTER_H
