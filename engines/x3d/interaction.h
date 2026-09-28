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

class Collision;
class Inventory;
class Scene;
class Sound;
class Talk;

// Hotspots, cursors and click actions of a unit
class Interaction {
public:
	Interaction(Scene &scene, Sound &sound, Talk &talk);
	~Interaction();

	// State set by the engine
	Math::Vector3d eye;             // the camera position, for voices that play at the listener
	Inventory *inventory = nullptr; // take and use-up steps show and hide it
	Collision *collision = nullptr; // take and step 9 switch objects out of it
	bool actionsEnabled = true;     // false: no action cursors, clicks do nothing

	// Data/Uxx/INFOOBJ.BIN hotspots and INFOACT.BIN actions
	void load(const Common::String &unitDir);

	// Hotspot index for a picked object, given its name then its parents' names, or -1
	int hotspotFor(const Common::StringArray &names) const;

	// Sets the cursor for the hotspot under the mouse (-1: none); call every frame
	void hover(int hotspot, uint32 millis);

	// Runs the hotspot's first runnable action; step-10 names go to unitActions
	void click(int hotspot, Common::StringArray &unitActions);

	void setCursorKind(const Common::String &hotspot, uint kind);

	// The item on the cursor ("U01_04", empty: none) and the plain cursor over 2D frames
	const Common::String &heldItem() const { return _heldItem; }
	void holdItem(const Common::String &item);
	void showCursor(uint kind);
	// Window pixels per game pixel, so cursors keep their size at high resolutions
	void setCursorScale(int scale);

	// Unit code access to actions by id (Mnn)
	void runAction(uint32 id, Common::StringArray &unitActions); // steps, then count the run
	void setCondition(uint32 id, const Common::String &condition);
	void exhaust(uint32 id); // count := max runs, exhausted, no step run
	void setRuns(uint32 id, int runs);
	uint32 lastRun() const { return _lastRun; } // the id of the action run last
	int runs(uint32 id) const;
	bool exhausted(uint32 id) const;
	// Steps 2 and 3 run by unit code: take a hotspot's object, use up the held item on one
	void take(const Common::String &hotspot);
	void useUp(const Common::String &hotspot);
	const Common::String &hotspotObject(const Common::String &hotspot) const; // "U01_07" -> "*U01_07"
	uint hotspotCount() const { return _hotspots.size(); }
	Common::StringArray hotspotNames() const;
	const Common::String &hotspotName(int index) const { return _hotspots[index].name; }
	// A click on it would do something: an action cursor, or a runnable action
	bool clickable(int index) const;

	// Saved state: the held item, run counts, exhaustion and conditions, hotspot cursors
	void syncState(Common::Serializer &s);

private:
	struct Hotspot {
		Common::String name; // "*U01_04"
		Common::String lowerName; // name in lower case, for hotspotFor
		uint32 type;
		uint32 cursor;
	};

	struct Action {
		uint32 id;
		Common::String name;
		Common::String condition;
		Common::String item;
		Common::String hotspot;
		Common::String target;
		int32 maxRuns;
		uint32 trigger;
		uint32 hotspotType;
		uint32 targetType;
		int hotspotIndex = -1; // findHotspot(hotspot), set at load
		bool running = false;  // inside run(): step 14 does not enter it again
		Common::Array<uint32> ops;
		Common::StringArray args;
	};

	// Run counts and exhausted flags belong to the action id, not the record:
	// two records with one id share them
	static const uint kIds = 256;
	int _runs[kIds] = {};
	uint32 _lastRun = 0;
	bool _exhausted[kIds] = {};

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
	int _cursorScale = 1;
	void replaceCursor(const Graphics::Surface &s, int hotspotX, int hotspotY);
};

} // End of namespace X3D

#endif // X3D_INTERACTION_H
