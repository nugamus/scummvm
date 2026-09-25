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

#ifndef X3D_INTERACTION_H
#define X3D_INTERACTION_H

#include "common/array.h"
#include "common/path.h"
#include "common/str.h"
#include "common/str-array.h"

#include "math/vector3d.h"

namespace Graphics {
struct Surface;
}

namespace Common {
class Serializer;
}

namespace X3D {

class Inventory;
class Scene;
class Sound;
class Talk;

// Hotspots, cursors and click actions of a unit (docs/engine-spec/interaction.md)
class Interaction {
public:
	Interaction(Scene &scene, Sound &sound, Talk &talk);

	Math::Vector3d eye; // the camera position, for voices that play at the listener
	~Interaction();

	// Data/Uxx/INFOOBJ.BIN hotspots and INFOACT.BIN actions
	void load(const Common::String &unitDir);

	// Hotspot index for a picked object, given its name then its parents' names, or -1
	// (interaction.md, Hotspots)
	int hotspotFor(const Common::StringArray &names) const;

	// Sets the cursor for the hotspot under the mouse (-1: none); call every frame
	void hover(int hotspot, uint32 millis);

	// Runs the hotspot's first runnable action; op-10 names go to unitActions
	void click(int hotspot, Common::StringArray &unitActions);

	void setCursorKind(const Common::String &hotspot, uint kind);

	// The item on the cursor ("U01_04", empty: none) and the plain cursor over 2D frames
	const Common::String &heldItem() const { return _heldItem; }
	void holdItem(const Common::String &item);
	void showCursor(uint kind);
	Inventory *inventory = nullptr; // take and use-up steps show and hide it

	// Unit code access to actions by id (Mnn)
	void runAction(uint32 id, Common::StringArray &unitActions); // steps, then count the run
	void setCondition(uint32 id, const Common::String &condition);
	int runs(uint32 id) const;
	bool exhausted(uint32 id) const;
	// Steps 2 and 3 run by unit code: take a hotspot's object, use up the held item on one
	void take(const Common::String &hotspot);
	void useUp(const Common::String &hotspot);
	const Common::String &hotspotObject(const Common::String &hotspot) const; // "U01_07" -> "*U01_07"
	Common::StringArray hotspotNames() const;

	bool actionsEnabled = true;

	// Saved state (save.md CURSOR, ACTIONS, OBJECTS): the held item, run counts,
	// exhaustion and conditions, hotspot cursors
	void syncState(Common::Serializer &s);

private:
	struct Hotspot {
		Common::String name; // "*U01_04"
		uint32 type, cursor;
	};

	struct Action {
		uint32 id;
		Common::String name, condition, item, hotspot, target;
		int32 maxRuns;
		uint32 trigger, hotspotType, targetType;
		Common::Array<uint32> ops;
		Common::StringArray args;
		int runs = 0;
		bool exhausted = false;
	};

	int findHotspot(const Common::String &name) const; // "U01_04" or "*U01_04"
	bool runnable(const Action &a, uint32 trigger) const;
	bool evaluate(const Common::String &condition) const;
	void run(Action &a, Common::StringArray &unitActions);
	Common::Path soundPath(const Common::String &name) const;
	Common::String hotspotName(const Action &a) const;
	void setCursor(uint kind);

	Sound &_sound;
	Talk &_talk;
	Scene &_scene;
	Common::String _soundDir;
	Common::Array<Hotspot> _hotspots;
	Common::Array<Action> _actions;

	Common::String _heldItem; // "U01_04" while an item is held
	Graphics::Surface *_heldImage = nullptr;
	Graphics::Surface *_cursors[6] = {};
	int _shownCursor = -2; // -1: held item, -3: nothing (blink off)
};

} // End of namespace X3D

#endif // X3D_INTERACTION_H
