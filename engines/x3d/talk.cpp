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

#include "x3d/detection.h"
#include "x3d/scene.h"
#include "x3d/sound.h"
#include "x3d/talk.h"

namespace X3D {

Talk::Talk(Scene &scene, Sound &sound, const Common::String &unitDir)
	: _scene(scene), _sound(sound), _unitDir(unitDir), _random("x3d_talk") {
}

void Talk::addTalker(const Common::String &character, const Common::String &face, const Common::String &clipDir) {
	static const char *const clips[] = { nullptr, "Yeux", "Ch", "Ch_yeux", "B", "E", "F", "O", "A" };
	// Loaded in the original's order: later nodes override earlier ones,
	// so A, left enabled at rest by a closed shape, never hides the other mouths
	static const int order[] = { 8, 4, 2, 3, 5, 6, 7, 1 };
	Talker t;
	t.character = character;
	t.face = face;
	int lowest = -1;
	for (int i : order)
		t.slots[i] = _scene.addFaceClip(face, (clipDir.empty() ? "Anim/" + character + "/" : clipDir) + clips[i] + ".A3D", character);
	for (int i = 1; i <= 8 && lowest < 0; i++)
		lowest = t.slots[i];
	// Empty slots, and slot 0, use the lowest-numbered loaded clip
	t.slots[0] = lowest;
	for (int i = 1; i <= 8; i++)
		if (t.slots[i] < 0)
			t.slots[i] = lowest;
	_talkers.push_back(t);
}

bool Talk::say(const Common::String &character, const Common::String &name) {
	int talker = -1;
	for (uint i = 0; i < _talkers.size(); i++)
		if (_talkers[i].character.equalsIgnoreCase(character))
			talker = i;
	if (talker < 0)
		return false;

	// Sound/<name>.WAV, or Sound/<name> when the name has the extension; the lip table has
	// the same name with .bin
	const bool hasExtension = name.hasSuffixIgnoreCase(".wav");
	const Common::String base = hasExtension ? name.substr(0, name.size() - 4) : name;
	const Common::String sound = _unitDir + "Sound/" + (hasExtension ? name : name + ".WAV");
	const Math::Vector3d at = _scene.facePosition(_talkers[talker].face, _talkers[talker].character);
	// The same line still playing goes on, lip sync included
	if (!_sound.emit(Sound::kVoiceEmitter, Common::Path(sound), at, false))
		return true;
	stop();

	debugC(1, kDebugSound, "%s says %s at %g,%g,%g", character.c_str(), name.c_str(), at.x(), at.y(), at.z());
	_current = talker;
	_selected = 0;
	_start = _lastChange = _nextRandom = _now;
	_times.clear();
	_shapes.clear();
	// Lip table: a .BIN #INDEX# chunk of {u32 time ms, u16 shape, u16 filler}
	if (Common::SeekableReadStream *s = openBinChunk(Common::Path(_unitDir + "Sound/" + base + ".bin"), "#INDEX#")) {
		const uint32 count = s->readUint32LE();
		for (uint32 i = 0; i < count && !s->err(); i++) {
			_times.push_back(s->readUint32LE());
			_shapes.push_back(s->readUint16LE());
			s->readUint16LE();
		}
		delete s;
	}
	return true;
}

void Talk::select(int slot) {
	if (slot == _selected)
		return;
	const Talker &t = _talkers[_current];
	if (_selected > 0 && t.slots[_selected] >= 0)
		_scene.setNode(t.slots[_selected], false, false);
	_selected = slot;
	if (t.slots[slot] >= 0)
		_scene.setNode(t.slots[slot], true, true);
}

void Talk::stop() {
	if (_current < 0)
		return;
	for (int slot : _talkers[_current].slots)
		if (slot >= 0)
			_scene.setNode(slot, false, false);
	_current = -1;
	_selected = 0;
	_times.clear();
	_shapes.clear();
}

void Talk::tick(uint32 now) {
	_now = now;
	if (_current < 0)
		return;
	if (!_sound.isGroupPlaying(Sound::kVoice)) {
		stop();
		return;
	}
	const Talker &t = _talkers[_current];
	const uint32 elapsed = now - _start;

	if (_times.empty()) {
		// No lip data: a random mouth every 200 ms
		if (now < _nextRandom)
			return;
		_nextRandom = now + 200;
		const int r = 1 + _random.getRandomNumber(8);
		if (r == 9) {
			if (_selected > 0 && t.slots[_selected] >= 0)
				_scene.setNode(t.slots[_selected], false, false);
		} else {
			select(r);
		}
		if (_selected > 0 && t.slots[_selected] >= 0)
			_scene.setNodeFps(t.slots[_selected], _random.getRandomNumber(3));
		return;
	}

	if (elapsed >= _times.back()) {
		stop();
		return;
	}
	uint16 shape = 1;
	for (uint i = 0; i < _times.size() && _times[i] <= elapsed; i++)
		shape = _shapes[i];

	if (shape == 1) {
		// Closed: the selected clip off, the A clip shown at rest
		if (_selected > 0) {
			if (t.slots[_selected] >= 0)
				_scene.setNode(t.slots[_selected], false, false);
			_selected = 0;
		}
		if (t.slots[8] >= 0) {
			_scene.setNode(t.slots[8], true, false);
			_scene.setNodeFrame(t.slots[8], 0);
		}
		_lastChange = _start;
	} else if (elapsed - (_lastChange - _start) >= 200) {
		_lastChange = now;
		if (shape <= 3) {
			select(shape);
		} else {
			// Any open shape: a random one of B, E, F, O at a random rate
			int r = 4 + _random.getRandomNumber(3);
			while (r == _selected)
				r = 1 + _random.getRandomNumber(3);
			select(r);
			if (t.slots[r] >= 0)
				_scene.setNodeFps(t.slots[r], 1 + _random.getRandomNumber(1));
		}
	}
}

} // End of namespace X3D
