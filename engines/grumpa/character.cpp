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
#include "common/file.h"

#include "grumpa/character.h"

namespace Grumpa {

namespace {

// A bounds-checked reader over a whole .abi file; any over-read or implausible count marks
// it bad.
struct Reader {
	Common::Array<byte> buf;
	uint32 pos = 0;
	bool ok = true;

	uint32 left() const { return pos <= buf.size() ? buf.size() - pos : 0; }
	void skip(uint32 n) {
		if (!ok || n > left()) { ok = false; return; }
		pos += n;
	}
	uint32 u32() {
		if (!ok || left() < 4) { ok = false; return 0; }
		uint32 v = READ_LE_UINT32(&buf[pos]);
		pos += 4;
		return v;
	}
	float f32() {
		uint32 v = u32();
		float f;
		memcpy(&f, &v, 4);
		return f;
	}
	uint32 count() {
		uint32 n = u32();
		if (n > 0x100000) ok = false;
		return ok ? n : 0;
	}
	Common::String str() {           // pascal string, NUL-terminated inside
		uint32 n = count();
		if (!ok || n > left()) { ok = false; return Common::String(); }
		Common::String s((const char *)&buf[pos], n);
		pos += n;
		uint p = s.findFirstOf('\0');
		if (p != Common::String::npos)
			s = Common::String(s.c_str(), p);
		return s;
	}
	void names(Common::Array<Common::String> &out) {
		uint32 n = count();
		for (uint32 i = 0; i < n && ok; i++)
			out.push_back(str());
	}
	void ccVec() {                   // CC = 5 u32 + u32 m + m * EC (5 u32)
		uint32 n = count();
		for (uint32 i = 0; i < n && ok; i++) {
			skip(20);
			skip(20 * count());
		}
	}
};

} // anonymous namespace

// The CFXCharacter record (docs/formats/README.md, E-0401): the header, the placement, the
// rule / command / message / reaction lists (kept for later, skipped here), the name lists,
// the carried objects, the kind and the pair list.
bool Characters::load() {
	Common::File f;
	if (!f.open(Common::Path("Actors/Characters.abi"))) {
		warning("Grumpa: Actors/Characters.abi not found");
		return false;
	}
	Reader r;
	r.buf.resize(f.size());
	f.read(r.buf.begin(), r.buf.size());
	_chars.clear();
	while (r.left() >= 8 && r.ok) {
		if (r.u32() != 0x03) {
			r.ok = false;
			break;
		}
		Character c;
		c.id = r.u32();
		c.active = r.u32() != 0;
		c.visible = r.u32() != 0;
		r.skip(4 * r.count());       // the six stats (Q-0402)
		c.home = (int32)r.u32();
		c.pos.x = r.f32(); c.pos.y = r.f32(); c.pos.z = r.f32();
		r.skip(4);
		c.yaw = r.f32();
		r.skip(4);
		r.skip(20);                  // [0x28c], [0x5fc], [0x290], [0x48c], [0x490]
		uint32 n = r.count();        // rules: EC vector + CC vector each
		for (uint32 i = 0; i < n && r.ok; i++) {
			r.skip(20 * r.count());
			r.ccVec();
		}
		r.ccVec();
		r.ccVec();
		n = r.count();               // messages: text + CC vector (none in the data)
		for (uint32 i = 0; i < n && r.ok; i++) {
			r.str();
			r.ccVec();
		}
		r.ccVec();
		n = r.count();               // reactions: character id + CC vector
		for (uint32 i = 0; i < n && r.ok; i++) {
			r.skip(4);
			r.ccVec();
		}
		r.skip(4);                   // [0x434]
		r.names(c.anims);
		r.names(c.sounds);
		c.texture = (int)r.u32();
		r.names(c.textures);
		n = r.count();
		for (uint32 i = 0; i < n && r.ok; i++) {
			Character::Attachment a;
			a.anb = r.str();
			a.tga = r.str();
			r.skip(12);
			c.attachments.push_back(a);
		}
		c.kind = (int)r.u32();
		c.parts[0] = (int)r.u32();
		c.parts[1] = (int)r.u32();
		n = r.count();
		for (uint32 i = 0; i < 2 * n && r.ok; i++)
			c.pairs.push_back(r.u32());
		if (r.ok)
			_chars.push_back(c);
	}
	debug(1, "Grumpa: %u characters loaded%s", (uint)_chars.size(), r.ok ? "" : " (read error)");
	return r.ok;
}

Character *Characters::find(int id) {
	for (uint i = 0; i < _chars.size(); i++)
		if ((int)_chars[i].id == id)
			return &_chars[i];
	return nullptr;
}

bool Characters::command(int id, int op, int arg1, int arg2) {
	if (id == -1) {
		for (uint i = 0; i < _chars.size(); i++)
			apply(_chars[i], op, arg1);
		return true;
	}
	Character *c = find(id);
	if (!c)
		return false;
	apply(*c, op, arg1);
	return true;
}

// CFXCharacter::DoCommand, the opcodes characters.md lists (E-0403). Roles, combat and the
// rule lists are Q-0403.
void Characters::apply(Character &c, int op, int arg1) {
	if (op == 0x34) {
		c.latched = false;
		return;
	}
	if (c.latched)
		return;
	switch (op) {
	case 2:
		c.visible = true;
		c.home = _scene;
		break;
	case 3:
		c.visible = false;
		break;
	case 0xb:
		c.active = true;
		break;
	case 0xc:
		c.active = false;
		break;
	case 0xd:
		c.active = c.visible = false;
		c.home = -1;
		c.latched = true;
		break;
	case 0x17:
		_scene = arg1;
		break;
	case 0x35:
		c.texture = arg1;            // clamped as the original does (count < arg -> count - 1)
		if ((int)c.textures.size() < arg1)
			c.texture = (int)c.textures.size() - 1;
		break;
	case 0x36:
		c.home = arg1;
		break;
	case 0x47: {
		Character *o = find(arg1);
		if (o)
			c.pos = o->pos;
		break;
	}
	default:
		debug(2, "Grumpa: character %u opcode %#x not modelled", c.id, op);
		break;
	}
}

} // End of namespace Grumpa
