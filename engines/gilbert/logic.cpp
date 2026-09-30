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

#include "common/debug.h"
#include "common/stream.h"
#include "common/system.h"

#include "gilbert/detection.h"
#include "gilbert/logic.h"

namespace Gilbert {

// The control map's cell size in room pixels (call-backs 15, 16 return 16).
static const int kCell = 16;
// Direction index (E, NE, N, NW, W, SW, S, SE) -> walking direction code (logic.md).
static const int kDirCode[8] = { 8, 4, 0, 28, 24, 20, 16, 12 };
static const int kDirX[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
static const int kDirY[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };

Logic::Logic(LogicListener *listener) : _listener(listener), _random("gilbert") {
}

// ---------------------------------------------------------------------------
// Starting, loading, saving (logic.md "Starting, loading, saving")

bool Logic::load(Common::SeekableReadStream &in) {
	pathStop();
	_walkmap = nullptr;
	_cua = nullptr;
	_dialog = nullptr;
	_walkmapList.clear();
	_cuaList.clear();
	_shownInventory.clear();
	_path.clear();
	_dirs.clear();
	if (!_db.load(in))
		return false;
	// Each object's current state number resolves to its state, the first if absent.
	for (Walkmap &w : _db.walkmaps)
		for (Cua &c : w.cuas)
			for (Obj &o : c.objs)
				if (!currentState(o) && !o.states.empty())
					o.state = o.states[0].state;
	renumberBooks();
	return true;
}

void Logic::save(Common::WriteStream &out) {
	// The header: the current walkmap, Gilbert's position, direction 0.
	_db.walkmap = walkmapId();
	const Common::Point p = _listener->gilbertPosition();
	_db.startX = p.x;
	_db.startY = p.y;
	_db.unk10 = 0;
	_db.save(out);
}

void Logic::startNewGame() {
	doEvent(1);
	updateInventory();
}

void Logic::continueGame() {
	gotoWalkmap(_db.walkmap, _db.startX, _db.startY, _db.unk10);
	updateInventory();
}

// ---------------------------------------------------------------------------
// Lookups

int Logic::findAnim(uint32 id) const {
	for (uint i = 0; i < _db.anims.size(); i++)
		if (_db.anims[i].id == id)
			return i;
	return -1;
}

Cua *Logic::findCua(uint32 id) {
	for (Walkmap &w : _db.walkmaps)
		for (Cua &c : w.cuas)
			if (c.id == id)
				return &c;
	return nullptr;
}

Logic::ObjRef Logic::findObj(uint32 id) {
	ObjRef r;
	for (Walkmap &w : _db.walkmaps)
		for (Cua &c : w.cuas)
			for (uint i = 0; i < c.objs.size(); i++)
				if (c.objs[i].id == id) {
					r.obj = &c.objs[i];
					r.cua = &c;
					r.index = i;
					return r;
				}
	for (uint i = 0; i < _db.inventory.size(); i++)
		if (_db.inventory[i].id == id) {
			r.obj = &_db.inventory[i];
			r.index = i;
			return r;
		}
	return r;
}

Obj *Logic::objById(uint32 id) {
	return findObj(id).obj;
}

ObjState *Logic::currentState(Obj &o) {
	for (ObjState &s : o.states)
		if (s.state == o.state)
			return &s;
	return nullptr;
}

void Logic::setState(Obj &o, uint32 state) {
	for (const ObjState &s : o.states)
		if (s.state == state) {
			o.state = state;
			return;
		}
	// The original stores a list node here (Q-0400); the data never asks for it.
	warning("Gilbert: object %d has no state %d", o.id, state);
}

void Logic::resetStateAnim(ObjState &s) {
	const int a = findAnim(s.cuaAnim);
	if (a >= 0) {
		s.currentAnim = a;
		_db.anims[a].time = 0;
	}
}

// ---------------------------------------------------------------------------
// Object lists (logic.md "Walkmap and CUA objects")

void Logic::buildWalkmapObjects() {
	_walkmapList.clear();
	if (!_walkmap)
		return;
	for (Cua &c : _walkmap->cuas)
		for (Obj &o : c.objs) {
			ObjState *s = currentState(o);
			if (!o.visible || !s || _walkmapList.size() >= 100)
				continue;
			const int a = findAnim(s->walkmapAnim);
			if (a < 0)
				continue;
			_walkmapList.push_back(o.id);
			s->currentAnim = a;
		}
	clearAnims(_walkmapList, false);
	_last = g_system->getMillis();
	buildSort(_walkmapList, false);
}

void Logic::buildCuaObjects(bool reset) {
	if (!_cua)
		return;
	_cuaList.clear();
	for (Obj &o : _cua->objs) {
		ObjState *s = currentState(o);
		if (!o.visible || !s || _cuaList.size() >= 100)
			continue;
		if (findAnim(s->cuaAnim) >= 0)
			_cuaList.push_back(o.id);
	}
	if (reset) {
		clearAnims(_cuaList, true);
		_last = g_system->getMillis();
	}
	buildSort(_cuaList, true);
}

void Logic::clearAnims(Common::Array<uint32> &list, bool cua) {
	for (uint32 id : list) {
		Obj *o = objById(id);
		if (!o)
			continue;
		for (ObjState &s : o->states) {
			const uint32 animId = cua ? s.cuaAnim : s.walkmapAnim;
			if (!animId)
				continue;
			const int a = findAnim(animId);
			if (a < 0)
				return; // the original stops here (none in the data)
			s.currentAnim = a;
			_db.anims[a].time = 0;
		}
	}
}

// Descending z; of equal z the last in the list comes first (logic.md "BuildSort").
void Logic::buildSort(Common::Array<uint32> &list, bool cua) {
	Common::Array<uint32> rest = list, sorted;
	Common::Array<int> z;
	for (uint32 id : rest) {
		Obj *o = objById(id);
		ObjState *s = o ? currentState(*o) : nullptr;
		const int a = s ? findAnim(cua ? s->cuaAnim : s->walkmapAnim) : -1;
		z.push_back(a >= 0 ? (int)_db.anims[a].z : 0);
	}
	while (!rest.empty()) {
		uint best = 0;
		for (uint i = 0; i < rest.size(); i++)
			if (z[best] <= z[i])
				best = i;
		sorted.push_back(rest[best]);
		rest.remove_at(best);
		z.remove_at(best);
	}
	list = sorted;
}

bool Logic::walkmapObject(uint i, ObjectData &out) {
	if (i >= _walkmapList.size())
		return false;
	Obj *o = objById(_walkmapList[i]);
	ObjState *s = o ? currentState(*o) : nullptr;
	if (!s || s->currentAnim < 0)
		return false;
	const Anim &a = _db.anims[s->currentAnim];
	out.code = o->id * 100 + s->state;
	out.picture = a.picture;
	out.icon = s->icon;
	out.x = a.x;
	out.y = a.y;
	out.pickable = s->pickable != 0;
	out.text = s->text;
	return true;
}

bool Logic::cuaObject(uint i, ObjectData &out) {
	if (i >= _cuaList.size())
		return false;
	Obj *o = objById(_cuaList[i]);
	ObjState *s = o ? currentState(*o) : nullptr;
	if (!s)
		return false;
	const int base = findAnim(s->cuaAnim);
	if (!s->cuaAnim || base < 0)
		return false;
	out.code = o->id * 100 + s->state;
	out.picture = s->currentAnim >= 0 ? (int)_db.anims[s->currentAnim].picture : -1;
	out.icon = s->icon;
	out.x = _db.anims[base].x;
	out.y = _db.anims[base].y;
	out.pickable = s->pickable != 0;
	out.text = s->text;
	return true;
}

Common::Rect Logic::radarRect() const {
	const Walkmap *w = _walkmap;
	if (_cua) {
		for (const Walkmap &m : _db.walkmaps)
			for (const Cua &c : m.cuas)
				if (&c == _cua)
					w = &m;
	}
	if (!w)
		return Common::Rect();
	return Common::Rect(w->radar[0], w->radar[1], w->radar[2], w->radar[3]);
}

// ---------------------------------------------------------------------------
// The tick (logic.md "The tick")

bool Logic::stepAnim(uint32 objId, uint32 dt, uint32 &endEvent) {
	Obj *o = objById(objId);
	ObjState *s = o ? currentState(*o) : nullptr;
	if (!s || s->currentAnim < 0)
		return false;
	Anim &a = _db.anims[s->currentAnim];
	if (!a.duration)
		return false;
	const uint32 t = a.time + dt;
	if (t > a.duration) {
		endEvent = a.endEvent;
		a.time = t % a.duration;
		const int next = findAnim(a.next);
		if (next >= 0)
			s->currentAnim = next;
		_db.anims[s->currentAnim].time = 0;
		return true;
	}
	a.time = t % a.duration;
	return false;
}

void Logic::tick(uint32 now) {
	const uint32 dt = now - _last;
	_last = now;
	const Common::Array<uint32> list = _cua ? _cuaList : _walkmapList;
	Common::Array<uint32> ends(list.size(), 0);
	bool changed = false;
	for (uint i = 0; i < list.size(); i++)
		changed |= stepAnim(list[i], dt, ends[i]);
	if (changed) {
		if (_cua)
			updateCua();
		else
			_listener->refreshWalkmap();
		for (uint32 e : ends)
			if (e)
				doEvent(e);
	}
	pathTick();
}

// ---------------------------------------------------------------------------
// Moving between places (logic.md "Moving between places")

void Logic::gotoWalkmap(uint32 id, int x, int y, int direction) {
	pathStop();
	_walkmap = nullptr;
	for (Walkmap &w : _db.walkmaps)
		if (w.id == id) {
			_walkmap = &w;
			break;
		}
	if (!_walkmap) {
		warning("Gilbert: no walkmap %d", id);
		return;
	}
	buildWalkmapObjects();
	_listener->gotoWalkmap(id, x, y, direction);
	_listener->refreshWalkmap();
}

void Logic::gotoCua(uint32 id) {
	pathStop();
	_cua = findCua(id);
	if (!_cua) {
		warning("Gilbert: no CUA %d", id);
		return;
	}
	buildCuaObjects(true);
	_listener->gotoCua(id);
	_listener->refreshCua();
	if (_cua->firstVisit) {
		Cua *c = _cua;
		doEvent(c->firstEvent);
		c->firstVisit = 0;
	} else {
		doEvent(_cua->event);
	}
}

void Logic::cuaEnd() {
	if (!_cua)
		return;
	const uint32 end = _cua->endEvent;
	_cua = nullptr;
	_cuaList.clear();
	if (_walkmap) {
		buildWalkmapObjects();
		_listener->refreshWalkmap();
	}
	if (end)
		doEvent(end);
}

void Logic::walkmapAreaHit(int n) {
	if (_walkmap)
		doEvent(_walkmap->id * 100 + n % 100);
}

// ---------------------------------------------------------------------------
// Objects and the inventory (logic.md "Objects and the inventory")

void Logic::updateCua() {
	_listener->refreshCua();
}

void Logic::updateInventory() {
	_shownInventory.clear();
	for (const Obj &o : _db.inventory)
		if (o.visible)
			_shownInventory.push_back(o.id);
	_listener->refreshInventory();
}

void Logic::clickObjectInCua(uint32 code) {
	Obj *o = objById(code / 100);
	ObjState *s = o ? currentState(*o) : nullptr;
	if (s && !s->pickable)
		doEvent(s->clickEvent);
}

void Logic::objectToInventory(uint32 code) {
	ObjRef r = findObj(code / 100);
	if (!r.obj || !r.cua)
		return;
	const Obj o = *r.obj;
	r.cua->objs.remove_at(r.index);
	_db.inventory.push_back(o);
	Obj &moved = _db.inventory.back();
	ObjState *s = currentState(moved);
	if (s)
		doEvent(s->takeEvent);
	buildCuaObjects(false);
	updateCua();
	updateInventory();
}

void Logic::useObjectOnObject(uint32 code, uint32 target) {
	for (const UseObj &u : _db.useObjs)
		if (u.obj == code && u.target == target) {
			doEvent(u.event);
			buildCuaObjects(false);
			updateCua();
			updateInventory();
			return;
		}
	debugC(1, kDebugScript, "UseObjectOnObject: can't use %d on %d", code, target);
}

bool Logic::inventoryObject(uint i, uint32 &code, uint32 &icon, Common::String &text) {
	if (i >= _shownInventory.size())
		return false;
	Obj *o = objById(_shownInventory[i]);
	ObjState *s = o ? currentState(*o) : nullptr;
	if (!s)
		return false;
	code = o->id * 100 + s->state;
	icon = s->icon;
	text = s->text;
	return true;
}

// ---------------------------------------------------------------------------
// Events (logic.md "Events")

void Logic::doEvent(uint32 id) {
	int guard = 0;
	while (id && guard++ < 10000) {
		uint32 jump = 0;
		for (const Event &e : _db.events) {
			if (e.id != id)
				continue;
			debugC(2, kDebugScript, "Event %d type %d", e.id, e.type);
			jump = runEvent(e);
			if (jump)
				break;
		}
		id = jump;
	}
}

uint32 Logic::runEvent(const Event &e) {
	const uint32 id = e.obj / 100, st = e.obj % 100;
	switch (e.type) {
	case 1: {
		ObjRef r = findObj(id);
		if (!r.obj)
			break;
		if (!r.cua) {
			_db.inventory.remove_at(r.index);
			updateInventory();
		} else {
			r.cua->objs.remove_at(r.index);
			buildCuaObjects(false);
			updateCua();
		}
		break;
	}
	case 2: {
		Obj *o = objById(id);
		if (!o)
			break;
		setState(*o, st);
		if (ObjState *s = currentState(*o))
			resetStateAnim(*s);
		buildCuaObjects(false);
		updateCua();
		break;
	}
	case 3:
		if (e.walkmap && e.cua)
			gotoCua(e.cua);
		break;
	case 4:
		if (_cua)
			cuaEnd();
		gotoWalkmap(e.walkmap, e.x, e.y, e.unk58);
		break;
	case 5:
	case 21: {
		Topic *t = findTopic(e.book, e.topic);
		if (!t)
			break;
		t->shown = e.type == 5 ? 1 : 0;
		renumberBooks();
		_listener->newTopic(e.type == 5);
		break;
	}
	case 6:
		if (e.soundName.empty())
			_listener->playWave(e.sound38, e.sound3c, e.unk48 != 0);
		else
			_listener->playStream(e.soundName, e.unk48 != 0, e.sound44);
		break;
	case 7: {
		ObjRef r = findObj(id);
		if (!r.obj || !r.cua)
			break;
		const Obj o = *r.obj;
		r.cua->objs.remove_at(r.index);
		_db.inventory.push_back(o);
		Obj &moved = _db.inventory.back();
		setState(moved, st);
		moved.visible = 1;
		updateInventory();
		buildCuaObjects(false);
		updateCua();
		break;
	}
	case 8: {
		ObjRef r = findObj(id);
		if (r.obj && !r.cua) {
			_db.inventory.remove_at(r.index);
			updateInventory();
		}
		break;
	}
	case 9:
		startDialog(e.dialog);
		break;
	case 10:
		if (!e.video.empty())
			_listener->startFilm(e.video);
		break;
	case 12:
	case 13: {
		Obj *o = objById(id);
		if (o && currentState(*o))
			for (ObjState &s : o->states)
				if (s.state == st) {
					s.pickable = e.type == 12 ? 1 : 0;
					break;
				}
		break;
	}
	case 14: {
		Obj *o = objById(id);
		ObjState *s = o ? currentState(*o) : nullptr;
		if (!s)
			break;
		setState(*o, s->state + 1);
		if (ObjState *n = currentState(*o))
			resetStateAnim(*n);
		buildCuaObjects(false);
		updateCua();
		break;
	}
	case 15:
	case 16: {
		Obj *o = objById(id);
		if (!o)
			break;
		if (ObjState *s = currentState(*o)) {
			const int a = findAnim(s->cuaAnim);
			if (a < 0)
				break;
			s->currentAnim = a;
			_db.anims[a].time = 0;
		}
		o->visible = e.type == 15 ? 1 : 0;
		if (e.type == 15)
			if (ObjState *s = currentState(*o))
				resetStateAnim(*s);
		buildCuaObjects(false);
		updateCua();
		updateInventory();
		break;
	}
	case 17:
		if (e.soundName.empty())
			_listener->stopWave(e.sound38, e.sound3c);
		break;
	case 18: {
		const int32 v = variable(e.var);
		bool jump;
		switch (e.cond) {
		case 0:
			jump = v == e.value;
			break;
		case 1:
			jump = v != e.value;
			break;
		case 2:
			jump = v < e.value;
			break;
		case 3:
			jump = v > e.value;
			break;
		case 4:
			jump = true;
			break;
		case 5:
			jump = (int32)_random.getRandomNumber(100) <= e.value;
			break;
		default:
			jump = false;
			break;
		}
		return jump ? e.jump : 0;
	}
	case 19:
		if (e.var < Database::kVariables)
			_db.vars[e.var] = e.value;
		break;
	case 20:
		if (e.var < Database::kVariables)
			_db.vars[e.var] = variable(e.var) + e.value;
		break;
	case 22: {
		Topic *t = findTopic(e.book, e.topic);
		Topic *t2 = findTopic(e.book, e.topic2);
		if (!t || !t2 || (t2->title.empty() && t2->text.empty()))
			break;
		t->title += t2->title;
		t->text += t2->text;
		t2->title.clear();
		t2->text.clear();
		renumberBooks();
		t->shown = 1;
		_listener->newTopic(true);
		break;
	}
	default:
		break;
	}
	return 0;
}

// ---------------------------------------------------------------------------
// Dialogues (logic.md "Dialogues")

void Logic::startDialog(uint32 id) {
	pathStop();
	_dialog = nullptr;
	for (Dialog &d : _db.dialogs)
		if (d.id == id) {
			_dialog = &d;
			break;
		}
	_listener->showDialog();
}

Common::String Logic::dialogTitle() const {
	return _dialog ? _dialog->title : "*NO DIALOG*";
}

Common::String Logic::dialogText() const {
	return _dialog ? _dialog->text : "*NO DIALOG*";
}

uint Logic::dialogChoiceCount() const {
	return _dialog ? _dialog->choices.size() : 0;
}

Common::String Logic::dialogChoice(uint i) const {
	return _dialog && i < _dialog->choices.size() ? _dialog->choices[i].text : "*NO CHOICE*";
}

void Logic::dialogEnd(uint i) {
	const Dialog *d = _dialog;
	_dialog = nullptr;
	if (d && i < d->choices.size())
		doEvent(d->choices[i].event);
}

// ---------------------------------------------------------------------------
// Books (logic.md "Books")

void Logic::renumberBooks() {
	for (int b = 0; b < Database::kBooks; b++) {
		int rank = 0;
		for (Topic &t : _db.books[b])
			t.index = t.shown ? rank++ : -1;
	}
}

Topic *Logic::findTopic(uint book, uint32 id) {
	if (book >= Database::kBooks)
		return nullptr;
	for (Topic &t : _db.books[book])
		if (t.id == id)
			return &t;
	return nullptr;
}

const Topic *Logic::topicByRank(uint book, int rank) const {
	if (book >= Database::kBooks)
		return nullptr;
	for (const Topic &t : _db.books[book])
		if (t.index == rank)
			return &t;
	return nullptr;
}

uint Logic::bookTopicCount(uint book) const {
	uint n = 0;
	if (book < Database::kBooks)
		for (const Topic &t : _db.books[book])
			n += t.shown ? 1 : 0;
	return n;
}

Common::String Logic::bookTopicTitle(uint book, int rank) const {
	const Topic *t = topicByRank(book, rank);
	return t ? t->title : "";
}

Common::String Logic::bookTopicText(uint book, int rank) const {
	const Topic *t = topicByRank(book, rank);
	return t ? t->text : "";
}

uint32 Logic::bookTopicFromIndex(uint book, int rank) const {
	const Topic *t = topicByRank(book, rank);
	return t ? t->id : 0;
}

int Logic::bookIndexFromTopic(uint book, uint32 id) const {
	if (book < Database::kBooks)
		for (const Topic &t : _db.books[book])
			if (t.id == id)
				return t.index;
	return 0;
}

int BookParser::first(const Common::String &s) {
	text = s;
	pos = 0;
	return next();
}

// GEBookParseNext (logic.md "Parser").
int BookParser::next() {
	auto digits = [this]() {
		const char *p = text.c_str() + pos;
		const long v = strtol(p, nullptr, 10);
		while (pos < text.size() && Common::isDigit(text[pos]))
			pos++;
		return (int)v;
	};
	auto skipSpace = [this]() {
		if (pos < text.size() && text[pos] == ' ')
			pos++;
	};
	for (;;) {
		if (pos >= text.size())
			return kTokenEnd;
		const byte c = text[pos];
		if (c == '\\' && pos + 1 < text.size()) {
			const char k = text[pos + 1];
			if (k == 'f') {
				pos += 2;
				format = digits();
				skipSpace();
				return kTokenFormat;
			}
			if (k == 'g') {
				pos += 2;
				picture = digits();
				skipSpace();
				return kTokenPicture;
			}
			if (k == 'h') {
				pos += 2;
				if (pos < text.size() && Common::isDigit(text[pos])) {
					linkBook = digits();
					if (pos < text.size() && text[pos] == ':') {
						pos++;
						linkTopic = digits();
					}
					skipSpace();
					return kTokenLink;
				}
				skipSpace();
				return kTokenLinkEnd;
			}
			if (k == 't') {
				pos += 2;
				return kTokenTab;
			}
			token = "\\";
			pos++;
			return kTokenText;
		}
		if (c == '\\') { // a backslash at the very end
			token = "\\";
			pos++;
			return kTokenText;
		}
		if (c == '\n') {
			token = "\n";
			pos++;
			return kTokenLineFeed;
		}
		if (c == ' ') {
			token = " ";
			pos++;
			return kTokenText;
		}
		if (c < 0x20) {
			pos++; // another control character: skipped, then a run
		}
		token.clear();
		while (pos < text.size() && text[pos] != '\\' && text[pos] != ' ' && (byte)text[pos] >= 0x20)
			token += text[pos++];
		return kTokenText;
	}
}

// ---------------------------------------------------------------------------
// The path finder (logic.md "Path finding")

bool Logic::walkable(Common::Point p, int targetValue) const {
	const int v = _grid[p.y * _mapW + p.x];
	if (v == 0)
		return true;
	if (v == 1)
		return false;
	return v == targetValue;
}

// Cost of a step in direction d, by its angle to the straight line to the target.
static int stepCost(Common::Point n, Common::Point target, int d) {
	const int dx = target.x - n.x, ndy = n.y - target.y;
	int bearing;
	if (dx != 0)
		bearing = (int)(atan((double)ndy / dx) * 57.29577951308232);
	else
		bearing = ndy > 0 ? 90 : (ndy < 0 ? -90 : 0);
	if (dx < 0)
		bearing += 180;
	int a = (bearing + 45 * (16 - d)) % 360;
	if (a >= 180)
		a = 359 - a;
	double cost = (a + 22.5) / 45 + 1;
	if (d & 1)
		cost *= 1.41;
	return (int)cost;
}

void Logic::search(const Common::Rect &rect, Common::Point start, Common::Point target) {
	for (Node &n : _nodes)
		n = Node{ -1, -1, 0x7FFFFFFF };
	const int targetValue = _grid[target.y * _mapW + target.x];
	const int s = start.y * _mapW + start.x, t = target.y * _mapW + target.x;
	_nodes[s].cost = 0;
	int tail = s, n = s;
	while (n >= 0) {
		if (n != t) {
			const Common::Point p(n % _mapW, n / _mapW);
			for (int d = 0; d < 8; d++) {
				const Common::Point m(p.x + kDirX[d], p.y + kDirY[d]);
				if (!rect.contains(m) || !walkable(m, targetValue))
					continue;
				const int mi = m.y * _mapW + m.x;
				const int c = _nodes[n].cost + stepCost(p, target, d);
				if (c <= _nodes[mi].cost) {
					_nodes[mi].cost = c;
					_nodes[mi].parent = n;
					if (_nodes[mi].next < 0 && mi != tail) {
						_nodes[tail].next = mi;
						tail = mi;
					}
				}
			}
		}
		const int nxt = _nodes[n].next;
		_nodes[n].next = -1;
		n = nxt;
	}
}

bool Logic::findPath(Common::Point start, Common::Point target) {
	Common::Rect rect(MIN(start.x, target.x) - 3, MIN(start.y, target.y) - 3,
	                  MAX(start.x, target.x) + 4, MAX(start.y, target.y) + 4);
	rect.clip(Common::Rect(_mapW, _mapH));
	_nodes.resize(_mapW * _mapH);
	const int t = target.y * _mapW + target.x, s = start.y * _mapW + start.x;
	search(rect, start, target);
	if (_nodes[t].parent < 0)
		search(Common::Rect(_mapW, _mapH), start, target);
	if (_nodes[t].parent < 0 || s == t)
		return false;
	Common::Array<int> chain;
	for (int n = t; n >= 0 && (int)chain.size() <= _mapW * _mapH; n = _nodes[n].parent) {
		chain.insert_at(0, n);
		if (n == s)
			break;
	}
	_path.clear();
	_dirs.clear();
	for (uint i = 0; i + 1 < chain.size(); i++) {
		const Common::Point a(chain[i] % _mapW, chain[i] / _mapW), b(chain[i + 1] % _mapW, chain[i + 1] / _mapW);
		int dir = 0;
		for (int d = 0; d < 8; d++)
			if (a.x + kDirX[d] == b.x && a.y + kDirY[d] == b.y)
				dir = d;
		_path.push_back(a);
		_dirs.push_back(dir);
	}
	return true;
}

bool Logic::pathNewPath(int x, int y) {
	_mapW = _listener->mapWidth();
	_mapH = _listener->mapHeight();
	if (_mapW <= 0 || _mapH <= 0 || _mapW > 100 || _mapH > 80)
		return false;
	_grid.resize(_mapW * _mapH);
	for (int cy = 0; cy < _mapH; cy++)
		for (int cx = 0; cx < _mapW; cx++)
			_grid[cy * _mapW + cx] = _listener->mapCell(cx, cy);
	const Common::Point g = _listener->gilbertPosition();
	const Common::Point start(g.x / kCell, g.y / kCell), target(x / kCell, y / kCell);
	const Common::Rect grid(_mapW, _mapH);
	if (!grid.contains(start) || !grid.contains(target) || !findPath(start, target)) {
		pathStop();
		return false;
	}
	pathStart();
	return true;
}

void Logic::pathStart() {
	_listener->walk(kDirCode[_dirs[0]]);
	_pathTarget = _path[_path.size() > 1 ? 1 : 0];
	_pathK = 1;
	_pathStopped = false;
	_pathDrift = false;
	_pathDist = 0;
	_pathLast = _listener->gilbertPosition();
}

void Logic::pathStop() {
	_listener->walk(-1);
	_pathStopped = true;
}

void Logic::pathTick() {
	if (_pathStopped || _path.empty())
		return;
	if (_pathK >= _path.size()) {
		pathStop();
		return;
	}
	const Common::Point pos = _listener->gilbertPosition();
	const Common::Point cell(pos.x / kCell, pos.y / kCell);
	if (cell == _pathTarget) {
		_listener->walk(kDirCode[_dirs[_pathK]]);
		_pathK++;
		_pathTarget = _path[MIN<uint>(_pathK, _path.size() - 1)];
		_pathDrift = false;
		_pathDist = 0;
		return;
	}
	const int ddx = _pathLast.x - pos.x, ddy = _pathLast.y - pos.y;
	_pathDist += (int)sqrt((double)(ddx * ddx + ddy * ddy));
	_pathLast = pos;
	if (_pathDist * _pathDist > kCell * kCell + 2 * kCell * kCell)
		_pathDrift = true;
	else if (!_pathDrift)
		return;
	int code;
	if (cell.x < _pathTarget.x)
		code = 8;
	else if (cell.x > _pathTarget.x)
		code = 24;
	else if (cell.y < _pathTarget.y)
		code = 16;
	else
		code = 0;
	_listener->walk(code);
}

} // End of namespace Gilbert
