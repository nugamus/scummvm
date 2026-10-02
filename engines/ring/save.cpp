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

// Saved games (spec/save.md): ScummVM saves holding the original's records in its order,
// puzzles, rotations, objects, variables, the bag, the timers, the place, the sounds, then
// the worlds left through Erda. A load runs the set-ups again and overlays the records, then
// makes the saved place current as entry 1000 does.

#include "common/memstream.h"
#include "common/serializer.h"
#include "common/system.h"

#include "ring/bag.h"
#include "ring/resources.h"
#include "ring/ring.h"
#include "ring/sound.h"
#include "ring/world.h"

namespace Ring {

enum {
	kSaveVersion = 1
};

static void syncTime(Common::Serializer &s, uint32 &time, uint32 now) {
	// Times are kept relative to the tick count, as the original does.
	uint32 age = now - time;
	s.syncAsUint32LE(age);
	if (s.isLoading())
		time = now - age;
}

static void syncAnimation(Common::Serializer &s, Animation &a, uint32 now) {
	s.syncAsSint32LE(a.id);
	s.syncAsSint32LE(a.start);
	s.syncAsSint32LE(a.mode);
	s.syncAsSint32LE(a.baseMode);
	s.syncAsSint32LE(a.frame);
	s.syncAsByte(a.backward);
	s.syncAsByte(a.active);
	s.syncAsByte(a.paused);
	s.syncAsByte(a.justStarted);
	s.syncAsByte(a.restart);
	s.syncAsSint32LE(a.lastReported);
	s.syncAsSint32LE(a.pauseFrame);
	s.syncAsSint32LE(a.pauseState);
	s.syncAsUint32LE(a.pauseMs);
	s.syncAsByte(a.stepped);
	syncTime(s, a.lastStep, now);
	syncTime(s, a.holdStart, now);
}

static void syncHotSpot(Common::Serializer &s, HotSpot &h) {
	s.syncAsByte(h.enabled);
}

static void syncMovabilities(Common::Serializer &s, Common::Array<Movability> &list) {
	for (Movability &m : list) {
		syncHotSpot(s, m.hotSpot);
		s.syncString(m.ride);
	}
}

static void syncSoundItems(Common::Serializer &s, SoundItems &items) {
	for (auto &i : items) {
		s.syncAsSint32LE(i->volume);
		s.syncAsSint32LE(i->pan);
		s.syncAsByte(i->active);
	}
}

template<typename T>
static void syncMap(Common::Serializer &s, Common::HashMap<int, T> &map, void (*one)(Common::Serializer &, T &)) {
	uint32 count = map.size();
	s.syncAsUint32LE(count);
	if (s.isSaving()) {
		for (auto &kv : map) {
			int32 key = kv._key;
			s.syncAsSint32LE(key);
			one(s, kv._value);
		}
		return;
	}
	for (uint32 i = 0; i < count; i++) {
		int32 key = 0;
		s.syncAsSint32LE(key);
		T value = T();
		one(s, value);
		map[key] = value;
	}
}

static void syncInt(Common::Serializer &s, int32 &v) { s.syncAsSint32LE(v); }
static void syncFloat(Common::Serializer &s, float &v) { s.syncAsFloatLE(v); }
static void syncString(Common::Serializer &s, Common::String &v) { s.syncString(v); }

void World::syncState(Common::Serializer &s, uint32 now) {
	// 1. the puzzles
	for (auto &p : _puzzles) {
		s.syncString(p->background);
		if (s.isLoading())
			p->bgImage.reset();
		syncMovabilities(s, p->movabilities);
		syncSoundItems(s, p->sounds);
		s.syncAsSint32LE(p->mode);
		s.syncAsSint32LE(p->modeObject);
		for (auto &img : p->images) {
			s.syncAsSint32LE(img->x);
			s.syncAsSint32LE(img->y);
			s.syncAsByte(img->active);
		}
		for (auto &anim : p->animations)
			syncAnimation(s, *anim, now);
	}
	// 2. the rotations
	for (auto &r : _rotations) {
		syncMovabilities(s, r->movabilities);
		for (Rotation::Layer &l : r->layers) {
			if (l.animation)
				syncAnimation(s, *l.animation, now);
			s.syncAsByte(l.shown);
			s.syncAsSint32LE(l.frame);
			l.dirty = true;
		}
		syncSoundItems(s, r->sounds);
		s.syncAsByte(r->paused);
		s.syncAsFloatLE(r->strength);
		s.syncAsByte(r->frozen);
		s.syncAsFloatLE(r->alpha);
		s.syncAsFloatLE(r->beta);
		s.syncAsFloatLE(r->ran);
	}
	// 3. the objects
	for (auto &o : _objects) {
		for (auto &acc : o->accessibilities)
			syncHotSpot(s, acc->hotSpot);
		for (Presentation &pr : o->presentations) {
			s.syncAsByte(pr.shown);
			for (auto &t : pr.texts)
				s.syncString(t->text);
		}
	}
	// 4. the variables
	for (auto &ints : _ints)
		syncMap<int32>(s, ints, syncInt);
	syncMap<float>(s, _floats, syncFloat);
	syncMap<Common::String>(s, _strings, syncString);
}

void Sounds::syncState(Common::Serializer &s, Common::Array<Common::Pair<int, bool> > &playingNow) {
	for (Sound &snd : _sounds) {
		s.syncAsSint32LE(snd.own);
		s.syncAsSint32LE(snd.typeVolume);
		s.syncAsSint32LE(snd.pan);
	}
	// The sounds playing, played again after a load (0x469790, 0x4696f0).
	if (s.isSaving()) {
		playingNow.clear();
		for (Sound &snd : _sounds)
			if (active(snd))
				playingNow.push_back(Common::Pair<int, bool>(snd.id, snd.loop));
	}
	uint32 count = playingNow.size();
	s.syncAsUint32LE(count);
	if (s.isLoading())
		playingNow.resize(count);
	for (auto &p : playingNow) {
		s.syncAsSint32LE(p.first);
		s.syncAsByte(p.second);
	}
}

bool RingEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	return _world && !_menuZone && _zone != kZoneSY && !_drag.active && !_bag->shown() && !_gameOver;
}

bool RingEngine::canLoadGameStateCurrently(Common::U32String *msg) {
	return _world != nullptr;
}

void RingEngine::syncGame(Common::Serializer &s) {
	uint32 now = g_system->getMillis();
	_world->syncState(s, now);
	// 5. the bag
	Common::Array<int> bag = _bag->contents();
	uint32 count = bag.size();
	s.syncAsUint32LE(count);
	if (s.isLoading())
		bag.resize(count);
	for (int &o : bag)
		s.syncAsSint32LE(o);
	if (s.isLoading()) {
		_bag->removeAll();
		for (int i = (int)bag.size() - 1; i >= 0; i--)
			_bag->add(bag[i]);
	}
	// 6. the timers
	auto syncTimers = [&](Common::Array<Timer> &timers) {
		uint32 n = timers.size();
		s.syncAsUint32LE(n);
		if (s.isLoading())
			timers.resize(n);
		for (Timer &t : timers) {
			s.syncAsSint32LE(t.id);
			s.syncAsUint32LE(t.period);
			int32 left = (int32)(t.due - now);
			s.syncAsSint32LE(left);
			if (s.isLoading())
				t.due = now + left;
		}
	};
	syncTimers(_timers);
	// 7. the place (the original's trailer)
	s.syncAsSint32LE(_zone);
	s.syncAsSint32LE(_mode);
	s.syncAsSint32LE(_puzzle);
	s.syncAsSint32LE(_rotation);
	s.syncAsByte(_bagWasFrozen);
	// The worlds left through Erda (their .ars files in the original).
	uint32 states = _worldStates.size();
	s.syncAsUint32LE(states);
	if (s.isSaving()) {
		for (auto &kv : _worldStates) {
			Common::String name = kv._key;
			s.syncString(name);
			uint32 nb = kv._value.bag.size();
			s.syncAsUint32LE(nb);
			for (int o : kv._value.bag)
				s.syncAsSint32LE(o);
			Common::Array<Timer> timers = kv._value.timers;
			for (Timer &t : timers)
				t.due = now + (t.due - kv._value.tick); // as if left now
			syncTimers(timers);
		}
	} else {
		_worldStates.clear();
		for (uint32 i = 0; i < states; i++) {
			Common::String name;
			s.syncString(name);
			WorldState &ws = _worldStates[name];
			uint32 nb = 0;
			s.syncAsUint32LE(nb);
			ws.bag.resize(nb);
			for (int &o : ws.bag)
				s.syncAsSint32LE(o);
			syncTimers(ws.timers);
			ws.tick = now;
		}
	}
	// 8. the sounds
	_sounds->syncState(s, _playingOnLoad);
}

Common::Error RingEngine::saveGameStream(Common::WriteStream *stream, bool isAutosave) {
	Common::Serializer s(nullptr, stream);
	s.setVersion(kSaveVersion);
	uint32 version = kSaveVersion;
	s.syncAsUint32LE(version);
	syncGame(s);
	return Common::kNoError;
}

Common::Error RingEngine::loadGameStream(Common::SeekableReadStream *stream) {
	// Applied in the main loop (a load from the menu can come from inside a zone handler).
	_pendingLoad.resize(stream->size() - stream->pos());
	stream->read(_pendingLoad.data(), _pendingLoad.size());
	return Common::kNoError;
}

void RingEngine::applyLoad() {
	Common::MemoryReadStream stream(_pendingLoad.data(), _pendingLoad.size());
	Common::Serializer s(&stream, nullptr);
	uint32 version = 0;
	s.syncAsUint32LE(version);
	if (version != kSaveVersion) {
		warning("Ring: unknown save version %u", version);
		_pendingLoad.clear();
		return;
	}
	// The set-ups again (0x408bc0, 0x431040), then the records over them.
	resetWorld();
	_drag = Drag();
	syncGame(s);
	_pendingLoad.clear();
	// Entry 1000: the place made current without zone code, the sounds that were playing.
	int puzzle = _puzzle, rotation = _rotation, mode = _mode;
	bool frozen = false;
	if (Rotation *r = _world->rotation(rotation))
		frozen = r->frozen;
	setZone(_zone);
	_menuZone = 0;
	_sounds->stopAll(8);
	_sounds->clearPlaces();
	if (mode == 2 && _world->puzzle(puzzle))
		puzSetAct(puzzle, false, true);
	else if (Rotation *r = _world->rotation(rotation)) {
		float a = r->alpha, b = r->beta, ran = r->ran;
		rotSetAct(rotation, false, true);
		r->alpha = a;
		r->beta = b;
		r->ran = ran;
		r->frozen = frozen;
	}
	_sounds->setTypeVolumes(_preferences[0], _preferences[1]); // aPre.ini again
	for (auto &p : _playingOnLoad)
		_sounds->play(p.first, p.second);
	_playingOnLoad.clear();
}

} // End of namespace Ring
