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

#include "common/savefile.h"
#include "common/system.h"

#include "peintre/peintre.h"

namespace Peintre {

// The original's SAVE\ files, kept byte for byte in ScummVM's save folder (save.md):
//   USERS.BIN        -> <target>.users
//   GGAME<p>.BIN     -> <target>.ggame<p>
//   GAME<pp><ss>.BIN -> <target>.game<pp><ss>

void GameState::clear() {
	// save.md "A new player's block": the EXE's initial data at 0x4aba40, whose non-zero
	// words are these (offset, value); e.g. 0x298 keeps the kite out of the church until
	// the garden, 0x1C8 sounds mangeurs' cuckoo on the first visit.
	static const uint32 kInitial[][2] = {
		{ 0x1AC, 0xFF }, { 0x1B8, 0x10001 }, { 0x1C4, 0xFF }, { 0x1C8, 1 },
		{ 0x1D0, 0xFFFFFF }, { 0x1D4, 0xFF }, { 0x1E0, 0xFFFFFF }, { 0x1E4, 0xFF }, { 0x1E8, 1 },
		{ 0x1F0, 0xFFFFFF }, { 0x1F4, 0xFF }, { 0x200, 0xFFFFFF }, { 0x204, 0xFF },
		{ 0x210, 0xFFFFFF }, { 0x214, 0xFF }, { 0x220, 0xFFFFFF }, { 0x230, 0xFF },
		{ 0x23C, 1 }, { 0x240, 0xFF }, { 0x24C, 1 }, { 0x250, 0xFF }, { 0x25C, 1 },
		{ 0x288, 0xFF }, { 0x294, 0x10001 }, { 0x298, 1 }, { 0x320, 1 }
	};
	memset(block3D, 0, sizeof(block3D));
	for (const auto &w : kInitial)
		WRITE_LE_UINT32(block3D + w[0], w[1]);
	memset(zoneDone, 0, sizeof(zoneDone));
	memset(placed, 0, sizeof(placed));
	memset(counters, 0, sizeof(counters));
}

uint32 GameState::held(uint object) const {
	return object < kNumObjects ? READ_LE_UINT32(block3D + 0x40 + 4 * object) : 0;
}

void GameState::setHeld(uint object, uint32 value) {
	if (object < kNumObjects)
		WRITE_LE_UINT32(block3D + 0x40 + 4 * object, value);
}

void PeintreEngine::loadPlayers() {
	// u32 count, then 40-byte records {name[32], i32 volume, u32 view_size}.
	_players.clear();
	Common::ScopedPtr<Common::InSaveFile> in(_saveFileMan->openForLoading(_targetName + ".users"));
	if (!in)
		return; // a missing file is 0 players
	const uint32 count = in->readUint32LE();
	for (uint32 i = 0; i < count && i < kMaxPlayers && !in->eos(); i++) {
		char name[33];
		in->read(name, 32);
		name[32] = 0;
		PlayerRecord p;
		p.name = name;
		p.volume = in->readSint32LE();
		p.viewSize = in->readUint32LE();
		_players.push_back(p);
	}
}

void PeintreEngine::savePlayers() {
	Common::ScopedPtr<Common::OutSaveFile> out(_saveFileMan->openForSaving(_targetName + ".users", false));
	if (!out)
		return;
	out->writeUint32LE(_players.size());
	for (const PlayerRecord &p : _players) {
		char name[32];
		memset(name, 0, sizeof(name));
		strncpy(name, p.name.c_str(), 31);
		out->write(name, 32);
		out->writeSint32LE(p.volume);
		out->writeUint32LE(p.viewSize);
	}
	out->finalize();
}

static Common::String gameName(const Common::String &target, uint player, uint slot) {
	return Common::String::format("%s.game%02u%02u", target.c_str(), player, slot);
}

static Common::String resumeName(const Common::String &target, uint player) {
	return Common::String::format("%s.ggame%u", target.c_str(), player);
}

void PeintreEngine::checkSessions() {
	// A player without a resume file loses its saves and its record; the later records
	// move down one index but their files are not renamed (original behaviour).
	for (uint p = 0; p < _players.size(); ) {
		Common::ScopedPtr<Common::InSaveFile> in(_saveFileMan->openForLoading(resumeName(_targetName, p)));
		if (in) {
			p++;
			continue;
		}
		warning("'%s' previous session was not properly closed: User record has been discarded from database.",
				_players[p].name.c_str());
		for (uint s = 0; s < kNumObjects; s++)
			_saveFileMan->removeSavefile(gameName(_targetName, p, s));
		_players.remove_at(p);
	}
}

void PeintreEngine::deletePlayerSaves(uint player) {
	_saveFileMan->removeSavefile(resumeName(_targetName, player));
	SaveOrder order = readSaveOrder();
	for (uint s = 0; s < kNumObjects; s++) {
		const Common::String name = gameName(_targetName, player, s);
		_saveFileMan->removeSavefile(name);
		for (uint i = 0; i < order.size(); i++)
			if (order[i].name == name)
				order.remove_at(i--);
	}
	writeSaveOrder(order);
}

static void writeBlocks(Common::WriteStream &out, const GameState &st) {
	out.write(st.block3D, k3DBlockSize);
	for (uint i = 0; i < kNumZones; i++)
		out.writeUint32LE(st.zoneDone[i]);
	for (uint i = 0; i < kNumObjects; i++)
		out.writeUint32LE(st.placed[i]);
	for (uint i = 0; i < 4; i++)
		out.writeUint32LE(st.counters[i]);
}

static void readBlocks(Common::ReadStream &in, GameState &st) {
	in.read(st.block3D, k3DBlockSize);
	for (uint i = 0; i < kNumZones; i++)
		st.zoneDone[i] = in.readUint32LE();
	for (uint i = 0; i < kNumObjects; i++)
		st.placed[i] = in.readUint32LE();
	for (uint i = 0; i < 4; i++)
		st.counters[i] = in.readUint32LE();
}

// The original lists saved games by their files' write times (save.md, Q-0355); ScummVM
// saves have none, so <target>.order keeps a write counter per file name ("name n"
// lines), beside the original's files (which stay byte for byte).
SaveOrder PeintreEngine::readSaveOrder() const {
	SaveOrder order;
	Common::ScopedPtr<Common::InSaveFile> in(_saveFileMan->openForLoading(_targetName + ".order"));
	while (in && !in->eos() && !in->err()) {
		const Common::String line = in->readLine();
		const size_t sp = line.findLastOf(' ');
		if (sp != Common::String::npos)
			order.push_back({ line.substr(0, sp), (uint32)strtoul(line.c_str() + sp + 1, nullptr, 10) });
	}
	return order;
}

void PeintreEngine::writeSaveOrder(const SaveOrder &order) {
	Common::ScopedPtr<Common::OutSaveFile> out(_saveFileMan->openForSaving(_targetName + ".order", false));
	if (!out)
		return;
	for (const SaveOrderEntry &e : order)
		out->writeString(Common::String::format("%s %u\n", e.name.c_str(), e.when));
	out->finalize();
}

uint32 PeintreEngine::saveOrder(uint player, uint slot) const {
	const Common::String name = gameName(_targetName, player, slot);
	for (const SaveOrderEntry &e : readSaveOrder())
		if (e.name == name)
			return e.when;
	return 0;
}

bool PeintreEngine::writeGame(uint slot) {
	const Common::String name = gameName(_targetName, _player, slot);
	Common::ScopedPtr<Common::OutSaveFile> out(_saveFileMan->openForSaving(name, false));
	if (!out)
		return false;
	writeBlocks(*out, _state);
	out->finalize();
	if (out->err())
		return false;
	SaveOrder order = readSaveOrder();
	uint32 last = 0;
	for (uint i = 0; i < order.size(); i++) {
		last = MAX(last, order[i].when);
		if (order[i].name == name)
			order.remove_at(i--);
	}
	order.push_back({ name, last + 1 });
	writeSaveOrder(order);
	return true;
}

bool PeintreEngine::writeResume(uint32 in2d) {
	Common::ScopedPtr<Common::OutSaveFile> out(_saveFileMan->openForSaving(resumeName(_targetName, _player), false));
	if (!out)
		return false;
	out->writeUint32LE(in2d);
	writeBlocks(*out, _state);
	out->finalize();
	return !out->err();
}

bool PeintreEngine::gameExists(uint player, uint slot) const {
	return _saveFileMan->exists(gameName(_targetName, player, slot));
}

bool PeintreEngine::readGame(uint player, uint slot) {
	Common::ScopedPtr<Common::InSaveFile> in(_saveFileMan->openForLoading(gameName(_targetName, player, slot)));
	if (!in || in->size() > 0x800) // larger files are ignored (Load3DGame)
		return false;
	readBlocks(*in, _state);
	return !in->err();
}

bool PeintreEngine::readResume(uint player, uint32 &in2d) {
	Common::ScopedPtr<Common::InSaveFile> in(_saveFileMan->openForLoading(resumeName(_targetName, player)));
	if (!in)
		return false;
	in2d = in->readUint32LE();
	readBlocks(*in, _state);
	return !in->err();
}

} // End of namespace Peintre
