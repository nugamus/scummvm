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

#ifndef GILBERT_DATABASE_H
#define GILBERT_DATABASE_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

namespace Common {
class SeekableReadStream;
class WriteStream;
}

namespace Gilbert {

// The game database, `Data/game/default.dat` and the saved games: an MFC archive of the
// original's game object (formats README, "default.dat"). Field names follow the README;
// unk_* fields are not understood and kept as they are.

struct ObjState {
	uint32 state = 0;
	Common::String name;
	uint32 walkmapAnim = 0;
	uint32 cuaAnim = 0;
	uint32 clickEvent = 0;
	uint32 takeEvent = 0;
	uint32 icon = 0; ///< pattern of inventory.wxi (logic.md)
	uint32 pickable = 0;
	Common::String text;

	int currentAnim = -1; ///< runtime: index into Database::anims, -1 none (logic.md)
};

struct Obj {
	Common::Array<ObjState> states;
	uint32 cuaId = 0;
	uint32 id = 0;
	uint32 visible = 0;
	uint32 state = 0; ///< current state number
};

struct Cua {
	Common::Array<Obj> objs;
	uint32 id = 0;
	Common::String name;
	uint32 firstEvent = 0;
	uint32 event = 0;
	uint32 endEvent = 0;
	uint32 firstVisit = 0;
};

struct Walkmap {
	Common::Array<Cua> cuas;
	uint32 id = 0;
	Common::String title;
	int32 radar[4] = { 0, 0, 0, 0 }; ///< left, top, right, bottom
};

struct Anim {
	uint32 time = 0;
	uint32 id = 0;
	uint32 next = 0;
	Common::String name;
	uint32 duration = 0;
	uint32 unk18 = 0;
	uint32 x = 0, y = 0; ///< the picture's top-left corner (logic.md)
	uint32 z = 0;
	uint32 picture = 0; ///< item of the room's or close-up's collection (logic.md)
	uint32 endEvent = 0;
};

struct UseObj {
	uint32 obj = 0;
	uint32 target = 0;
	uint32 event = 0;
};

struct Event {
	uint32 id = 0, type = 0, cond = 0, var = 0;
	int32 value = 0;
	uint32 jump = 0, walkmap = 0, cua = 0, obj = 0, book = 0, topic = 0, topic2 = 0, dialog = 0;
	uint32 sound38 = 0, sound3c = 0;
	Common::String soundName;
	uint32 sound44 = 0, unk48 = 0;
	Common::String video;
	uint32 x = 0, y = 0, unk58 = 0;
	Common::String comment;
};

struct DialogChoice {
	uint32 unk04 = 0;
	Common::String text;
	uint32 event = 0;
};

struct Dialog {
	Common::Array<DialogChoice> choices;
	uint32 id = 0;
	Common::String title;
	Common::String text;
};

struct Topic {
	uint32 id = 0;
	Common::String title;
	Common::String text;
	uint32 shown = 0;
	int32 index = -1;
};

struct Text {
	uint32 id = 0;
	Common::String text;
};

struct Database {
	enum {
		kVariables = 200,
		kBooks = 10
	};

	uint32 walkmap = 0;
	uint32 startX = 0, startY = 0;
	uint32 unk10 = 0;
	int32 vars[kVariables];
	Common::Array<Walkmap> walkmaps;
	Common::Array<Obj> inventory;
	Common::Array<UseObj> useObjs;
	Common::Array<Event> events;
	Common::Array<Dialog> dialogs;
	Common::Array<Text> texts;
	Common::Array<Topic> books[kBooks];
	Common::Array<Anim> anims;

	Database();
	void clear();
	/** Reads the whole archive; false if it is not one (the original's load then fails). */
	bool load(Common::SeekableReadStream &in);
	void save(Common::WriteStream &out) const;
};

} // End of namespace Gilbert

#endif // GILBERT_DATABASE_H
