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
#include "common/serializer.h"
#include "common/system.h"

#include "engines/metaengine.h"

#include "cryomni3d/china/engine.h"

namespace CryOmni3D {
namespace China {

// A ScummVM save holds what the original's .sav holds (E-0208): the game variables, each
// object's state and slot, the place, the two view angles and the notes, plus the held
// object. It starts with the 20-byte description the metaengine lists.
static const uint32 kSaveVersion = 2;

bool CryOmni3DEngine_China::syncGame(Common::Serializer &s) {
	if (!s.syncVersion(kSaveVersion)) {
		return false;
	}
	for (uint i = 0; i < kVarCount; i++) {
		s.syncAsUint32LE(_vars[i]);
	}
	for (uint i = 0; i < kObjectCount; i++) {
		s.syncAsUint32LE(_objects[i].state);
		s.syncAsSint32LE(_objects[i].slot);
		if (_objects[i].state > 3) {
			return false;
		}
	}
	s.syncAsUint32LE(_heldObject);
	if (_heldObject > kNoObject) {
		return false;
	}
	Common::String place = _place ? _place->name : "";
	s.syncString(place);
	float alpha = _omni3D.getAlpha(), beta = _omni3D.getBeta();
	s.syncAsFloatLE(alpha);
	s.syncAsFloatLE(beta);
	uint32 count = _minutes.size();
	s.syncAsUint32LE(count);
	// The original holds at most 50 notes (E-0208)
	if (count > 50) {
		return false;
	}
	if (s.isLoading()) {
		_minutes.resize(count);
		const PlaceDef *found = findPlace(place);
		if (!found) {
			warning("China: saved place %s is unknown", place.c_str());
			return false;
		}
		_place = found;
		setAngles(alpha, beta);
	}
	for (uint i = 0; i < count; i++) {
		s.syncString(_minutes[i]);
	}
	// Version 2: the label and document keys places changed (E-1107)
	for (uint i = 0; i < kObjectCount; i++) {
		s.syncString(_objLabelKey[i], 2);
		s.syncString(_objDocKey[i], 2);
	}
	return !s.err();
}

Common::Error CryOmni3DEngine_China::saveGameState(int slot, const Common::String &desc, bool isAutosave) {
	Common::OutSaveFile *out = _saveFileMan->openForSaving(getMetaEngine()->getSavegameFile(slot, _targetName.c_str()));
	if (!out) {
		return Common::kWritingFailed;
	}
	char name[kSaveDescriptionLen];
	memset(name, 0, sizeof(name));
	Common::strlcpy(name, desc.c_str(), sizeof(name));
	out->write(name, sizeof(name));
	Common::Serializer s(nullptr, out);
	syncGame(s);
	out->finalize();
	const bool ok = !out->err();
	delete out;
	return ok ? Common::kNoError : Common::kWritingFailed;
}

// A load during play waits for the play loop, so no place procedure keeps running on the
// loaded state; blocking screens end early meanwhile (shouldAbort).
Common::Error CryOmni3DEngine_China::loadGameState(int slot) {
	if (!getSaveFileManager()->exists(getMetaEngine()->getSavegameFile(slot, _targetName.c_str()))) {
		return Common::kReadingFailed;
	}
	_pendingLoad = slot;
	if (_inPlay) {
		return Common::kNoError;
	}
	applyLoad();
	return _loadedGame ? Common::kNoError : Common::kReadingFailed;
}

void CryOmni3DEngine_China::applyLoad() {
	const int slot = _pendingLoad;
	_pendingLoad = -1;
	_loadedGame = false;
	Common::InSaveFile *in = _saveFileMan->openForLoading(getMetaEngine()->getSavegameFile(slot, _targetName.c_str()));
	if (!in) {
		return;
	}
	in->skip(kSaveDescriptionLen);
	Common::Serializer s(in, nullptr);
	const bool ok = syncGame(s) && !in->err();
	delete in;
	if (!ok) {
		warning("China: save %d is damaged", slot);
		newGame();
		return;
	}
	// The saved place's entry part runs on the next tick, with the saved angles
	resetPlayState();
	if (_heldObject != kNoObject) {
		loadSprite(Common::Path(Common::String::format("SPRITES/OBJETS/R_%s.SPR", objectStem(_heldObject))), _heldCursor);
	}
	_gameRunning = !visitMode();
	_entryPending = true;
	_loadedGame = true;
	musicForPlace(_place->name);
}

bool CryOmni3DEngine_China::canSaveGameStateCurrently(Common::U32String *msg) {
	return (_gameRunning || visitMode()) && _inPlay && !_inPlaceCall;
}

bool CryOmni3DEngine_China::canLoadGameStateCurrently(Common::U32String *msg) {
	return true;
}

} // End of namespace China
} // End of namespace CryOmni3D
