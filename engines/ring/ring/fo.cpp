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

// Zone FO: the Forest, Siegmund's world (games/ring/docs/fo.md).

#include "common/textconsole.h"

#include "ring/resources.h"
#include "ring/ring/zone.h"
#include "ring/ring/zones.h"

namespace Ring {
namespace FO {

using namespace Api;

enum {
	kStatue = 30001, kPedestal = 30002, kScroll = 30009, kDial = 30016, kBerries = 30017, kPoison = 30018,
	kWolfVision = 30019, kHare = 30020, kPanel = 30021, kDoorHare = 30025, kWall = 30026, kDialCentre = 30027,
	kFurnace = 30028, kPan = 30029, kTear = 30030, kJuice = 30031, kPoisonJuice = 30032, kMetal = 30033,
	kMine = 30040, kIngots = 30041, kMold = 30042, kGolem = 30043, kGolemPlace = 30044, kBells = 30045,
	kPole = 30046, kPoleWorms = 30047, kFish = 30048, kWorms = 30049, kTree = 30050, kFire = 30051,
	kArrow = 30053, kFishKey = 30054, kMedallion = 30055, kSiegMedallion = 30056, kSword = 30058, kWalls = 30059,
	kLightning = 30061, kDoor = 30100, kSieglinde = 30102, kCup = 30108, kStory = 30109, kHut = 30110, kLamps = 30200,
	kWords = 6 ///< SY's object of the ending's words, on puzzle 1
};

// FO's score is SY's float 90007; "(first)" adds only once, guarded by a byte.
static void score3(double n) { w().setVarFloat(90007, (float)(w().varFloat(90007) + n)); }
static void scoreFirst(int guard, double n) {
	if (byte_(guard) == 0) {
		score3(n);
		setByte(guard, 1);
	}
}

static void restart(int timer, uint32 ms) {
	g_engine->timSto(timer);
	g_engine->timSta(timer, ms);
}

// RotSetAct, then alpha and beta.
static void rotAfter(int id, float alpha, float beta) {
	rotAct(id);
	if (Rotation *r = w().rotation(id)) {
		r->setAlpha(alpha);
		r->beta = beta;
	}
}

static void volumes(int from, int to, int volume) {
	for (int s = from; s <= to; s++)
		snd().setVolume(s, volume);
}

void enter(RingEngine *vm, int entry) {
	switch (entry) {
	case 0:
		bag().removeAll();
		bag().add(kTear);
		vm->timSta(0, 5000);
		vm->timSta(1, 3000);
		vm->timSta(2, 5000);
		vm->timSta(3, 17000);
		vm->timSta(4, 10000);
		vm->timSta(5, 10);
		bag().add(kWolfVision);
		cin("1217");
		cin("1218");
		rot(30003, 180.0f, 0.0f, 85.3f, false);
		break;
	case 10: // resumed after Erda: world 3's record "sie" (spec/bag.md)
		bag().removeAll();
		if (w().varByte(90019) == 0) {
			if (Rotation *r = w().rotation(dword(90023))) {
				rotAct(r->id);
				r->frozen = w().varByte(90027) != 0;
			}
		} else {
			puz(dword(90023));
		}
		if (!vm->loadWorldState("sie"))
			warning("Ring: Wrong Erda AS / FO");
		break;
	case 999: // a test entry (Q-0050)
		for (int o : { (int)kWolfVision, (int)kFishKey, (int)kPanel, kPanel + 1, kPanel + 2 })
			bag().add(o);
		puz(35011);
		break;
	default:
		warning("Ring: FO entry %d is not implemented", entry);
		break;
	}
}

// The end: both medallions on the sword (fo.md, "The end").
static void theEnd() {
	show(kSword, 3);
	cin("1200");
	w().setVarFloat(90007, 100.0f);
	rot(30001, 180.0f, 0.0f, 85.3f, false);
	snd().setTypeVolume(2, 0);
	g_engine->timStoAll();
	play(30007, true);
	for (int o = 30001; o <= 30200; o++)
		if (w().object(o))
			accOff(o);
	for (int id = 30001; id <= 30801; id++)
		if (w().rotation(id))
			movOff(id);
	show(kWords, 0);
	show(kWords, 5);
	play(30120);
}

void onClick(RingEngine *vm, int object, int value) {
	int h = held();
	switch (object) {
	case kStatue:
		if (!h) {
			if (value == 4) {
				bag().add(kMedallion);
				hide(kStatue, 4);
				accOff(kStatue, 4);
				score3(4.0);
				setByte(30043, 1);
			}
			break;
		}
		if (h == kFishKey && value == 0) {
			show(kStatue, 0);
			accOff(kStatue, 0);
			accOn(kStatue, 1, 3);
			if (byte_(30040) && byte_(30041) && byte_(30042)) {
				show(kStatue, 4);
				accOn(kStatue, 4);
			} else {
				for (int k = 1; k <= 3; k++) {
					if (byte_(30039 + k)) {
						show(kStatue, k);
						accOff(kStatue, k);
					}
				}
			}
		} else if (h >= kPanel && h <= kPanel + 2 && value >= 1 && value <= 3) {
			setByte(30039 + value, 1);
			show(kStatue, value);
			accOff(kStatue, value);
			bag().remove(h);
			if (byte_(30040) && byte_(30041) && byte_(30042)) {
				cin("1167");
				for (int k = 1; k <= 3; k++)
					hide(kStatue, k);
				show(kStatue, 4);
				accOn(kStatue, 4);
				accOn(kDoorHare, 1);
			}
		}
		drop();
		break;
	case kPedestal:
	case kPedestal + 1:
	case kPedestal + 2:
	case kPedestal + 3:
	case kPedestal + 4:
	case kPedestal + 5:
	case kPedestal + 6: {
		int k = object - kStatue, slot = 30008 + k;
		if (!h) {
			if (byte_(slot) != 0) {
				bag().add(byte_(slot) + 30008);
				cin("1171");
				play(30109 + k);
				setByte(slot, 0);
			}
			break;
		}
		if (h >= kScroll && h <= kScroll + 6) {
			if (byte_(slot) == 0) {
				setByte(slot, h - 30008);
				bag().remove(h);
				cin("1168");
			} else {
				cin("1169");
			}
		}
		bool ordered = true;
		for (int i = 1; i <= 7; i++)
			ordered = ordered && byte_(30008 + i) == i;
		if (ordered) {
			for (int o = kPedestal; o <= kPedestal + 6; o++)
				accOff(o);
			cin("1170");
			score3(5.5);
			rotAct(30302);
			vm->timSto(1);
			vm->timSto(3);
		}
		drop();
		break;
	}
	case kBerries:
		if (h) {
			if (value == 5) {
				if (h == kPoison && byte_(30060) == 0) {
					bag().remove(kPoison);
					setByte(30063, 1);
					show(kBerries, 4);
				}
				if (held() == kBerries && byte_(30063) == 0) {
					bag().remove(kBerries);
					setByte(30060, 1);
					show(kBerries, 4);
				}
			}
			drop();
			break;
		}
		if (value >= 1 && value <= 3)
			bag().add(kBerries);
		if (byte_(30017) == 1 && (value == 0 || value == 4)) { // with the Wolf Vision
			bag().add(kPoison);
			scoreFirst(30201, 3.3);
			hide(kBerries, 3);
			setByte(30075, 1);
			accOff(kBerries, 0);
			bag().add(kPanel + 1);
			score3(3.3);
			hide(kBerries, 0);
			hide(kBerries, 2);
			setByte(30076, 1);
		}
		if (value == 5) { // the press
			if (byte_(30060) == 0 && byte_(30063) == 0)
				cin("1173");
			if (byte_(30060) == 1 && byte_(30061) == 0) {
				cin("1174");
				setByte(30061, 1);
				hide(kBerries, 4);
				show(kBerries, 5);
			} else if (byte_(30060) == 1 && byte_(30061) == 1) {
				bag().add(kJuice);
				hide(kBerries, 5);
				setByte(30060, 0);
				setByte(30061, 0);
			}
			if (byte_(30063) == 1 && byte_(30064) == 0) {
				cin("1175");
				setByte(30064, 1);
				hide(kBerries, 4);
				show(kBerries, 5);
			} else if (byte_(30063) == 1 && byte_(30064) == 1) {
				bag().add(kPoisonJuice);
				scoreFirst(30203, 6.6);
				hide(kBerries, 5);
				setByte(30063, 0);
				setByte(30064, 0);
			}
		}
		break;
	case kDoorHare:
		if (h) {
			if (h == kHare && value == 0) {
				bag().remove(kHare);
				accOff(kDoorHare, 0);
				cin("1176");
				vm->timSto(2);
				stop(30501);
				if (byte_(30077) == 0) {
					cin("1177");
					bag().add(kPanel + 2);
					score3(5.5);
					setByte(30077, 1);
				}
			}
			drop();
		} else if (value == 0) {
			cin("1178");
		} else if (value == 1) {
			if (byte_(30074) != 0) {
				cin("1179");
				rotAfter(30101, 130.0f, 20.0f);
			} else if (byte_(30056) != 0) {
				puz(35100);
			} else {
				cin("1178");
			}
		}
		break;
	case kWall:
		if (h) {
			drop();
		} else if (value == 0) {
			play(30511);
			show(kWall);
			accOff(kWall, 0);
			for (int k = 1; k <= 6; k++)
				if (byte_(30049 + k))
					hide(kWall, k);
		} else if (value >= 1 && value <= 6) {
			play(30512);
			hide(kWall, value);
			accOff(kWall, value);
			setByte(30049 + value, 1);
			if (value < 5) {
				bag().add(kHare);
				scoreFirst(30204, 1.1);
			} else {
				bag().add(kWall);
				score3(1.1);
				hide(kWall, 5);
				hide(kWall, 6);
				accOff(kWall, 5, 6);
				setByte(30054, 1);
				setByte(30055, 1);
			}
		}
		break;
	case kDialCentre:
		if (!h && byte_(kDial) == 0) {
			cin("1172");
			score3(3.3);
			volumes(30300, 30326, 90);
			setByte(30017, 0);
			setByte(30035, 1);
			movOn(30402, 2, 2);
			show(kTree, 0);
			show(kTree, 2);
			rotAfter(30501, 88.0f, 13.0f);
			for (int o = kPedestal; o <= kPedestal + 6; o++)
				accOff(o);
		}
		break;
	case kFurnace:
		if (h) {
			if (value == 1) {
				if (h == kTear) {
					if (byte_(30019) == 0) {
						cin("1180");
					} else {
						cin("1181");
						bag().remove(kTear);
						setByte(30021, 1);
						if (byte_(30020) == 0) {
							hide(kFurnace, 0);
							show(kFurnace, 1);
						}
					}
				}
				if (held() == kPan && byte_(30019) == 1 && byte_(30021) == 1) {
					hide(kFurnace, 0);
					show(kFurnace, 2);
					setByte(30020, 1);
					bag().remove(kPan);
				}
				if (held() == kIngots && byte_(30020) == 1) {
					bag().remove(kIngots);
					for (int m = kMetal; m <= kMetal + 6; m++)
						bag().add(m);
					bag().add(kTear);
					scoreFirst(30206, 4.4);
					bag().add(kPan);
					cin("1182");
					hide(kFurnace);
					hide(kFurnace, 3);
					setByte(30019, 0);
					setByte(30020, 0);
					setByte(30021, 0);
					accOn(kFurnace, 0);
				}
			}
			drop();
		} else if (value == 0 && byte_(30020) == 0) {
			cin("1183");
			show(kFurnace, 0);
			show(kFurnace, 3);
			setByte(30019, 1);
			accOff(kFurnace, 0);
		} else if (value == 1 && byte_(30021) == 1 && byte_(30020) == 0) {
			cin("1184");
			hide(kFurnace);
			show(kFurnace, 0);
			setByte(30021, 0);
			bag().add(kTear);
		} else if (value == 1 && byte_(30021) == 1 && byte_(30020) == 1) {
			hide(kFurnace, 2);
			show(kFurnace, 1);
			setByte(30020, 0);
			bag().add(kPan);
			score3(1.1);
		}
		break;
	case kMine:
		if (h)
			break;
		if (value == 0 && byte_(30022) == 0) {
			show(kMine, byte_(30024) == 0 ? 1 : 0);
			if (byte_(30024) == 0)
				accOn(kMine, 4);
			setByte(30022, 1);
			accOff(kMine, 0, 1);
			accOn(kMine, 2);
			play(30508);
		} else if (value == 1 && byte_(30023) == 0) {
			show(kMine, 2);
			setByte(30023, 1);
			accOff(kMine, 0, 1);
			accOn(kMine, 3);
			accOn(kMine, 5);
			play(30508);
		} else if (value == 2 && byte_(30022) == 1) {
			hide(kMine, 0);
			hide(kMine, 1);
			setByte(30022, 0);
			accOff(kMine, 2);
			accOff(kMine, 4);
			accOn(kMine, 1);
			accOn(kMine, 0);
			play(30508);
		} else if (value == 3 && byte_(30023) == 1) {
			hide(kMine, 2);
			setByte(30023, 0);
			accOff(kMine, 3);
			accOff(kMine, 5);
			accOn(kMine, 0);
			accOn(kMine, 1);
			play(30508);
		} else if (value == 4) {
			bag().add(kPan);
			show(kMine, 0);
			hide(kMine, 1);
			accOff(kMine, 4);
			setByte(30024, 1);
		} else if (value == 5) {
			bag().add(kIngots);
			scoreFirst(30205, 1.1);
			accOff(kMine, 5);
			accOff(kMine, 3);
			show(kMine, 4);
		}
		break;
	case kMold:
		if (h) {
			if (value >= 1 && value <= 7) {
				play(30509);
				if (h >= kMetal && h <= kMetal + 6) {
					setByte(30025 + (h - kMetal), value);
					bag().remove(h);
					show(kMold, value);
					accOff(kMold, value);
				}
				bool right = true, full = true;
				for (int i = 0; i < 7; i++) {
					right = right && byte_(30025 + i) == i + 1;
					full = full && byte_(30025 + i) != 0;
				}
				if (right) { // the Golem
					hide(kMold);
					score3(9.9);
					setByte(30032, 1);
					cin("1185");
					bag().add(kGolem);
					rotAct(30601);
					accOff(kMold);
				} else if (full) { // all used up, wrongly
					hide(kMold);
					cin("1186");
					rotAct(30601);
					accOn(kMold, 0);
					for (int i = 0; i < 7; i++)
						setByte(30025 + i, 0);
				}
			}
			drop();
		} else if (value == 0) {
			if (byte_(30019) != 0) {
				show(kMold, 0);
				hide(kMold, 8);
				hide(kFurnace, 0);
				hide(kFurnace, 3);
				setByte(30019, 0);
				accOn(kMold, 1, 7);
				accOff(kMold, 0);
				accOn(kFurnace, 0);
				cin("1187");
			} else {
				show(kMold, 8);
				hide(kMold, 0);
				accOff(kMold, 0);
				play(30508);
			}
		}
		break;
	case kGolemPlace:
		if (h == kGolem) {
			if (value == 0) {
				show(kGolemPlace);
				accOff(kGolemPlace, 0);
				accOn(kGolemPlace, 1);
				bag().remove(kGolem);
				cin("1188");
			} else if (value == 1) {
				show(kGolemPlace);
			}
		} else if (h == kPoisonJuice && value == 1) {
			hide(kGolemPlace);
			accOff(kGolemPlace, 1);
			bag().add(kGolem);
			bag().add(kPanel);
			cin("1189");
			movOff(30601, 9, 9);
			accOff(kGolemPlace);
			score3(3.3);
		}
		if (h)
			drop();
		break;
	case kBells:
		if (h)
			break;
		if (value >= 0 && value <= 6) {
			static const int sounds[] = { 30150, 30151, 30154, 30152, 30155, 30156, 30153 };
			hide(kBells);
			show(kBells, value);
			play(sounds[value]);
		} else if (value == 7) {
			play(30201);
			setByte(30047, 1);
			vm->rotSetRolTo(30602, 175.0f, -23.0f, 85.3f);
		}
		break;
	case kPole:
		if (h) {
			if (value == 3) {
				if (h == kPoleWorms) {
					if (byte_(30034) == 0) {
						bag().add(kFish);
						scoreFirst(30207, 2.2);
						cin("1190");
					} else {
						bag().add(kFishKey);
						score3(5.5);
						cin("1191");
					}
					bag().add(kPole);
					bag().remove(kPoleWorms);
					setByte(30038, 0);
				} else if (h == kPole) {
					cin("1192");
				}
			}
			drop();
		} else if (value == 0) {
			play(30508);
			if (byte_(30033) == 0) {
				hide(kPole, 0);
				show(kPole, 1);
				accOff(kPole, 0);
				accOn(kPole, 1, 2);
			} else {
				show(kPole, 0);
				hide(kPole, 1);
				accOff(kPole, 0, 1);
				accOn(kPole, 2);
			}
			setByte(30070, 1);
		} else if (value == 1) {
			hide(kPole, 1);
			show(kPole, 0);
			accOff(kPole, 0, 1);
			setByte(30033, 1);
			if (byte_(30038) != 0) {
				bag().add(kPoleWorms);
				scoreFirst(30208, 6.6);
				bag().remove(kWorms);
			} else {
				bag().add(kPole);
			}
		} else if (value == 2) {
			play(30511);
			if (byte_(30070) != 0) {
				hide(kPole);
				accOn(kPole, 0);
				accOff(kPole, 1, 2);
			} else {
				show(kPole, byte_(30033) ? 0 : 1);
			}
			setByte(30070, 0);
		}
		break;
	case kWorms:
		if (h == kGolem && byte_(30038) == 0) {
			if (byte_(30033) == 0) {
				bag().add(kWorms);
				scoreFirst(30209, 2.2);
			} else {
				bag().add(kPoleWorms);
				scoreFirst(30208, 6.6);
				bag().remove(kPole);
			}
			cin("1193");
			setByte(30038, 1);
			if (byte_(30076) == 0) {
				bag().add(kPanel + 1);
				bag().add(kPoison);
				score3(3.3);
				score3(3.3);
				setByte(30076, 1);
				setByte(30039, 1);
				cin("1194");
				hide(kWorms, 0);
				hide(kWorms, 1);
				hide(kBerries, 0);
				accOff(kWorms);
				drop();
				break;
			}
		}
		if (h)
			drop();
		break;
	case kTree:
		if (h)
			break;
		if (byte_(30035) == 0) {
			show(kTree, 1);
			play(30514);
		} else if (byte_(30037) == 1) {
			cin("1195");
			score3(4.4);
			show(kFire, 2);
			hide(kTree);
			setByte(30035, 0);
			setByte(30036, 1);
			setByte(30037, 0);
			movOff(30402, 2, 2);
			movOff(30011, 1, 1);
		}
		break;
	case kFire:
		if (h) {
			if ((value == 1 || value == 2) && h == kWall) {
				bag().add(kArrow);
				scoreFirst(30210, 4.4);
				bag().remove(kWall);
			}
			if (value == 0) {
				if (h == kArrow) {
					bag().remove(kArrow);
					bag().add(kWall);
					setByte(30034, 1);
					cin("1196");
					score3(5.5);
					hide(kFire, 2);
					accOff(kFire, 0);
					accOff(kFire, 3);
				}
				if (held() == kWall)
					cin("1197");
			} else if (value == 3) {
				if (h == kArrow) {
					cin("1198");
					bag().remove(kArrow);
					bag().add(kWall);
				}
				if (held() == kWall)
					cin("1197");
			}
			drop();
		}
		break;
	case kSword:
		if (h) {
			if (h == kMedallion || h == kSiegMedallion) {
				bag().remove(h);
				setByte(h == kMedallion ? 30066 : 30067, 1);
				if (byte_(30066) && byte_(30067)) {
					theEnd();
				} else {
					cin("1201");
					show(kSword, 2);
				}
			}
			drop();
		} else if (byte_(30066) || byte_(30067)) {
			cin("1202");
			hide(kSword, 2);
			if (byte_(30066)) {
				bag().add(kMedallion);
				setByte(30066, 0);
			} else {
				bag().add(kSiegMedallion);
				setByte(30067, 0);
			}
		}
		break;
	case kWalls:
		if (h) {
			drop();
		} else {
			cin(value == 0 ? "1203" : "1204");
			scoreFirst(30211, 1.1);
		}
		break;
	case kDoor:
		if (h) {
			drop();
		} else if (value == 0) { // the knock
			snd().setVolume(30506, 91 + rnd(10));
			movOff(35100, 0, 0);
			play(30506);
			accOff(kDoor, 0);
		} else if (value == 1) {
			accOff(kDoor, 1);
			puz(35101);
			play(30100);
		}
		break;
	case kSieglinde:
		if (h) {
			if (h == kMedallion && byte_(30072)) {
				puz(35104);
				play(30105);
			}
			drop();
		} else if (byte_(30072) == 0) {
			vm->rotSetRolTo(30101, 130.0f, 20.0f, 85.3f);
			puz(35104);
			play(30102);
		} else if (byte_(30078) == 0) {
			puz(35103);
			play(30118);
		}
		break;
	case kCup:
		if (h == kPoisonJuice) {
			cin("1199");
			hide(kHut, 1);
			show(kHut, 2);
			rotAfter(30101, 130.0f, 20.0f);
			accOff(kSieglinde, 0);
			setByte(30072, 1);
			score3(6.6);
		}
		if (h)
			drop();
		break;
	case kStory:
		if (h)
			drop();
		else
			puz(35109);
		break;
	case kLamps:
		if (h)
			drop();
		else
			play(30162 + value);
		break;
	default:
		break;
	}
}

void onBagClick(RingEngine *vm, int object) {
	// Only the Wolf Vision; each case keeps it out of the hand (app+0x78).
	if (object != kWolfVision)
		return;
	int place = vm->currentPlace();
	if (place == 30302 || place == 30303) {
		bool on = byte_(30017) != 1;
		Rotation *from = w().rotation(on ? 30302 : 30303), *to = w().rotation(on ? 30303 : 30302);
		if (from && to) {
			to->alpha = from->alpha;
			to->beta = from->beta;
			to->ran = from->ran;
			rotAct(to->id);
		}
		setByte(30017, on ? 1 : 0);
	} else if (place == 35020) {
		if (byte_(30017) == 0) {
			setByte(30017, 1);
			show(kWorms, 0);
			show(kWorms, 1);
			accOn(kWorms, 0);
		} else {
			setByte(30017, 0);
			hide(kWorms);
			accOff(kWorms, 0);
		}
	} else if (place == 35019) {
		if (byte_(30017) == 1) {
			setByte(30017, 0);
			hide(kSword);
			accOff(kSword, 0);
		} else {
			setByte(30017, 1);
			accOn(kSword, 0);
			show(kSword, 0);
			if (byte_(30066) || byte_(30067))
				show(kSword, 2);
		}
	} else if (place == 35002) {
		if (byte_(30017) == 0) {
			setByte(30017, 1);
			show(kBerries, 1);
			if (byte_(30075))
				hide(kBerries, 3);
			else
				show(kBerries, 3);
			if (byte_(30076))
				hide(kBerries, 2);
			else
				show(kBerries, 2);
		} else {
			hide(kBerries, 1);
			hide(kBerries, 2);
			hide(kBerries, 3);
			setByte(30017, 0);
		}
	} else {
		return; // elsewhere it goes in hand as any object
	}
	vm->keepBagObject();
}

void onDrag(RingEngine *vm, int object, int phase) {
	if (object != kDial)
		return;
	Drag &d = vm->drag();
	if (phase == 1) {
		d.mode = 2;
		play(30500, true);
		d.reference = Common::Point(440, 248);
	} else if (phase == 3) {
		// Around the reference: a clockwise move turns the dial on, a counter-clockwise one back.
		bool right = d.previous.x < d.current.x, left = d.previous.x > d.current.x;
		bool down = d.previous.y < d.current.y, up = d.previous.y > d.current.y;
		bool east = d.current.x > d.reference.x, west = d.current.x < d.reference.x;
		bool north = d.current.y < d.reference.y, south = d.current.y > d.reference.y;
		int step = 0;
		if (east && north)
			step = right && down ? 1 : left && up ? -1 : 0;
		else if (east && south)
			step = left && down ? 1 : right && up ? -1 : 0;
		else if (west && south)
			step = left && up ? 1 : right && down ? -1 : 0;
		else if (west && north)
			step = right && up ? 1 : left && down ? -1 : 0;
		if (step) {
			int v = byte_(kDial) + step;
			v = v > 48 ? 0 : v < 0 ? 48 : v;
			setByte(kDial, v);
			hide(kDial);
			show(kDial, v);
		}
		float dx = (float)(d.current.x - d.previous.x), dy = (float)(d.current.y - d.previous.y);
		snd().setVolume(30500, (int)(sqrtf(dx * dx + dy * dy) + 80.0f));
	} else if (phase == 2) {
		stop(30500);
	}
}

void onBeforeMove(RingEngine *vm, int from, int to, int kind) {
	if (kind == 0 && from == 30402 && to == 30501) {
		setByte(30037, 1);
		show(kFire, 0);
		hide(kTree);
		show(kTree, 3);
		movOff(30011, 1, 2);
		accOn(kFire, 0);
		accOn(kFire, 3);
	} else if (kind == 1 && from == 30601 && to == 35003) {
		accOn(kWall, 0);
	} else if (kind == 2) {
		if (from == 35002 && to == 30801 && byte_(30076) == 0) {
			hide(kBerries);
			setByte(30017, 0);
		} else if (from == 35003 && to == 30601) {
			hide(kWall);
		} else if (from == 35006 && to == 30601) {
			hide(kMine);
			setByte(30022, 0);
			setByte(30023, 0);
			accOn(kMine, 0, 1);
			accOff(kMine, 2, 5);
		} else if (from == 35007 && to == 30601) {
			hide(kMold);
			accOff(kMold, 1, 7);
			accOn(kMold, 0);
			for (int i = 0; i < 7; i++)
				setByte(30025 + i, 0);
		} else if (from == 35004 && to == 30601 && byte_(30019) && byte_(30021)) {
			cin("1184");
			bag().add(kTear);
			if (byte_(30020)) {
				bag().add(kPan);
				setByte(30020, 0);
			}
			hide(kFurnace);
			show(kFurnace, 0);
			show(kFurnace, 3);
			setByte(30021, 0);
		} else if (from == 35008 && to == 30601 && byte_(30032)) {
			bag().add(kGolem);
			hide(kGolemPlace, 0);
		} else if (from == 35009 && to == 30601) {
			hide(kBells);
		} else if (from == 35009 && to == 30602) {
			cin("1205");
			show(kBells, 7);
		}
		// The Wolf Vision ends with any puzzle left.
		accOff(kSword, 0);
		hide(kSword);
		hide(kWorms, 0);
		hide(kWorms, 1);
		hide(kBerries, 3);
		hide(kBerries, 1);
		setByte(30017, 0);
		accOff(kWorms, 0);
	}
}

void onAfterMove(RingEngine *vm, int to, int from, int kind) {
	if (kind == 0) {
		if (to == 30501 && from == 30012) {
			vm->timSto(1);
			vm->timSto(3);
			if (byte_(30071) == 0) {
				play(30117);
				setByte(30071, 1);
			}
		} else if (to == 30012 && from == 30501) {
			restart(1, 10);
			restart(3, 10);
		} else if (to == 30401 && from == 30011) {
			volumes(30300, 30326, 85);
		} else if (to == 30011 && from == 30401) {
			volumes(30300, 30326, 100);
		} else if (to == 30101 && from == 30003) {
			volumes(30300, 30313, 80);
			volumes(30300, 30313, 100);
		} else if (to == 30008 && from == 30006) {
			volumes(30300, 30313, 80);
		} else if (to == 30006 && from == 30008) {
			volumes(30300, 30313, 100);
		} else if (to == 30006 && from == 30005) {
			w().setRide(30005, 1, "fom");
		}
	} else if (kind == 2) {
		if (to == 30703 && from == 35010) {
			accOn(kPole, 0);
			accOff(kPole, 1, 2);
			hide(kPole);
		} else if (to == 30301 && from == 35011) {
			hide(kStatue);
			accOff(kStatue, 1, 3);
			if (byte_(30043) == 0)
				accOn(kStatue, 0);
		}
	}
}

void onTimer(RingEngine *vm, int id) {
	switch (id) {
	case 0: { // the lightning
		static const uint32 next[] = { 100, 10, 100, 0 };
		int state = byte_(30073);
		if (state == 0 || state == 2)
			show(kLightning);
		else
			hide(kLightning);
		setByte(30073, (state + 1) % 4);
		restart(0, state == 3 ? (rnd(10) + 15) * 1000 : next[state]);
		break;
	}
	case 1: {
		int s = 30301 + rnd(15);
		snd().setPan(s, 10 - rnd(20));
		play(s);
		restart(1, (rnd(10) + 10) * 500);
		break;
	}
	case 2: // the hare
		w().pauseAnimations(kDoorHare, 0, false);
		vm->timSto(2);
		break;
	case 3: {
		int s = 30316 + rnd(3);
		snd().setVolume(s, 95);
		play(s);
		restart(3, (rnd(10) + 30) * 500);
		break;
	}
	case 4: {
		int s = 30319 + rnd(9);
		snd().setPan(s, 5 - rnd(10));
		play(s);
		restart(4, (rnd(10) + 5) * 4000);
		break;
	}
	case 5:
		w().pauseAnimations(kHut, 1, false);
		w().pauseAnimations(kStory, 0, false);
		accOff(kStory, 0);
		restart(5, rnd(10) * 2000 + 15000);
		break;
	default:
		break;
	}
}

void onAnimation(RingEngine *vm, int id, int frame) {
	switch (id) {
	case 30000: // the bells' ride
		if (frame == 28 && byte_(30047)) {
			cin("1207");
			setByte(30047, 0);
			puz(35009);
		}
		break;
	case 30001:
	case 30007: {
		bool story = id == 30007;
		if (frame == 10) {
			play(30505);
			snd().setVolume(30505, story ? 95 : 90);
		} else if (frame == 90) {
			play(30503);
			snd().setVolume(30503, story ? 95 : 90);
		} else if (frame == 125) {
			play(30504);
			snd().setVolume(30504, story ? 100 : 95);
		} else if (frame == 202) { // only the story's animation has so many frames (Q-0050)
			if (story) {
				w().pauseAnimations(kStory, 0, true);
				accOn(kStory, 0);
			} else {
				w().pauseAnimations(kHut, 1, true);
			}
		}
		break;
	}
	case 30006: // the hare
		if (frame == 10) {
			play(30502);
		} else if (frame == 26) {
			w().pauseAnimations(kDoorHare, 0, true);
			restart(2, (rnd(10) + 5) * 1000);
		}
		break;
	case 30008:
		if (frame == 25) {
			accOn(kMine, 5);
			accOn(kMine, 3);
		}
		break;
	default:
		break;
	}
}

void onSound(RingEngine *vm, int id, int type, int reason, int ended) {
	if (!ended)
		return;
	// The ending: words freed and shown over a tour of the forest (0x442e40, 30120..30136).
	static const struct {
		int sound, free1, free2, rotation, show1, show2;
	} ending[] = {
		{ 30120, 0, -1, 30002, 1, -1 }, { 30121, 1, 5, 30003, 2, 6 }, { 30122, 2, -1, 30004, 3, -1 },
		{ 30123, 3, 6, 30005, 4, 7 }, { 30124, 4, 7, 30006, 8, 9 }, { 30125, 9, -1, 30008, 10, -1 },
		{ 30126, 8, 10, 30009, 11, 15 }, { 30127, 11, -1, 30010, 12, -1 }, { 30128, 12, 15, 30011, 13, 16 },
		{ 30129, 13, -1, 30012, 14, -1 }, { 30130, 14, 16, 30701, 17, 18 }, { 30131, 18, -1, 30702, 19, -1 },
		{ 30132, 17, 19, 30703, 20, 24 }, { 30133, 20, 24, 30704, 21, 25 }, { 30134, 21, -1, 30401, 22, -1 },
		{ 30135, 22, 25, 30402, 23, 26 }
	};
	for (const auto &e : ending) {
		if (e.sound != id)
			continue;
		w().hideAndFree(kWords, e.free1);
		if (e.free2 >= 0)
			w().hideAndFree(kWords, e.free2);
		rotAct(e.rotation);
		play(id + 1);
		show(kWords, e.show1);
		if (e.show2 >= 0)
			show(kWords, e.show2);
		return;
	}
	// Sieglinde's story: "v -> p, n" = PlyCin(v), PuzSetAct(p), play(n).
	static const struct {
		int sound;
		const char *video;
		int puzzle, next;
	} story[] = {
		{ 30102, nullptr, 35103, 30103 }, { 30103, nullptr, 35104, 30104 }, { 30104, "1208", 35105, 30106 },
		{ 30106, "1209", 35110, 30107 }, { 30107, "1210", 35106, 30108 }, { 30108, "1211", 35107, 30161 },
		{ 30161, "1212", 35108, 30109 }
	};
	for (const auto &s : story) {
		if (s.sound != id)
			continue;
		if (s.video)
			cin(s.video);
		puz(s.puzzle);
		play(s.next);
		return;
	}
	switch (id) {
	case 30136: // FO is done
		w().hideAndFree(kWords, 23);
		w().hideAndFree(kWords, 26);
		bag().removeAll();
		vm->timStoAll();
		snd().stopAll(0x400);
		cin("1215");
		AS::returnFromWorld(vm, 3);
		break;
	case 30100:
		play(30101);
		break;
	case 30101:
		cin("1179");
		rotAfter(30101, 130.0f, 20.0f);
		if (byte_(30074) == 0)
			score3(2.2);
		setByte(30074, 1);
		break;
	case 30506: // the knock
		if (byte_(30043) == 1 && bag().has(kPoisonJuice)) {
			cin("1216");
			show(kDoor, 0);
			accOff(kDoor, 0);
			accOn(kDoor, 1);
		} else {
			accOn(kDoor, 0);
			movOn(35100, 0, 0);
		}
		break;
	case 30109:
		cin("1213");
		puz(35111);
		show(kStory, 0);
		break;
	case 30105: // the medallion shown to her
		cin("1214");
		bag().add(kSiegMedallion);
		score3(3.3);
		show(kHut, 3);
		accOff(kSieglinde, 1);
		rotAfter(30101, 130.0f, 20.0f);
		break;
	case 30118:
		rotAfter(30101, 130.0f, 20.0f);
		setByte(30078, 1);
		break;
	default:
		break;
	}
}

} // End of namespace FO
} // End of namespace Ring
