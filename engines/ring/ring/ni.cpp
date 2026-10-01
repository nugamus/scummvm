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

// Zone NI: Nibelheim, Alberich's world (games/ring/docs/ni.md).

#include "common/system.h"
#include "common/textconsole.h"

#include "ring/bag.h"
#include "ring/resources.h"
#include "ring/ring.h"
#include "ring/sound.h"
#include "ring/world.h"
#include "ring/ring/zones.h"

namespace Ring {
namespace NI {

enum {
	// Objects
	kBrutality = 10000, kGlug = 10001, kMinerals = 10002, kCar = 10003,
	kConsole = 10100, kCover1 = 10101, kCover2 = 10102, kHandle = 10103, kTileHolder = 10104,
	kHologram = 10105, kCross = 10106, kDam = 10107, kMosaic = 10109, kMosaicVoices = 10110,
	kSpeaker = 10200, kSpeakerHandle = 10201, kMime = 10300, kFrog = 10302, kTile = 10303, kTear = 10305,
	kLeftValve = 10420, kRightValve = 10421, kCasing = 10430, kThermometer = 10431, kBoiling = 10432,
	kDoor = 10440, kWater1 = 10450, kWater2 = 10460, kHelmet = 10503, kHelmetFrog = 10504, kCells = 10505,
	kTiles = 10600, kTile1 = 10601,
	// Bytes
	kGlugFed = 10000, kTilePlaced = 10102, kHoloOn = 10103, kCrossAt12 = 10104, kDamOpen = 10105,
	kCrossSolved = 10106, kBoilingOn = 10107, kMosaicFirst = 10108, kMosaicCount = 10113,
	kSpeakerDone = 10200, kFrogTaken = 10300, kMimeState = 10301, kBackFromFO = 10303,
	kLeftOpen = 10420, kRightOpen = 10421, kTearIn = 10430, kHeaterOn = 10431, kHeat = 10432, kHelmetTaken = 10500,
	// Words and dwords
	kCrossWord = 10100, kTileWord = 10600, kGlugPlace = 10000, kGlugAlpha = 10001,
	kScore = 90005 // SY's float
};

// Shorthands for the zone API (games/ring/docs/ni.md, first paragraph).
static RingEngine *g_vm;
static World &w() { return g_vm->world(); }
static Bag &bag() { return g_vm->bag(); }
static Sounds &snd() { return g_vm->sounds(); }
static int byte_(int id) { return w().varByte(id); }
static void setByte(int id, int v) { w().setVarByte(id, v); }
static int word(int id) { return w().var(World::kVarWord, id); }
static void setWord(int id, int v) { w().setVar(World::kVarWord, id, v); }
static int dword(int id) { return w().var(World::kVarDword, id); }
static void setDword(int id, int v) { w().setVar(World::kVarDword, id, v); }
static void score(float n) { w().setVarFloat(kScore, w().varFloat(kScore) + n); }
static void show(int o, int p = -1) { w().showPresentation(o, p, true, g_system->getMillis()); }
static void hide(int o, int p = -1) { w().showPresentation(o, p, false); }
static void accOn(int o, int from = -1, int to = -1) { w().setAccessibilities(o, true, from, to < 0 ? from : to); }
static void accOff(int o, int from = -1, int to = -1) { w().setAccessibilities(o, false, from, to < 0 ? from : to); }
static void play(int id, bool loop = false) { snd().play(id, loop); }
static void stop(int id) { snd().stop(id, 0x400); }
static bool playing(int id) { return snd().playing(id); }
static void cin(const char *name) { g_vm->plyCin(name); }
static void puz(int id) { g_vm->puzSetAct(id); }
static int held() { return bag().held(); }
static void drop() { g_vm->dropObject(); }
static int rnd(int n) { return g_vm->rnd().getRandomNumber(n - 1); }

static void rot(int id, float alpha, float beta, float ran, bool setBeta = true) {
	if (Rotation *r = w().rotation(id)) {
		r->setAlpha(alpha);
		if (setBeta)
			r->beta = beta;
		r->ran = ran;
		g_vm->rotSetAct(id);
	}
}

static void movOff(int place, int from = -1, int to = -1) { w().setMovabilities(place, false, from, to < 0 ? from : to); }
static void movOn(int place, int from = -1, int to = -1) { w().setMovabilities(place, true, from, to < 0 ? from : to); }

// Steps an object's presentations one per frame from `from` to `to` (hide, show, frame).
static void steps(int object, int from, int to) {
	for (int i = from; i != to; i += from < to ? 1 : -1) {
		hide(object);
		show(object, i);
		g_vm->renderFrame();
	}
	hide(object);
	show(object, to);
	g_vm->renderFrame();
}

// The car at the rail line's stops: layer 0 (presentations 3..7) or layer 1 (8..12).
static void carAtLayer(int layer) {
	for (int i = 3; i <= 12; i++) {
		if ((i >= 8) == (layer == 1))
			show(kCar, i);
		else
			hide(kCar, i);
	}
}

// 0x445930: the hologram follows the cross and the dam.
static void hologram() {
	int value = word(kCrossWord);
	hide(kCross);
	if (byte_(kDamOpen) == 1) {
		show(kCross, 38);
		show(kCross, value + 19);
		if (value == 0) {
			show(kCross, 39);
			setByte(kCrossSolved, 1);
		}
	} else {
		show(kCross, value);
		setByte(kCrossSolved, 0);
	}
}

// 0x445a10: the heater, after every valve, casing or cross change.
static void heater() {
	int h = byte_(kHeaterOn) + byte_(kCrossSolved), v = byte_(kLeftOpen) + byte_(kRightOpen);
	if (h == 2 && v != 2 && byte_(kBoilingOn) == 0) {
		play(10412, true);
		show(kBoiling, 0);
		snd().setVolume(10412, 80);
		setByte(kHeat, 0);
		g_vm->timSto(1);
		g_vm->timSta(0, 1000);
		setByte(kBoilingOn, 1);
	} else if (h == 2 && v == 2) {
		play(10412, true);
		show(kBoiling, 0);
		snd().setVolume(10412, 80);
		g_vm->timSto(0);
		bool wasBoiling = byte_(kBoilingOn) != 0;
		setByte(kBoilingOn, 0);
		setByte(kHeat, 0);
		if (wasBoiling)
			g_vm->timSto(1);
		g_vm->timSta(1, 1000);
		hide(kThermometer);
		show(kThermometer, 6);
	} else if (h != 2 && byte_(kBoilingOn) == 1) {
		stop(10412);
		hide(kBoiling);
		hide(kThermometer);
		setByte(kBoilingOn, 0);
		g_vm->timSto(0);
		g_vm->timSto(1);
	}
}

// The Helmet and the Frog become one (0x445c80 / the Frog's sound end).
static void helmetAndFrog() {
	score(5);
	bag().remove(kFrog);
	bag().remove(kHelmet);
	bag().add(kHelmetFrog);
	cin("1522");
}

void enter(RingEngine *vm, int entry) {
	g_vm = vm;
	switch (entry) {
	case 0: // from AS: the Mime's welcome
		cin("1540");
		play(14001, true);
		cin("1541");
		bag().removeAll();
		bag().add(kBrutality);
		puz(10390);
		play(10001);
		break;
	case 3: // back from FO
		vm->timStoAll();
		movOn(10410, 0, 0);
		cin("1550");
		rot(10301, 160.0f, 0.0f, 85.7f, false);
		puz(10392);
		play(14001, true);
		play(10021);
		setByte(kBackFromFO, 1);
		break;
	case 10: // resumed after Erda (games/ring/docs/ni.md, spec/bag.md)
		bag().removeAll();
		if (w().varByte(90017) == 0) {
			if (Rotation *r = w().rotation(dword(90021))) {
				vm->rotSetAct(r->id);
				r->frozen = w().varByte(90025) != 0;
			}
		} else {
			puz(dword(90021));
		}
		if (!vm->loadWorldState("alb"))
			warning("Ring: Wrong Erda AS / NI");
		break;
	case 999: // a test entry (Q-0020)
		bag().removeAll();
		for (int o : { kBrutality, kGlug, kMinerals, kTile, kTear, kHelmetFrog })
			bag().add(o);
		setByte(kCrossSolved, 1);
		show(1);
		play(10409, true);
		vm->rotSetAct(10415);
		break;
	default:
		warning("Ring: NI entry %d is not implemented", entry);
		break;
	}
}

void onClick(RingEngine *vm, int object, int value, int place) {
	g_vm = vm;
	int h = held();
	switch (object) {
	case kGlug:
		if (h && value == 1 && h == kGlug) { // put Glug down
			cin("1513");
			bag().remove(kGlug);
			score(2);
			puz(10002);
			setByte(kGlugFed, 1);
			show(kGlug);
			w().pauseOnFrame(kGlug, 0, 1, 1000, 0);
			play(10800, true);
			drop();
		} else if (h && value == 2 && h == kMinerals && !bag().has(kGlug)) { // feed it: the car comes
			stop(10800);
			cin("1514");
			for (int r : { 10003, 10201, 10401 })
				movOff(r, 0, 0);
			movOn(dword(kGlugPlace), 0, 0);
			if (!playing(10901)) {
				play(10900);
				play(10901, true);
			}
			carAtLayer(dword(kGlugPlace) == 10401 ? 1 : 0);
			drop();
		} else if (h) {
			drop();
		} else if (value == 0 || value == 3) { // call Glug
			setDword(kGlugPlace, place);
			if (Rotation *r = w().rotation(place)) {
				float a = r->alpha + 135.0f;
				setDword(kGlugAlpha, (int)(a >= 360.0f ? a - 360.0f : a));
			}
			puz(10000);
			vm->renderFor(200);
			if (byte_(kGlugFed)) {
				puz(10002);
				play(10800, true);
			} else {
				puz(10001);
			}
			play(10803);
		} else if (value == 4) { // back to where Glug was called
			puz(10000);
			vm->renderFor(200);
			play(10804);
			rot(dword(kGlugPlace), (float)dword(kGlugAlpha), 60.0f, 85.7f);
		}
		return;
	case kConsole:
		if (h) {
			drop();
		} else if (value == 1) {
			vm->rotSetRolTo(10101, 270.4f, 10.4f, 85.7f);
			puz(10100);
			accOn(kCover1, 0);
			accOn(kCover1, 2);
			accOn(kCover2, 0);
			accOn(kCover2, 2);
			accOn(kConsole);
			vm->setMouse(505, 205);
		}
		return;
	case kTileHolder:
		if (h == kTile && byte_(kTilePlaced) == 0) {
			play(10106);
			setByte(kTilePlaced, 1);
			score(5);
			show(kConsole, 1);
			bag().remove(kTile);
			accOff(kTileHolder);
			accOn(kMosaic);
			accOn(kMosaicVoices);
		}
		if (h)
			drop();
		return;
	case kHologram:
		if (h == kBrutality && byte_(kHoloOn) == 0) {
			setByte(kHoloOn, 1);
			score(3);
			cin("1510");
			show(kConsole, 2);
			play(10104);
			show(kCover2, 2);
			for (int o : { kConsole, kCover1, kCover2, kHandle })
				accOff(o);
			accOff(kHologram, 1);
		} else if (!h && byte_(kHoloOn) == 1) {
			puz(10102);
		}
		if (h)
			drop();
		return;
	default:
		break;
	}

	// The rest want empty hands: an object in hand is only dropped.
	if (h && object != kMime && object != kCasing && object != kWater1) {
		drop();
		return;
	}
	switch (object) {
	case kMosaic:
		if (value == 0) { // the centre: check the order 5, 2, 3, 4, 1
			static const int order[] = { 5, 2, 3, 4, 1 };
			play(10106);
			hide(kMosaic, 0);
			vm->renderFrame();
			bool right = true;
			for (int i = 0; i < 5; i++)
				right = right && byte_(kMosaicFirst + i) == order[i];
			if (right) {
				accOff(kMosaic);
				accOff(kMosaicVoices);
				w().setBackground(10100, "NIS01N01P02.0001.bmp");
				setByte(kMosaicCount, 9);
				hide(kCover1, 0);
				show(kCover1, 5);
				show(kCover1, 6);
				hide(kMosaic);
			} else {
				for (int i = 0; i <= 5; i++)
					setByte(kMosaicFirst + i, 0);
			}
		}
		break;
	case kMosaicVoices:
		play(10032 + value);
		break;
	case kSpeaker:
		accOff(kSpeaker);
		play(10022);
		break;
	case kMime:
		if (h == kBrutality) {
			cin("1512");
			if (byte_(kMimeState) == 1) {
				setByte(kMimeState, 2);
				puz(10390);
				play(10014);
			} else {
				puz(10390);
				play(10015 + rnd(3));
			}
		}
		if (h)
			drop();
		break;
	case kFrog:
		setByte(kMimeState, 0);
		puz(10392);
		play(10010);
		break;
	case kTile:
		puz(10392);
		play(10012);
		break;
	case kTear:
		setByte(kMimeState, 0);
		puz(10392);
		play(10005);
		break;
	case kCasing:
		if (h) {
			if (h == kTear && value == 1) {
				score(3);
				show(kCasing, 6);
				show(kCasing, 1);
				bag().remove(kTear);
				setByte(kTearIn, 1);
				drop();
				heater();
			} else {
				drop();
			}
			break;
		}
		setByte(kHeaterOn, 0);
		if (value == 1 && byte_(kTearIn) == 1) {
			hide(kCasing, 1);
			hide(kCasing, 6);
			bag().add(kTear);
			score(-3);
			setByte(kTearIn, 0);
		}
		if (value == 1 || value == 2) {
			heater();
		} else if (value == 0) { // opening the casing
			hide(kCasing);
			movOff(10411);
			accOff(kCasing, 0);
			if (byte_(kTearIn) == 0) {
				show(kCasing, 2);
			} else {
				show(kCasing, 4);
				show(kCasing, 1);
			}
			show(kCasing, 0);
			heater();
		}
		break;
	case kDoor:
		if (byte_(kCrossSolved) == 1 && byte_(kHeaterOn) == 1) {
			play(10414);
		} else if (byte_(kCrossSolved) != 0) {
			cin("1516");
			if (byte_(kCrossSolved) == 1 && !playing(10409))
				play(10409, true);
			rot(10415, 270.0f, 0.3f, 85.7f);
		} else {
			cin("1515");
			rot(10405, 270.0f, 0.3f, 85.7f);
		}
		break;
	case kWater1:
		if (byte_(kCrossSolved) != 1) {
			cin("1519");
			vm->gameOver(1);
		} else if (h == kHelmetFrog) {
			cin("1517");
			score(8);
			rot(10406, 270.0f, 0.3f, 85.7f);
			accOff(kWater1);
			drop();
		} else {
			cin("1518");
			vm->gameOver(2);
		}
		break;
	case kWater2: // on to RH (0x445720)
		vm->timStoAll();
		snd().stopAll(0x400);
		cin("1520");
		score(3);
		vm->goZone(kZoneRH, 0);
		break;
	case kHelmet:
		if (byte_(kHelmetTaken) == 0) {
			show(kHelmet);
			setByte(kHelmetTaken, 1);
			accOff(kHelmet);
			bag().add(kHelmet);
			score(2);
			if (bag().has(kFrog))
				helmetAndFrog();
		}
		break;
	case kCells:
		cin("1521");
		setByte(10501, 1);
		bag().add(kCells);
		break;
	case kTiles: { // the bottom band: the three tiles turn back to their rest positions
		static const int rest[] = { 12, 0, 24 };
		int dir[3];
		dir[0] = (word(kTileWord) + 36) % 48 < 37 ? -1 : 1;
		dir[1] = word(kTileWord + 1) < 25 ? -1 : 1;
		dir[2] = (word(kTileWord + 2) + 24) % 48 < 25 ? -1 : 1;
		while (!vm->shouldQuit() && (word(kTileWord) != rest[0] || word(kTileWord + 1) != rest[1] || word(kTileWord + 2) != rest[2])) {
			if (!playing(10401))
				play(10401);
			for (int t = 0; t < 3; t++) {
				int v = word(kTileWord + t);
				if (v == rest[t])
					continue;
				v = (v + dir[t] + 48) % 48;
				setWord(kTileWord + t, v);
				w().hideAndFree(kTile1 + t);
				show(kTile1 + t, v);
			}
			vm->renderFrame();
		}
		vm->rotSetAct(10601);
		break;
	}
	default:
		break;
	}
}

void onButtonDown(RingEngine *vm, int object, int value) {
	g_vm = vm;
	switch (object) {
	case kMosaic:
		if (held())
			return;
		play(10106);
		hide(kMosaic);
		show(kMosaic, value);
		if (value != 0) {
			setByte(kMosaicFirst + byte_(kMosaicCount), value);
			setByte(kMosaicCount, byte_(kMosaicCount) + 1);
		}
		break;
	case kCover1:
		if (held()) {
			drop();
			return;
		}
		if (value == 1) { // open
			vm->rotSetRolTo(10101, 270.4f, 10.4f, 85.7f);
			puz(10100);
			hide(kCover2);
			play(10102);
			accOff(kCover1, 1, 2);
			accOff(kCover2, 1, 2);
			accOff(kConsole);
			accOff(kHandle);
			if (byte_(kTilePlaced) == 0) {
				show(kCover1, 0);
				show(kCover1, 1);
			} else {
				if (byte_(kMosaicCount) == 9) {
					hide(kCover1, 0);
					show(kCover1, 5);
				} else {
					accOn(kMosaicVoices);
				}
				show(kConsole, 1);
				show(kCover1, 2);
			}
			show(kConsole, 0);
		} else if (value == 0) { // close
			play(10103);
			accOff(kCover1, 1, 2);
			accOff(kTileHolder);
			accOff(kMosaic);
			accOff(kMosaicVoices);
			show(kCover1, byte_(kTilePlaced) == 0 ? 3 : 4);
			hide(kConsole);
			hide(kMosaic);
		}
		break;
	case kCover2:
		if (held()) {
			drop();
			return;
		}
		if (byte_(kMosaicCount) != 9)
			return;
		if (value == 1) { // open
			hide(kCover2);
			if (byte_(kTilePlaced) != 1) {
				vm->rotSetRolTo(10101, 270.4f, 10.4f, 85.7f);
				puz(10100);
				show(kCover2, 4);
				for (int i = 0; i < 4; i++)
					vm->renderFrame();
				hide(kCover2, 4);
				accOn(kCover1, 0);
				accOn(kCover1, 2);
			} else {
				play(10104);
				accOff(kCover1, 1, 2);
				accOff(kCover2, 1, 2);
				accOff(kConsole);
				accOff(kHandle);
				vm->rotSetRolTo(10101, 270.4f, 10.4f, 85.7f);
				puz(10100);
				show(kCover2, 0);
				if (byte_(kHoloOn) == 0) {
					show(kCover2, 1);
				} else {
					show(kConsole, 2);
					show(kCover2, 2);
				}
			}
		} else if (value == 0) { // close
			accOff(kCover2, 1, 2);
			if (byte_(kHoloOn) == 0) {
				accOff(kHologram);
				puz(10100);
				hide(kCover2, 0);
				hide(kConsole);
				accOn(kCover2, 0);
				accOn(kCover2, 2);
			} else {
				play(10105);
				accOff(kHologram);
				show(kCover2, 3);
				hide(kConsole);
			}
		}
		break;
	case kCasing:
		if (value == 2) { // closing the casing
			accOff(kCasing, 1, 2);
			show(kCasing, byte_(kTearIn) == 0 ? 3 : 5);
			hide(kCasing, 0);
			hide(kCasing, 1);
		}
		break;
	default:
		break;
	}
}

// The drags' counters (the original's globals 0x4a1ce0.., 0x4a1cec).
static int s_counter, s_base, s_offset;
static bool s_valveFlag;

// The cross's sector angle for a position relative to the reference (243, 276).
static int crossAngle(int x, int y) {
	int base, part;
	if (x < 1) {
		base = y > 0 ? 30 : 20;
		part = y > 0 ? (y + 40 + x) / 6 : (y - x + 40) / 6;
	} else {
		base = y > 0 ? 0 : 10;
		part = y > 0 ? (x - y + 40) / 6 : (40 - y - x) / 6;
	}
	return base + MAX(part, 0);
}

static int wrap19(int v) {
	if (v < 0)
		v += ((18 - v) / 19) * 19;
	if (v > 18)
		v %= 19;
	return v;
}

void onDrag(RingEngine *vm, int object, int value, int phase) {
	g_vm = vm;
	Drag &d = vm->drag();
	int dx = ABS(d.current.x - d.press.x), dy = ABS(d.current.y - d.press.y);
	bool xMoved = d.current.x != d.previous.x, yMoved = d.current.y != d.previous.y;
	switch (object) {
	case kHandle:
		if (phase == 1) {
			s_counter = 0;
			d.mode = 2;
			d.limit = Common::Rect(495, 194, 598, 284);
		} else if (phase == 3 && xMoved) {
			s_counter = d.current.x < d.press.x ? 0 : CLIP(dx / 5, 0, 12);
			hide(kHandle);
			show(kHandle, s_counter);
		} else if (phase == 2) {
			if (!playing(10401))
				stop(10401); // as coded (Q-0021)
			accOff(kHandle);
			if (s_counter < 7) {
				steps(kHandle, s_counter, 0);
				accOn(kHandle);
			} else {
				steps(kHandle, s_counter, 12);
				vm->renderFor(1000);
				steps(kHandle, 12, 0);
				accOn(kHandle);
				carAtLayer(0);
				cin("1511");
				rot(10005, 270.0f, 0.0f, 85.7f, false);
				movOff(10005);
				movOn(10005, 1, 1);
				play(10901, true);
			}
		}
		break;
	case kCross: {
		// Positions relative to the reference point, negative to the left and above.
		int x = d.current.x < d.reference.x ? -ABS(d.current.x - d.reference.x) : ABS(d.current.x - d.reference.x);
		int y = d.current.y < d.reference.y ? -ABS(d.current.y - d.reference.y) : ABS(d.current.y - d.reference.y);
		if (phase == 1) {
			d.reference = Common::Point(243, 276);
			x = d.current.x < 243 ? -ABS(d.current.x - 243) : ABS(d.current.x - 243);
			y = d.current.y < 276 ? -ABS(d.current.y - 276) : ABS(d.current.y - 276);
			s_offset = wrap19(crossAngle(x, y) - word(kCrossWord));
			s_base = byte_(kDamOpen) == 1 ? 19 : 0;
		} else if (phase == 3) {
			if (!xMoved || ABS(x) > 40 || ABS(y) > 40)
				break;
			s_counter = wrap19(s_offset + crossAngle(x, y));
			hide(kCross);
			show(kCross, s_base + s_counter);
			setWord(kCrossWord, s_counter);
			hologram();
		} else if (phase == 2) {
			// To 0 or 12, whichever is nearer in the original's table.
			int v = s_counter;
			if (v != 12) {
				int step = (v >= 1 && v <= 6) || (v >= 13 && v <= 15) ? -1 : 1;
				while (v != 0 && v != 12 && !vm->shouldQuit()) {
					v += step;
					if (v > 18)
						v = 0;
					if (v < 0)
						v = 18;
					s_counter = v;
					w().hideAndFree(kCross);
					show(kCross, s_base + v);
					setWord(kCrossWord, v);
					hologram();
					vm->renderFrame();
				}
			}
			setWord(kCrossWord, v);
			setByte(kCrossAt12, v == 12);
			heater();
			if (byte_(kCrossSolved) == 1)
				play(10101);
			movOn(10102, v == 12 ? 2 : 1, v == 12 ? 2 : 1);
			movOff(10102, v == 12 ? 1 : 2, v == 12 ? 1 : 2);
			hologram();
		}
		break;
	}
	case kDam:
		if (phase == 1) {
			d.mode = 2;
			d.limit = Common::Rect(295, 255, 345, 375);
		} else if (phase == 3 && yMoved) {
			int v = value == 0 ? (d.current.y < d.press.y ? 0 : dy / 3) : (d.current.y > d.press.y ? 13 : 13 - dy / 3);
			setByte(kDamOpen, 0);
			if (v > 12) {
				v = 13;
				setByte(kDamOpen, 1);
			}
			s_counter = MAX(v, 0);
			hide(kDam);
			show(kDam, s_counter);
			hologram();
		} else if (phase == 2) {
			bool flip = dy >= 20 && (value == 0 ? d.current.y > d.press.y : d.current.y < d.press.y);
			bool open = value == 0 ? flip : !flip; // value 0 starts closed, 1 open
			hide(kDam);
			show(kDam, open ? 13 : 0);
			setByte(kDamOpen, open);
			if (flip) {
				accOff(kDam, open ? 0 : 1);
				accOn(kDam, open ? 1 : 0);
			}
			heater();
			if (byte_(kCrossSolved) == 1)
				play(10101);
			hologram();
		}
		break;
	case kSpeakerHandle:
		if (phase == 1) {
			d.mode = 2;
			d.limit = Common::Rect(299, 214, 431, 356);
		} else if (phase == 3 && yMoved) {
			s_counter = d.current.y < d.press.y ? 0 : CLIP(dy / 5, 0, 13);
			hide(kSpeakerHandle);
			show(kSpeakerHandle, s_counter);
			if (!playing(10401))
				play(10401);
		} else if (phase == 2) {
			if (!playing(10401))
				stop(10401); // as coded (Q-0021)
			if (s_counter < 4) {
				steps(kSpeakerHandle, s_counter, 0);
			} else if (s_counter > 10) {
				accOff(kSpeakerHandle);
				steps(kSpeakerHandle, s_counter, 13);
				vm->renderFor(1000);
				steps(kSpeakerHandle, 13, 0);
				cin("1525");
				cin("1526");
				accOn(kSpeaker);
			} else {
				steps(kSpeakerHandle, s_counter, 6);
				vm->renderFor(1000);
				steps(kSpeakerHandle, 6, 0);
				cin("1527");
				if (byte_(kSpeakerDone) != 0) {
					puz(10202);
					play(10030);
				} else {
					setByte(kSpeakerDone, 1);
					puz(10203);
					play(10027);
					score(2);
				}
			}
		}
		break;
	case kLeftValve:
	case kRightValve: {
		int byteId = object == kLeftValve ? kLeftOpen : kRightOpen;
		if (phase == 1) {
			d.mode = 2;
			d.limit = Common::Rect(263, 206, 390, 341);
			s_valveFlag = value != 0;
		} else if (phase == 3 && yMoved) {
			int v = value == 0 ? (d.current.y < d.press.y ? 1 : dy / 5 + 1) : (d.current.y > d.press.y ? 12 : 11 - dy / 5);
			v = CLIP(v, 1, 12);
			w().hideAndFree(object);
			show(object, v);
			if (v > 8 && !s_valveFlag) {
				play(10402);
				s_valveFlag = true;
				score(3);
				if (byte_(kBoilingOn)) {
					setByte(kHeat, byte_(kHeat) / 2);
					if (object == kLeftValve)
						accOff(kLeftValve);
					play(10404);
					heater();
				}
			} else if (v <= 4 && s_valveFlag) {
				stop(10404);
				play(10403);
				score(-3);
				s_valveFlag = false;
			}
		} else if (phase == 2) {
			hide(object);
			bool open;
			if (value == 0)
				open = dy >= 30 && d.current.y >= d.press.y;
			else
				open = !(dy >= 30 && d.current.y <= d.press.y);
			show(object, open ? 12 : 1);
			accOn(object, open ? 1 : 0);
			accOff(object, open ? 0 : 1);
			setByte(byteId, open);
			heater();
		}
		break;
	}
	case kTile1:
	case kTile1 + 1:
	case kTile1 + 2: {
		int wordId = kTileWord + (object - kTile1);
		if (phase == 1) {
			s_counter = word(wordId);
		} else if (phase == 3 && xMoved) {
			int v = s_counter + (d.current.x < d.press.x ? -dx / 12 : dx / 12);
			v = ((v % 48) + 48) % 48;
			setWord(wordId, v);
			w().hideAndFree(object);
			show(object, v);
			if (!playing(10401))
				play(10401);
		} else if (phase == 2) {
			if (word(kTileWord) == 0 && word(kTileWord + 1) == 0 && word(kTileWord + 2) == 0) {
				setWord(kTileWord, 12);
				setWord(kTileWord + 1, 0);
				setWord(kTileWord + 2, 24);
				cin("1524");
				rot(10501, 232.0f, 0.0f, 85.3f, false);
			}
		}
		break;
	}
	default:
		break;
	}
}

void onAccessibility(RingEngine *vm, int object, int value) {
	g_vm = vm;
	if (object == kConsole && value == 0) { // the console's border: leave it
		Common::Point mouse = vm->mouse();
		vm->rotSetAct(10101);
		accOn(kCover1, 0);
		accOn(kCover1, 2);
		accOn(kCover2, 0);
		accOn(kCover2, 2);
		accOn(kConsole, 4);
		vm->setMouse(mouse.x, mouse.y);
	}
}

void onBeforeMove(RingEngine *vm, int from, int to, int value, int kind) {
	g_vm = vm;
	if (kind == 0 && from == 10005 && to == 10101) {
		stop(10901);
		play(13001 + rnd(9));
	} else if (kind == 1) {
		if (value == 41)
			cin(byte_(kLeftOpen) ? "1529" : "1528");
		else if (value == 42)
			cin(byte_(kRightOpen) ? "1531" : "1530");
		else if (value == 61) {
			static const int rest[] = { 12, 0, 24 };
			for (int t = 0; t < 3; t++) {
				setWord(kTileWord + t, rest[t]);
				hide(kTile1 + t);
				show(kTile1 + t, rest[t]);
			}
		}
	} else if (kind == 2) {
		if (value == 41)
			cin(byte_(kLeftOpen) ? "1534" : "1533");
		else if (value == 42)
			cin(byte_(kRightOpen) ? "1536" : "1535");
	} else if (kind == 3) {
		if ((to == 10501 || to == 10511 || to == 10521) && value == 0)
			play(10501);
		if (from == 10501 || from == 10511 || from == 10521)
			play(10502);
		if (from == 10001)
			play(10804);
		if (from == 10002) {
			play(10804);
			stop(10800);
		}
	}
}

// The music of the place: 14001 on the rail line, 14002 at the speaker, 14003 elsewhere.
static void music(int to) {
	int m = to == 10201 ? 14002 : (to > 10201 || to == 10101) ? 14003 : (to >= 10000 && to <= 10005) ? 14001 : 0;
	if (!m)
		return;
	for (int other : { 14001, 14002, 14003 })
		if (other != m)
			stop(other);
	if (!playing(m))
		play(m, true);
}

void onAfterMove(RingEngine *vm, int to, int from, int value, int kind) {
	g_vm = vm;
	if (value == 100)
		movOff(to, 0, 0);
	if (value == 110) {
		stop(10800);
		if (playing(10901))
			play(10902);
		stop(10901);
		if (to == 10301) {
			play(10300, true);
			setByte(kMimeState, 0);
		}
	}
	if (kind == 3 && value == 55) { // the message to RH
		cin("1537");
		puz(12001);
		play(12001);
	}
	if (kind != 0)
		return;
	music(to);
	switch (value) {
	case 1:
		movOff(to);
		movOn(to, 0, 0);
		if (to == 10000 || to == 10002)
			movOn(to, 2, 2);
		break;
	case 2:
		movOff(to);
		movOn(to, 1, 1);
		break;
	case 5:
	case 21:
		carAtLayer(0);
		movOn(to, 0, 0);
		break;
	case 41:
		carAtLayer(1);
		movOn(to, 0, 0);
		break;
	case 3:
		movOff(to, 0, 0);
		break;
	case 16:
		vm->gameOver(3);
		break;
	default:
		break;
	}
	if (from == 10005 && to == 10101)
		for (int s = 13001; s <= 13009; s++)
			stop(s);
	if (from == 10415)
		stop(10409);
	if (to == 10406 && !playing(10410)) {
		play(10410, true);
		play(10411, true);
	}
}

void onTimer(RingEngine *vm, int id) {
	g_vm = vm;
	if (id == 0) { // boiling
		int v = byte_(kHeat) + 1;
		setByte(kHeat, v);
		v = (uint8)v;
		if (v >= 11 && v <= 69) {
			hide(kThermometer);
			show(kThermometer, (v - 10) / 5);
		} else if (v == 100) {
			hide(kThermometer);
			show(kThermometer, 12);
		}
		if (v % MAX(5, (120 - v) / 10) == 0) {
			play(10415);
			show(kBoiling, 1 + rnd(2));
		}
		snd().setVolume(10412, MIN(100, v / 5 + 80));
		if (v > 120) {
			vm->timSto(0);
			snd().stopType(3, 0x400);
			snd().stopType(1, 0x400);
			cin("1538");
			vm->gameOver(4);
		}
	} else if (id == 1) { // steady steam: NI is done once back from FO
		int v = byte_(kHeat) + 1;
		setByte(kHeat, v);
		if (v >= 11 && byte_(kBackFromFO) == 1) {
			vm->timStoAll();
			snd().stopAll(0x400);
			w().setVarFloat(kScore, 100.0f);
			cin("1539");
			AS::returnFromWorld(vm, 1);
		}
	}
}

void onAnimation(RingEngine *vm, int id, int frame) {
	g_vm = vm;
	auto consoleOpen = [&]() {
		puz(10100);
		accOn(kCover1, 0);
		accOn(kCover1, 2);
		accOn(kCover2, 0);
		accOn(kCover2, 2);
		accOn(kConsole);
		accOn(kHandle);
	};
	switch (id) {
	case 10000:
		if (frame == 1) {
			int n = rnd(10);
			w().pauseOnFrame(kGlug, 0, 1, n > 4 ? 0 : n * 300, 0);
		}
		break;
	case 10200:
		if (frame == 1) {
			int n = rnd(10);
			w().pauseOnFrame(kSpeaker, 1, 1, n > 4 ? 0 : n * 1000, 0);
		}
		break;
	case 10106: // cover 2 closed
		if (frame == 1) {
			hide(kCover1, 0);
			hide(kCover2, 0);
			consoleOpen();
		}
		break;
	case 10100: // cover 1 closed
	case 10102:
		if ((id == 10100 && frame == 1) || (id == 10102 && frame == 36)) {
			puz(10100);
			if (id == 10100) {
				hide(kCover1, 3);
				hide(kCover1, 0);
			} else {
				hide(kCover1, 4);
				hide(kCover1, 0);
				hide(kCover1, 5);
			}
			consoleOpen();
		}
		break;
	case 10101: // cover 1 open
		if (frame == 36) {
			accOn(kTileHolder);
			accOff(kMosaic);
			accOn(kCover1, 1);
		}
		break;
	case 10103:
		if (frame == 1) {
			accOff(kTileHolder);
			accOff(kMosaic);
			accOn(kCover1, 1);
			if (byte_(kMosaicCount) <= 8)
				accOn(kMosaic);
		}
		break;
	case 10104:
		if (frame == 15) {
			accOn(kCover2, 1);
			accOn(kHologram, 0);
		}
		break;
	case 10105:
		if (frame == 36) {
			accOn(kHologram, 1);
			accOn(kCover2, 1);
		}
		break;
	case 10300:
		if (frame == 3) {
			vm->setSoundItem(10301, 10301, true);
			vm->setSoundItem(10301, 10301, false);
			play(10301);
		}
		break;
	case 10422: // the casing opening (empty) and 10424 (with the tear)
	case 10424:
		if (frame == 1) {
			if (id == 10424) {
				hide(kBoiling, 0);
				show(kCasing, 6);
			}
			accOn(kCasing, 1, 2);
		} else if (frame == 2) {
			stop(10407);
		} else if (frame == 14) {
			play(10407);
		} else if (frame == 15) {
			stop(10405);
		} else if (frame == 26) {
			play(10405);
		}
		break;
	case 10423: // the casing closing (empty) and 10425 (with the tear)
	case 10425:
		if (frame == 27 && id == 10423) {
			movOn(10411);
			accOn(kCasing, 0);
		} else if (frame == 26) {
			if (id == 10425) {
				movOn(10411);
				accOn(kCasing, 0);
				setByte(kHeaterOn, 1);
				show(kBoiling, 0);
				heater();
			}
			stop(10406);
			stop(10408);
		} else if (frame == 15) {
			play(10406);
		} else if (frame == 2) {
			play(10408);
			if (id == 10425)
				hide(kCasing, 6);
		}
		break;
	default:
		break;
	}
}

void onSound(RingEngine *vm, int id, int type, int reason, int ended) {
	g_vm = vm;
	if (!ended)
		return;
	// "a -> p, n": PuzSetAct(p), play(n).
	static const struct {
		int sound, puzzle, next;
	} chain[] = {
		{ 10003, 10391, 10004 }, { 10005, 10390, 10006 }, { 10006, 10392, 10007 }, { 10007, 10390, 10008 },
		{ 10008, 10392, 10009 }, { 10010, 10391, 10011 }, { 10012, 0, 0 }, { 10022, 10202, 10023 },
		{ 10023, 10200, 10024 }, { 10024, 10202, 10025 }, { 10025, 10200, 10026 }, { 10027, 10205, 10028 },
		{ 10028, 10203, 10029 }, { 10029, 10204, 10030 }, { 10030, 10203, 10031 }, { 10031, 10204, 10032 },
		{ 12001, 12003, 12003 }, { 12003, 12002, 12002 }
	};
	for (const auto &c : chain) {
		if (c.sound == id && c.puzzle) {
			puz(c.puzzle);
			play(c.next);
			return;
		}
	}
	switch (id) {
	case 10001:
		show(kMime, 2);
		puz(10391);
		play(10002);
		break;
	case 10002:
		bag().add(kGlug);
		bag().add(kMinerals);
		cin("1543");
		puz(10392);
		play(10003);
		break;
	case 10004:
		rot(10301, 160.0f, 0.0f, 85.7f, false);
		break;
	case 10009:
		cin("1544");
		bag().add(kTear);
		score(5);
		hide(kTear);
		show(kTear, 0);
		accOff(kTear);
		break;
	case 10011:
		cin("1545");
		bag().add(kFrog);
		score(2);
		accOff(kFrog);
		setByte(kFrogTaken, 1);
		if (bag().has(kHelmet))
			helmetAndFrog();
		break;
	case 10012:
		cin("1546");
		setByte(kMimeState, 1);
		puz(10391);
		play(10013);
		break;
	case 10014:
		cin("1547");
		bag().add(kTile);
		score(5);
		show(kTile, 0);
		accOff(kTile);
		break;
	case 10015:
	case 10016:
	case 10017:
		puz(10392);
		play(10018 + rnd(3));
		break;
	case 10026:
		accOn(kSpeakerHandle, 0);
		break;
	case 10032:
		cin("1548");
		cin("1549");
		accOn(kSpeaker);
		vm->rotSetAct(10201);
		movOff(10201, 0, 0);
		break;
	case 12002:
		cin("1542");
		puz(10521);
		break;
	default:
		break;
	}
	// Back to the Mime's place after these.
	switch (id) {
	case 10004:
	case 10009:
	case 10011:
	case 10013:
	case 10014:
	case 10018:
	case 10019:
	case 10020:
	case 10021:
		vm->rotSetAct(10301);
		break;
	default:
		break;
	}
}

} // End of namespace NI
} // End of namespace Ring
