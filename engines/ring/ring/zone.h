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

#ifndef RING_RING_ZONE_H
#define RING_RING_ZONE_H

// Shorthands for the zone code, named as the zone specs name the API calls
// (games/ring/docs/ni.md, first paragraph).

#include "common/system.h"

#include "ring/bag.h"
#include "ring/ring.h"
#include "ring/sound.h"
#include "ring/world.h"

namespace Ring {
namespace Api {

inline World &w() { return g_engine->world(); }
inline Bag &bag() { return g_engine->bag(); }
inline Sounds &snd() { return g_engine->sounds(); }
inline int byte_(int id) { return w().varByte(id); }
inline void setByte(int id, int v) { w().setVarByte(id, v); }
inline int word(int id) { return w().var(World::kVarWord, id); }
inline void setWord(int id, int v) { w().setVar(World::kVarWord, id, v); }
inline int dword(int id) { return w().var(World::kVarDword, id); }
inline void setDword(int id, int v) { w().setVar(World::kVarDword, id, v); }
/** SY's float 90005, the world's score. */
inline void score(float n) { w().setVarFloat(90005, w().varFloat(90005) + n); }
/** `ObjPreSho` / `ObjPreHid`: one presentation, or all of them. */
inline void show(int o, int p = -1) { w().showPresentation(o, p, true, g_system->getMillis()); }
inline void hide(int o, int p = -1) { w().showPresentation(o, p, false); }
/** Accessibilities on / off: all, one, or `from`..`to`. */
inline void accOn(int o, int from = -1, int to = -1) { w().setAccessibilities(o, true, from, to < 0 ? from : to); }
inline void accOff(int o, int from = -1, int to = -1) { w().setAccessibilities(o, false, from, to < 0 ? from : to); }
inline void movOn(int place, int from = -1, int to = -1) { w().setMovabilities(place, true, from, to < 0 ? from : to); }
inline void movOff(int place, int from = -1, int to = -1) { w().setMovabilities(place, false, from, to < 0 ? from : to); }
inline void play(int id, bool loop = false) { snd().play(id, loop); }
inline void stop(int id) { snd().stop(id, 0x400); }
inline bool playing(int id) { return snd().playing(id); }
inline void cin(const Common::String &name) { g_engine->plyCin(name); }
inline void puz(int id) { g_engine->puzSetAct(id); }
inline void rotAct(int id) { g_engine->rotSetAct(id); }
inline int held() { return bag().held(); }
inline void drop() { g_engine->dropObject(); }
/** rand() × n / 32768. */
inline int rnd(int n) { return g_engine->rnd().getRandomNumber(n - 1); }

/** `RotSetAlp` (and `RotSetBet` unless `setBeta` is false), `RotSetRan`, `RotSetAct`. */
inline void rot(int id, float alpha, float beta, float ran, bool setBeta = true) {
	if (Rotation *r = w().rotation(id)) {
		r->setAlpha(alpha);
		if (setBeta)
			r->beta = beta;
		r->ran = ran;
		g_engine->rotSetAct(id);
	}
}

} // End of namespace Api
} // End of namespace Ring

#endif // RING_RING_ZONE_H
