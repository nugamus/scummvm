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
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/stream.h"

#include "gilbert/database.h"
#include "gilbert/detection.h"

namespace Gilbert {

namespace {

/**
 * MFC 6 CArchive reading (formats README, "MFC primitives"): counts, CStrings, and objects
 * behind class tags. Classes and objects share one numbering from 1; the database holds no
 * references to objects already loaded.
 */
class ArchiveReader {
public:
	explicit ArchiveReader(Common::SeekableReadStream &in) : _in(in) {
		_loaded.push_back(""); // 0 = NULL
	}

	bool ok() const { return _ok && !_in.err(); }
	uint32 u32() { return _in.readUint32LE(); }
	int32 s32() { return _in.readSint32LE(); }

	uint32 count() {
		const uint16 n = _in.readUint16LE();
		return n == 0xFFFF ? _in.readUint32LE() : n;
	}

	Common::String string() {
		uint32 n = _in.readByte();
		if (n == 0xFF) {
			n = _in.readUint16LE();
			if (n == 0xFFFE) { // Unicode: not in the corpus
				_ok = false;
				return Common::String();
			}
			if (n == 0xFFFF)
				n = _in.readUint32LE();
		}
		if (n > (uint32)(_in.size() - _in.pos())) {
			_ok = false;
			return Common::String();
		}
		return _in.readString(0, n);
	}

	/** An object's tag; true when it is an object of `cls` whose body follows. */
	bool object(const char *cls) {
		const uint16 tag = _in.readUint16LE();
		uint32 big = tag == 0x7FFF ? _in.readUint32LE() : ((uint32)(tag & 0x8000) << 16) | (tag & 0x7FFF);
		Common::String name;
		if (tag == 0xFFFF) {
			const uint16 schema = _in.readUint16LE();
			const uint16 len = _in.readUint16LE();
			name = _in.readString(0, len);
			if (schema != 1)
				_ok = false;
			_loaded.push_back(name);
		} else if (big & 0x80000000) {
			big &= 0x7FFFFFFF;
			if (big == 0 || big >= _loaded.size() || _loaded[big].empty()) {
				_ok = false;
				return false;
			}
			name = _loaded[big];
		} else {
			// A reference to an object already loaded, or NULL: none in the database.
			_ok = false;
			return false;
		}
		_loaded.push_back(""); // the object's own number
		if (name != cls) {
			warning("Gilbert: database: %s where %s belongs", name.c_str(), cls);
			_ok = false;
			return false;
		}
		return true;
	}

private:
	Common::SeekableReadStream &_in;
	Common::Array<Common::String> _loaded;
	bool _ok = true;
};

class ArchiveWriter {
public:
	explicit ArchiveWriter(Common::WriteStream &out) : _out(out) {}

	void u32(uint32 v) { _out.writeUint32LE(v); }
	void s32(int32 v) { _out.writeSint32LE(v); }

	void count(uint32 n) {
		if (n < 0xFFFF) {
			_out.writeUint16LE(n);
		} else {
			_out.writeUint16LE(0xFFFF);
			_out.writeUint32LE(n);
		}
	}

	void string(const Common::String &s) {
		const uint32 n = s.size();
		if (n < 0xFF) {
			_out.writeByte(n);
		} else if (n < 0xFFFE) {
			_out.writeByte(0xFF);
			_out.writeUint16LE(n);
		} else {
			_out.writeByte(0xFF);
			_out.writeUint16LE(0xFFFF);
			_out.writeUint32LE(n);
		}
		_out.write(s.c_str(), n);
	}

	void object(const char *cls) {
		if (!_classes.contains(cls)) {
			_out.writeUint16LE(0xFFFF);
			_out.writeUint16LE(1); // schema
			_out.writeUint16LE(strlen(cls));
			_out.write(cls, strlen(cls));
			_classes[cls] = _next++;
		} else {
			const uint32 index = _classes[cls];
			if (index < 0x7FFF) {
				_out.writeUint16LE(0x8000 | index);
			} else {
				_out.writeUint16LE(0x7FFF);
				_out.writeUint32LE(0x80000000 | index);
			}
		}
		_next++; // the object's own number
	}

private:
	Common::WriteStream &_out;
	Common::HashMap<Common::String, uint32> _classes;
	uint32 _next = 1;
};

// One Serialize per class, in archive order (formats README, "Classes").

bool read(ArchiveReader &r, ObjState &s) {
	s.state = r.u32();
	s.name = r.string();
	s.walkmapAnim = r.u32();
	s.cuaAnim = r.u32();
	s.clickEvent = r.u32();
	s.takeEvent = r.u32();
	s.icon = r.u32();
	s.pickable = r.u32();
	s.text = r.string();
	return true;
}

void write(ArchiveWriter &w, const ObjState &s) {
	w.u32(s.state);
	w.string(s.name);
	w.u32(s.walkmapAnim);
	w.u32(s.cuaAnim);
	w.u32(s.clickEvent);
	w.u32(s.takeEvent);
	w.u32(s.icon);
	w.u32(s.pickable);
	w.string(s.text);
}

template<class T>
bool readList(ArchiveReader &r, Common::Array<T> &list, const char *cls);
template<class T>
void writeList(ArchiveWriter &w, const Common::Array<T> &list, const char *cls);

bool read(ArchiveReader &r, Obj &o) {
	if (!readList(r, o.states, "CObjState"))
		return false;
	o.cuaId = r.u32();
	o.id = r.u32();
	o.visible = r.u32();
	o.state = r.u32();
	return true;
}

void write(ArchiveWriter &w, const Obj &o) {
	writeList(w, o.states, "CObjState");
	w.u32(o.cuaId);
	w.u32(o.id);
	w.u32(o.visible);
	w.u32(o.state);
}

bool read(ArchiveReader &r, Cua &c) {
	if (!readList(r, c.objs, "CObj"))
		return false;
	c.id = r.u32();
	c.name = r.string();
	c.firstEvent = r.u32();
	c.event = r.u32();
	c.endEvent = r.u32();
	c.firstVisit = r.u32();
	return true;
}

void write(ArchiveWriter &w, const Cua &c) {
	writeList(w, c.objs, "CObj");
	w.u32(c.id);
	w.string(c.name);
	w.u32(c.firstEvent);
	w.u32(c.event);
	w.u32(c.endEvent);
	w.u32(c.firstVisit);
}

bool read(ArchiveReader &r, Walkmap &m) {
	if (!readList(r, m.cuas, "CCUA"))
		return false;
	m.id = r.u32();
	m.title = r.string();
	for (int i = 0; i < 4; i++)
		m.radar[i] = r.s32();
	return true;
}

void write(ArchiveWriter &w, const Walkmap &m) {
	writeList(w, m.cuas, "CCUA");
	w.u32(m.id);
	w.string(m.title);
	for (int i = 0; i < 4; i++)
		w.s32(m.radar[i]);
}

bool read(ArchiveReader &r, Anim &a) {
	a.time = r.u32();
	a.id = r.u32();
	a.next = r.u32();
	a.name = r.string();
	a.duration = r.u32();
	a.unk18 = r.u32();
	a.x = r.u32();
	a.y = r.u32();
	a.z = r.u32();
	a.picture = r.u32();
	a.endEvent = r.u32();
	return true;
}

void write(ArchiveWriter &w, const Anim &a) {
	w.u32(a.time);
	w.u32(a.id);
	w.u32(a.next);
	w.string(a.name);
	w.u32(a.duration);
	w.u32(a.unk18);
	w.u32(a.x);
	w.u32(a.y);
	w.u32(a.z);
	w.u32(a.picture);
	w.u32(a.endEvent);
}

bool read(ArchiveReader &r, UseObj &u) {
	u.obj = r.u32();
	u.target = r.u32();
	u.event = r.u32();
	return true;
}

void write(ArchiveWriter &w, const UseObj &u) {
	w.u32(u.obj);
	w.u32(u.target);
	w.u32(u.event);
}

bool read(ArchiveReader &r, Event &e) {
	e.id = r.u32();
	e.type = r.u32();
	e.cond = r.u32();
	e.var = r.u32();
	e.value = r.s32();
	e.jump = r.u32();
	e.walkmap = r.u32();
	e.cua = r.u32();
	e.obj = r.u32();
	e.book = r.u32();
	e.topic = r.u32();
	e.topic2 = r.u32();
	e.dialog = r.u32();
	e.sound38 = r.u32();
	e.sound3c = r.u32();
	e.soundName = r.string();
	e.sound44 = r.u32();
	e.unk48 = r.u32();
	e.video = r.string();
	e.x = r.u32();
	e.y = r.u32();
	e.unk58 = r.u32();
	e.comment = r.string();
	return true;
}

void write(ArchiveWriter &w, const Event &e) {
	w.u32(e.id);
	w.u32(e.type);
	w.u32(e.cond);
	w.u32(e.var);
	w.s32(e.value);
	w.u32(e.jump);
	w.u32(e.walkmap);
	w.u32(e.cua);
	w.u32(e.obj);
	w.u32(e.book);
	w.u32(e.topic);
	w.u32(e.topic2);
	w.u32(e.dialog);
	w.u32(e.sound38);
	w.u32(e.sound3c);
	w.string(e.soundName);
	w.u32(e.sound44);
	w.u32(e.unk48);
	w.string(e.video);
	w.u32(e.x);
	w.u32(e.y);
	w.u32(e.unk58);
	w.string(e.comment);
}

bool read(ArchiveReader &r, DialogChoice &c) {
	c.unk04 = r.u32();
	c.text = r.string();
	c.event = r.u32();
	return true;
}

void write(ArchiveWriter &w, const DialogChoice &c) {
	w.u32(c.unk04);
	w.string(c.text);
	w.u32(c.event);
}

bool read(ArchiveReader &r, Dialog &d) {
	if (!readList(r, d.choices, "CDialogChoice"))
		return false;
	d.id = r.u32();
	d.title = r.string();
	d.text = r.string();
	return true;
}

void write(ArchiveWriter &w, const Dialog &d) {
	writeList(w, d.choices, "CDialogChoice");
	w.u32(d.id);
	w.string(d.title);
	w.string(d.text);
}

bool read(ArchiveReader &r, Topic &t) {
	t.id = r.u32();
	t.title = r.string();
	t.text = r.string();
	t.shown = r.u32();
	t.index = r.s32();
	return true;
}

void write(ArchiveWriter &w, const Topic &t) {
	w.u32(t.id);
	w.string(t.title);
	w.string(t.text);
	w.u32(t.shown);
	w.s32(t.index);
}

bool read(ArchiveReader &r, Text &t) {
	t.id = r.u32();
	t.text = r.string();
	return true;
}

void write(ArchiveWriter &w, const Text &t) {
	w.u32(t.id);
	w.string(t.text);
}

// CObList::Serialize: a count, then one object per element.
template<class T>
bool readList(ArchiveReader &r, Common::Array<T> &list, const char *cls) {
	list.clear();
	const uint32 n = r.count();
	for (uint32 i = 0; i < n && r.ok(); i++) {
		if (!r.object(cls))
			return false;
		T item;
		if (!read(r, item))
			return false;
		list.push_back(item);
	}
	return r.ok();
}

template<class T>
void writeList(ArchiveWriter &w, const Common::Array<T> &list, const char *cls) {
	w.count(list.size());
	for (const T &item : list) {
		w.object(cls);
		write(w, item);
	}
}

} // End of anonymous namespace

Database::Database() {
	clear();
}

void Database::clear() {
	walkmap = startX = startY = unk10 = 0;
	for (int i = 0; i < kVariables; i++)
		vars[i] = 0;
	walkmaps.clear();
	inventory.clear();
	useObjs.clear();
	events.clear();
	dialogs.clear();
	texts.clear();
	for (int i = 0; i < kBooks; i++)
		books[i].clear();
	anims.clear();
}

// The header, then the lists (formats README, "default.dat").
bool Database::load(Common::SeekableReadStream &in) {
	clear();
	ArchiveReader r(in);
	walkmap = r.u32();
	startX = r.u32();
	startY = r.u32();
	unk10 = r.u32();
	if (r.u32() != kVariables)
		return false;
	for (int i = 0; i < kVariables; i++)
		vars[i] = r.s32();
	walkmap = r.u32();
	if (!readList(r, walkmaps, "CWalkmap") || !readList(r, inventory, "CObj") ||
	    !readList(r, useObjs, "CUseObj") || !readList(r, events, "CEvent") ||
	    !readList(r, dialogs, "CDialogs") || !readList(r, texts, "CText"))
		return false;
	if (r.u32() != kBooks)
		return false;
	for (int i = 0; i < kBooks; i++)
		if (!readList(r, books[i], "CTopic"))
			return false;
	if (!readList(r, anims, "CAnim"))
		return false;
	debugC(1, kDebugLoad, "Database: %d walkmaps, %d events, %d dialogs, %d anims, %d left over",
	       walkmaps.size(), events.size(), dialogs.size(), anims.size(), (int)(in.size() - in.pos()));
	return r.ok();
}

void Database::save(Common::WriteStream &out) const {
	ArchiveWriter w(out);
	w.u32(walkmap);
	w.u32(startX);
	w.u32(startY);
	w.u32(unk10);
	w.u32(kVariables);
	for (int i = 0; i < kVariables; i++)
		w.s32(vars[i]);
	w.u32(walkmap);
	writeList(w, walkmaps, "CWalkmap");
	writeList(w, inventory, "CObj");
	writeList(w, useObjs, "CUseObj");
	writeList(w, events, "CEvent");
	writeList(w, dialogs, "CDialogs");
	writeList(w, texts, "CText");
	w.u32(kBooks);
	for (int i = 0; i < kBooks; i++)
		writeList(w, books[i], "CTopic");
	writeList(w, anims, "CAnim");
}

} // End of namespace Gilbert
