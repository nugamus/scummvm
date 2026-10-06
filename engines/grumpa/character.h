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
#include "audio/mixer.h"

#include "grumpa/mesh.h"
#include "grumpa/walk.h"

namespace Grumpa {

class EventVM;
class GrumpaEngine;

// One CFXCharacter form from Actors/Characters.abi (docs/spec/characters.md).
struct Character {
	// A carried object (characters.md Attachments, E-1700): drawn on a face of the body while
	// worn; its bonuses go to state slots 2 (attack) and 3 (defence).
	struct Attachment {
		Common::String anb, tga;
		int face = 0;
		int32 attack = 0, defence = 0;
		bool loaded = false;     // mesh and skin, on first draw
		Mesh mesh;
		Graphics::Surface skin;
		bool alpha = false;
	};

	uint32 id = 0;
	bool active = false, visible = false;
	int32 home = -1;                 // the scene it is in (-1: none)
	Vec3 pos;
	float yaw = 0.0f;                // radians about +Y
	Common::Array<Common::String> anims, sounds, textures;  // anims[0] is the idle
	int texture = 0;
	Common::Array<Attachment> attachments;
	uint32 worn = 0;                 // +0x39c: bit k, attachment k is worn
	int kind = 0;                    // 0 creature, 1 mount, 2 rider form of parts[0] + parts[1]
	int parts[2] = { 0, 0 };
	Common::Array<uint32> pairs;     // (other id, form id) pairs, flattened
	bool latched = false;            // disabled by 0xd until 0x34
	Vec3 pending;                    // +0x294: a move the next animation tick takes (0x47)
	// One animation slot (E-0815): the .anb and its .amb per-frame root motion, on first use.
	struct Clip {
		bool loaded = false;
		Mesh mesh;
		Common::Array<Vec3> motion;
	};
	Common::Array<Clip> clips;       // +0x2dc: 44 slots, by the number the .anb name starts with
	int clip = 0;                    // +0x434: the slot playing
	Common::Array<int> queue;        // +0x404: the slots to play next
	int idleStarts = 0;              // +0x4a8: clip-0 starts in a row (the fidget, E-1740)
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
	/** State slot 4 (+0x564) is the role: 0 none, 1 the player's, 2 the follower's, 3..7 a
	 *  fighter's (E-1530, combat.md). */
	int role() const { return state.size() > 4 ? state[4] : 0; }
	void setRole(int r) {
		if (state.size() > 4)
			state[4] = r;
	}
	// Sound slots and the speech queue (E-1640): slot n is the .wav whose name starts with n.
	Common::Array<Common::String> voiceFiles;  // +0x324: 100 slots, "" = none; on first use
	Audio::SoundHandle voices[100];
	Common::Array<int> speech;       // +0x3d0: the queued slots, the front one speaking
};

/** The character database, loaded once and kept for the whole game. */
class Characters {
public:
	~Characters();
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
	/** Wear (or take off) attachment `k` of character `id` (0x421780, E-1700): one shield
	 *  (0, 5) and one weapon (1..4) at a time, with their bonuses. */
	void wear(int id, int k, bool on);
	/** The mesh of the clip `c` plays (loaded on first use); nullptr if it has none. */
	const Mesh *mesh(Character &c);
	/** The player's character (actor 3's, E-0811), present or not; nullptr if none. */
	Character *player() { return find(_player); }
	/** The character actor 3 or 4 holds (-1: none, or no such actor). */
	int held(int actor) const { return actor == kPlayerActor ? _player : actor == kFollowerActor ? _follow.id : -1; }
	/** The companion actor 4 holds, present or not; nullptr if none. */
	Character *follower() { return find(_follow.id); }
	/** Opcodes actors 3 and 4 pass on to their character (E-0811, E-1222). */
	static bool passedOn(int op) {
		return op <= 3 || (op >= 0xb && op <= 0xd) || (op >= 0x32 && op <= 0x36) || op == 0x48 || op == 500 || op == 501;
	}
	/** Backspace (E-1503): a rider form splits, else the companion is let go. */
	void backspace();
	/** The sphere test (E-0705): `c`'s sphere and (centre, r) overlap. */
	static bool touches(const Character &c, const Vec3 &centre, float r);
	const Floor &floor() const { return _floor; }
	/** A load: the next scene entry is a first one (no entry placement), or, with
	 *  `firstEntry`, puts the player at the scene's first entry, at home there. */
	void forgetScene(bool firstEntry);
	/** Saves: each character's position, yaw, home, active and visible (version 4), the
	 *  characters actors 3 and 4 hold (5); state slots, texture, worn attachments and latch
	 *  (6). */
	bool syncState(Common::Serializer &s);
	/** The engine, for loading meshes, and the VM, for the commands characters send. */
	void attach(GrumpaEngine *vm, EventVM *events) { _vm = vm; _events = events; }

	// Actor 3 (the player controller) and actor 4 (CFXFollower) start holding Grumpa and the
	// Scharlakanskraken (global2.atx <22>, <23>; E-1530).
	enum { kPlayerActor = 3, kFollowerActor = 4, kGrumpa = 10, kKraken = 16, kScore = 8, kFloor = 600 };
	/** State slot `slot` of character `id` for a condition (E-0201); false if `id` is no character. */
	bool stateOf(int id, int slot, int32 &value);
	/** A scene sound of speaker `id` plays: an absent speaker's queue is cut (E-1620). */
	void flushSpeech(int id);

private:
	void freeSurfaces();
	void apply(Character &c, int op, int arg1);
	Character::Clip *clip(Character &c, int slot);
	void enter(int scene);
	void makePlayer(Character &c);
	void release(int actor);
	void split(Character &form);
	void follow();
	void enterFollower(int scene);
	void shuffleAside(Character &c);
	void nudge(Character &c, float ux, float uz);
	void setRoles();
	bool voiceLoaded(Character &c, int n);
	void playVoice(Character &c, int n);
	void flushSpeech(Character &c);
	void pumpSpeech(Character &c);

	Common::Array<Character> _chars;
	int _scene = -1;                 // +0x448, set by the scene-entry broadcast 0x17
	Floor _floor;                    // the scene's walk mesh (walk.cpp)
	bool _firstEntry = false;        // the next entry places the player at the first entry
	int _player = kGrumpa;           // actor 3's +0x298
	struct Follower {                // actor 4, CFXFollower (E-1220)
		int id = kKraken;            // +0x290
		bool place = true;           // +0x27c: the next entry puts it behind the player
		int freeze = 0;              // +0x280: rule runs left with the turn held at 0
		int divider = 0;             // +0x284: the rule runs every second update
		int state = -1;              // +0x288: the last request band
	} _follow;
	GrumpaEngine *_vm = nullptr;
	EventVM *_events = nullptr;
};

} // End of namespace Grumpa

#endif // GRUMPA_CHARACTER_H
