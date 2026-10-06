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

#include "common/serializer.h"
#include "common/stream.h"

#include "grumpa/events.h"
#include "grumpa/grumpa.h"

namespace Grumpa {

// ScummVM saves holding what the original keeps in Save\Current\ (docs/spec/save.md):
// the current scene, the items and the inventory, then the event VM's state.
enum {
	kSaveVersion = 7  // 2: the event VM block; 3: the score and the ambience; 4: the characters;
	                  // 5: the characters actors 3 and 4 hold
	                  // 6: the characters' state slots, texture, attachments and latch
	                  // 7: the scenes' walk-mesh walls
};

bool GrumpaEngine::hasFeature(EngineFeature f) const {
	return f == kSupportsReturnToLauncher || f == kSupportsLoadingDuringRuntime
		|| f == kSupportsSavingDuringRuntime;
}

bool GrumpaEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	return _sceneNum >= 0;
}

bool GrumpaEngine::canLoadGameStateCurrently(Common::U32String *msg) {
	return true;
}

bool GrumpaEngine::syncGame(Common::Serializer &s) {
	if (!s.syncVersion(kSaveVersion))
		return false;
	int32 scene = _sceneNum;
	s.syncAsSint32LE(scene);
	_inventory.syncState(s);
	syncEvents(s);
	if (s.getVersion() >= 4 && !_characters.syncState(s))
		return false;
	if (s.isLoading() && s.getVersion() < 6)
		_inventory.wearEquipment();  // no worn flags yet: from the equipment slots (E-1700)
	if (s.isLoading()) {
		if (s.err() || scene < 0)
			return false;
		_nextScene = scene;
		// The first entry after a load places no one (E-0804); a save from before the
		// characters were kept puts the player at the scene's first entry.
		_characters.forgetScene(s.getVersion() < 4);
		_playerScene = -1;
		_rippleCount = _rippleFrame = 0;
		_leftHeld = _leftWas = _backspace = false;
		_restoring = true;
	}
	return !s.err();
}

// The event VM's block (actor states, kept scene statuses, the command lists): events.cpp.
void GrumpaEngine::syncEvents(Common::Serializer &s) {
	_events->syncState(s);  // events.cpp
}

Common::Error GrumpaEngine::saveGameStream(Common::WriteStream *stream, bool isAutosave) {
	Common::Serializer s(nullptr, stream);
	return syncGame(s) ? Common::kNoError : Common::kWritingFailed;
}

Common::Error GrumpaEngine::loadGameStream(Common::SeekableReadStream *stream) {
	if (!_inventory.load() || !_characters.load())  // the data's defaults first
		return Common::kReadingFailed;
	Common::Serializer s(stream, nullptr);
	return syncGame(s) ? Common::kNoError : Common::kReadingFailed;
}

} // End of namespace Grumpa
