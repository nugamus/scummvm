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

#ifndef GILBERT_LOGIC_H
#define GILBERT_LOGIC_H

#include "common/array.h"
#include "common/random.h"
#include "common/rect.h"
#include "common/str.h"

#include "gilbert/database.h"

namespace Gilbert {

/**
 * The call-backs from the game rules into the game's screens (logic.md "Game object and
 * call-backs"; the original passes 22 function pointers to GEInit).
 */
class LogicListener {
public:
	virtual ~LogicListener() {}
	/** 1: load the room, Gilbert at (x, y) facing `direction` (0 N, 4 NE ... 28 NW). */
	virtual void gotoWalkmap(uint32 id, int x, int y, int direction) = 0;
	/** 2: re-read the walkmap objects. */
	virtual void refreshWalkmap() = 0;
	/** 3: open a close-up. */
	virtual void gotoCua(uint32 id) = 0;
	/** 4: re-read the close-up's objects. */
	virtual void refreshCua() = 0;
	/** 5: re-read the inventory. */
	virtual void refreshInventory() = 0;
	/** 6: show the current dialogue. */
	virtual void showDialog() = 0;
	/** 7, 8: waves of a list. */
	virtual void playWave(int list, int index, bool loop) = 0;
	virtual void stopWave(int list, int index) = 0;
	/** 10: a streamed sound; kind 0 room music, 1 dialogue voice, 2 other. */
	virtual void playStream(const Common::String &name, bool loop, int kind) = 0;
	/** 12: a film. */
	virtual void startFilm(const Common::String &name) = 0;
	/** 13, 14: Gilbert's position in room pixels. */
	virtual Common::Point gilbertPosition() = 0;
	/** 17, 18, 19: the room's control map. */
	virtual int mapWidth() = 0;
	virtual int mapHeight() = 0;
	virtual int mapCell(int x, int y) = 0;
	/** 20: walk in a direction code, or stop (-1). */
	virtual void walk(int direction) = 0;
	/** 21: a book topic appeared or went. */
	virtual void newTopic(bool shown) = 0;
};

/** What GE*GetObjectData returns for one object (logic.md). */
struct ObjectData {
	uint32 code = 0;
	int picture = -1;
	uint32 icon = 0;
	int x = 0, y = 0;
	bool pickable = false;
	Common::String text;
};

/** The topic-text tokens of GEBookParseNext (logic.md "Books"). */
enum BookToken {
	kTokenEnd = 0,
	kTokenText = 1,
	kTokenFormat = 2,
	kTokenLink = 3,
	kTokenLinkEnd = 4,
	kTokenPicture = 5,
	kTokenTab = 6,
	kTokenLineFeed = 7
};

struct BookParser {
	Common::String text;
	uint pos = 0;
	Common::String token;
	int format = 0, picture = 0, linkBook = 0, linkTopic = 0;

	int first(const Common::String &s);
	int next();
};

/** ge.dll: the game rules over the database (logic.md). */
class Logic {
public:
	explicit Logic(LogicListener *listener);

	Database &db() { return _db; }

	// Starting, loading, saving.
	bool load(Common::SeekableReadStream &in);
	void save(Common::WriteStream &out);
	void startNewGame();
	void continueGame();

	/** GEEllapsed: once per timer tick. */
	void tick(uint32 now);

	// Walkmaps and close-ups.
	uint walkmapObjectCount() const { return _walkmapList.size(); }
	bool walkmapObject(uint i, ObjectData &out);
	uint cuaObjectCount() const { return _cuaList.size(); }
	bool cuaObject(uint i, ObjectData &out);
	Common::Rect radarRect() const;
	uint32 walkmapId() const { return _walkmap ? _walkmap->id : 0; }
	uint32 cuaId() const { return _cua ? _cua->id : 0; }
	void walkmapAreaHit(int n);
	void cuaEnd();
	void clickObjectInCua(uint32 code);
	void objectToInventory(uint32 code);
	void useObjectOnObject(uint32 code, uint32 target);

	// Inventory.
	uint inventoryCount() const { return _shownInventory.size(); }
	bool inventoryObject(uint i, uint32 &code, uint32 &icon, Common::String &text);

	// Dialogues.
	Common::String dialogTitle() const;
	Common::String dialogText() const;
	uint dialogChoiceCount() const;
	Common::String dialogChoice(uint i) const;
	void dialogEnd(uint i);

	// Books, addressed by rank.
	uint bookTopicCount(uint book) const;
	Common::String bookTopicTitle(uint book, int rank) const;
	Common::String bookTopicText(uint book, int rank) const;
	uint32 bookTopicFromIndex(uint book, int rank) const;
	int bookIndexFromTopic(uint book, uint32 id) const;

	int32 variable(uint n) const { return n < Database::kVariables ? _db.vars[n] : 0; }

	// The path finder.
	bool pathNewPath(int x, int y);
	uint pathItemCount() const { return _path.size(); }
	Common::Point pathItem(uint i) const { return i < _path.size() ? _path[i] : Common::Point(); }
	void pathStop();

private:
	struct ObjRef {
		Obj *obj = nullptr;
		Cua *cua = nullptr; ///< the close-up it lies in; null in the inventory
		int index = -1;     ///< its index in that list
	};

	void repairData();
	ObjRef findObj(uint32 id);
	Obj *objById(uint32 id);
	ObjState *currentState(Obj &o);
	int findAnim(uint32 id) const;
	Cua *findCua(uint32 id);
	void setState(Obj &o, uint32 state);
	void resetStateAnim(ObjState &s);
	void updateCua();
	void updateInventory();
	void buildWalkmapObjects();
	void buildCuaObjects(bool reset);
	void clearAnims(Common::Array<uint32> &list, bool cua);
	void buildSort(Common::Array<uint32> &list, bool cua);
	bool stepAnim(uint32 objId, uint32 dt, uint32 &endEvent);
	void gotoWalkmap(uint32 id, int x, int y, int direction);
	void gotoCua(uint32 id);
	void startDialog(uint32 id);
	void doEvent(uint32 id);
	/** One event record; returns the event ID to jump to, or 0. */
	uint32 runEvent(const Event &e);
	void renumberBooks();
	const Topic *topicByRank(uint book, int rank) const;
	Topic *findTopic(uint book, uint32 id);

	// The path finder.
	bool findPath(Common::Point start, Common::Point target);
	void search(const Common::Rect &rect, Common::Point start, Common::Point target);
	bool walkable(Common::Point p, int targetValue) const;
	void pathStart();
	void pathTick();

	LogicListener *_listener;
	Database _db;
	Common::RandomSource _random;
	Walkmap *_walkmap = nullptr;
	Cua *_cua = nullptr;
	Dialog *_dialog = nullptr;
	Common::Array<uint32> _walkmapList, _cuaList; ///< object IDs
	Common::Array<uint32> _shownInventory;        ///< object IDs
	uint32 _last = 0;

	// Path state (logic.md "Path finding").
	int _mapW = 0, _mapH = 0;
	Common::Array<int> _grid;
	struct Node {
		int parent, next, cost;
	};
	Common::Array<Node> _nodes;
	Common::Array<Common::Point> _path;
	Common::Array<int> _dirs;
	bool _pathStopped = true;
	uint _pathK = 0;
	Common::Point _pathTarget, _pathLast;
	int _pathDist = 0;
	bool _pathDrift = false;
};

} // End of namespace Gilbert

#endif // GILBERT_LOGIC_H
