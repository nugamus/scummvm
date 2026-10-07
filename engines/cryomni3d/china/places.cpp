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

// China's place procedures: one function per place, in the game's list order. Each runs
// its entry part once on arrival, then its event part every frame.
// Generated from the place logic specification; regenerate rather than edit.

#include "cryomni3d/china/engine.h"

namespace CryOmni3D {
namespace China {

// Game variables (the variable table's order, E-0208)
enum {
	kV_MODE_VISITE = 0,
	kV_CHAPITRE = 1,
	kV_NWFH401 = 2,
	kV_Venant_de_BOUTON = 3,
	kV_DAMH501 = 4,
	kV_Venant_de_BOUDDHA = 5,
	kV_Liste_Boites = 6,
	kV_PDC_ouvert = 7,
	kV_Ruyi = 8,
	kV_Venant_de_HORLOGE = 9,
	kV_ANXN2121 = 10,
	kV_ANDA2141 = 11,
	kV_DAMD2131 = 12,
	kV_DAMD1011 = 13,
	kV_DAMD2011 = 14,
	kV_DAMD2021 = 15,
	kV_DAMD2111 = 16,
	kV_DAMD2121 = 17,
	kV_DAMD3111 = 20,
	kV_DAMD3121 = 21,
	kV_DAMD3211 = 22,
	kV_ENED3111 = 23,
	kV_GIAD3111 = 24,
	kV_GIBD3111 = 25,
	kV_GIED3111 = 26,
	kV_GIFD3111 = 27,
	kV_GIMD3211 = 28,
	kV_GIND3211 = 29,
	kV_GIMD3221 = 30,
	kV_GIND3221 = 31,
	kV_GIID4011 = 32,
	kV_GIJD4011 = 33,
	kV_GIKD4011 = 34,
	kV_GILD3111 = 35,
	kV_GILD4011 = 36,
	kV_GIKDIK11 = 37,
	kV_GISD4011 = 38,
	kV_GITD3111 = 39,
	kV_GITD4011 = 40,
	kV_GISD3111 = 41,
	kV_EGJD1011 = 42,
	kV_EGJD4011 = 43,
	kV_ANEJ4111 = 44,
	kV_XPRD4111 = 45,
	kV_EGJD1021 = 46,
	kV_EGJD1041 = 47,
	kV_JIXW111_ouvert = 48,
	kV_XNED1011 = 50,
	kV_GICD1011 = 51,
	kV_GIDD1011 = 52,
	kV_GICD3111 = 53,
	kV_GIDD3111 = 54,
	kV_ANXN1111 = 55,
	kV_ANXN1121 = 56,
	kV_ANXN1151 = 57,
	kV_BOUDDHA = 58,
	kV_ENED1011 = 59,
	kV_ANXN2111 = 60,
	kV_ANXN1131 = 61,
	kV_ANXN1141 = 62,
	kV_ANXN2011 = 63,
	kV_ANXN2021 = 64,
	kV_ANXN2131 = 65,
	kV_ANXN2141 = 66,
	kV_PNEH201 = 68,
	kV_Burin = 69,
	kV_Tournevis = 70,
	kV_Cle_Wang_Utilisee = 71,
	kV_ANEN3121 = 72,
	kV_GIGD3111 = 73,
	kV_GCD3111 = 74,
	kV_GIGD4011 = 75,
	kV_GIHD4011 = 76,
	kV_GIHD3111 = 77,
	kV_GIG3111 = 78,
	kV_GIID3111 = 79,
	kV_GIJ3111 = 80,
	kV_ANXI1011 = 83,
	kV_ANXI1121 = 84,
	kV_ANXI1111 = 85,
	kV_ANXI3011 = 86,
	kV_3GIED11 = 87,
	kV_ANXI3151 = 88,
	kV_ANMI3211 = 89,
	kV_Cache_Wen = 91,
	kV_3GIMD01 = 94,
	kV_ANMI4021 = 95,
	kV_MINXQ411 = 96,
	kV_ANMI4111 = 97,
	kV_ANMW2031 = 99,
	kV_Venant_de_GO = 100,
	kV_Premiere_entree_ESP = 101,
	kV_MWED1011 = 102,
	kV_ANMW2011 = 103,
	kV_ANMW201A = 104,
	kV_GO = 105,
	kV_ANMW3151 = 107,
	kV_XPFD3211 = 116,
	kV_ANEF3231 = 117,
	kV_EGCD2111 = 118,
	kV_EGCD2141 = 119,
	kV_EGCD4111 = 121,
	kV_ANXP4211 = 122,
	kV_EGCD4211 = 123,
	kV_Mandat_Montre_EGC = 124,
	kV_ANXQ2111 = 126,
	kV_XPRD2111 = 127,
	kV_ORGH211 = 128,
	kV_Pierre_ouverte = 132,
	kV_Cle_jarre_prise = 133,
	kV_VAR_Venant_de_PUZZLE4 = 136,
	kV_VAR_Venant_de_JIX111 = 137,
	kV_VAR_ANID1121 = 138,
	kV_VAR_ANID1131 = 139,
	kV_VAR_Pieces_prises = 140,
	kV_VAR_LGE_entree = 142,
	kV_ANEP4211 = 143,
	kV_ANEQ4111 = 144,
	kV_EQRD4211 = 148,
	kV_PDCH501 = 149,
	kV_C520_ouvert = 150,
	kV_C510_ouvert = 151,
	kV_ANEQ4311 = 152,
	kV_DQRD4211 = 153,
	kV_DQRD4321 = 156,
	kV_PENJING = 157,
	kV_ORGH221 = 158,
	kV_CQRD4311 = 159,
	kV_Marteau = 162,
	kV_MWED5011 = 163,
	kV_Pinceau_mouille = 164,
	kV_crans = 165,
	kV_ANXF3231 = 166,
	kV_ANMW2021 = 168,
	kV_PRND1011 = 169,
	kV_PRND2011 = 170,
	kV_ANMW2111 = 171,
	kV_ANMW2121 = 172,
	kV_ANMW3111 = 174,
	kV_ANMW3121 = 175,
	kV_ANMW4111 = 176,
	kV_ANXI2011 = 178,
	kV_ANXI2021 = 179,
	kV_ANXI2111 = 180,
	kV_ANXI2131 = 182,
	kV_ANXI3111 = 183,
	kV_ANXI3121 = 184,
	kV_ANDQ4231 = 185,
	kV_ANEQ4241 = 186,
	kV_ANXQ2121 = 188,
	kV_XPRD2131 = 189,
	kV_XPRD2141 = 190,
	kV_ANMI4311 = 192,
	kV_ANMW3211 = 194,
	kV_ANXI2211 = 195,
	kV_EPFD3211 = 196,
	kV_MWED1111 = 201,
	kV_XPID3122 = 203,
	kV_entree_PPF = 204,
	kV_GITD3221 = 205,
	kV_GISD3221 = 206,
	kV_ANXP4111 = 207,
	kV_GISD3211 = 208,
	kV_GITD3211 = 209,
	kV_FIGHTED = 210,
	kV_LVICT = 211,
	kV_ANCP4101 = 212,
	kV_XPRD4101 = 213,
	kV_T14 = 214,
	kV_VAR_LETTRE_REVELEE = 215,
	kV_Aide_Go_1 = 216,
	kV_MWED2131 = 217,
	kV_ANMI5121 = 218,
	kV_ANMI5111 = 219,
	kV_var220 = 220,
	kV_var221 = 221,
};

// Objects (E-0905)
enum {
	kO_LISTE_BOITES = 0,
	kO_ORIGINAUX = 1,
	kO_POSTHUME = 2,
	kO_CONFES1 = 3,
	kO_CONFES2 = 4,
	kO_CONFES3 = 5,
	kO_CONFES4 = 6,
	kO_INDIC1 = 7,
	kO_INDIC2 = 8,
	kO_INDIC3 = 9,
	kO_INDIC4 = 10,
	kO_LISTE_VICTIMES = 11,
	kO_PROCLA = 12,
	kO_EDI = 13,
	kO_LETTRE_VIERGE = 14,
	kO_PLBOMB = 16,
	kO_INDICE_CACHETS = 17,
	kO_INDICE_CACHETS2 = 18,
	kO_SCEAUX = 19,
	kO_TOURNEVIS = 20,
	kO_CIRE = 21,
	kO_RUYI = 22,
	kO_BURIN = 24,
	kO_MARTEAU = 25,
	kO_CLE_WANG = 26,
	kO_CURE_DENTS = 27,
	kO_PINCEAU_ESP = 28,
	kO_PIECES = 29,
	kO_MANDAT1 = 30,
	kO_MANDAT2 = 31,
	kO_MANDAT3 = 32,
	kO_MANDAT4 = 33,
	kO_CLE_JARRE = 34,
};

// Script_Start (0x436db0)
static void place_Script_Start(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.setVar(kV_CHAPITRE, 1);
		g.minutesAdd("MIN001");
		g.setAngles(4.7, 0.0);
		g.gotoPlace("pne140");
		return;
	}
	g.zoneHandler();
}

// cth140 (0x436cc0)
static void place_cth140(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(215, 1420, 475, 1675, 0, "cth210", 0, -1.0, -1.0);
		g.zoneGo(200, 0, 458, 121, 0, "cth130", 0, -1.0, -1.0);
		g.zoneGo(200, 1931, 458, 2047, 0, "cth130", 0, -1.0, -1.0);
		g.zoneGo(224, 954, 426, 1124, 0, "cth150", 0, -1.0, -1.0);
		g.zoneDoc(80, 168, 467, 870, 0, "shs");
		g.warp("cth140");
	}
	g.zoneHandler();
}

// cth210 (0x436ba0)
static void place_cth210(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(215, 376, 480, 640, 0, "cth140", 0, -1.0, -1.0);
		g.zoneGo(242, 27, 458, 216, 0, "cth130", 0, -1.0, -1.0);
		g.zoneGo(260, 1723, 430, 1858, 0, "cth230", 0, -1.0, -1.0);
		g.zoneGo(252, 1449, 433, 1634, 0, "cth220", 0, -1.0, -1.0);
		g.zoneGo(251, 1214, 431, 1360, 0, "cth290", 0, -1.0, -1.0);
		g.zoneGo(232, 846, 435, 1018, 0, "cth150", 0, -1.0, -1.0);
		g.warp("cth210");
	}
	g.zoneHandler();
}

// cth130 (0x436a60)
static void place_cth130(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(220, 955, 430, 1100, 0, "cth140", 0, -1.0, -1.0);
		g.zoneGo(230, 128, 410, 271, 0, "cth110", 0, -1.0, -1.0);
		g.zoneGo(200, 0, 445, 80, 0, "cth120", 0, -1.0, -1.0);
		g.zoneGo(200, 1905, 445, 2047, 0, "cth120", 0, -1.0, -1.0);
		g.zoneGo(230, 1129, 431, 1267, 0, "cth210", 0, -1.0, -1.0);
		g.zoneGo(256, 1380, 405, 1480, 0, "cth230", 0, -1.0, -1.0);
		g.zoneDoc(130, 284, 450, 840, 0, "shs");
		g.zoneLabel(463, 1471, 616, 1634, 0, "rampe");
		g.warp("cth130");
	}
	g.zoneHandler();
}

// cth230 (0x436970)
static void place_cth230(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(205, 392, 437, 547, 0, "cth130", 0, -1.0, -1.0);
		g.zoneGo(222, 1442, 479, 1634, 0, "cth240", 0, -1.0, -1.0);
		g.zoneGo(222, 1018, 460, 1184, 0, "cth220", 0, -1.0, -1.0);
		g.zoneGo(219, 815, 466, 993, 0, "cth210", 0, -1.0, -1.0);
		g.zoneDoc(148, 1120, 441, 1492, 0, "shm");
		g.warp("cth230");
	}
	g.zoneHandler();
}

// cth120 (0x4368a0)
static void place_cth120(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(204, 402, 432, 620, 0, "cth110", 0, -1.0, -1.0);
		g.zoneGo(201, 900, 452, 1144, 0, "cth130", 0, -1.0, -1.0);
		g.zoneDoc(210, 630, 418, 900, 0, "shs");
		g.zoneLabel(310, 1175, 391, 1370, 0, "shm");
		g.zoneLabel(309, 1375, 384, 1479, 0, "shp");
		g.warp("cth120");
	}
	g.zoneHandler();
}

// cth110 (0x4367e0)
static void place_cth110(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(190, 1400, 470, 1651, 0, "cth120", 0, -1.0, -1.0);
		g.zoneGo(230, 1149, 452, 1287, 0, "cth130", 0, -1.0, -1.0);
		g.zoneGo(340, 480, 420, 542, 0, "chs150", 0, -1.0, -1.0);
		g.zoneDoc(170, 684, 430, 1108, 0, "shs");
		g.warp("cth110");
	}
	g.zoneHandler();
}

// cth150 (0x4366b0)
static void place_cth150(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(233, 0, 455, 139, 0, "cth140", 0, -1.0, -1.0);
		g.zoneGo(233, 2000, 455, 2047, 0, "cth140", 0, -1.0, -1.0);
		g.zoneGo(270, 1590, 410, 1671, 0, "cth290", 0, -1.0, -1.0);
		g.zoneGo(247, 1827, 438, 1962, 0, "cth210", 0, -1.0, -1.0);
		g.zoneGo(211, 950, 445, 1130, 0, "cth160", 0, -1.0, -1.0);
		g.zoneGo(226, 760, 410, 900, 0, "cth170", 0, -1.0, -1.0);
		g.zoneDoc(135, 180, 450, 724, 0, "shs");
		g.warp("cth150");
	}
	g.zoneHandler();
}

// cth170 (0x4365d0)
static void place_cth170(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(210, 1403, 473, 1649, 0, "cth160", 0, -1.0, -1.0);
		g.zoneGo(250, 1775, 433, 1961, 0, "cth150", 0, -1.0, -1.0);
		g.zoneGo(338, 480, 418, 547, 0, "chs110", 0, -1.0, -1.0);
		g.zoneDoc(182, 1972, 438, 2047, 0, "shs");
		g.zoneDoc(182, 0, 438, 332, 0, "shs");
		g.warp("cth170");
	}
	g.zoneHandler();
}

// cth160 (0x4364e0)
static void place_cth160(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(220, 380, 438, 608, 0, "cth170", 0, -1.0, -1.0);
		g.zoneGo(200, 1906, 460, 2047, 0, "cth150", 0, -1.0, -1.0);
		g.zoneGo(200, 0, 460, 143, 0, "cth150", 0, -1.0, -1.0);
		g.zoneDoc(200, 80, 420, 375, 0, "shs");
		g.zoneLabel(315, 1682, 390, 1898, 0, "shm");
		g.zoneLabel(306, 1584, 386, 1676, 0, "shp");
		g.warp("cth160");
	}
	g.zoneHandler();
}

// cth290 (0x4363f0)
static void place_cth290(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(240, 1897, 453, 2046, 0, "cth220", 0, -1.0, -1.0);
		g.zoneGo(190, 1450, 475, 1673, 0, "cth280", 0, -1.0, -1.0);
		g.zoneGo(232, 500, 429, 633, 0, "cth150", 0, -1.0, -1.0);
		g.zoneGo(240, 37, 453, 210, 0, "cth210", 0, -1.0, -1.0);
		g.zoneDoc(185, 1562, 450, 1945, 0, "shm");
		g.warp("cth290");
	}
	g.zoneHandler();
}

// cth220 (0x436330)
static void place_cth220(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(250, 1907, 438, 2047, 0, "cth230", 0, -1.0, -1.0);
		g.zoneGo(250, 1007, 435, 1149, 0, "cth290", 0, -1.0, -1.0);
		g.zoneGo(180, 360, 495, 644, 0, "cth210", 0, -1.0, -1.0);
		g.zoneDoc(80, 1342, 476, 1740, 0, "shm");
		g.warp("cth220");
	}
	g.zoneHandler();
}

// cth280 (0x436280)
static void place_cth280(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(210, 1454, 430, 1636, 0, "cth270", 0, -1.0, -1.0);
		g.zoneGo(220, 420, 445, 600, 0, "cth290", 0, -1.0, -1.0);
		g.zoneDoc(109, 1840, 524, 2047, 0, "shm");
		g.zoneDoc(109, 0, 524, 185, 0, "shm");
		g.warp("cth280");
	}
	g.zoneHandler();
}

// cth240 (0x4361e0)
static void place_cth240(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(190, 1405, 442, 1620, 0, "cth250", 0, -1.0, -1.0);
		g.zoneGo(190, 440, 438, 617, 0, "cth230", 0, -1.0, -1.0);
		g.zoneDoc(120, 800, 494, 1223, 0, "shm");
		g.warp("cth240");
	}
	g.zoneHandler();
}

// cth250 (0x4360d0)
static void place_cth250(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(225, 439, 455, 623, 0, "cth240", 0, -1.0, -1.0);
		g.zoneGo(236, 1724, 440, 1927, 0, "cth340", 0, -1.0, -1.0);
		g.zoneGo(233, 1531, 438, 1703, 0, "cth330", 0, -1.0, -1.0);
		g.zoneGo(230, 1085, 455, 1284, 0, "cth310", 0, -1.0, -1.0);
		g.zoneGo(248, 875, 454, 1040, 0, "cth260", 0, -1.0, -1.0);
		g.zoneDoc(162, 550, 448, 907, 0, "shm");
		g.warp("cth250");
	}
	g.zoneHandler();
}

// cth270 (0x435fa0)
static void place_cth270(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(221, 0, 448, 134, 0, "cth260", 0, -1.0, -1.0);
		g.zoneGo(221, 1994, 448, 2047, 0, "cth260", 0, -1.0, -1.0);
		g.zoneGo(220, 1780, 449, 1970, 0, "cth310", 0, -1.0, -1.0);
		g.zoneGo(229, 1439, 436, 1600, 0, "cth360", 0, -1.0, -1.0);
		g.zoneGo(221, 1165, 435, 1385, 0, "cth370", 0, -1.0, -1.0);
		g.zoneGo(244, 403, 455, 607, 0, "cth280", 0, -1.0, -1.0);
		g.zoneDoc(186, 83, 438, 434, 0, "shm");
		g.warp("cth270");
	}
	g.zoneHandler();
}

// cth260 (0x435eb0)
static void place_cth260(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(248, 847, 449, 1036, 0, "cth270", 0, -1.0, -1.0);
		g.zoneGo(208, 1394, 490, 1661, 0, "cth310", 0, -1.0, -1.0);
		g.zoneGo(228, 0, 442, 140, 0, "cth250", 0, -1.0, -1.0);
		g.zoneGo(228, 2000, 442, 2047, 0, "cth250", 0, -1.0, -1.0);
		g.zoneDoc(80, 323, 468, 690, 0, "shm");
		g.warp("cth260");
	}
	g.zoneHandler();
}

// cth310 (0x435d90)
static void place_cth310(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(200, 1410, 460, 1650, 0, "cth320", 0, -1.0, -1.0);
		g.zoneGo(215, 1103, 450, 1330, 0, "cth360", 0, -1.0, -1.0);
		g.zoneGo(240, 697, 442, 892, 0, "cth270", 0, -1.0, -1.0);
		g.zoneGo(220, 400, 448, 618, 0, "cth260", 0, -1.0, -1.0);
		g.zoneGo(226, 119, 425, 308, 0, "cth250", 0, -1.0, -1.0);
		g.zoneGo(221, 1733, 437, 1958, 0, "cth330", 0, -1.0, -1.0);
		g.warp("cth310");
	}
	g.zoneHandler();
}

// cth330 (0x435c60)
static void place_cth330(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(240, 1001, 439, 1110, 0, "cth320", 0, -1.0, -1.0);
		g.zoneGo(240, 810, 440, 962, 0, "cth310", 0, -1.0, -1.0);
		g.zoneGo(270, 500, 431, 650, 0, "cth250", 0, -1.0, -1.0);
		g.zoneGo(210, 0, 455, 136, 0, "cth340", 0, -1.0, -1.0);
		g.zoneGo(210, 1940, 455, 2047, 0, "cth340", 0, -1.0, -1.0);
		g.zoneGo(260, 1716, 430, 1872, 0, "cth350", 0, -1.0, -1.0);
		g.zoneDoc(108, 1212, 460, 1680, 0, "shp");
		g.warp("cth330");
	}
	g.zoneHandler();
}

// cth340 (0x435ba0)
static void place_cth340(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(240, 1429, 480, 1653, 0, "cth350", 0, -1.0, -1.0);
		g.zoneGo(230, 960, 440, 1140, 0, "cth330", 0, -1.0, -1.0);
		g.zoneGo(280, 683, 420, 802, 0, "cth250", 0, -1.0, -1.0);
		g.zoneDoc(180, 1120, 420, 1327, 0, "shp");
		g.warp("cth340");
	}
	g.zoneHandler();
}

// cth320 (0x435ab0)
static void place_cth320(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(205, 1948, 436, 2047, 0, "cth330", 0, -1.0, -1.0);
		g.zoneGo(205, 0, 436, 128, 0, "cth330", 0, -1.0, -1.0);
		g.zoneGo(210, 903, 441, 1126, 0, "cth360", 0, -1.0, -1.0);
		g.zoneGo(210, 380, 488, 660, 0, "cth310", 0, -1.0, -1.0);
		g.zoneDoc(110, 1180, 464, 1880, 0, "shp");
		g.warp("cth320");
	}
	g.zoneHandler();
}

// cth360 (0x435980)
static void place_cth360(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(270, 1220, 430, 1353, 0, "cth380", 0, -1.0, -1.0);
		g.zoneGo(210, 853, 448, 1090, 0, "cth370", 0, -1.0, -1.0);
		g.zoneGo(300, 420, 412, 524, 0, "cth270", 0, -1.0, -1.0);
		g.zoneGo(240, 56, 449, 220, 0, "cth310", 0, -1.0, -1.0);
		g.zoneGo(243, 1960, 439, 2047, 0, "cth320", 0, -1.0, -1.0);
		g.zoneDoc(100, 1382, 460, 1840, 0, "shp");
		g.zoneGo(243, 0, 439, 15, 0, "cth320", 0, -1.0, -1.0);
		g.warp("cth360");
	}
	g.zoneHandler();
}

// cth350 (0x435880)
static void place_cth350(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(240, 711, 420, 850, 0, "cth330", 0, -1.0, -1.0);
		g.zoneGo(190, 390, 440, 630, 0, "cth340", 0, -1.0, -1.0);
		g.zoneDoc(135, 861, 433, 1110, 0, "shp");
		g.zoneGo(309, 1421, 529, 1645, 0, "cgc220", 0, 3.15, 0.0);
		g.zoneDoc(439, 953, 580, 1091, 0, "jarres");
		g.zoneDoc(436, 1990, 570, 2047, 0, "jarres");
		g.zoneDoc(436, 0, 570, 62, 0, "jarres");
		g.warp("cth350");
	}
	g.zoneHandler();
}

// cth380 (0x435770)
static void place_cth380(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(200, 399, 448, 661, 0, "cth370", 0, -1.0, -1.0);
		g.zoneGo(240, 199, 430, 327, 0, "cth360", 0, -1.0, -1.0);
		g.zoneDoc(130, 1953, 436, 2047, 0, "shp");
		g.zoneDoc(130, 0, 436, 158, 0, "shp");
		g.zoneGo(310, 1415, 525, 1636, 0, "cgc210", 0, 3.15, 0.0);
		g.zoneDoc(441, 969, 572, 1092, 0, "jarres");
		g.zoneDoc(439, 1970, 584, 2047, 0, "jarres");
		g.zoneDoc(439, 0, 584, 56, 0, "jarres");
		g.warp("cth380");
	}
	g.zoneHandler();
}

// cth370 (0x435680)
static void place_cth370(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(241, 1438, 461, 1640, 0, "cth380", 0, -1.0, -1.0);
		g.zoneGo(210, 1933, 438, 2047, 0, "cth360", 0, -1.0, -1.0);
		g.zoneGo(210, 0, 438, 75, 0, "cth360", 0, -1.0, -1.0);
		g.zoneGo(270, 189, 424, 319, 0, "cth270", 0, -1.0, -1.0);
		g.zoneDoc(160, 1745, 420, 1932, 0, "shp");
		g.warp("cth370");
	}
	g.zoneHandler();
}

// chs150 (0x435590)
static void place_chs150(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(289, 1500, 462, 1627, 0, "cth110", 0, -1.0, -1.0);
		g.zoneGo(220, 840, 434, 1010, 0, "chs140", 0, -1.0, -1.0);
		g.zoneDoc(130, 1040, 455, 1230, 0, "shs");
		g.zoneDoc(369, 1243, 494, 1342, 0, "jarres");
		g.zoneDoc(372, 1745, 484, 1842, 0, "jarres");
		g.zoneLabel(352, 539, 391, 615, 0, "phs");
		g.zoneLabel(344, 223, 405, 342, 0, "pba");
		g.warp("chs150");
	}
	g.zoneHandler();
}

// chs110 (0x4354a0)
static void place_chs110(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(295, 1486, 453, 1612, 0, "cth170", 0, -1.0, -1.0);
		g.zoneGo(220, 7, 450, 184, 0, "chs120", 0, -1.0, -1.0);
		g.zoneDoc(130, 1836, 444, 2029, 0, "shs");
		g.zoneDoc(368, 1254, 488, 1344, 0, "jarres");
		g.zoneDoc(371, 1715, 480, 1820, 0, "jarres");
		g.zoneLabel(353, 401, 391, 479, 0, "phs");
		g.zoneLabel(343, 676, 399, 796, 0, "pnr");
		g.warp("chs110");
	}
	g.zoneHandler();
}

// chs120 (0x435350)
static void place_chs120(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(230, 1986, 449, 2047, 0, "chs130", 0, -1.0, -1.0);
		g.zoneGo(230, 0, 449, 95, 0, "chs130", 0, -1.0, -1.0);
		g.zoneGo(230, 125, 447, 313, 0, "chs250", 0, -1.0, -1.0);
		g.zoneGo(230, 471, 445, 670, 0, "chs240", 0, -1.0, -1.0);
		g.zoneGo(230, 970, 445, 1135, 0, "chs110", 0, -1.0, -1.0);
		g.zoneDoc(100, 1257, 475, 1805, 0, "shs");
		g.zoneDoc(180, 1805, 444, 1947, 0, "shs");
		g.zoneLabel(347, 726, 400, 846, 0, "pnr");
		g.zoneLabel(382, 699, 421, 720, 0, "tortue");
		g.warp("chs120");
	}
	g.zoneHandler();
}

// chs130 (0x435220)
static void place_chs130(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(235, 1430, 401, 1652, 0, "chs010", 0, -1.0, -1.0);
		g.zoneGo(240, 1910, 429, 2043, 0, "chs140", 0, -1.0, -1.0);
		g.zoneGo(230, 104, 440, 280, 0, "chs260", 0, -1.0, -1.0);
		g.zoneGo(200, 372, 470, 639, 0, "chs250", 0, -1.0, -1.0);
		g.zoneGo(230, 760, 442, 928, 0, "chs240", 0, -1.0, -1.0);
		g.zoneGo(240, 980, 440, 1130, 0, "chs120", 0, -1.0, -1.0);
		g.zoneDoc(150, 1200, 453, 1864, 0, "shs");
		g.warp("chs130");
	}
	g.zoneHandler();
}

// chs250 (0x435030)
static void place_chs250(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(240, 1418, 464, 1628, 0, "chs130", 0, -1.0, -1.0);
		g.zoneGo(270, 1771, 434, 1899, 0, "chs140", 0, -1.0, -1.0);
		g.zoneGo(257, 0, 450, 80, 0, "chs260", 0, -1.0, -1.0);
		g.zoneGo(260, 100, 440, 210, 0, "chs230", 0, -1.0, -1.0);
		g.zoneGo(230, 399, 490, 627, 0, "chs220", 0, -1.0, -1.0);
		g.zoneGo(260, 820, 440, 932, 0, "chs210", 0, -1.0, -1.0);
		g.zoneGo(260, 959, 450, 1104, 0, "chs240", 0, -1.0, -1.0);
		g.zoneGo(270, 1150, 439, 1287, 0, "chs120", 0, -1.0, -1.0);
		g.zoneLabel(208, 1318, 402, 1764, 0, "shs");
		g.zoneGo(257, 1972, 450, 2047, 0, "chs260", 0, -1.0, -1.0);
		g.zoneLabel(381, 258, 415, 270, 0, "brule");
		g.zoneLabel(381, 378, 421, 392, 0, "brule");
		g.zoneLabel(381, 644, 420, 658, 0, "brule");
		g.zoneLabel(380, 758, 412, 770, 0, "brule");
		g.warp("chs250");
	}
	g.zoneHandler();
}

// chs240 (0x434ea0)
static void place_chs240(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(240, 1400, 480, 1575, 0, "chs120", 0, -1.0, -1.0);
		g.zoneGo(240, 1810, 450, 1940, 0, "chs130", 0, -1.0, -1.0);
		g.zoneGo(240, 1975, 451, 2047, 0, "chs250", 0, -1.0, -1.0);
		g.zoneGo(240, 0, 451, 120, 0, "chs250", 0, -1.0, -1.0);
		g.zoneGo(240, 150, 450, 272, 0, "chs220", 0, -1.0, -1.0);
		g.zoneGo(230, 520, 480, 702, 0, "chs210", 0, -1.0, -1.0);
		g.zoneLabel(342, 751, 412, 764, 0, "grue");
		g.zoneLabel(382, 1008, 421, 1045, 0, "tortue");
		g.zoneLabel(341, 750, 399, 870, 0, "pnr");
		g.zoneLabel(381, 277, 415, 290, 0, "brule");
		g.zoneLabel(381, 404, 421, 418, 0, "brule");
		g.zoneLabel(348, 426, 394, 512, 0, "phs");
		g.warp("chs240");
	}
	g.zoneHandler();
}

// chs140 (0x434d50)
static void place_chs140(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(230, 1947, 440, 2047, 0, "chs150", 0, -1.0, -1.0);
		g.zoneGo(230, 0, 440, 60, 0, "chs150", 0, -1.0, -1.0);
		g.zoneGo(240, 349, 459, 513, 0, "chs260", 0, -1.0, -1.0);
		g.zoneGo(230, 714, 461, 884, 0, "chs250", 0, -1.0, -1.0);
		g.zoneGo(230, 920, 460, 1060, 0, "chs130", 0, -1.0, -1.0);
		g.zoneDoc(100, 1210, 470, 1850, 0, "shs");
		g.zoneDoc(197, 1105, 432, 1209, 0, "shs");
		g.zoneLabel(350, 503, 390, 583, 0, "phs");
		g.zoneLabel(351, 180, 403, 294, 0, "pba");
		g.warp("chs140");
	}
	g.zoneHandler();
}

// chs010 (0x434c80)
static void place_chs010(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(225, 384, 485, 637, 0, "chs130", 0, -1.0, -1.0);
		g.zoneGo(110, 1408, 492, 1672, 0, "shs140", 0, -1.0, -1.0);
		g.zoneLabel(354, 165, 397, 264, 0, "pba");
		g.zoneLabel(352, 751, 393, 853, 0, "pnr");
		g.zoneLabel(500, 438, 614, 581, 0, "rampe");
		g.warp("chs010");
	}
	g.zoneHandler();
}

// chs260 (0x434b20)
static void place_chs260(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(267, 378, 461, 577, 0, "chs230", 0, -1.0, -1.0);
		g.zoneGo(275, 742, 448, 843, 0, "chs220", 0, -1.0, -1.0);
		g.zoneGo(269, 882, 447, 1047, 0, "chs250", 0, -1.0, -1.0);
		g.zoneGo(270, 1149, 454, 1298, 0, "chs130", 0, -1.0, -1.0);
		g.zoneGo(240, 1445, 452, 1678, 0, "chs140", 0, -1.0, -1.0);
		g.zoneLabel(336, 308, 414, 324, 0, "grue");
		g.zoneLabel(400, 2025, 442, 2047, 0, "tortue");
		g.zoneLabel(385, 0, 442, 24, 0, "tortue");
		g.zoneLabel(340, 152, 397, 272, 0, "pba");
		g.zoneLabel(381, 640, 420, 655, 0, "brule");
		g.warp("chs260");
	}
	g.zoneHandler();
}

// chs210 (0x4349e0)
static void place_chs210(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(288, 1415, 454, 1599, 0, "chs240", 0, -1.0, -1.0);
		g.zoneGo(230, 1843, 437, 2028, 0, "chs250", 0, -1.0, -1.0);
		g.zoneGo(230, 0, 433, 155, 0, "chs220", 0, -1.0, -1.0);
		g.zoneLabel(287, 1075, 447, 1116, 0, "grue");
		g.zoneLabel(380, 173, 434, 197, 0, "brule");
		g.zoneLabel(380, 391, 470, 435, 0, "brule");
		g.zoneLabel(335, 756, 483, 800, 1, "edicule");
		g.zoneLabel(385, 1365, 415, 1388, 0, "tortue");
		g.zoneLabel(347, 416, 392, 509, 0, "phs");
		g.zoneLabel(339, 759, 401, 906, 0, "pnr");
		g.warp("chs210");
	}
	g.zoneHandler();
}

// chs220 (0x434830)
static void place_chs220(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(255, 939, 446, 1040, 0, "chs210", 0, -1.0, -1.0);
		g.zoneGo(251, 1078, 443, 1189, 0, "chs240", 0, -1.0, -1.0);
		g.zoneGo(238, 1428, 488, 1632, 0, "chs250", 0, -1.0, -1.0);
		g.zoneGo(257, 1871, 442, 1985, 0, "chs260", 0, -1.0, -1.0);
		g.zoneGo(257, 0, 439, 88, 0, "chs230", 0, -1.0, -1.0);
		g.zoneGo(257, 2009, 439, 2047, 0, "chs230", 0, -1.0, -1.0);
		g.zoneLabel(381, 205, 441, 228, 0, "brule");
		g.zoneLabel(377, 801, 436, 825, 0, "brule");
		g.zoneLabel(345, 463, 396, 554, 0, "phs");
		g.zoneLabel(380, 908, 419, 922, 0, "brule");
		g.zoneLabel(383, 101, 425, 117, 0, "brule");
		g.zoneLabel(345, 95, 394, 206, 0, "pba");
		g.zoneLabel(348, 803, 396, 929, 0, "pnr");
		g.warp("chs220");
	}
	g.zoneHandler();
}

// chs230 (0x4346e0)
static void place_chs230(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(200, 1415, 464, 1653, 0, "chs260", 0, -1.0, -1.0);
		g.zoneGo(230, 1052, 448, 1240, 0, "chs250", 0, -1.0, -1.0);
		g.zoneGo(230, 864, 444, 1022, 0, "chs220", 0, -1.0, -1.0);
		g.zoneLabel(297, 1962, 448, 2001, 0, "grue");
		g.zoneLabel(365, 236, 484, 282, 0, "astronomie");
		g.zoneLabel(378, 547, 475, 591, 0, "brule");
		g.zoneLabel(378, 813, 435, 839, 0, "brule");
		g.zoneLabel(381, 1688, 414, 1712, 0, "tortue");
		g.zoneLabel(349, 507, 394, 603, 0, "phs");
		g.zoneLabel(339, 122, 401, 250, 0, "pba");
		g.zoneDoc(331, 1968, 368, 1995, 0, "animaux_reels");
		g.warp("chs230");
	}
	g.zoneHandler();
}

// shs140 (0x434580)
static void place_shs140(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(255, 1460, 451, 1660, 0, "shs240", 0, -1.0, -1.0);
		g.zoneGo(278, 935, 435, 1073, 0, "shs130", 0, -1.0, -1.0);
		g.zoneGo(194, 332, 510, 613, 0, "chs010", 0, -1.0, -1.0);
		g.zoneGo(309, 0, 455, 110, 0, "shs150", 0, -1.0, -1.0);
		g.zoneGo(309, 1957, 455, 2047, 0, "shs150", 0, -1.0, -1.0);
		g.zoneLabel(42, 1467, 100, 1625, 0, "plafond");
		g.zoneLabel(32, 1240, 495, 1297, 0, "colonne_or");
		g.zoneLabel(40, 1812, 477, 1863, 0, "colonne_or");
		g.zoneLabel(161, 1445, 383, 1464, 0, "colonne_or");
		g.zoneLabel(161, 1625, 376, 1645, 0, "colonne_or");
		g.warp("shs140");
	}
	g.zoneHandler();
}

// shs270 (0x4344a0)
static void place_shs270(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(255, 349, 507, 650, 0, "shs170", 0, -1.0, -1.0);
		g.zoneGo(268, 905, 502, 1188, 0, "shs260", 0, -1.0, -1.0);
		g.zoneLabel(342, 1832, 446, 1891, 0, "armoire");
		g.zoneLabel(332, 32, 456, 95, 0, "armoire");
		g.zoneLabel(227, 1890, 258, 2047, 0, "vairocana");
		g.zoneLabel(227, 0, 258, 18, 0, "vairocana");
		g.warp("shs270");
	}
	g.zoneHandler();
}

// shs260 (0x4343d0)
static void place_shs260(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(300, 894, 457, 1145, 0, "shs250", 0, -1.0, -1.0);
		g.zoneGo(274, 367, 483, 677, 0, "shs160", 0, -1.0, -1.0);
		g.zoneGo(300, 0, 495, 178, 0, "shs270", 0, -1.0, -1.0);
		g.zoneGo(300, 1871, 495, 2047, 0, "shs270", 0, -1.0, -1.0);
		g.warp("shs260");
	}
	g.zoneHandler();
}

// shs250 (0x434300)
static void place_shs250(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(253, 864, 501, 1184, 0, "shs240", 0, -1.0, -1.0);
		g.zoneGo(262, 365, 489, 670, 0, "shs150", 0, -1.0, -1.0);
		g.zoneGo(314, 0, 517, 217, 0, "shs260", 0, -1.0, -1.0);
		g.zoneGo(314, 1875, 517, 2047, 0, "shs260", 0, -1.0, -1.0);
		g.warp("shs250");
	}
	g.zoneHandler();
}

// shs240 (0x4340b0)
static void place_shs240(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(290, 965, 513, 1186, 0, "shs230", 0, -1.0, -1.0);
		g.zoneGo(262, 356, 511, 681, 0, "shs140", 0, -1.0, -1.0);
		g.zoneGo(290, 0, 500, 181, 0, "shs250", 0, -1.0, -1.0);
		g.zoneGo(290, 1869, 500, 2047, 0, "shs250", 0, -1.0, -1.0);
		g.zoneLabel(40, 211, 484, 267, 0, "colonne_or");
		g.zoneLabel(40, 749, 480, 802, 0, "colonne_or");
		g.zoneLabel(75, 1347, 387, 1385, 0, "colonne_or");
		g.zoneLabel(75, 1691, 378, 1726, 0, "colonne_or");
		g.zoneTake(285, 1498, 387, 1570, 1, nullptr);
		g.zoneLabel(285, 1498, 387, 1570, 0, "trone_shs");
		g.zoneLabel(333, 1288, 376, 1317, 0, "BRULE");
		g.zoneLabel(318, 1402, 368, 1437, 0, "BRULE");
		g.zoneLabel(319, 1639, 372, 1673, 0, "BRULE");
		g.zoneLabel(329, 1759, 372, 1784, 0, "BRULE");
		g.zoneLabel(259, 1737, 382, 1749, 0, "BRULE");
		g.zoneLabel(258, 1326, 382, 1337, 0, "BRULE");
		g.zoneLabel(413, 1460, 572, 1634, 0, "tapis");
		g.warp("shs240");
		if (((int32)g.var(kV_CHAPITRE) >= 16) && ((int32)g.var(kV_Venant_de_HORLOGE) != 0)) {
			g.zoneEnable(8);
		}
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 8) && (g.puzzle(8, 0) != 0)) {
		g.screenEffect();
		g.video("fin");
		g.screenEffect();
		g.unknownCall(0x405aa0);
		g.setMem(0x48f200, 0);
		g.setMem(0x48f1fc, 1);
	}
}

// trone (0x434030)
static void place_trone(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(404, 0, 479, 639, 0, "shs240", 0, -1.0, -1.0);
		g.zoneTake(173, 196, 324, 432, 0, "bombe1");
		g.image("trone");
	}
	g.zoneHandler();
}

// bombe1 (0x433f80)
static void place_bombe1(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(404, 0, 479, 639, 0, "shs240", 0, -1.0, -1.0);
		g.zoneUse(209, 270, 289, 421, 0);
		g.image("bombe1");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 1) && ((g.heldObject() == kO_TOURNEVIS ? 1 : 0) != 0)) {
		g.soundQueue("bombe2a");
		g.gotoPlace("bombe2");
	}
}

// bombe2 (0x433f40)
static void place_bombe2(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.image("bombe2");
	}
	g.zoneHandler();
}

// shs230 (0x433e70)
static void place_shs230(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(314, 1905, 538, 2047, 0, "shs240", 0, -1.0, -1.0);
		g.zoneGo(314, 0, 538, 185, 0, "shs240", 0, -1.0, -1.0);
		g.zoneGo(245, 353, 498, 685, 0, "shs130", 0, -1.0, -1.0);
		g.zoneGo(269, 928, 512, 1194, 0, "shs220", 0, -1.0, -1.0);
		g.warp("shs230");
	}
	g.zoneHandler();
}

// shs220 (0x433da0)
static void place_shs220(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(275, 1873, 501, 2047, 0, "shs230", 0, -1.0, -1.0);
		g.zoneGo(275, 0, 501, 172, 0, "shs230", 0, -1.0, -1.0);
		g.zoneGo(286, 373, 487, 662, 0, "shs120", 0, -1.0, -1.0);
		g.zoneGo(274, 939, 481, 1174, 0, "shs210", 0, -1.0, -1.0);
		g.warp("shs220");
	}
	g.zoneHandler();
}

// shs210 (0x433cb0)
static void place_shs210(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(271, 1908, 495, 2047, 0, "shs220", 0, -1.0, -1.0);
		g.zoneGo(271, 0, 495, 200, 0, "shs220", 0, -1.0, -1.0);
		g.zoneGo(241, 350, 497, 658, 0, "shs110", 0, -1.0, -1.0);
		g.zoneDoc(329, 921, 473, 989, 0, "mobilier");
		g.zoneDoc(338, 1189, 450, 1250, 0, "mobilier");
		g.zoneLabel(218, 1006, 256, 1174, 0, "vairocana");
		g.warp("shs210");
	}
	g.zoneHandler();
}

// shs170 (0x433c20)
static void place_shs170(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(265, 908, 511, 1132, 0, "shs160", 0, -1.0, -1.0);
		g.zoneGo(243, 1364, 501, 1681, 0, "shs270", 0, -1.0, -1.0);
		g.warp("shs170");
	}
	g.zoneHandler();
}

// shs160 (0x433b50)
static void place_shs160(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(269, 927, 494, 1128, 0, "shs150", 0, -1.0, -1.0);
		g.zoneGo(247, 1379, 527, 1663, 0, "shs260", 0, -1.0, -1.0);
		g.zoneGo(277, 1846, 524, 2047, 0, "shs170", 0, -1.0, -1.0);
		g.zoneGo(277, 0, 524, 122, 0, "shs170", 0, -1.0, -1.0);
		g.warp("shs160");
	}
	g.zoneHandler();
}

// shs150 (0x433a80)
static void place_shs150(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(290, 929, 509, 1206, 0, "shs140", 0, -1.0, -1.0);
		g.zoneGo(275, 1411, 507, 1714, 0, "shs250", 0, -1.0, -1.0);
		g.zoneGo(292, 0, 494, 234, 0, "shs160", 0, -1.0, -1.0);
		g.zoneGo(292, 1919, 494, 2047, 0, "shs160", 0, -1.0, -1.0);
		g.warp("shs150");
	}
	g.zoneHandler();
}

// shs130 (0x4339b0)
static void place_shs130(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(263, 915, 506, 1138, 0, "shs120", 0, -1.0, -1.0);
		g.zoneGo(241, 1404, 506, 1644, 0, "shs230", 0, -1.0, -1.0);
		g.zoneGo(252, 1889, 524, 2047, 0, "shs140", 0, -1.0, -1.0);
		g.zoneGo(252, 0, 524, 179, 0, "shs140", 0, -1.0, -1.0);
		g.warp("shs130");
	}
	g.zoneHandler();
}

// shs120 (0x4338e0)
static void place_shs120(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(278, 927, 494, 1116, 0, "shs110", 0, -1.0, -1.0);
		g.zoneGo(268, 1436, 496, 1707, 0, "shs220", 0, -1.0, -1.0);
		g.zoneGo(286, 0, 480, 142, 0, "shs130", 0, -1.0, -1.0);
		g.zoneGo(286, 1926, 480, 2047, 0, "shs130", 0, -1.0, -1.0);
		g.warp("shs120");
	}
	g.zoneHandler();
}

// shs110 (0x433830)
static void place_shs110(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(267, 1429, 490, 1649, 0, "shs210", 0, -1.0, -1.0);
		g.zoneGo(290, 0, 495, 163, 0, "shs120", 0, -1.0, -1.0);
		g.zoneGo(290, 1938, 495, 2047, 0, "shs120", 0, -1.0, -1.0);
		g.warp("shs110");
	}
	g.zoneHandler();
}

// cpc330 (0x433740)
static void place_cpc330(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(278, 1940, 621, 2047, 0, "cpc340", 0, -1.0, -1.0);
		g.zoneGo(278, 0, 621, 134, 0, "cpc340", 0, -1.0, -1.0);
		g.zoneGo(213, 946, 500, 1084, 0, "cpc320", 0, -1.0, -1.0);
		g.zoneGo(317, 1462, 503, 1616, 0, "cpc440", 0, -1.0, -1.0);
		g.zoneDoc(196, 215, 500, 599, 0, "poc");
		g.warp("cpc330");
	}
	g.zoneHandler();
}

// cpc720 (0x4335f0)
static void place_cpc720(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(210, 369, 480, 620, 0, "cpc600", 0, -1.0, -1.0);
		g.zoneGo(207, 1900, 487, 2047, 0, "cpc730", 0, -1.0, -1.0);
		g.zoneGo(207, 0, 487, 137, 0, "cpc730", 0, -1.0, -1.0);
		g.zoneGo(210, 860, 490, 1172, 0, "cpc710", 0, -1.0, -1.0);
		g.zoneLabel(361, 166, 450, 206, 0, "brule");
		g.zoneLabel(361, 800, 455, 843, 0, "brule");
		g.zoneDoc(242, 1300, 407, 1760, 0, "ppc");
		g.zoneDoc(195, 1410, 241, 1668, 0, "ppc");
		g.zoneDoc(372, 1212, 471, 1290, 0, "jarres");
		g.zoneDoc(378, 1815, 463, 1885, 0, "jarres");
		g.warp("cpc720");
	}
	g.zoneHandler();
}

// cpc350 (0x433500)
static void place_cpc350(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(217, 918, 524, 1093, 0, "cpc340", 0, -1.0, -1.0);
		g.zoneGo(305, 1461, 501, 1632, 0, "cpc540", 0, -1.0, -1.0);
		g.zoneGo(231, 2003, 472, 2047, 0, "cpc360", 0, -1.0, -1.0);
		g.zoneGo(231, 0, 472, 79, 0, "cpc360", 0, -1.0, -1.0);
		g.zoneDoc(211, 415, 518, 787, 0, "poc");
		g.warp("cpc350");
	}
	g.zoneHandler();
}

// cpc600 (0x433470)
static void place_cpc600(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(264, 389, 489, 634, 0, "cpc340", 0, -1.0, -1.0);
		g.zoneGo(279, 1405, 496, 1657, 0, "cpc720", 0, -1.0, -1.0);
		g.warp("cpc600");
	}
	g.zoneHandler();
}

// cpc340 (0x433380)
static void place_cpc340(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(262, 1412, 482, 1655, 0, "cpc600", 0, -1.0, -1.0);
		g.zoneGo(234, 1977, 531, 2047, 0, "cpc350", 0, -1.0, -1.0);
		g.zoneGo(234, 0, 531, 108, 0, "cpc350", 0, -1.0, -1.0);
		g.zoneGo(244, 967, 484, 1071, 0, "cpc330", 0, -1.0, -1.0);
		g.zoneDoc(184, 216, 512, 760, 0, "poc");
		g.warp("cpc340");
	}
	g.zoneHandler();
}

// cpc440 (0x4332b0)
static void place_cpc440(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(271, 1183, 497, 1359, 0, "cpc430", 0, -1.0, -1.0);
		g.zoneGo(269, 451, 436, 569, 0, "cpc330", 0, -1.0, -1.0);
		g.zoneGo(320, 1488, 448, 1579, 0, "cpc450", 0, -1.0, -1.0);
		g.zoneGo(355, 890, 449, 953, 0, "cpc420", 0, -1.0, -1.0);
		g.warp("cpc440");
	}
	g.zoneHandler();
}

// cpc320 (0x4331d0)
static void place_cpc320(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(256, 1955, 571, 2047, 0, "cpc330", 0, -1.0, -1.0);
		g.zoneGo(256, 0, 571, 109, 0, "cpc330", 0, -1.0, -1.0);
		g.zoneGo(244, 489, 575, 610, 0, "cpc310", 0, -1.0, -1.0);
		g.zoneLabel(330, 1520, 375, 1656, 0, "ppc");
		g.zoneDoc(173, 122, 499, 449, 0, "poc");
		g.warp("cpc320");
	}
	g.zoneHandler();
}

// cpc450 (0x433120)
static void place_cpc450(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(272, 1488, 369, 1584, 0, "cpc710", 0, -1.0, -1.0);
		g.zoneGo(343, 471, 427, 535, 0, "cpc440", 0, -1.0, -1.0);
		g.zoneGo(256, 723, 504, 928, 0, "cpc430", 0, -1.0, -1.0);
		g.warp("cpc450");
	}
	g.zoneHandler();
}

// cpc310 (0x433090)
static void place_cpc310(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(225, 1455, 575, 1611, 0, "cpc320", 0, -1.0, -1.0);
		g.zoneGo(304, 997, 439, 1050, 0, "cpc140", 0, -1.0, -1.0);
		g.warp("cpc310");
	}
	g.zoneHandler();
}

// cpc140 (0x432fa0)
static void place_cpc140(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(326, 2027, 442, 2047, 0, "cpc310", 0, -1.0, -1.0);
		g.zoneGo(326, 0, 442, 20, 0, "cpc310", 0, -1.0, -1.0);
		g.zoneGo(318, 998, 437, 1045, 0, "cpc130", 0, -1.0, -1.0);
		g.zoneGo(426, 1182, 533, 1278, 0, "cpc420", 0, -1.0, -1.0);
		g.zoneLabel(319, 1592, 377, 1700, 0, "ppc");
		g.warp("cpc140");
	}
	g.zoneHandler();
}

// cpc130 (0x432eb0)
static void place_cpc130(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(332, 2013, 464, 2047, 0, "cpc140", 0, -1.0, -1.0);
		g.zoneGo(332, 0, 464, 30, 0, "cpc140", 0, -1.0, -1.0);
		g.zoneGo(299, 1494, 457, 1559, 0, "cpc120", 0, -1.0, -1.0);
		g.zoneGo(380, 1689, 512, 1818, 0, "cpc420", 0, -1.0, -1.0);
		g.zoneLabel(320, 1650, 382, 1732, 0, "ppc");
		g.warp("cpc130");
	}
	g.zoneHandler();
}

// cpc120 (0x432ce0)
static void place_cpc120(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(311, 472, 486, 570, 0, "cpc130", 0, -1.0, -1.0);
		g.zoneGo(322, 1566, 441, 1588, 0, "cpc110", 0, -1.0, -1.0);
		g.zoneGo(284, 1938, 600, 2047, 0, "cpc420", 0, -1.0, -1.0);
		g.zoneGo(284, 0, 600, 149, 0, "cpc420", 0, -1.0, -1.0);
		g.zoneLabel(318, 1660, 373, 1760, 0, "ppc");
		g.zoneGo(254, 1320, 499, 1408, 0, nullptr, 0, 0.0, 0.0);
		g.zoneLabel(196, 940, 570, 1276, 0, "bpi");
		g.warp("cpc120");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 5) {
		if ((((int32)g.var(kV_CHAPITRE) >= 0) && ((int32)g.var(kV_CHAPITRE) <= 9)) || (((int32)g.var(kV_CHAPITRE) == 10) && ((g.objectState(kO_INDICE_CACHETS2) != 0 ? 1 : 0) == 0))) {
			g.setAngles(3.2, 0.0);
			g.gotoPlace("bpiw101");
			return;
		}
		if (((int32)g.var(kV_CHAPITRE) > 10) || (((int32)g.var(kV_CHAPITRE) == 10) && ((g.objectState(kO_INDICE_CACHETS2) != 0 ? 1 : 0) != 0))) {
			g.setAngles(3.2, 0.0);
			g.gotoPlace("bpiw102");
		}
	}
}

// cpc110 (0x432880)
static void place_cpc110(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(351, 500, 448, 526, 0, "cpc120", 0, -1.0, -1.0);
		g.zoneGo(270, 1916, 568, 2047, 0, "cpc410", 0, -1.0, -1.0);
		g.zoneGo(270, 0, 568, 114, 0, "cpc410", 0, -1.0, -1.0);
		g.zoneLabel(306, 1674, 380, 1790, 0, "ppc");
		g.zoneLabel(320, 145, 396, 230, 0, "poc");
		g.zoneGo(224, 916, 503, 1119, 0, "aio200", 0, -1.0, -1.0);
		g.zoneDoc(386, 755, 545, 792, 0, "gardes");
		g.zoneDoc(386, 1199, 566, 1248, 0, "gardes");
		g.zoneTalk(350, 762, 385, 786, 1);
		g.zoneTalk(348, 1204, 385, 1234, 1);
		g.warp("cpc110");
		if ((((int32)g.var(kV_CHAPITRE) == 9) && (((int32)g.var(kV_GIAD3111) == 1) || ((int32)g.var(kV_GIBD3111) == 1)) && ((int32)g.var(kV_GIGD3111) == 0) && ((int32)g.var(kV_GICD3111) == 0)) || (((int32)g.var(kV_CHAPITRE) == 12) && ((int32)g.var(kV_GIGD4011) == 0) && ((int32)g.var(kV_GIHD4011) == 0))) {
			g.zoneEnable(8);
		}
		if ((((int32)g.var(kV_CHAPITRE) == 9) && ((int32)g.var(kV_GIHD3111) == 0) && ((int32)g.var(kV_GIG3111) == 0) && (((int32)g.var(kV_GIAD3111) == 1) || ((int32)g.var(kV_GIBD3111) == 1))) || (((int32)g.var(kV_CHAPITRE) == 12) && ((int32)g.var(kV_GIGD4011) == 0) && ((int32)g.var(kV_GIHD4011) == 0))) {
			g.zoneEnable(9);
		}
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 8) {
		if (((int32)g.var(kV_CHAPITRE) == 9) && (((int32)g.var(kV_GIAD3111) == 1) || ((int32)g.var(kV_GIBD3111) == 1)) && ((int32)g.var(kV_GIGD3111) == 0) && ((int32)g.var(kV_GCD3111) == 0)) {
			g.dialogue("GIGD3111", "E110GIG", "E110ANJ");
			g.minutesAdd("MINCP311");
			g.minutesAdd("MINCP312");
			g.setVar(kV_GIGD3111, 1);
			g.zoneDisable(8);
			g.zoneDisable(9);
		}
		if (((int32)g.var(kV_CHAPITRE) == 12) && ((int32)g.var(kV_GIGD4011) == 0) && ((int32)g.var(kV_GIHD4011) == 0)) {
			g.dialogue("GIGD4011", "E110GIG", "E110ANJ");
			g.setVar(kV_GIGD4011, 1);
			g.zoneDisable(8);
			g.zoneDisable(9);
		}
	}
	if (g.clickedZone() == 9) {
		if (((int32)g.var(kV_CHAPITRE) == 9) && (((int32)g.var(kV_GIAD3111) == 1) || ((int32)g.var(kV_GIBD3111) == 1)) && ((int32)g.var(kV_GIHD3111) == 0) && ((int32)g.var(kV_GIG3111) == 0)) {
			g.dialogue("GIGD3111", "E110GIG", "E110ANJ");
			g.minutesAdd("MINCP311");
			g.minutesAdd("MINCP312");
			g.setVar(kV_GIGD3111, 1);
			g.zoneDisable(9);
			g.zoneDisable(8);
		}
		if (((int32)g.var(kV_CHAPITRE) == 12) && ((int32)g.var(kV_GIGD4011) == 0) && ((int32)g.var(kV_GIHD4011) == 0)) {
			g.dialogue("GIHD4011", "E110GIH", "E110ANJ");
			g.setVar(kV_GIHD4011, 1);
			g.zoneDisable(9);
			g.zoneDisable(8);
		}
	}
}

// cpc410 (0x432650)
static void place_cpc410(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(211, 933, 397, 1105, 0, "cpc110", 0, -1.0, -1.0);
		g.zoneGo(310, 502, 443, 602, 0, "cpc420", 0, -1.0, -1.0);
		g.zoneGo(248, 1935, 511, 2047, 0, "cpc430", 0, -1.0, -1.0);
		g.zoneGo(248, 0, 511, 105, 0, "cpc430", 0, -1.0, -1.0);
		g.zoneLabel(290, 1654, 361, 1760, 0, "ppc");
		g.zoneLabel(300, 180, 376, 290, 0, "poc");
		g.zoneLabel(326, 140, 376, 179, 0, "poc");
		g.zoneLabel(326, 1761, 362, 1801, 0, "ppc");
		g.zoneLabel(321, 1616, 359, 1653, 0, "ppc");
		g.zoneGo(388, 1779, 408, 1803, 1, nullptr, 0, 0.0, 0.0);
		g.warp("cpc410");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_CHAPITRE) == 9) && (((int32)g.var(kV_GIGD3111) == 1) || ((int32)g.var(kV_GIHD3111) == 1)) && ((g.objectState(kO_INDICE_CACHETS) != 0 ? 1 : 0) == 0)) {
		g.zoneEnable(9);
	}
	if ((g.clickedZone() == 9) && (((int32)g.var(kV_GIGD3111) == 1) || ((int32)g.var(kV_GIHD3111) == 1))) {
		g.setVar(kV_CHAPITRE, 10);
		g.objectToInventory(kO_INDICE_CACHETS);
		g.video("GRTH301");
		g.dialogue("ANJGRT34", "R100ANJ", nullptr);
		g.minutesAdd("MINCP319");
		g.zoneDisable(9);
		g.setAngles(1.39, 0.0);
		g.gotoPlace("bpiw201");
	}
}

// cpc420 (0x432510)
static void place_cpc420(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(304, 372, 501, 685, 0, "cpc130", 0, -1.0, -1.0);
		g.zoneGo(337, 246, 437, 317, 0, "cpc140", 0, -1.0, -1.0);
		g.zoneGo(324, 1450, 500, 1580, 0, "cpc410", 0, -1.0, -1.0);
		g.zoneGo(276, 971, 497, 1126, 0, "cpc120", 0, -1.0, -1.0);
		g.zoneGo(257, 1699, 501, 1936, 0, "cpc430", 0, -1.0, -1.0);
		g.zoneGo(321, 2009, 442, 2047, 0, "cpc440", 0, -1.0, -1.0);
		g.zoneGo(321, 0, 442, 56, 0, "cpc440", 0, -1.0, -1.0);
		g.warp("cpc420");
	}
	g.zoneHandler();
}

// cpc430 (0x432410)
static void place_cpc430(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(329, 943, 486, 1064, 0, "cpc410", 0, -1.0, -1.0);
		g.zoneGo(344, 675, 455, 764, 0, "cpc420", 0, -1.0, -1.0);
		g.zoneGo(345, 256, 459, 375, 0, "cpc440", 0, -1.0, -1.0);
		g.zoneGo(340, 1643, 465, 1747, 0, "cpc450", 0, -1.0, -1.0);
		g.zoneLabel(325, 1535, 380, 1750, 0, "ppc");
		g.zoneLabel(289, 1580, 324, 1705, 0, "ppc");
		g.warp("cpc430");
	}
	g.zoneHandler();
}

// cpc540 (0x432310)
static void place_cpc540(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(200, 455, 355, 560, 0, "cpc350", 0, -1.0, -1.0);
		g.zoneGo(304, 1500, 424, 1585, 0, "cpc550", 0, -1.0, -1.0);
		g.zoneGo(245, 1730, 480, 1940, 0, "cpc530", 0, -1.0, -1.0);
		g.zoneGo(297, 2030, 415, 2047, 0, "cpc520", 0, -1.0, -1.0);
		g.zoneGo(297, 0, 415, 70, 0, "cpc520", 0, -1.0, -1.0);
		g.warp("cpc540");
	}
	g.zoneHandler();
}

// cpc360 (0x432280)
static void place_cpc360(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(274, 949, 488, 1082, 0, "cpc350", 0, -1.0, -1.0);
		g.zoneGo(259, 383, 558, 489, 0, "cpc370", 0, -1.0, -1.0);
		g.warp("cpc360");
	}
	g.zoneHandler();
}

// cpc550 (0x4321d0)
static void place_cpc550(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(286, 1492, 374, 1590, 0, "cpc730", 0, -1.0, -1.0);
		g.zoneGo(265, 102, 526, 337, 0, "cpc530", 0, -1.0, -1.0);
		g.zoneGo(314, 469, 452, 558, 0, "cpc540", 0, -1.0, -1.0);
		g.warp("cpc550");
	}
	g.zoneHandler();
}

// cpc370 (0x432120)
static void place_cpc370(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(260, 1484, 599, 1647, 0, "cpc360", 0, -1.0, -1.0);
		g.zoneGo(330, 2033, 426, 2047, 0, "cpc230", 0, -1.0, -1.0);
		g.zoneGo(330, 0, 426, 16, 0, "cpc230", 0, -1.0, -1.0);
		g.warp("cpc370");
	}
	g.zoneHandler();
}

// cpc230 (0x431f90)
static void place_cpc230(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(323, 1004, 430, 1045, 0, "cpc370", 0, -1.0, -1.0);
		g.zoneGo(260, 1430, 540, 1650, 0, "cpc520", 0, -1.0, -1.0);
		g.zoneGo(300, 1990, 507, 2047, 0, "cpc220", 0, -1.0, -1.0);
		g.zoneGo(300, 0, 507, 57, 0, "cpc220", 0, -1.0, -1.0);
		g.zoneLabel(340, 1330, 385, 1450, 0, "ppc");
		g.zoneGo(270, 360, 580, 571, 0, nullptr, 0, 0.0, 0.0);
		g.zoneDoc(168, 284, 570, 676, 0, "education");
		g.zoneLabel(318, 1363, 339, 1437, 0, "ppc");
		g.warp("cpc230");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 5) {
		if (((int32)g.var(kV_CHAPITRE) >= 0) && ((int32)g.var(kV_CHAPITRE) <= 14)) {
			g.setAngles(3.12, 0.0);
			g.gotoPlace("espw101");
			return;
		}
		if ((int32)g.var(kV_CHAPITRE) >= 15) {
			g.gotoPlace("espw102");
		}
	}
}

// cpc520 (0x431e90)
static void place_cpc520(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(268, 436, 405, 560, 0, "cpc230", 0, -1.0, -1.0);
		g.zoneGo(273, 276, 404, 349, 0, "cpc220", 0, -1.0, -1.0);
		g.zoneGo(279, 1546, 466, 1687, 0, "cpc510", 0, -1.0, -1.0);
		g.zoneGo(260, 1087, 518, 1303, 0, "cpc530", 0, -1.0, -1.0);
		g.zoneGo(335, 940, 431, 1019, 0, "cpc540", 0, -1.0, -1.0);
		g.warp("cpc520");
	}
	g.zoneHandler();
}

// cpc530 (0x431d70)
static void place_cpc530(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(318, 672, 448, 771, 0, "cpc540", 0, -1.0, -1.0);
		g.zoneGo(290, 1980, 457, 2047, 0, "cpc510", 0, -1.0, -1.0);
		g.zoneGo(290, 0, 457, 64, 0, "cpc510", 0, -1.0, -1.0);
		g.zoneGo(300, 165, 450, 277, 0, "cpc520", 0, -1.0, -1.0);
		g.zoneGo(300, 1320, 439, 1425, 0, "cpc550", 0, -1.0, -1.0);
		g.zoneLabel(305, 1350, 365, 1515, 0, "ppc");
		g.zoneLabel(278, 1373, 304, 1479, 0, "ppc");
		g.warp("cpc530");
	}
	g.zoneHandler();
}

// cpc510 (0x431c60)
static void place_cpc510(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(166, 1940, 404, 2047, 0, "cpc210", 0, -1.0, -1.0);
		g.zoneGo(166, 0, 404, 130, 0, "cpc210", 0, -1.0, -1.0);
		g.zoneGo(280, 415, 480, 570, 0, "cpc520", 0, -1.0, -1.0);
		g.zoneGo(275, 941, 533, 1163, 0, "cpc530", 0, -1.0, -1.0);
		g.zoneLabel(315, 1280, 372, 1426, 0, "ppc");
		g.zoneLabel(310, 745, 372, 865, 0, "poc");
		g.zoneLabel(287, 1315, 314, 1401, 0, "ppc");
		g.warp("cpc510");
	}
	g.zoneHandler();
}

// cpc210 (0x431880)
static void place_cpc210(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(319, 920, 540, 1150, 0, "cpc510", 0, -1.0, -1.0);
		g.zoneGo(226, 600, 535, 657, 0, "cpc220", 0, -1.0, -1.0);
		g.zoneLabel(316, 1268, 378, 1392, 0, "ppc");
		g.zoneLabel(318, 792, 380, 880, 0, "poc");
		g.zoneGo(192, 1870, 520, 2047, 0, "aie200", 0, -1.0, -1.0);
		g.zoneGo(192, 0, 520, 213, 0, "aie200", 0, -1.0, -1.0);
		g.zoneTalk(347, 1759, 383, 1799, 1);
		g.zoneTalk(336, 240, 387, 274, 1);
		g.warp("cpc210");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((((int32)g.var(kV_CHAPITRE) == 9) && ((int32)g.var(kV_GIAD3111) == 1) && ((int32)g.var(kV_GIID3111) == 0) && ((int32)g.var(kV_GIJ3111) == 0)) || ((((int32)g.var(kV_GIGD4011) == 1) || ((int32)g.var(kV_GIHD4011) == 0)) && ((int32)g.var(kV_GIJD4011) == 0) && ((int32)g.var(kV_GIID3111) == 0) && ((int32)g.var(kV_CHAPITRE) == 12))) {
		g.zoneEnable(6);
		g.zoneEnable(7);
	}
	if (g.clickedZone() == 6) {
		if (((int32)g.var(kV_CHAPITRE) == 9) && ((int32)g.var(kV_GIAD3111) == 1) && ((int32)g.var(kV_GIID3111) == 0) && ((int32)g.var(kV_GIJ3111) == 0)) {
			g.dialogue("GIID3111", "E210GII", "E210ANJ");
			g.setVar(kV_GIID3111, 1);
			g.minutesAdd("MINCP312");
			g.zoneDisable(6);
			g.zoneDisable(7);
		}
		if (((int32)g.var(kV_CHAPITRE) == 12) && (((int32)g.var(kV_GIGD4011) == 1) || ((int32)g.var(kV_GIHD4011) == 0)) && ((int32)g.var(kV_GIJD4011) == 0) && ((int32)g.var(kV_GIID3111) == 0)) {
			g.dialogue("GIID4011", "E210GII", "E210ANJ");
			g.minutesAdd("MINCP401");
			g.setVar(kV_GIID4011, 1);
			g.zoneDisable(6);
			g.zoneDisable(7);
		}
	}
	if (g.clickedZone() == 7) {
		if (((int32)g.var(kV_CHAPITRE) == 9) && ((int32)g.var(kV_GIAD3111) == 1) && ((int32)g.var(kV_GIID3111) == 0) && ((int32)g.var(kV_GIJ3111) == 0)) {
			g.dialogue("GIID3111", "E210GII", "E210ANJ");
			g.setVar(kV_GIID3111, 1);
			g.minutesAdd("MINCP312");
			g.zoneDisable(6);
			g.zoneDisable(7);
		}
		if (((int32)g.var(kV_CHAPITRE) == 12) && (((int32)g.var(kV_GIGD4011) == 1) || ((int32)g.var(kV_GIHD4011) == 0)) && ((int32)g.var(kV_GIJD4011) == 0) && ((int32)g.var(kV_GIID3111) == 0)) {
			g.dialogue("GIJD4011", "E210GIJ", "E210ANJ");
			g.minutesAdd("MINCP401");
			g.setVar(kV_GIJD4011, 1);
			g.zoneDisable(6);
			g.zoneDisable(7);
		}
	}
}

// cpc220 (0x4317d0)
static void place_cpc220(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(248, 962, 540, 1081, 0, "cpc230", 0, -1.0, -1.0);
		g.zoneGo(290, 1290, 500, 1445, 0, "cpc520", 0, -1.0, -1.0);
		g.zoneGo(350, 1500, 419, 1518, 0, "cpc210", 0, -1.0, -1.0);
		g.warp("cpc220");
	}
	g.zoneHandler();
}

// cpc710 (0x431680)
static void place_cpc710(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(330, 450, 480, 552, 0, "cpc450", 0, -1.0, -1.0);
		g.zoneGo(250, 1912, 474, 2047, 0, "cpc720", 0, -1.0, -1.0);
		g.zoneGo(250, 0, 474, 140, 0, "cpc720", 0, -1.0, -1.0);
		g.zoneDoc(372, 1712, 495, 1805, 0, "jarres");
		g.zoneDoc(260, 1377, 410, 1820, 0, "ppc");
		g.zoneDoc(205, 1487, 259, 1740, 0, "ppc");
		g.zoneLabel(346, 227, 473, 283, 0, "brule");
		g.zoneLabel(351, 759, 466, 809, 0, "brule");
		g.zoneLabel(356, 1032, 433, 1051, 0, "grue");
		g.zoneLabel(374, 1254, 423, 1281, 0, "tortue");
		g.zoneLabel(349, 909, 427, 936, 0, "edicule");
		g.warp("cpc710");
	}
	g.zoneHandler();
}

// cpc730 (0x431550)
static void place_cpc730(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(330, 464, 480, 568, 0, "cpc550", 0, -1.0, -1.0);
		g.zoneGo(250, 900, 479, 1148, 0, "cpc720", 0, -1.0, -1.0);
		g.zoneDoc(374, 1285, 504, 1382, 0, "jarres");
		g.zoneDoc(268, 1245, 408, 1710, 0, "ppc");
		g.zoneDoc(205, 1330, 267, 1582, 0, "ppc");
		g.zoneLabel(350, 209, 466, 262, 0, "brule");
		g.zoneLabel(346, 740, 470, 796, 0, "brule");
		g.zoneLabel(368, 84, 427, 119, 0, "astronomie");
		g.zoneLabel(361, 2017, 435, 2040, 0, "grue");
		g.zoneLabel(376, 1798, 422, 1823, 0, "tortue");
		g.warp("cpc730");
	}
	g.zoneHandler();
}

// pne150 (0x431430)
static void place_pne150(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(283, 2000, 423, 2047, 0, "pne140", 0, -1.0, -1.0);
		g.zoneGo(283, 0, 423, 50, 0, "pne140", 0, -1.0, -1.0);
		g.zoneGo(332, 1601, 540, 1680, 0, nullptr, 0, 0.0, 0.0);
		g.warp("pne150");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 2) {
		if ((((int32)g.var(kV_CHAPITRE) >= 0) && ((int32)g.var(kV_CHAPITRE) <= 8)) || ((int32)g.var(kV_MODE_VISITE) == 1)) {
			g.setAngles(3.15, 0.0);
			g.gotoPlace("lgaw101");
			return;
		}
		if ((int32)g.var(kV_CHAPITRE) >= 9) {
			g.setAngles(3.15, 0.0);
			g.gotoPlace("lgaw102");
		}
	}
}

// pne140 (0x431050)
static void place_pne140(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(277, 974, 439, 1084, 0, "pne150", 0, -1.0, -1.0);
		g.zoneTalk(381, 1483, 512, 1517, 1);
		g.zoneTalk(381, 1584, 512, 1616, 1);
		g.zoneGo(244, 1471, 475, 1612, 1, nullptr, 0, 0.0, 0.0);
		g.zoneGo(295, 1990, 439, 2047, 0, "pne130", 0, -1.0, -1.0);
		g.zoneGo(295, 0, 439, 48, 0, "pne130", 0, -1.0, -1.0);
		g.zoneLabel(354, 1358, 460, 1408, 0, "lionne_pne");
		g.zoneLabel(358, 1702, 454, 1756, 0, "lion_pne");
		g.warp("pne140");
		if ((int32)g.var(kV_MODE_VISITE) == 1) {
			g.zoneEnable(3);
		}
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((((int32)g.var(kV_CHAPITRE) == 1) && ((int32)g.var(kV_XNED1011) == 0) && ((int32)g.var(kV_GICD1011) == 0) && ((int32)g.var(kV_GIDD1011) == 0)) || (((int32)g.var(kV_CHAPITRE) == 9) && ((int32)g.var(kV_ENED3111) == 1) && ((int32)g.var(kV_GICD3111) == 0) && ((int32)g.var(kV_GIDD3111) == 0))) {
		g.zoneEnable(1);
		g.zoneEnable(2);
	}
	if (g.clickedZone() == 1) {
		if (((int32)g.var(kV_CHAPITRE) == 1) && ((int32)g.var(kV_XNED1011) == 0) && ((int32)g.var(kV_GICD1011) == 0) && ((int32)g.var(kV_GIDD1011) == 0)) {
			g.dialogue("GICD1011", "D140gia", "D140ANJ");
			g.setVar(kV_GICD1011, 1);
			g.zoneDisable(1);
			g.zoneDisable(2);
		}
		if (((int32)g.var(kV_CHAPITRE) == 9) && ((int32)g.var(kV_ENED3111) == 1) && ((int32)g.var(kV_GICD3111) == 0) && ((int32)g.var(kV_GIDD3111) == 0)) {
			g.dialogue("GICD3111", "D140gia", "D140ANJ");
			g.setVar(kV_GICD3111, 1);
			g.minutesAdd("MINAO311");
			g.zoneDisable(1);
			g.zoneDisable(2);
		}
	}
	if (g.clickedZone() == 2) {
		if (((int32)g.var(kV_CHAPITRE) == 1) && ((int32)g.var(kV_XNED1011) == 0) && ((int32)g.var(kV_GICD1011) == 0) && ((int32)g.var(kV_GIDD1011) == 0)) {
			g.dialogue("GIDD1011", "D140gib", "D140ANJ");
			g.setVar(kV_GIDD1011, 1);
			g.zoneDisable(1);
			g.zoneDisable(2);
		}
		if (((int32)g.var(kV_CHAPITRE) == 9) && ((int32)g.var(kV_ENED3111) == 1) && ((int32)g.var(kV_GICD3111) == 0) && ((int32)g.var(kV_GIDD3111) == 0)) {
			g.dialogue("GIDD3111", "D140gib", "D140ANJ");
			g.setVar(kV_GIDD3111, 1);
			g.minutesAdd("MINAO311");
			g.zoneDisable(1);
			g.zoneDisable(2);
		}
	}
	if (g.clickedZone() == 3) {
		g.setAngles(1.58, 0.0);
		if ((int32)g.var(kV_MODE_VISITE) == 1) {
			g.gotoPlace("pne210");
		}
	}
}

// pne130 (0x430fa0)
static void place_pne130(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(278, 973, 428, 1075, 0, "pne140", 0, -1.0, -1.0);
		g.zoneGo(250, 1904, 551, 2047, 0, "pne120", 0, -1.0, -1.0);
		g.zoneGo(250, 0, 551, 125, 0, "pne120", 0, -1.0, -1.0);
		g.warp("pne130");
	}
	g.zoneHandler();
}

// pne120 (0x430f10)
static void place_pne120(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(243, 939, 483, 1121, 0, "pne130", 0, -1.0, -1.0);
		g.zoneGo(230, 1454, 556, 1657, 0, "pne110", 0, -1.0, -1.0);
		g.warp("pne120");
	}
	g.zoneHandler();
}

// pne210 (0x430e20)
static void place_pne210(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(188, 412, 517, 615, 0, "pne140", 0, -1.0, -1.0);
		g.zoneGo(309, 1134, 460, 1219, 0, "pne220", 0, -1.0, -1.0);
		g.zoneGo(327, 1853, 459, 1927, 0, "pne230", 0, -1.0, -1.0);
		g.zoneLabel(150, 1290, 560, 1790, 0, "ecran");
		g.zoneDoc(403, 50, 528, 150, 0, "jarres");
		g.zoneDoc(402, 875, 533, 972, 0, "jarres");
		g.warp("pne210");
	}
	g.zoneHandler();
}

// pne220 (0x430cf0)
static void place_pne220(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(288, 206, 458, 283, 0, "pne210", 0, -1.0, -1.0);
		g.zoneGo(280, 1955, 490, 2047, 0, "pne240", 0, -1.0, -1.0);
		g.zoneGo(280, 0, 490, 74, 0, "pne240", 0, -1.0, -1.0);
		g.zoneDoc(386, 1263, 492, 1303, 0, "animaux_reels");
		g.zoneDoc(386, 1769, 546, 1828, 0, "astronomie");
		g.zoneLabel(380, 1867, 455, 1895, 0, "brule");
		g.zoneLabel(377, 1940, 427, 1961, 0, "brule");
		g.zoneLabel(392, 289, 462, 354, 0, "jarres");
		g.zoneDoc(198, 1255, 238, 1320, 0, "tuiles");
		g.warp("pne220");
	}
	g.zoneHandler();
}

// pne240 (0x430bf0)
static void place_pne240(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(270, 960, 491, 1115, 0, "pne220", 0, -1.0, -1.0);
		g.zoneGo(325, 1500, 425, 1571, 0, "pnew310", 0, -1.0, -1.0);
		g.zoneGo(270, 1915, 482, 2047, 0, "pne230", 0, -1.0, -1.0);
		g.zoneLabel(189, 300, 547, 750, 0, "ecran");
		g.zoneLabel(379, 1303, 502, 1357, 0, "brule");
		g.zoneLabel(380, 1717, 501, 1768, 0, "brule");
		g.zoneLabel(409, 1144, 477, 1175, 0, "astronomie");
		g.warp("pne240");
	}
	g.zoneHandler();
}

// pne230 (0x430b20)
static void place_pne230(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(284, 743, 461, 823, 0, "pne210", 0, -1.0, -1.0);
		g.zoneGo(270, 980, 498, 1124, 0, "pne240", 0, -1.0, -1.0);
		g.zoneLabel(384, 1182, 458, 1212, 0, "brule");
		g.zoneDoc(227, 1594, 279, 1741, 0, "tuiles");
		g.zoneDoc(225, 1423, 276, 1587, 0, "toiture");
		g.warp("pne230");
	}
	g.zoneHandler();
}

// pne110 (0x430a70)
static void place_pne110(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(274, 469, 537, 711, 0, "pne120", 0, -1.0, -1.0);
		g.zoneGo(277, 1974, 524, 2047, 0, "aio100", 0, -1.0, -1.0);
		g.zoneGo(277, 0, 524, 93, 0, "aio100", 0, -1.0, -1.0);
		g.warp("pne110");
	}
	g.zoneHandler();
}

// pnew310 (0x4308a0)
static void place_pnew310(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(345, 407, 551, 582, 0, "pne240", 0, 4.72, 0.0);
		g.zoneLabel(390, 1669, 482, 1682, 0, "colonettes");
		g.zoneLabel(392, 1393, 486, 1406, 0, "colonettes");
		g.zoneLabel(395, 1281, 468, 1324, 0, "vases_sdt");
		g.zoneLabel(398, 1747, 463, 1790, 0, "vases_sdt");
		g.zoneLabel(395, 1505, 450, 1570, 0, "trone");
		g.zoneLabel(251, 1499, 279, 1570, 0, "bandeau_sdt");
		g.zoneLabel(76, 1522, 99, 1565, 0, "plafond");
		g.zoneLabel(312, 1506, 390, 1566, 0, "inscriptions_sdt");
		g.zoneLabel(215, 1150, 301, 1173, 0, "lampions");
		g.zoneLabel(250, 1280, 322, 1298, 0, "lampions");
		g.zoneLabel(282, 1349, 337, 1364, 0, "lampions");
		g.zoneLabel(281, 1705, 333, 1720, 0, "lampions");
		g.zoneLabel(254, 1775, 316, 1793, 0, "lampions");
		g.zoneLabel(215, 1894, 305, 1919, 0, "lampions");
		g.zoneLabel(45, 1030, 155, 1090, 0, "lampions");
		g.zoneLabel(43, 1978, 156, 2036, 0, "lampions");
		g.zoneLabel(226, 1049, 258, 1207, 0, "vairocana");
		g.zoneLabel(232, 1860, 260, 2015, 0, "vairocana");
		g.warp("pnew310");
	}
	g.zoneHandler();
}

// ctp110 (0x430770)
static void place_ctp110(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(231, 2003, 349, 2047, 0, "ctp310", 0, -1.0, -1.0);
		g.zoneGo(231, 0, 349, 41, 0, "ctp310", 0, -1.0, -1.0);
		g.zoneLabel(227, 1926, 339, 2047, 0, "spf");
		g.zoneLabel(227, 0, 339, 106, 0, "spf");
		g.zoneLabel(232, 163, 343, 422, 0, "ppc");
		g.zoneLabel(230, 1584, 329, 1886, 0, "ptt");
		g.zoneGo(303, 972, 397, 1052, 0, "aio500", 0, -1.0, -1.0);
		g.zoneDoc(348, 1805, 416, 1963, 0, "logement_en");
		g.zoneDoc(344, 75, 415, 262, 0, "logement_en");
		g.warp("ctp110");
	}
	g.zoneHandler();
}

// ctp310 (0x4306a0)
static void place_ctp310(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(263, 917, 503, 1128, 0, "ctp110", 0, -1.0, -1.0);
		g.zoneGo(247, 392, 470, 541, 0, "ctp320", 0, -1.0, -1.0);
		g.zoneDoc(110, 1800, 542, 2047, 0, "spf");
		g.zoneDoc(110, 0, 542, 258, 0, "spf");
		g.zoneLabel(223, 1356, 405, 1647, 0, "ptt");
		g.warp("ctp310");
	}
	g.zoneHandler();
}

// ctp320 (0x4305a0)
static void place_ctp320(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(277, 1492, 461, 1611, 0, "ctp310", 0, -1.0, -1.0);
		g.zoneGo(266, 1955, 492, 2047, 0, "ctp330", 0, -1.0, -1.0);
		g.zoneGo(266, 0, 492, 123, 0, "ctp330", 0, -1.0, -1.0);
		g.zoneDoc(160, 1578, 464, 1870, 0, "spf");
		g.zoneDoc(215, 1871, 451, 1947, 0, "spf");
		g.zoneDoc(184, 230, 432, 780, 0, "ppc");
		g.zoneDoc(230, 135, 424, 229, 0, "ppc");
		g.warp("ctp320");
	}
	g.zoneHandler();
}

// ctp330 (0x4303c0)
static void place_ctp330(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(244, 1090, 463, 1230, 0, "ctp320", 0, -1.0, -1.0);
		g.zoneGo(255, 1871, 485, 2038, 0, "ctp340", 0, -1.0, -1.0);
		g.zoneGo(250, 1391, 486, 1720, 0, nullptr, 0, 0.0, 0.0);
		g.zoneDoc(130, 1240, 486, 1840, 0, "spf");
		g.zoneDoc(174, 240, 454, 780, 0, "ppc");
		g.zoneDoc(215, 781, 436, 869, 0, "ppc");
		g.zoneDoc(218, 130, 428, 239, 0, "ppc");
		g.warp("ctp330");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 2) {
		if ((int32)g.var(kV_MODE_VISITE) == 1) {
			g.setAngles(4.7, 0.0);
			g.gotoPlace("spfw100");
			return;
		}
		if (((int32)g.var(kV_CHAPITRE) != 9) && (((int32)g.var(kV_CHAPITRE) != 10) || ((int32)g.var(kV_ANMI3211) != 0))) {
			if (((int32)g.var(kV_CHAPITRE) < 11) && (((int32)g.var(kV_CHAPITRE) != 10) || ((int32)g.var(kV_ANMI3211) != 1))) {
				g.setAngles(1.53, 0.0);
				return;
			}
			g.setAngles(4.7, 0.0);
			g.gotoPlace("spfw102");
			return;
		}
		g.setAngles(4.7, 0.0);
		g.gotoPlace("spfw101");
	}
}

// ctp340 (0x4302e0)
static void place_ctp340(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(238, 908, 493, 1148, 0, "ctp330", 0, -1.0, -1.0);
		g.zoneGo(264, 1463, 452, 1576, 0, "ctp350", 0, -1.0, -1.0);
		g.zoneDoc(207, 1121, 463, 1510, 0, "spf");
		g.zoneDoc(120, 1230, 206, 1483, 0, "spf");
		g.zoneDoc(155, 270, 444, 770, 0, "ppc");
		g.zoneDoc(220, 771, 438, 887, 0, "ppc");
		g.warp("ctp340");
	}
	g.zoneHandler();
}

// ctp350 (0x430200)
static void place_ctp350(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(246, 472, 446, 612, 0, "ctp340", 0, -1.0, -1.0);
		g.zoneGo(321, 1953, 514, 2047, 0, "ctp210", 0, -1.0, -1.0);
		g.zoneGo(321, 0, 514, 87, 0, "ctp210", 0, -1.0, -1.0);
		g.zoneDoc(100, 730, 532, 1350, 0, "spf");
		g.zoneLabel(222, 1437, 408, 1702, 0, "ptt");
		g.warp("ctp350");
	}
	g.zoneHandler();
}

// ctp210 (0x430070)
static void place_ctp210(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(255, 980, 350, 1067, 0, "ctp350", 0, -1.0, -1.0);
		g.zoneLabel(240, 914, 338, 1135, 0, "spf");
		g.zoneLabel(240, 615, 337, 845, 0, "ppc");
		g.zoneLabel(224, 1200, 338, 1450, 0, "ptt");
		g.zoneGo(290, 2011, 400, 2047, 0, "aie500", 0, -1.0, -1.0);
		g.zoneGo(290, 0, 400, 49, 0, "aie500", 0, -1.0, -1.0);
		g.zoneGo(369, 1143, 411, 1201, 1, nullptr, 0, 0.0, 0.0);
		g.zoneDoc(351, 770, 412, 949, 0, "logement_en");
		g.zoneDoc(350, 1099, 411, 1246, 0, "logement_en");
		g.warp("ctp210");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_CHAPITRE) == 10) && ((int32)g.var(kV_ANEF3231) == 1)) {
		g.zoneEnable(6);
	}
	if ((g.clickedZone() == 6) && ((int32)g.var(kV_ANEF3231) == 1)) {
		g.setAngles(3.15, 0.0);
		g.gotoPlace("lgew100");
	}
}

// lgew100 (0x42ff60)
static void place_lgew100(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(260, 1933, 620, 2047, 0, nullptr, 0, 0.0, 0.0);
		g.zoneGo(260, 0, 620, 113, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(610, 850, 701, 1110, 1, "coffre");
		g.zoneLabel(615, 260, 700, 650, 0, "LIT_LGE");
		g.warp("lgew100");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((int32)g.var(kV_VAR_Pieces_prises) == 0) {
		g.zoneEnable(2);
	}
	if ((g.clickedZone() == 0) || (g.clickedZone() == 1)) {
		if ((int32)g.var(kV_VAR_LGE_entree) == 0) {
			g.setVar(kV_VAR_LGE_entree, 1);
			g.video("LGEH331");
			g.minutesAdd("MINLG399");
			return;
		}
		g.gotoPlace("ctp210");
	}
}

// coffre (0x42fec0)
static void place_coffre(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(442, 0, 479, 639, 0, "lgew100", 0, -1.0, -1.0);
		g.zoneTake(170, 273, 362, 496, 0, nullptr);
		g.image("coffre");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("coffre");
		g.gotoPlace("cachwen1");
	}
}

// natte (0x42fe30)
static void place_natte(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(430, 0, 479, 639, 0, "lgew100", 0, -1.0, -1.0);
		g.zoneTake(218, 225, 299, 323, 0, nullptr);
		g.image("natte");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("coffrea");
	}
}

// cachwen1 (0x42fd60)
static void place_cachwen1(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(453, 0, 479, 639, 0, "lgew100", 0, -1.0, -1.0);
		g.zoneUse(314, 362, 412, 549, 0);
		g.image("cachwen1");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 1) && (((g.heldObject() == kO_BURIN ? 1 : 0) != 0) || ((g.heldObject() == kO_RUYI ? 1 : 0) != 0))) {
		if ((int32)g.var(kV_VAR_Pieces_prises) == 0) {
			g.gotoPlace("cachwen2");
			return;
		}
		g.soundQueue("dalle");
		g.gotoPlace("cachwen3");
	}
}

// cachwen2 (0x42fcc0)
static void place_cachwen2(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(447, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.image("cachwen2");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.minutesAdd("MINLG401");
		g.setVar(kV_VAR_Pieces_prises, 1);
		g.setVar(kV_CHAPITRE, 12);
		g.gotoPlace("lgew100");
	}
}

// cachwen3 (0x42fc40)
static void place_cachwen3(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(440, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.image("cachwen3");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.setVar(kV_CHAPITRE, 12);
		g.gotoPlace("lgew100");
	}
}

// pdc005 (0x42fbb0)
static void place_pdc005(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(297, 453, 450, 569, 0, "pdc010", 0, -1.0, -1.0);
		g.zoneGo(301, 1496, 439, 1575, 0, "aie600b", 0, 3.15, 0.0);
		g.warp("pdc005");
	}
	g.zoneHandler();
}

// pdc010 (0x42f7b0)
static void place_pdc010(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(284, 1488, 440, 1586, 0, "pdc005", 0, 1.58, 0.0);
		g.zoneTalk(318, 1815, 382, 1877, 1);
		g.zoneUse(391, 1792, 553, 1905, 1);
		g.zoneGo(160, 1911, 536, 2047, 1, nullptr, 0, 0.0, 0.0);
		g.zoneGo(160, 0, 536, 120, 1, nullptr, 0, 0.0, 0.0);
		g.warp("pdc010");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((((int32)g.var(kV_CHAPITRE) == 5) && ((((int32)g.var(kV_EGCD2141) == 0) && ((int32)g.var(kV_Mandat_Montre_EGC) == 1)) || ((int32)g.var(kV_EGCD2111) == 0)) && ((int32)g.var(kV_EGCD2111) == 0)) || (((int32)g.var(kV_CHAPITRE) == 13) && ((int32)g.var(kV_EGCD4111) == 0)) || (((int32)g.var(kV_CHAPITRE) == 14) && ((int32)g.var(kV_EGCD4211) == 0))) {
		g.zoneEnable(1);
	}
	if (((int32)g.var(kV_MODE_VISITE) != 0) || (((int32)g.var(kV_CHAPITRE) == 5) && ((int32)g.var(kV_Mandat_Montre_EGC) != 0)) || (((int32)g.var(kV_CHAPITRE) == 13) && ((int32)g.var(kV_EGCD4111) != 0))) {
		g.zoneEnable(3);
		g.zoneEnable(4);
	}
	if (((int32)g.var(kV_EGCD2111) != 0) && ((int32)g.var(kV_Mandat_Montre_EGC) == 0)) {
		g.zoneEnable(2);
	}
	if ((int32)g.var(kV_CHAPITRE) > 13) {
		g.zoneEnable(3);
		g.zoneEnable(4);
	}
	if ((g.clickedZone() == 3) || (g.clickedZone() == 4)) {
		if ((int32)g.var(kV_CHAPITRE) >= 13) {
			g.gotoPlace("pdc112");
			return;
		}
		g.gotoPlace("pdc110");
		return;
	}
	if (g.clickedZone() == 1) {
		if ((int32)g.var(kV_CHAPITRE) == 5) {
			if ((int32)g.var(kV_EGCD2111) == 0) {
				g.dialogue("EGCD2111", "L010EGC", "L010ANJ");
				g.setVar(kV_EGCD2111, 1);
				g.zoneDisable(1);
			}
			if (((int32)g.var(kV_EGCD2141) == 0) && ((int32)g.var(kV_Mandat_Montre_EGC) == 1)) {
				g.dialogue("EGCD2141", "L010EGC", "L010ANJ");
				g.setVar(kV_EGCD2141, 1);
				g.zoneDisable(1);
			}
		}
		if (((int32)g.var(kV_CHAPITRE) == 13) && ((int32)g.var(kV_EGCD4111) == 0)) {
			g.setAngles(0.0, 0.0);
			g.gotoPlace("pdc012");
			return;
		}
		if (((int32)g.var(kV_CHAPITRE) == 14) && ((int32)g.var(kV_ANXP4211) == 0) && ((int32)g.var(kV_EGCD4211) == 0)) {
			g.dialogue("ANEC4211", "L010ANJ", "L010EGC");
			g.minutesAdd("MINHB421");
			g.setVar(kV_EGCD4211, 1);
			g.zoneDisable(1);
		}
	}
	if ((g.clickedZone() == 2) && ((g.heldObject() == kO_MANDAT3 ? 1 : 0) != 0) && ((int32)g.var(kV_Mandat_Montre_EGC) == 0)) {
		if ((int32)g.var(kV_EGCD2111) == 0) {
			g.dialogue("ANEC2121", "L010ANJ", "L010EGC");
			g.video("PDCH211");
			g.setAngles(0.0, 0.0);
			g.gotoPlace("pdc011");
			return;
		}
		g.dialogue("ANEC212A", "L010ANJ", "L010EGC");
		g.video("PDCH211");
		g.setAngles(0.0, 0.0);
		g.gotoPlace("pdc011");
		return;
	}
}

// pdc011 (0x42f720)
static void place_pdc011(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.warp("pdc011");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	g.voice("EGCD2131");
	g.video("PDCH212");
	g.dialogue("EGCD2141", "L010EGC", "L010ANJ");
	g.setVar(kV_Mandat_Montre_EGC, 1);
	g.gotoPlace("pdc010");
}

// pdc012 (0x42f670)
static void place_pdc012(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.warp("pdc012");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	g.dialogue("EGCD4111", "L010EGC", "L010ANJ");
	g.video("PDCH211");
	g.voice("EGCD4131");
	g.video("PDCH212");
	g.dialogue("EGCD4141", "L010EGC", nullptr);
	g.setVar(kV_EGCD4111, 1);
	g.gotoPlace("pdc010");
}

// pdc110 (0x42f590)
static void place_pdc110(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(217, 900, 533, 1175, 0, "pdc010", 0, -1.0, -1.0);
		g.zoneLabel(156, 1850, 632, 2047, 0, "ecran");
		g.zoneLabel(156, 0, 632, 194, 0, "ecran");
		g.zoneGo(352, 1582, 453, 1650, 0, "pdc120", 0, -1.0, -1.0);
		g.zoneGo(346, 396, 453, 468, 0, "pdc160", 0, -1.0, -1.0);
		g.warp("pdc110");
	}
	g.zoneHandler();
}

// pdc140 (0x42f480)
static void place_pdc140(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneLabel(148, 870, 589, 1182, 0, "ecran");
		g.zoneGo(270, 1453, 480, 1625, 0, "pdc130", 0, -1.0, -1.0);
		g.zoneGo(270, 417, 480, 574, 0, "pdc150", 0, -1.0, -1.0);
		g.zoneGo(240, 1973, 443, 2047, 0, "pdc170", 0, -1.0, -1.0);
		g.zoneGo(240, 0, 443, 75, 0, "pdc170", 0, -1.0, -1.0);
		g.zoneDoc(212, 1830, 410, 2047, 0, "logement_cc");
		g.zoneDoc(212, 0, 410, 226, 0, "logement_cc");
		g.warp("pdc140");
	}
	g.zoneHandler();
}

// pdc170 (0x42f1c0)
static void place_pdc170(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(263, 914, 529, 1127, 0, nullptr, 0, 0.0, 0.0);
		g.zoneDoc(354, 1860, 500, 1907, 0, "chefs_eunuques");
		g.zoneTalk(313, 1871, 353, 1899, 1);
		g.zoneDoc(128, 1823, 431, 2047, 0, "logement_cc");
		g.zoneDoc(128, 0, 431, 225, 0, "logement_cc");
		g.warp("pdc170");
		if (((int32)g.var(kV_CHAPITRE) == 5) && ((int32)g.var(kV_XPRD2111) == 0)) {
			g.zoneEnable(2);
		}
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_XPRD2111) == 1) && ((int32)g.var(kV_ORGH211) == 0)) {
		g.zoneDisable(0);
	} else {
		g.zoneEnable(0);
	}
	if (g.clickedZone() == 0) {
		g.setAngles(3.14, 0.0);
		g.gotoPlace("pdc140");
		return;
	}
	if ((g.clickedZone() == 2) && ((int32)g.var(kV_CHAPITRE) == 5) && ((int32)g.var(kV_XPRD2111) == 0)) {
		g.dialogue("XPRD2111", "L170XPR", "L170ANJ");
		g.setVar(kV_XPRD2111, 1);
		g.zoneDisable(2);
	}
	if (g.clickedZone() == 1) {
		if (((int32)g.var(kV_XPRD2111) != 0) && ((g.heldObject() == kO_LISTE_BOITES ? 1 : 0) != 0) && ((int32)g.var(kV_ORGH211) == 0)) {
			if ((int32)g.var(kV_XPRD2111) == 0) {
				g.dialogue("ANXP2151", "L170ANJ", "L170XPR");
				g.video("ORGH211");
				g.setAngles(0.03, -0.1);
				g.gotoPlace("pdc178");
				return;
			}
			g.dialogue("ANXP215A", "L170ANJ", "L170XPR");
			g.video("ORGH211");
			g.setAngles(0.03, -0.1);
			g.gotoPlace("pdc178");
			return;
		}
		if (((g.heldObject() == kO_CONFES2 ? 1 : 0) != 0) && ((int32)g.var(kV_XPRD2131) == 0) && ((int32)g.var(kV_XPRD2111) == 0)) {
			g.dialogue("XPRD2131", "L170XPR", "L170ANJ");
			g.setVar(kV_XPRD2131, 1);
		}
		if (((g.heldObject() == kO_INDIC2 ? 1 : 0) != 0) && ((int32)g.var(kV_XPRD2141) == 0) && ((int32)g.var(kV_XPRD2111) == 0)) {
			g.dialogue("XPRD2141", "L170XPR", "L170ANJ");
			g.setVar(kV_XPRD2141, 1);
		}
	}
}

// pdc178 (0x42f140)
static void place_pdc178(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.warp("pdc178");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	g.voice("XPRD2161");
	g.video("ORGH212");
	g.setVar(kV_ORGH211, 1);
	g.gotoPlace("pdcw171");
}

// pdc179 (0x42f070)
static void place_pdc179(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.warp("pdc179");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	g.voice("CPRD2111");
	g.video("ORGH222");
	g.setVar(kV_ORGH221, 1);
	g.minutesAdd("MINPD219");
	g.objectDestroy(kO_LISTE_BOITES);
	g.dialogue("ANXP2211", "L170ANJ", "L170XPR");
	g.screenEffect();
	g.soundPlayWait("ANJBPI");
	g.setVar(kV_CHAPITRE, 6);
	g.gotoPlace("pdc170");
}

// pdc130 (0x42efb0)
static void place_pdc130(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(280, 430, 491, 574, 0, "pdc140", 0, -1.0, -1.0);
		g.zoneGo(282, 1460, 524, 1622, 0, "pdc120", 0, -1.0, -1.0);
		g.zoneDoc(403, 1834, 458, 1889, 0, "jarres");
		g.zoneGo(279, 653, 455, 748, 0, "pdc110", 0, -1.0, -1.0);
		g.warp("pdc130");
	}
	g.zoneHandler();
}

// pdc150 (0x42eef0)
static void place_pdc150(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(259, 1472, 490, 1630, 0, "pdc140", 0, -1.0, -1.0);
		g.zoneGo(286, 433, 498, 582, 0, "pdc160", 0, -1.0, -1.0);
		g.zoneDoc(402, 176, 453, 228, 0, "jarres");
		g.zoneGo(251, 1312, 453, 1366, 0, "pdc110", 0, -1.0, -1.0);
		g.warp("pdc150");
	}
	g.zoneHandler();
}

// pdc120 (0x42ee10)
static void place_pdc120(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(240, 429, 470, 545, 0, "pdc130", 0, -1.0, -1.0);
		g.zoneGo(300, 600, 427, 634, 0, "pdc110", 0, -1.0, -1.0);
		g.zoneDoc(405, 2041, 471, 2047, 0, "jarres");
		g.zoneDoc(405, 0, 471, 53, 0, "jarres");
		g.zoneLabel(237, 1319, 445, 1360, 0, "colonne_pdc_g");
		g.zoneLabel(234, 1718, 444, 1760, 0, "colonne_pdc_d");
		g.warp("pdc120");
	}
	g.zoneHandler();
}

// pdc160 (0x42ec00)
static void place_pdc160(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(259, 1500, 490, 1632, 0, "pdc150", 0, -1.0, -1.0);
		g.zoneGo(309, 1405, 436, 1436, 0, "pdc110", 0, -1.0, -1.0);
		g.zoneDoc(407, 2038, 465, 2047, 0, "jarres");
		g.zoneDoc(407, 0, 465, 47, 0, "jarres");
		g.zoneDoc(338, 585, 512, 632, 0, "chefs_eunuques");
		g.zoneLabel(254, 1716, 406, 1948, 0, "logement_cc");
		g.zoneTalk(293, 597, 337, 625, 1);
		g.zoneGo(303, 466, 463, 539, 0, "pdcw510", 0, 0.0, 0.0);
		g.warp("pdc160");
		if ((((int32)g.var(kV_ANXQ2111) == 0) && ((int32)g.var(kV_CHAPITRE) == 5)) || ((int32)g.var(kV_CHAPITRE) == 13)) {
			g.zoneEnable(6);
		}
		if ((int32)g.var(kV_CHAPITRE) == 5) {
			g.zoneDisable(7);
		}
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 6) && ((int32)g.var(kV_CHAPITRE) == 5) && ((int32)g.var(kV_ANXQ2111) == 0)) {
		g.dialogue("ANXQ2111", "L160ANJ", "L160XQR");
		g.setVar(kV_ANXQ2111, 1);
		g.zoneDisable(6);
	}
	if ((g.clickedZone() == 4) && ((g.heldObject() == kO_LISTE_BOITES ? 1 : 0) != 0) && ((int32)g.var(kV_ANXQ2121) == 0) && ((int32)g.var(kV_ANXQ2111) == 0)) {
		g.dialogue("XQRD2121", "L160XQR", "L160ANJ");
		g.setVar(kV_ANXQ2121, 1);
	}
}

// pdcw510 (0x42ead0)
static void place_pdcw510(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(240, 901, 610, 1130, 0, "pdc160", 0, 1.58, 0.0);
		g.zoneGo(304, 1479, 615, 1595, 0, "pdcw520", 0, -1.0, -1.0);
		g.zoneDoc(344, 1831, 582, 1854, 0, "eclairage");
		g.zoneLabel(340, 201, 581, 225, 0, "chandelier_email");
		g.zoneDoc(389, 2035, 456, 2047, 0, "porcelaines");
		g.zoneDoc(389, 0, 456, 11, 0, "porcelaines");
		g.zoneDoc(457, 1958, 483, 2047, 0, "mobilier");
		g.zoneDoc(457, 0, 483, 94, 0, "mobilier");
		g.zoneDoc(484, 1967, 550, 1983, 0, "mobilier");
		g.zoneDoc(484, 68, 550, 81, 0, "mobilier");
		g.warp("pdcw510");
	}
	g.zoneHandler();
}

// pdcw520 (0x42e8d0)
static void place_pdcw520(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(230, 450, 620, 565, 0, "pdcw510", 0, -1.0, -1.0);
		g.zoneDoc(449, 996, 512, 1056, 0, "penjing");
		g.zoneLabel(370, 1506, 458, 1536, 0, "vase_bleu_cqr");
		g.zoneDoc(336, 1387, 378, 1444, 0, "porcelaines");
		g.zoneDoc(223, 1318, 272, 1343, 0, "porcelaines");
		g.zoneDoc(335, 1305, 405, 1345, 0, "porcelaines");
		g.zoneLabel(327, 1975, 383, 2047, 0, "bandeau_cqr");
		g.zoneLabel(327, 0, 383, 80, 0, "bandeau_cqr");
		g.zoneDoc(259, 1878, 399, 1914, 0, "eclairage");
		g.zoneDoc(257, 150, 400, 177, 0, "eclairage");
		g.zoneDoc(172, 1485, 212, 1521, 0, "porcelaines");
		g.zoneDoc(517, 1502, 561, 1539, 0, "porcelaines");
		g.zoneDoc(576, 1510, 620, 1534, 0, "porcelaines");
		g.zoneDoc(554, 1424, 610, 1450, 0, "porcelaines");
		g.zoneLabel(541, 1619, 610, 1756, 0, "cuvette");
		g.zoneDoc(482, 1157, 596, 1271, 0, "mobilier");
		g.zoneDoc(474, 803, 614, 891, 0, "mobilier");
		g.zoneLabel(446, 905, 517, 954, 1, "vase");
		g.zoneLabel(445, 1104, 514, 1149, 1, "vase");
		g.zoneLabel(534, 1776, 614, 1864, 0, "tabouret");
		g.zoneLabel(195, 1825, 218, 1956, 1, "poesie");
		g.warp("pdcw520");
	}
	g.zoneHandler();
}

// pdc112 (0x42e7f0)
static void place_pdc112(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(217, 900, 533, 1175, 0, "pdc010", 0, -1.0, -1.0);
		g.zoneLabel(156, 1850, 632, 2047, 0, "ecran");
		g.zoneLabel(156, 0, 632, 194, 0, "ecran");
		g.zoneGo(352, 1582, 453, 1650, 0, "pdc122", 0, -1.0, -1.0);
		g.zoneGo(346, 396, 453, 468, 0, "pdc162", 0, -1.0, -1.0);
		g.warp("pdc112");
	}
	g.zoneHandler();
}

// pdc142 (0x42e6e0)
static void place_pdc142(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneLabel(148, 870, 589, 1182, 0, "ecran");
		g.zoneGo(270, 1453, 480, 1625, 0, "pdc132", 0, -1.0, -1.0);
		g.zoneGo(270, 417, 480, 574, 0, "pdc152", 0, -1.0, -1.0);
		g.zoneGo(240, 1973, 443, 2047, 0, "pdc172", 0, -1.0, -1.0);
		g.zoneGo(240, 0, 443, 75, 0, "pdc172", 0, -1.0, -1.0);
		g.zoneDoc(212, 1830, 410, 2047, 0, "logement_cc");
		g.zoneDoc(212, 0, 410, 226, 0, "logement_cc");
		g.warp("pdc142");
	}
	g.zoneHandler();
}

// pdc172 (0x42e5f0)
static void place_pdc172(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(263, 914, 529, 1127, 0, "pdc142", 0, -1.0, -1.0);
		g.zoneDoc(322, 28, 547, 86, 0, "eunuques");
		g.zoneTalk(281, 44, 321, 74, 1);
		g.zoneDoc(128, 1823, 431, 2047, 0, "logement_cc");
		g.zoneDoc(128, 0, 431, 225, 0, "logement_cc");
		g.warp("pdc172");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 3) && ((int32)g.var(kV_ANEP4211) == 0)) {
		g.dialogue("ANEP4211", "L172EPR", "L172ANJ");
		g.setVar(kV_ANEP4211, 1);
	}
}

// bougies (0x42e550)
static void place_bougies(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(410, 0, 479, 639, 0, "pdcw512", 0, -1.0, -1.0);
		g.zoneUse(60, 306, 136, 330, 0);
		g.image("bougies");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 1) && ((g.heldObject() == kO_LETTRE_VIERGE ? 1 : 0) != 0)) {
		g.gotoPlace("lvierge");
	}
}

// lvierge (0x42e4e0)
static void place_lvierge(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.image("lvierge");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	g.soundQueue("jingfind");
	g.minutesAdd("MINPD423");
	g.gotoPlace("rebus");
}

// rebus (0x42e480)
static void place_rebus(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(447, 0, 479, 639, 0, "pdcw512", 0, -1.0, -1.0);
		g.image("rebus");
	}
	g.zoneHandler();
}

// pdc132 (0x42e3c0)
static void place_pdc132(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(280, 430, 491, 574, 0, "pdc142", 0, -1.0, -1.0);
		g.zoneGo(282, 1460, 524, 1622, 0, "pdc122", 0, -1.0, -1.0);
		g.zoneDoc(403, 1834, 458, 1889, 0, "jarres");
		g.zoneGo(279, 653, 455, 748, 0, "pdc112", 0, -1.0, -1.0);
		g.warp("pdc132");
	}
	g.zoneHandler();
}

// pdc152 (0x42e300)
static void place_pdc152(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(259, 1472, 490, 1630, 0, "pdc142", 0, -1.0, -1.0);
		g.zoneGo(286, 433, 498, 582, 0, "pdc162", 0, -1.0, -1.0);
		g.zoneDoc(402, 176, 453, 228, 0, "jarres");
		g.zoneGo(251, 1312, 453, 1366, 0, "pdc112", 0, -1.0, -1.0);
		g.warp("pdc152");
	}
	g.zoneHandler();
}

// pdc122 (0x42e220)
static void place_pdc122(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(240, 429, 470, 545, 0, "pdc132", 0, -1.0, -1.0);
		g.zoneGo(300, 600, 427, 634, 0, "pdc112", 0, -1.0, -1.0);
		g.zoneLook(405, 2041, 471, 2047, 1, "jarre2", 0);
		g.zoneLook(405, 0, 471, 53, 1, "jarre2", 0);
		g.warp("pdc122");
		if ((int32)g.var(kV_CHAPITRE) == 14) {
			g.zoneEnable(2);
			g.zoneEnable(3);
		}
	}
	g.zoneHandler();
}

// jarre2 (0x42e100)
static void place_jarre2(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(444, 0, 479, 639, 0, "pdc122", 0, -1.0, -1.0);
		g.zoneTake(359, 275, 437, 320, 1, nullptr);
		g.image("jarre2");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((int32)g.var(kV_VAR_LETTRE_REVELEE) != 0) {
		g.zoneEnable(1);
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("jarrea");
		if ((int32)g.var(kV_Pierre_ouverte) == 0) {
			g.gotoPlace("pierre2");
			return;
		}
		if ((int32)g.var(kV_Pierre_ouverte) == 1) {
			if ((int32)g.var(kV_Cle_jarre_prise) == 0) {
				g.soundQueue("jingfind");
				g.gotoPlace("pierre21");
				return;
			}
			g.gotoPlace("pierre22");
		}
	}
}

// pierre2 (0x42dff0)
static void place_pierre2(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(445, 0, 479, 639, 0, "jarre2", 0, -1.0, -1.0);
		g.zoneUse(367, 281, 433, 310, 0);
		g.zoneUse(360, 269, 437, 280, 0);
		g.zoneUse(358, 311, 435, 321, 0);
		g.zoneUse(358, 275, 366, 313, 0);
		g.image("pierre2");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 1) && ((g.heldObject() == kO_MARTEAU ? 1 : 0) != 0)) {
		g.soundQueue("pierrec");
	}
	if ((g.clickedZone() >= 2) && (g.clickedZone() <= 4) && ((g.heldObject() == kO_BURIN ? 1 : 0) != 0)) {
		g.soundQueue("pierred");
		g.setVar(kV_Pierre_ouverte, 1);
		g.gotoPlace("pierre21");
	}
}

// pierre21 (0x42df60)
static void place_pierre21(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(389, 276, 428, 308, 0, nullptr);
		g.image("pierre21");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.minutesAdd("MINPD425");
		g.objectToInventory(kO_CLE_JARRE);
		g.setVar(kV_Cle_jarre_prise, 1);
		g.gotoPlace("pierre22");
	}
}

// pierre22 (0x42df00)
static void place_pierre22(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(448, 0, 479, 639, 0, "pdc122", 0, -1.0, -1.0);
		g.image("pierre22");
	}
	g.zoneHandler();
}

// pdc162 (0x42dac0)
static void place_pdc162(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(260, 1500, 490, 1632, 0, "pdc152", 0, -1.0, -1.0);
		g.zoneGo(309, 1405, 436, 1436, 0, "pdc112", 0, -1.0, -1.0);
		g.zoneLook(407, 2038, 465, 2047, 1, "jarre1", 0);
		g.zoneLook(407, 0, 465, 47, 1, "jarre1", 0);
		g.zoneDoc(339, 482, 518, 530, 0, "eunuques");
		g.zoneLabel(250, 1716, 406, 1950, 0, "logement_cc");
		g.zoneLook(550, 1900, 655, 2047, 1, "arbre1", 0);
		g.zoneLook(550, 0, 645, 82, 1, "arbre1", 0);
		g.zoneLook(540, 1840, 630, 1899, 1, "arbre1", 0);
		g.zoneTalk(304, 494, 338, 519, 1);
		g.zoneGo(269, 449, 461, 556, 1, "pdcw512", 0, 0.0, 0.0);
		g.warp("pdc162");
		if ((((int32)g.var(kV_CHAPITRE) == 13) && ((int32)g.var(kV_EGCD4111) == 1) && ((int32)g.var(kV_ANEQ4111) == 0)) || (((int32)g.var(kV_EQRD4211) == 0) && ((int32)g.var(kV_CHAPITRE) == 14) && ((int32)g.var(kV_EGCD4211) == 1)) || (((int32)g.var(kV_CHAPITRE) == 15) && ((int32)g.var(kV_ANEQ4311) == 0))) {
			g.zoneEnable(9);
		}
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_CHAPITRE) == 15) && ((int32)g.var(kV_ANEQ4311) != 0)) {
		g.zoneEnable(10);
	}
	if (((int32)g.var(kV_CHAPITRE) == 14) && ((int32)g.var(kV_C510_ouvert) != 0)) {
		if ((g.objectState(kO_SCEAUX) != 0 ? 1 : 0) == 0) {
			g.zoneEnable(6);
			g.zoneEnable(7);
			g.zoneEnable(8);
		}
		g.zoneEnable(10);
	}
	if (g.clickedZone() == 9) {
		if (((int32)g.var(kV_CHAPITRE) == 13) && ((int32)g.var(kV_MINXQ411) == 0) && ((int32)g.var(kV_ANEQ4111) == 0)) {
			g.dialogue("ANEQ4111", "L162ANJ", "L162EQR");
			g.setVar(kV_ANEQ4111, 1);
			g.setVar(kV_MINXQ411, 1);
			g.zoneDisable(9);
			g.video("PDCH401");
			g.setAngles(4.68, 0.0);
			g.gotoPlace("pdc175");
			return;
		}
		if (((int32)g.var(kV_CHAPITRE) == 14) && ((int32)g.var(kV_EQRD4211) == 0)) {
			g.dialogue("EQRD4211", "L162EQR", "L162ANJ");
			g.video("PDCH401");
			g.setAngles(4.68, 0.0);
			g.gotoPlace("pdc176");
			return;
		}
		if (((int32)g.var(kV_CHAPITRE) == 15) && ((int32)g.var(kV_ANEQ4311) == 0)) {
			g.setVar(kV_C520_ouvert, 1);
			g.dialogue("EQRD4311", "L162EQR", "L162ANJ");
			g.video("PDCH401");
			g.setAngles(4.68, 0.0);
			g.gotoPlace("pdc177");
			return;
		}
	}
	if ((g.clickedZone() == 4) && ((g.heldObject() == kO_LETTRE_VIERGE ? 1 : 0) != 0) && ((int32)g.var(kV_VAR_LETTRE_REVELEE) != 0) && ((int32)g.var(kV_ANEQ4241) == 0)) {
		g.dialogue("ANEQ4241", "L162ANJ", "L162EQR");
		g.setVar(kV_ANEQ4241, 1);
	}
}

// pdc175 (0x42da10)
static void place_pdc175(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.warp("pdc175");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	g.voice("EQRD4131");
	g.voice("DQRD4131");
	g.video("PDCH402");
	g.dialogue("EQRD4141", "L162EQR", "L162ANJ");
	g.minutesAdd("MINPD411");
	g.setAngles(4.71, 0.0);
	g.gotoPlace("pdc162");
}

// pdc176 (0x42d960)
static void place_pdc176(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.warp("pdc176");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	g.voice("EQRD4221");
	g.video("PDCH402");
	g.dialogue("EQRD4231", "L162EQR", "L162ANJ");
	g.setVar(kV_EQRD4211, 1);
	g.setVar(kV_C510_ouvert, 1);
	g.setAngles(4.71, 0.0);
	g.gotoPlace("pdc162");
}

// pdc177 (0x42d8b0)
static void place_pdc177(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.warp("pdc177");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	g.voice("EQRD4321");
	g.video("PDCH402");
	g.dialogue("EQRD4331", "L162EQR", "L162ANJ");
	g.setVar(kV_ANEQ4311, 1);
	g.setVar(kV_C510_ouvert, 1);
	g.setAngles(4.71, 0.0);
	g.gotoPlace("pdc162");
}

// jarre1 (0x42d810)
static void place_jarre1(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(448, 0, 479, 639, 0, "pdc162", 0, -1.0, -1.0);
		g.zoneTake(370, 260, 445, 297, 0, nullptr);
		g.image("jarre1");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("jarrea");
		g.gotoPlace("pierre1");
	}
}

// arbre1 (0x42d760)
static void place_arbre1(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(429, 0, 479, 639, 0, "pdc162", 0, -1.0, -1.0);
		g.zoneTake(205, 172, 293, 331, 0, nullptr);
		g.zoneTake(229, 332, 306, 507, 0, nullptr);
		g.image("arbre1");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 1) || (g.clickedZone() == 2)) {
		g.soundQueue("arbre1");
		g.gotoPlace("arbre2");
	}
}

// arbre2 (0x42d6a0)
static void place_arbre2(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(429, 0, 479, 639, 0, "pdc162", 0, -1.0, -1.0);
		g.zoneUse(205, 172, 293, 331, 0);
		g.zoneUse(229, 332, 306, 507, 0);
		g.image("arbre2");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((g.clickedZone() == 1) || (g.clickedZone() == 2)) && ((g.heldObject() == kO_CLE_JARRE ? 1 : 0) != 0)) {
		g.video("arbre");
		g.gotoPlace("arbre3");
	}
}

// arbre3 (0x42d5d0)
static void place_arbre3(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(223, 273, 274, 456, 0, nullptr);
		g.zoneGo(429, 0, 479, 639, 0, "arbre2", 0, -1.0, -1.0);
		g.image("arbre3");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) && (g.puzzle(3, 1) != 0)) {
		g.objectDestroy(kO_CLE_JARRE);
		g.objectDestroy(kO_LETTRE_VIERGE);
		g.objectDestroy(kO_INDICE_CACHETS2);
		g.objectToInventory(kO_SCEAUX);
		g.minutesAdd("MINPD429");
		g.gotoPlace("pdc162");
	}
}

// fsceaux (0x42d590)
static void place_fsceaux(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.image("fsceaux");
	}
	g.zoneHandler();
}

// pierre1 (0x42d470)
static void place_pierre1(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(428, 0, 479, 639, 0, "jarre1", 0, -1.0, -1.0);
		g.zoneUse(267, 173, 382, 221, 0);
		g.zoneUse(267, 222, 400, 235, 0);
		g.zoneUse(252, 159, 387, 172, 0);
		g.zoneUse(251, 167, 266, 201, 0);
		g.zoneUse(258, 202, 271, 230, 0);
		g.zoneUse(383, 158, 396, 190, 0);
		g.zoneUse(385, 191, 400, 229, 0);
		g.image("pierre1");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 1) && ((g.heldObject() == kO_MARTEAU ? 1 : 0) != 0)) {
		g.soundQueue("pierrea");
	}
	if ((g.clickedZone() >= 2) && (g.clickedZone() <= 7) && ((g.heldObject() == kO_BURIN ? 1 : 0) != 0)) {
		g.soundQueue("pierrea");
	}
}

// pdcw171 (0x42d3f0)
static void place_pdcw171(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneLook(269, 18, 310, 150, 0, "boite31", 0);
		g.zoneLook(304, 326, 361, 443, 0, "boite21", 0);
		g.zoneLook(222, 524, 313, 610, 0, "boite11", 0);
		g.image("pdcw171");
	}
	g.zoneHandler();
}

// boite11 (0x42d350)
static void place_boite11(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(445, 0, 479, 639, 0, "pdcw171", 0, -1.0, -1.0);
		g.zoneTake(153, 192, 416, 453, 0, nullptr);
		g.image("boite11");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("boite1");
		g.gotoPlace("boite12");
	}
}

// boite12 (0x42d270)
static void place_boite12(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(446, 0, 479, 639, 0, "pdcw171", 0, -1.0, -1.0);
		g.zoneTake(160, 171, 444, 211, 0, nullptr);
		g.zoneTake(161, 432, 417, 499, 0, nullptr);
		g.zoneTake(158, 220, 381, 426, 0, nullptr);
		g.image("boite12");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 1) || (g.clickedZone() == 2)) {
		g.soundQueue("boite1");
		g.gotoPlace("boite11");
		return;
	}
	if (g.clickedZone() == 3) {
		g.soundQueue("ANJBOX1");
		return;
	}
}

// boite21 (0x42d1d0)
static void place_boite21(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(424, 0, 479, 639, 0, "pdcw171", 0, -1.0, -1.0);
		g.zoneTake(129, 187, 351, 445, 0, nullptr);
		g.image("boite21");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("boite2");
		g.gotoPlace("boite22");
	}
}

// boite22 (0x42d130)
static void place_boite22(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(429, 0, 479, 639, 0, "pdcw171", 0, -1.0, -1.0);
		g.zoneTake(149, 173, 366, 352, 0, nullptr);
		g.zoneTake(245, 353, 366, 442, 0, nullptr);
		g.image("boite22");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 1) || (g.clickedZone() == 2)) {
		g.soundQueue("ANJBOX2");
	}
}

// boite31 (0x42d090)
static void place_boite31(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(425, 0, 479, 639, 0, "pdcw171", 0, -1.0, -1.0);
		g.zoneTake(90, 53, 276, 593, 0, nullptr);
		g.image("boite31");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundPlayWait("boite1");
		g.gotoPlace("boite32");
	}
}

// boite32 (0x42cfe0)
static void place_boite32(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(425, 0, 479, 639, 0, "boite31", 0, -1.0, -1.0);
		g.zoneUse(90, 53, 276, 593, 0);
		g.image("boite32");
		g.soundQueue("anjbox3");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 1) && ((g.heldObject() == kO_TOURNEVIS ? 1 : 0) != 0)) {
		g.soundQueue("boite32a");
		g.gotoPlace("boite331");
	}
}

// boite33 (0x42cf70)
static void place_boite33(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(196, 93, 277, 544, 0, nullptr);
		g.image("boite33");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.gotoPlace("origine");
	}
}

// boite331 (0x42cf00)
static void place_boite331(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(196, 93, 277, 544, 0, nullptr);
		g.image("boite331");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.objectToInventory(kO_ORIGINAUX);
		g.gotoPlace("origine");
	}
}

// origine (0x42ce30)
static void place_origine(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(428, 5, 475, 635, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(104, 194, 297, 364, 1, nullptr);
		g.image("origine");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) && ((int32)g.var(kV_ORGH221) == 0)) {
		g.dialogue("ANJORG21", "L170ANJ", nullptr);
		g.video("ORGH221");
		g.setAngles(0.03, -0.1);
		g.gotoPlace("pdc179");
	}
}

// penjing (0x42cdf0)
static void place_penjing(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.image("penjing");
	}
	g.zoneHandler();
}

// penjing2 (0x42cd60)
static void place_penjing2(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(410, 282, 453, 336, 0, nullptr);
		g.image("penjing2");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.objectToInventory(kO_PROCLA);
		g.soundQueue("penjingb");
		g.setVar(kV_CHAPITRE, 17);
		g.gotoPlace("proclam");
	}
}

// proclam (0x42cbf0)
static void place_proclam(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(437, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(5, 490, 434, 579, 0, nullptr);
		g.image("proclam");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("proclam");
	}
	if ((g.clickedZone() == 0) && ((int32)g.var(kV_PDCH501) == 0)) {
		g.soundStop();
		g.video("cqrh501");
		g.dialogue("ANCQ5011", "L520ANJ", "L520CQR");
		g.screenEffect();
		g.soundPlayWait("JINGBPI");
		g.dialogue("MPID5011", "I202MPI", "I202ANJ");
		g.screenEffect();
		g.dialogue("MPID5015", "I202MPI", nullptr);
		g.setVar(kV_PDCH501, 1);
		g.setVar(kV_C520_ouvert, 1);
		g.setVar(kV_C510_ouvert, 1);
		g.minutesAdd("MINPD501");
		g.setAngles(1.39, 0.0);
		g.gotoPlace("bpiw202");
	}
}

// pdcw512 (0x42c7d0)
static void place_pdcw512(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(240, 901, 610, 1130, 0, nullptr, 0, 0.0, 0.0);
		g.zoneGo(304, 1479, 615, 1595, 1, nullptr, 0, 0.0, 0.0);
		g.zoneUse(342, 204, 391, 216, 1);
		g.zoneDoc(389, 2035, 456, 2047, 0, "porcelaines");
		g.zoneDoc(389, 0, 456, 11, 0, "porcelaines");
		g.zoneDoc(457, 1958, 483, 2047, 0, "mobilier");
		g.zoneDoc(457, 0, 483, 94, 0, "mobilier");
		g.zoneDoc(484, 1967, 550, 1983, 0, "mobilier");
		g.zoneDoc(484, 68, 550, 81, 0, "mobilier");
		g.zoneDoc(436, 1886, 547, 1945, 0, "mobilier");
		g.zoneDoc(436, 105, 538, 170, 0, "mobilier");
		g.zoneDoc(392, 1775, 566, 1885, 0, "DAMES_COUR");
		g.zoneDoc(567, 1789, 656, 1878, 0, "DAMES_COUR");
		g.zoneTalk(319, 1812, 391, 1850, 1);
		g.warp("pdcw512");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.setAngles(1.58, 0.0);
		g.gotoPlace("pdc162");
		return;
	}
	if (((int32)g.var(kV_CHAPITRE) == 14) && ((int32)g.var(kV_VAR_LETTRE_REVELEE) == 0)) {
		g.zoneEnable(2);
	}
	if ((g.clickedZone() == 2) && ((g.heldObject() == kO_LETTRE_VIERGE ? 1 : 0) != 0)) {
		g.setVar(kV_VAR_LETTRE_REVELEE, 1);
		g.unknownCall(0x403540, 14, 4595792);
		g.unknownCall(0x403560, 14, 4578956);
		g.gotoPlace("lvierge");
		return;
	}
	if ((((g.objectState(kO_LETTRE_VIERGE) != 0 ? 1 : 0) == 0) && ((int32)g.var(kV_DQRD4211) == 0) && ((int32)g.var(kV_CHAPITRE) == 14)) || (((int32)g.var(kV_var220) == 1) && ((int32)g.var(kV_DQRD4321) == 0) && ((int32)g.var(kV_CHAPITRE) > 14))) {
		g.zoneEnable(13);
	}
	if ((int32)g.var(kV_C520_ouvert) == 1) {
		g.zoneEnable(1);
	}
	if ((g.clickedZone() == 1) && ((int32)g.var(kV_C520_ouvert) == 1)) {
		g.setAngles(1.58, 0.0);
		g.gotoPlace("pdcw522");
		return;
	}
	if (g.clickedZone() == 13) {
		if (((int32)g.var(kV_CHAPITRE) == 14) && ((int32)g.var(kV_DQRD4211) == 0)) {
			g.dialogue("DQRD4211", "L512DQR", "L512ANJ");
			g.screenEffect();
			g.dialogue("DQRD4221", "L512DQR", "L512ANJ");
			g.objectToCursor(kO_LETTRE_VIERGE);
			g.minutesAdd("MINPD422");
			g.setVar(kV_DQRD4211, 1);
			g.zoneDisable(13);
		}
		if (((int32)g.var(kV_CHAPITRE) == 15) && ((int32)g.var(kV_var220) == 1) && ((int32)g.var(kV_DQRD4321) == 0)) {
			g.dialogue("ANDQ4321", "L512ANJ", "L512DQR");
			g.setVar(kV_DQRD4321, 1);
			g.zoneDisable(13);
		}
	}
	if (((g.clickedZone() == 11) || (g.clickedZone() == 12)) && ((g.heldObject() == kO_LETTRE_VIERGE ? 1 : 0) != 0) && ((int32)g.var(kV_VAR_LETTRE_REVELEE) != 0) && ((int32)g.var(kV_ANDQ4231) == 0)) {
		g.dialogue("ANDQ4231", "L512ANJ", "L512DQR");
		g.setVar(kV_ANDQ4231, 1);
	}
}

// pdcw522 (0x42c4e0)
static void place_pdcw522(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTalk(278, 3, 377, 40, 1);
		g.zoneGo(230, 450, 620, 565, 0, "pdcw512", 0, -1.0, -1.0);
		g.zoneTake(449, 996, 512, 1056, 1, nullptr);
		g.zoneDoc(370, 1506, 458, 1536, 0, "porcelaines");
		g.zoneDoc(336, 1387, 378, 1444, 0, "porcelaines");
		g.zoneDoc(223, 1318, 272, 1343, 0, "porcelaines");
		g.zoneDoc(335, 1305, 405, 1345, 0, "porcelaines");
		g.zoneLabel(327, 1975, 383, 2047, 0, "bandeau_cqr");
		g.zoneLabel(327, 0, 383, 80, 0, "bandeau_cqr");
		g.zoneDoc(259, 1878, 399, 1914, 0, "eclairage");
		g.zoneDoc(257, 150, 400, 177, 0, "eclairage");
		g.zoneDoc(172, 1485, 212, 1521, 0, "porcelaines");
		g.zoneDoc(517, 1502, 561, 1539, 0, "porcelaines");
		g.zoneDoc(576, 1510, 620, 1534, 0, "porcelaines");
		g.zoneDoc(554, 1424, 610, 1450, 0, "porcelaines");
		g.zoneLabel(541, 1619, 610, 1756, 0, "cuvette");
		g.zoneDoc(482, 1157, 596, 1271, 0, "mobilier");
		g.zoneDoc(474, 803, 614, 891, 0, "mobilier");
		g.zoneDoc(490, 1907, 565, 2047, 1, "mobilier");
		g.zoneDoc(490, 0, 565, 140, 1, "mobilier");
		g.warp("pdcw522");
		if (((int32)g.var(kV_CHAPITRE) == 15) && ((int32)g.var(kV_CQRD4311) == 0)) {
			g.zoneEnable(0);
		}
		if (((int32)g.var(kV_CQRD4311) != 0) && ((int32)g.var(kV_PENJING) == 0)) {
			g.zoneEnable(2);
		}
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 2) && ((int32)g.var(kV_PENJING) == 0) && ((int32)g.var(kV_CQRD4311) != 0)) {
		if (g.puzzle(1, 0) == 1) {
			g.setVar(kV_PENJING, 1);
			g.setVar(kV_CHAPITRE, 17);
			g.gotoPlace("penjing2");
			return;
		}
		g.setVar(kV_var220, 1);
	}
	if (g.clickedZone() == 0) {
		g.dialogue("CQRD4311", "L520CQR", "L520ANJ");
		g.setVar(kV_CQRD4311, 1);
		g.zoneDisable(0);
		g.zoneEnable(2);
	}
}

// aie100 (0x42c460)
static void place_aie100(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(283, 429, 436, 563, 0, "cgc160", 0, 6.25, 0.0);
		g.zoneGo(304, 1495, 439, 1603, 0, "aie200", 0, -1.0, -1.0);
		g.warp("aie100");
	}
	g.zoneHandler();
}

// aie200 (0x42c1f0)
static void place_aie200(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(299, 483, 431, 587, 0, "aie100", 0, -1.0, -1.0);
		g.zoneTalk(314, 962, 401, 986, 1);
		g.zoneTalk(314, 1073, 401, 1100, 1);
		g.zoneGo(252, 954, 401, 1100, 0, "cpc210", 0, -1.0, -1.0);
		g.zoneGo(300, 1469, 422, 1539, 0, "aie300", 0, -1.0, -1.0);
		g.zoneDoc(389, 1541, 437, 1588, 0, "jarres");
		g.warp("aie200");
		if (((int32)g.var(kV_CHAPITRE) == 12) && (((int32)g.var(kV_GIID4011) == 0) || ((int32)g.var(kV_GIJD4011) == 0)) && ((int32)g.var(kV_GIKD4011) == 0) && ((int32)g.var(kV_GILD3111) == 0)) {
			g.zoneEnable(1);
			g.zoneEnable(2);
		}
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 1) && ((int32)g.var(kV_CHAPITRE) == 12) && (((int32)g.var(kV_GIID4011) == 0) || ((int32)g.var(kV_GIJD4011) == 0)) && ((int32)g.var(kV_GIKD4011) == 0) && ((int32)g.var(kV_GILD3111) == 0)) {
		g.dialogue("GIKD4011", "C500GIB", "C200ANJ");
		g.minutesAdd("MINAE401");
		g.setVar(kV_GIKD4011, 1);
		g.zoneDisable(1);
		g.zoneDisable(2);
	}
	if ((g.clickedZone() == 2) && ((int32)g.var(kV_CHAPITRE) == 12) && (((int32)g.var(kV_GIID4011) == 0) || ((int32)g.var(kV_GIJD4011) == 0)) && ((int32)g.var(kV_GILD4011) == 0) && ((int32)g.var(kV_GIKDIK11) == 0)) {
		g.dialogue("GILD4011", "C500GIA", "C200ANJ");
		g.minutesAdd("MINAE401");
		g.setVar(kV_GILD4011, 1);
		g.zoneDisable(1);
		g.zoneDisable(2);
	}
}

// aie300 (0x42c150)
static void place_aie300(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(310, 474, 423, 545, 0, "aie200", 0, -1.0, -1.0);
		g.zoneGo(300, 1480, 446, 1588, 0, "aie400", 0, -1.0, -1.0);
		g.zoneDoc(386, 438, 421, 471, 0, "jarres");
		g.warp("aie300");
	}
	g.zoneHandler();
}

// aie400 (0x42c0b0)
static void place_aie400(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(340, 480, 414, 550, 0, "aie300", 0, -1.0, -1.0);
		g.zoneGo(320, 1501, 413, 1561, 0, "aie500", 0, -1.0, -1.0);
		g.zoneDoc(386, 1562, 416, 1590, 0, "jarres");
		g.warp("aie400");
	}
	g.zoneHandler();
}

// aie500 (0x42bb00)
static void place_aie500(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(320, 490, 415, 550, 0, "aie400", 0, -1.0, -1.0);
		g.zoneGo(312, 1498, 410, 1560, 0, "aie600b", 0, -1.0, -1.0);
		g.zoneTalk(305, 943, 323, 970, 1);
		g.zoneTalk(304, 1074, 327, 1104, 1);
		g.zoneDoc(386, 468, 417, 488, 0, "jarres");
		g.zoneUse(329, 1070, 401, 1107, 1);
		g.zoneUse(326, 941, 402, 973, 1);
		g.zoneGo(239, 941, 398, 1104, 1, "ctp210", 0, -1.0, -1.0);
		g.warp("aie500");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_CHAPITRE) == 10) && ((int32)g.var(kV_entree_PPF) == 0)) {
		g.zoneEnable(5);
		g.zoneEnable(6);
	}
	if (((int32)g.var(kV_MODE_VISITE) != 0) || ((int32)g.var(kV_entree_PPF) != 0)) {
		g.zoneEnable(7);
	}
	if ((((int32)g.var(kV_CHAPITRE) == 12) && (((int32)g.var(kV_GILD4011) == 0) || ((int32)g.var(kV_GIKD4011) == 0)) && ((int32)g.var(kV_GISD4011) == 0) && ((int32)g.var(kV_GITD4011) == 0)) || (((int32)g.var(kV_CHAPITRE) == 10) && ((int32)g.var(kV_GISD3211) == 0) && ((int32)g.var(kV_GITD3211) == 0) && ((int32)g.var(kV_GISD3221) == 0) && ((int32)g.var(kV_GITD3221) == 0) && ((int32)g.var(kV_entree_PPF) == 0))) {
		g.zoneEnable(2);
		g.zoneEnable(3);
	}
	if ((g.clickedZone() == 3) && ((int32)g.var(kV_CHAPITRE) == 12) && (((int32)g.var(kV_GILD4011) == 0) || ((int32)g.var(kV_GIKD4011) == 0)) && ((int32)g.var(kV_GISD4011) == 0) && ((int32)g.var(kV_GITD3111) == 0)) {
		g.dialogue("GISD4011", "C500GIA", "C500ANJ");
		g.minutesAdd("MINAE402");
		g.setVar(kV_GISD4011, 1);
		g.zoneDisable(2);
		g.zoneDisable(3);
	}
	if ((g.clickedZone() == 2) && ((int32)g.var(kV_CHAPITRE) == 12) && (((int32)g.var(kV_GILD4011) == 0) || ((int32)g.var(kV_GIKD4011) == 0)) && ((int32)g.var(kV_GITD4011) == 0) && ((int32)g.var(kV_GISD3111) == 0)) {
		g.dialogue("GITD4011", "C500GIB", "C500ANJ");
		g.minutesAdd("MINAE402");
		g.setVar(kV_GITD4011, 1);
		g.zoneDisable(2);
		g.zoneDisable(3);
	}
	if ((g.clickedZone() == 3) && ((int32)g.var(kV_CHAPITRE) == 10) && ((int32)g.var(kV_entree_PPF) == 0) && (((int32)g.var(kV_GISD3211) == 0) || ((int32)g.var(kV_GITD3211) == 0))) {
		g.dialogue("GISD3211", "C500GIA", "C500ANJ");
		g.setVar(kV_GISD3211, 1);
		g.zoneDisable(2);
		g.zoneDisable(3);
	}
	if ((g.clickedZone() == 2) && ((int32)g.var(kV_CHAPITRE) == 10) && ((int32)g.var(kV_entree_PPF) == 0) && (((int32)g.var(kV_GISD3211) == 0) || ((int32)g.var(kV_GITD3211) == 0))) {
		g.dialogue("GITD3211", "C500GIB", "C500ANJ");
		g.setVar(kV_GITD3211, 1);
		g.zoneDisable(2);
		g.zoneDisable(3);
	}
	if ((g.clickedZone() == 5) && ((g.heldObject() == kO_MANDAT4 ? 1 : 0) != 0) && ((int32)g.var(kV_GITD3221) == 0) && ((int32)g.var(kV_GISD3221) == 0)) {
		g.dialogue("GITD3221", "C500GIA", "C500ANJ");
		g.setVar(kV_GITD3221, 1);
		g.setVar(kV_entree_PPF, 1);
		g.zoneDisable(2);
		g.zoneDisable(3);
		g.zoneDisable(5);
		g.zoneDisable(6);
		g.zoneEnable(7);
	}
	if ((g.clickedZone() == 6) && ((g.heldObject() == kO_MANDAT4 ? 1 : 0) != 0) && ((int32)g.var(kV_GISD3221) == 0) && ((int32)g.var(kV_GITD3221) == 0)) {
		g.dialogue("GISD3221", "C500GIB", "C500ANJ");
		g.setVar(kV_GISD3221, 1);
		g.setVar(kV_entree_PPF, 1);
		g.zoneDisable(2);
		g.zoneDisable(3);
		g.zoneDisable(6);
		g.zoneDisable(5);
		g.zoneEnable(7);
	}
}

// aie600b (0x42b5c0)
static void place_aie600b(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTalk(373, 1502, 389, 1516, 1);
		g.zoneGo(331, 478, 414, 544, 0, "aie500", 0, -1.0, -1.0);
		g.zoneGo(252, 1980, 488, 2047, 1, nullptr, 0, 0.0, 0.0);
		g.zoneGo(252, 0, 488, 100, 1, nullptr, 0, 0.0, 0.0);
		g.zoneUse(390, 1497, 444, 1522, 1);
		g.zoneGo(318, 1490, 439, 1580, 0, nullptr, 0, 0.0, 0.0);
		g.zoneDoc(383, 456, 412, 478, 0, "jarres");
		g.warp("aie600b");
		if ((((int32)g.var(kV_CHAPITRE) == 1) && ((int32)g.var(kV_ENED1011) != 0) && ((int32)g.var(kV_EGJD1011) == 0)) || (((int32)g.var(kV_CHAPITRE) == 12) && ((int32)g.var(kV_EGJD4011) == 0)) || (((int32)g.var(kV_CHAPITRE) == 13) && ((int32)g.var(kV_ANEJ4111) == 0))) {
			g.zoneEnable(0);
		}
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_JIXW111_ouvert) == 0) && ((int32)g.var(kV_EGJD1011) == 1)) {
		g.zoneEnable(4);
	}
	if (((int32)g.var(kV_CHAPITRE) >= 5) || ((int32)g.var(kV_MODE_VISITE) != 0)) {
		g.zoneEnable(2);
		g.zoneEnable(3);
	}
	if ((g.clickedZone() == 2) || (g.clickedZone() == 3)) {
		if ((int32)g.var(kV_CHAPITRE) >= 5) {
			g.setAngles(4.79, 0.0);
			g.gotoPlace("pdc005");
			return;
		}
	}
	if (g.clickedZone() == 0) {
		if (((int32)g.var(kV_CHAPITRE) == 1) && ((int32)g.var(kV_EGJD1011) == 0)) {
			g.dialogue("EGJD1011", "C600EGJ", "C600ANJ");
			g.setVar(kV_EGJD1011, 1);
			g.zoneDisable(0);
		}
		if (((int32)g.var(kV_CHAPITRE) == 12) && ((int32)g.var(kV_EGJD4011) == 0)) {
			g.dialogue("EGJD4011", "C600EGJ", "C600ANJ");
			g.setVar(kV_EGJD4011, 1);
			g.zoneDisable(0);
		}
		if (((int32)g.var(kV_CHAPITRE) == 13) && ((int32)g.var(kV_ANEJ4111) == 0) && ((int32)g.var(kV_XPRD4111) == 0)) {
			g.dialogue("ANEJ4111", "C600ANJ", "C600EGJ");
			g.setVar(kV_ANEJ4111, 1);
			g.zoneDisable(0);
		}
	}
	if ((int32)g.var(kV_CHAPITRE) == 1) {
		if (g.clickedZone() == 4) {
			if (((g.heldObject() == kO_MANDAT1 ? 1 : 0) != 0) && ((int32)g.var(kV_EGJD1021) == 0)) {
				g.dialogue("EGJD1021", "C600EGJ", "C600ANJ");
				g.setVar(kV_EGJD1021, 1);
				g.zoneDisable(4);
			}
			if ((g.clickedZone() == 4) && ((g.heldObject() == kO_MANDAT2 ? 1 : 0) != 0) && ((int32)g.var(kV_EGJD1041) == 0)) {
				g.dialogue("EGJD1041", "C600EGJ", "C600ANJ");
				g.setVar(kV_EGJD1041, 1);
				g.setVar(kV_JIXW111_ouvert, 1);
				g.zoneDisable(4);
			}
		}
	} else {
		if ((g.clickedZone() == 4) && ((g.heldObject() == kO_MANDAT2 ? 1 : 0) != 0) && ((int32)g.var(kV_EGJD1041) == 0)) {
			g.dialogue("EGJD1041", "C600EGJ", "C600ANJ");
			g.setVar(kV_EGJD1041, 1);
			g.setVar(kV_JIXW111_ouvert, 1);
			g.zoneDisable(4);
		}
	}
	if (g.clickedZone() == 5) {
		if ((int32)g.var(kV_MODE_VISITE) != 0) {
			g.video("jixh007");
			g.gotoPlace("jixw110");
			return;
		}
		if ((int32)g.var(kV_CHAPITRE) >= 12) {
			g.video("jixh007");
			if (((int32)g.var(kV_CHAPITRE) == 13) && (((int32)g.var(kV_ANXP4111) == 0) || ((int32)g.var(kV_ANCP4101) == 0))) {
				g.gotoPlace("jixw112");
				return;
			}
			g.gotoPlace("jixw110");
			return;
		}
		if ((int32)g.var(kV_JIXW111_ouvert) != 0) {
			if ((int32)g.var(kV_CHAPITRE) < 13) {
				g.video("jixh007");
				g.gotoPlace("jixw111");
				return;
			}
		} else {
			g.dialogue("EGJD1031", "C600EGJ", "C600ANJ");
			g.setAngles(1.5, 0.0);
		}
	}
	if ((g.clickedZone() == 7) && ((int32)g.var(kV_EGJD1021) == 0)) {
		g.dialogue("EGJD1021", "C600EGJ", "C600ANJ");
		g.setVar(kV_EGJD1021, 1);
	}
	if (((int32)g.var(kV_ANXI2131) == 1) || ((int32)g.var(kV_MODE_VISITE) == 1)) {
		g.zoneEnable(2);
		g.zoneEnable(3);
	}
	if ((g.clickedZone() == 2) || (g.clickedZone() == 3)) {
		g.setAngles(4.72, 0.0);
		g.gotoPlace("pdc005");
	}
}

// bpiw100 (0x42b490)
static void place_bpiw100(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(155, 1895, 670, 2047, 0, "cpc120", 0, -1.0, -1.0);
		g.zoneGo(155, 0, 670, 135, 0, "cpc120", 0, -1.0, -1.0);
		g.zoneGo(255, 705, 550, 865, 0, "bpiw200", 0, -1.0, -1.0);
		g.zoneDoc(495, 506, 535, 532, 0, "ecriture");
		g.zoneDoc(446, 506, 470, 521, 0, "ecriture");
		g.zoneLabel(433, 800, 472, 883, 0, "kang");
		g.zoneLabel(438, 916, 472, 971, 0, "kang");
		g.zoneLabel(480, 941, 537, 981, 0, "dossiers");
		g.zoneDoc(343, 1253, 452, 1278, 0, "archive");
		g.warp("bpiw100");
	}
	g.zoneHandler();
}

// bpiw200 (0x42b320)
static void place_bpiw200(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(281, 1766, 539, 1890, 0, "bpiw100", 0, -1.0, -1.0);
		g.zoneDoc(478, 2010, 511, 2026, 0, "ecriture");
		g.zoneDoc(465, 174, 492, 185, 0, "ecriture");
		g.zoneLabel(472, 1055, 552, 1141, 0, "kang");
		g.zoneLabel(470, 817, 540, 976, 0, "kang");
		g.zoneLabel(554, 1524, 575, 1558, 0, "pose_pinceau");
		g.zoneDoc(527, 1437, 569, 1459, 0, "ecriture");
		g.zoneDoc(446, 1008, 477, 1023, 0, "the");
		g.zoneDoc(483, 379, 500, 396, 0, "the");
		g.zoneDoc(488, 544, 506, 565, 0, "the");
		g.zoneDoc(471, 692, 489, 712, 0, "the");
		g.zoneLabel(343, 1464, 394, 1592, 0, "bandeau_bpi");
		g.zoneDoc(329, 1329, 469, 1402, 0, "archive");
		g.zoneLabel(494, 1574, 552, 1617, 0, "dossiers");
		g.warp("bpiw200");
	}
	g.zoneHandler();
}

// bpiw101 (0x42b130)
static void place_bpiw101(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(130, 1880, 670, 2047, 0, "cpc120", 0, -1.0, -1.0);
		g.zoneGo(130, 0, 670, 155, 0, "cpc120", 0, -1.0, -1.0);
		g.zoneGo(255, 705, 550, 865, 0, "bpiw201", 0, -1.0, -1.0);
		g.zoneDoc(478, 526, 520, 552, 0, "ecriture");
		g.zoneDoc(438, 516, 463, 531, 0, "ecriture");
		g.zoneLabel(425, 800, 464, 883, 0, "kang");
		g.zoneLabel(430, 916, 464, 971, 0, "kang");
		g.zoneLabel(465, 920, 513, 959, 0, "dossiers");
		g.zoneDoc(341, 1231, 450, 1271, 0, "archive");
		g.zoneDoc(544, 1065, 649, 1140, 0, "eunuques");
		g.zoneDoc(399, 1057, 543, 1153, 0, "eunuques");
		g.zoneDoc(339, 1091, 398, 1121, 0, "eunuques");
		g.zoneDoc(437, 1002, 520, 1055, 0, "mandarins");
		g.zoneDoc(465, 963, 506, 1001, 0, "mandarins");
		g.zoneDoc(393, 1023, 437, 1050, 0, "mandarins");
		g.zoneDoc(467, 370, 580, 454, 0, "mandarins");
		g.zoneDoc(550, 455, 580, 510, 0, "mandarins");
		g.zoneDoc(488, 455, 518, 516, 0, "mandarins");
		g.zoneDoc(402, 380, 466, 414, 0, "mandarins");
		g.warp("bpiw101");
	}
	g.zoneHandler();
}

// bpiw201 (0x42a930)
static void place_bpiw201(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(285, 1780, 547, 1923, 0, "bpiw101", 0, -1.0, -1.0);
		g.zoneDoc(465, 2040, 497, 2047, 0, "ecriture");
		g.zoneDoc(465, 0, 497, 8, 0, "ecriture");
		g.zoneDoc(448, 191, 476, 204, 0, "ecriture");
		g.zoneDoc(550, 1423, 594, 1448, 0, "ecriture");
		g.zoneLabel(588, 1540, 604, 1584, 0, "pose_pinceau");
		g.zoneLabel(465, 1037, 555, 1119, 0, "kang");
		g.zoneLabel(466, 797, 535, 946, 0, "kang");
		g.zoneDoc(325, 1322, 460, 1398, 0, "archive");
		g.zoneDoc(437, 986, 463, 1000, 0, "the");
		g.zoneDoc(464, 533, 482, 549, 0, "the");
		g.zoneDoc(453, 671, 470, 686, 0, "the");
		g.zoneLabel(459, 383, 474, 396, 0, "bol");
		g.zoneLabel(329, 1467, 382, 1604, 0, "bandeau_bpi");
		g.zoneLabel(499, 1599, 571, 1655, 0, "dossiers");
		g.zoneDoc(394, 1646, 519, 1704, 0, "eunuques");
		g.zoneDoc(357, 1667, 393, 1686, 0, "eunuques");
		g.zoneDoc(480, 1478, 538, 1598, 0, "mandarins");
		g.zoneDoc(452, 1497, 479, 1574, 0, "mandarins");
		g.zoneTalk(400, 1522, 451, 1552, 1);
		g.zoneDoc(520, 1450, 552, 1477, 0, "mandarins");
		g.zoneDoc(439, 1968, 478, 2047, 0, "mandarins");
		g.zoneDoc(422, 1990, 438, 2033, 0, "mandarins");
		g.zoneDoc(392, 2003, 421, 2024, 0, "mandarins");
		g.zoneLabel(523, 1656, 593, 1694, 0, "chandelier_verre");
		g.warp("bpiw201");
		if ((((int32)g.var(kV_CHAPITRE) == 1) && ((int32)g.var(kV_ANXI1011) == 0) && ((int32)g.var(kV_ENED1011) == 1)) || (((int32)g.var(kV_CHAPITRE) == 2) && ((int32)g.var(kV_ANXI1111) == 0)) || (((int32)g.var(kV_CHAPITRE) == 8) && ((int32)g.var(kV_ANXI3011) == 0)) || (((int32)g.var(kV_CHAPITRE) == 9) && ((int32)g.var(kV_ANXI3151) == 0) && ((int32)g.var(kV_3GIED11) == 1))) {
			g.zoneEnable(19);
		}
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 19) {
		if (((int32)g.var(kV_CHAPITRE) == 1) && ((int32)g.var(kV_ENED1011) == 1) && ((int32)g.var(kV_ANXI1011) == 0)) {
			g.dialogue("ANXI1011", "I201ANJ", "I201XPI");
			g.screenEffect();
			g.dialogue("XPID1012", "I201XPI", nullptr);
			g.minutesAdd("MINBP101");
			g.setVar(kV_ANXI1011, 1);
			g.objectDestroy(kO_MANDAT1);
			g.objectToInventory(kO_MANDAT2);
			g.zoneDisable(19);
		}
		if (((int32)g.var(kV_CHAPITRE) == 2) && ((int32)g.var(kV_ANXI1121) == 0) && ((int32)g.var(kV_ANXI1111) == 0)) {
			g.dialogue("ANXI1111", "I201ANJ", "I201XPI");
			g.setVar(kV_ANXI1111, 1);
			g.zoneDisable(19);
		}
		if (((int32)g.var(kV_CHAPITRE) == 8) && ((int32)g.var(kV_ANXI3011) == 0)) {
			g.dialogue("ANXI3011", "I201ANJ", "I201XPI");
			g.setVar(kV_ANXI3011, 1);
			g.zoneDisable(19);
		}
		if (((int32)g.var(kV_CHAPITRE) == 9) && ((int32)g.var(kV_3GIED11) == 1) && ((int32)g.var(kV_ANXI3151) == 0)) {
			g.dialogue("ANXI3151", "I201ANJ", "I201XPI");
			g.setVar(kV_ANXI3151, 1);
			g.zoneDisable(19);
		}
	}
	if ((g.clickedZone() == 17) || (g.clickedZone() == 18) || (g.clickedZone() == 20)) {
		if (((g.heldObject() == kO_POSTHUME ? 1 : 0) != 0) && ((int32)g.var(kV_ANXI1121) == 0)) {
			g.dialogue("ANXI1121", "I201ANJ", "I201XPI");
			g.screenEffect();
			g.dialogue("XPID1122", "I201XPI", "I201ANJ");
			g.setVar(kV_ANXI1121, 1);
		}
		if (((g.heldObject() == kO_CONFES1 ? 1 : 0) != 0) && ((int32)g.var(kV_ANXI2011) == 0)) {
			g.dialogue("ANXI2011", "I201ANJ", "I201XPI");
			g.screenEffect();
			g.dialogue("XPID2012", "I201XPI", "I201ANJ");
			g.setVar(kV_ANXI2011, 1);
		}
		if (((g.heldObject() == kO_INDIC1 ? 1 : 0) != 0) && ((int32)g.var(kV_ANXI2021) == 0)) {
			g.dialogue("ANXI2021", "I201ANJ", "I201XPI");
			g.setVar(kV_ANXI2021, 1);
		}
		if (((g.heldObject() == kO_CONFES2 ? 1 : 0) != 0) && ((int32)g.var(kV_ANXI2111) == 0)) {
			g.dialogue("ANXI2111", "I201ANJ", "I201XPI");
			g.screenEffect();
			g.dialogue("XPID2112", "I201XPI", "I201ANJ");
			g.minutesAdd("MINBP211");
			g.setVar(kV_ANXI2111, 1);
		}
		if (((g.heldObject() == kO_LISTE_BOITES ? 1 : 0) != 0) && ((int32)g.var(kV_ANXI2131) == 0)) {
			g.dialogue("ANXI2131", "I201ANJ", "I201XPI");
			g.screenEffect();
			g.dialogue("XPID2132", "I201XPI", "I201ANJ");
			g.minutesAdd("MINBP212");
			g.setVar(kV_ANXI2131, 1);
			g.objectDestroy(kO_MANDAT2);
			g.objectToInventory(kO_MANDAT3);
		}
		if (((g.heldObject() == kO_ORIGINAUX ? 1 : 0) != 0) && ((int32)g.var(kV_ANXI2211) == 0)) {
			g.dialogue("ANXI2211", "I201ANJ", "I201XPI");
			g.screenEffect();
			g.minutesAdd("MINBP221");
			g.setVar(kV_ANXI2211, 1);
		}
		if (((g.heldObject() == kO_CONFES3 ? 1 : 0) != 0) && ((int32)g.var(kV_ANXI3111) == 0)) {
			g.dialogue("ANXI3111", "I201ANJ", "I201XPI");
			g.screenEffect();
			g.dialogue("XPID3112", "I201XPI", "I201ANJ");
			g.minutesAdd("MINBP311");
			g.setVar(kV_ANXI3111, 1);
		}
		if (((g.heldObject() == kO_INDIC3 ? 1 : 0) != 0) && ((int32)g.var(kV_ANXI3121) == 0)) {
			g.dialogue("ANXI3121", "I201ANJ", "I201XPI");
			g.setVar(kV_ANXI3121, 1);
		}
		if (((g.heldObject() == kO_INDICE_CACHETS ? 1 : 0) != 0) && ((int32)g.var(kV_XPID3122) == 0)) {
			g.screenEffect();
			g.dialogue("XPID3122", "I201XPI", "I201ANJ");
			g.minutesAdd("MINBP319");
			g.setVar(kV_XPID3122, 1);
			g.objectDestroy(kO_MANDAT3);
			g.objectToInventory(kO_MANDAT4);
		}
	}
}

// bpiw102 (0x42a790)
static void place_bpiw102(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(130, 1880, 670, 2047, 0, "cpc120", 0, -1.0, -1.0);
		g.zoneGo(130, 0, 670, 155, 0, "cpc120", 0, -1.0, -1.0);
		g.zoneGo(255, 705, 550, 865, 0, "bpiw202", 0, -1.0, -1.0);
		g.zoneDoc(478, 526, 520, 552, 0, "ecriture");
		g.zoneDoc(438, 516, 463, 531, 0, "ecriture");
		g.zoneLabel(425, 800, 464, 883, 0, "kang");
		g.zoneLabel(430, 916, 464, 971, 0, "kang");
		g.zoneLabel(467, 911, 521, 950, 0, "dossiers");
		g.zoneDoc(341, 1231, 450, 1271, 0, "archive");
		g.zoneDoc(544, 1065, 649, 1140, 0, "eunuques");
		g.zoneDoc(399, 1057, 543, 1153, 0, "eunuques");
		g.zoneDoc(339, 1091, 398, 1121, 0, "eunuques");
		g.zoneDoc(437, 986, 520, 1039, 0, "mandarins");
		g.zoneDoc(460, 954, 506, 985, 0, "mandarins");
		g.zoneDoc(393, 1007, 437, 1034, 0, "mandarins");
		g.warp("bpiw102");
	}
	g.zoneHandler();
}

// bpiw202 (0x42a1d0)
static void place_bpiw202(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(285, 1780, 547, 1923, 0, "bpiw102", 0, -1.0, -1.0);
		g.zoneDoc(465, 2040, 497, 2047, 0, "ecriture");
		g.zoneDoc(465, 0, 497, 8, 0, "ecriture");
		g.zoneDoc(448, 191, 476, 204, 0, "ecriture");
		g.zoneDoc(542, 1423, 594, 1448, 0, "ecriture");
		g.zoneLabel(588, 1540, 608, 1584, 0, "pose_pinceau");
		g.zoneDoc(465, 1037, 555, 1119, 0, "chauffage");
		g.zoneDoc(466, 797, 535, 946, 0, "chauffage");
		g.zoneDoc(325, 1322, 460, 1398, 0, "archive");
		g.zoneDoc(437, 986, 463, 1000, 0, "the");
		g.zoneDoc(464, 533, 482, 549, 0, "the");
		g.zoneDoc(453, 671, 470, 686, 0, "the");
		g.zoneDoc(459, 383, 474, 396, 0, "the");
		g.zoneLabel(329, 1467, 382, 1604, 0, "bandeau_bpi");
		g.zoneLabel(503, 1619, 544, 1667, 0, "dossiers");
		g.zoneLabel(545, 1619, 579, 1655, 0, "dossiers");
		g.zoneDoc(394, 1648, 517, 1703, 0, "eunuques");
		g.zoneDoc(355, 1668, 393, 1687, 0, "eunuques");
		g.zoneDoc(476, 1475, 538, 1591, 0, "mandarins");
		g.zoneDoc(448, 1495, 475, 1571, 0, "mandarins");
		g.zoneDoc(510, 1592, 563, 1618, 0, "mandarins");
		g.zoneDoc(510, 1447, 560, 1474, 0, "mandarins");
		g.zoneTalk(399, 1519, 447, 1551, 1);
		g.warp("bpiw202");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((((int32)g.var(kV_CHAPITRE) == 12) && (((int32)g.var(kV_Cache_Wen) == 1) || ((int32)g.var(kV_3GIMD01) == 0) || ((int32)g.var(kV_ANMI4021) == 0))) || (((int32)g.var(kV_CHAPITRE) == 13) && ((int32)g.var(kV_ANMI4111) == 0))) {
		g.zoneEnable(22);
	}
	if (g.clickedZone() == 22) {
		if (((int32)g.var(kV_CHAPITRE) == 12) && ((int32)g.var(kV_3GIMD01) == 0)) {
			g.dialogue("MPID3991", "I202MPI", nullptr);
			g.setVar(kV_3GIMD01, 1);
			g.setMem(0x48f27c, -1);
		}
		if (((int32)g.var(kV_CHAPITRE) == 13) && ((int32)g.var(kV_ANMI4111) == 0)) {
			g.dialogue("MPID4091", "I202MPI", nullptr);
			g.dialogue("ANMI4111", "I202ANJ", "I202MPI");
			g.setVar(kV_ANMI4111, 1);
			g.zoneDisable(22);
		}
		if (g.clickedZone() == 22) {
			if (((int32)g.var(kV_CHAPITRE) == 12) && ((int32)g.var(kV_ANMI4021) == 0)) {
				g.dialogue("ANMI4021", "I202ANJ", "I202MPI");
				g.setVar(kV_ANMI4021, 1);
				g.minutesAdd("MINBP401");
				g.zoneDisable(22);
			}
		}
	}
	if ((g.clickedZone() == 18) || (g.clickedZone() == 19) || (g.clickedZone() == 21) || (g.clickedZone() == 22)) {
		if (((g.heldObject() == kO_INDICE_CACHETS2 ? 1 : 0) != 0) && ((int32)g.var(kV_ANMI3211) == 0)) {
			g.dialogue("MPID3211", "I202MPI", "I202ANJ");
			g.screenEffect();
			g.dialogue("MPID3221", "I202MPI", nullptr);
			g.minutesAdd("MINBP321");
			g.objectDestroy(kO_CIRE);
			g.setVar(kV_ANMI3211, 1);
		}
		if (((g.heldObject() == kO_CIRE ? 1 : 0) != 0) && ((int32)g.var(kV_ANMI3211) == 0)) {
			g.dialogue("MPID3211", "I202MPI", "I202ANJ");
			g.screenEffect();
			g.dialogue("MPID3221", "I202MPI", "I202ANJ");
			g.minutesAdd("MINBP321");
			g.objectDestroy(kO_CIRE);
			g.setVar(kV_ANMI3211, 1);
		}
		if ((g.heldObject() == kO_PIECES ? 1 : 0) != 0) {
			g.objectDestroy(kO_PIECES);
		}
		if (((g.heldObject() == kO_SCEAUX ? 1 : 0) != 0) && ((int32)g.var(kV_ANMI4311) == 0)) {
			g.dialogue("ANMI4311", "I202ANJ", "I202MPI");
			g.screenEffect();
			g.dialogue("MPID4312", "I202MPI", "I202ANJ");
			g.objectDestroy(kO_SCEAUX);
			g.minutesAdd("MINBP431");
			g.screenEffect();
			g.setVar(kV_ANMI4311, 1);
			g.setVar(kV_CHAPITRE, 15);
		}
		if (((g.heldObject() == kO_PLBOMB ? 1 : 0) != 0) && ((int32)g.var(kV_ANMI5121) == 0)) {
			g.dialogue("ANMI5121", "I202ANJ", "I202MPI");
			g.setVar(kV_ANMI5121, 1);
		}
		if (((g.heldObject() == kO_EDI ? 1 : 0) != 0) && ((int32)g.var(kV_ANMI5111) == 0)) {
			g.dialogue("ANMI5111", "I202ANJ", "I202MPI");
			g.setVar(kV_ANMI5111, 1);
		}
	}
}

// aio100 (0x429f80)
static void place_aio100(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(295, 465, 431, 554, 0, "cgc120", 0, 0.0, 0.0);
		g.zoneGo(258, 943, 491, 1101, 0, "pne110", 0, -1.0, -1.0);
		g.zoneGo(318, 1493, 426, 1575, 0, "aio200", 0, -1.0, -1.0);
		g.zoneDoc(334, 715, 489, 753, 0, "eclairage");
		g.zoneDoc(334, 1286, 491, 1327, 0, "eclairage");
		g.zoneLabel(163, 1002, 223, 1044, 0, "pne");
		g.zoneTalk(353, 1138, 383, 1165, 1);
		g.zoneTalk(352, 891, 383, 915, 1);
		g.warp("aio100");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_CHAPITRE) == 9) && ((int32)g.var(kV_ENED3111) == 1) && ((int32)g.var(kV_GIBD3111) == 0) && ((int32)g.var(kV_GIAD3111) == 0)) {
		g.zoneEnable(6);
		g.zoneEnable(7);
	}
	if ((g.clickedZone() == 6) && ((int32)g.var(kV_CHAPITRE) == 9) && ((int32)g.var(kV_ENED3111) == 1) && ((int32)g.var(kV_GIBD3111) == 0) && ((int32)g.var(kV_GIAD3111) == 0)) {
		g.dialogue("GIAD3111", "B100gid", "B100ANJ");
		g.setVar(kV_GIAD3111, 1);
		g.zoneDisable(6);
		g.zoneDisable(7);
	}
	if ((g.clickedZone() == 7) && ((int32)g.var(kV_CHAPITRE) == 9) && ((int32)g.var(kV_ENED3111) == 1) && ((int32)g.var(kV_GIAD3111) == 0) && ((int32)g.var(kV_GIBD3111) == 0)) {
		g.dialogue("GIBD3111", "B100gic", "B100ANJ");
		g.setVar(kV_GIBD3111, 1);
		g.zoneDisable(7);
		g.zoneDisable(6);
	}
}

// aio200 (0x429d00)
static void place_aio200(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(318, 473, 424, 551, 0, "aio100", 0, -1.0, -1.0);
		g.zoneTalk(289, 1946, 407, 1981, 1);
		g.zoneTalk(289, 48, 407, 83, 1);
		g.zoneGo(237, 1954, 399, 2047, 0, "cpc110", 0, -1.0, -1.0);
		g.zoneGo(237, 0, 399, 79, 0, "cpc110", 0, -1.0, -1.0);
		g.zoneGo(325, 1498, 427, 1577, 0, "aio300", 0, -1.0, -1.0);
		g.zoneDoc(389, 1439, 435, 1482, 0, "jarres");
		g.warp("aio200");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_CHAPITRE) == 9) && (((int32)g.var(kV_GIAD3111) == 1) || ((int32)g.var(kV_GIBD3111) == 1)) && ((int32)g.var(kV_GIED3111) == 0) && ((int32)g.var(kV_GIFD3111) == 0)) {
		g.zoneEnable(1);
		g.zoneEnable(2);
	}
	if ((g.clickedZone() == 1) && ((int32)g.var(kV_CHAPITRE) == 9) && (((int32)g.var(kV_GIAD3111) == 1) || ((int32)g.var(kV_GIBD3111) == 1)) && ((int32)g.var(kV_GIED3111) == 0) && ((int32)g.var(kV_GIFD3111) == 0)) {
		g.dialogue("GIED3111", "C200GIA", "B500ANJ");
		g.setVar(kV_GIED3111, 1);
		g.zoneDisable(1);
		g.zoneDisable(2);
	}
	if ((g.clickedZone() == 2) && ((int32)g.var(kV_CHAPITRE) == 9) && (((int32)g.var(kV_GIAD3111) == 1) || ((int32)g.var(kV_GIBD3111) == 1)) && ((int32)g.var(kV_GIED3111) == 0) && ((int32)g.var(kV_GIFD3111) == 0)) {
		g.dialogue("GIFD3111", "C200GIB", "B500ANJ");
		g.setVar(kV_GIFD3111, 1);
		g.zoneDisable(2);
		g.zoneDisable(1);
	}
}

// aio300 (0x429c60)
static void place_aio300(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(316, 471, 434, 547, 0, "aio200", 0, -1.0, -1.0);
		g.zoneGo(287, 1481, 443, 1602, 0, "aio400", 0, -1.0, -1.0);
		g.zoneDoc(386, 551, 420, 584, 0, "jarres");
		g.warp("aio300");
	}
	g.zoneHandler();
}

// aio400 (0x429bc0)
static void place_aio400(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(325, 478, 419, 544, 0, "aio300", 0, -1.0, -1.0);
		g.zoneGo(322, 1499, 418, 1579, 0, "aio500", 0, -1.0, -1.0);
		g.zoneDoc(383, 1471, 416, 1499, 0, "jarres");
		g.warp("aio400");
	}
	g.zoneHandler();
}

// aio500 (0x4297e0)
static void place_aio500(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(323, 477, 416, 544, 0, "aio400", 0, -1.0, -1.0);
		g.zoneTalk(290, 1946, 311, 1991, 1);
		g.zoneTalk(290, 62, 309, 93, 1);
		g.zoneGo(285, 2001, 399, 2047, 0, "ctp110", 0, -1.0, -1.0);
		g.zoneGo(285, 0, 399, 48, 0, "ctp110", 0, -1.0, -1.0);
		g.zoneUse(316, 1939, 414, 1994, 1);
		g.zoneUse(313, 56, 412, 96, 1);
		g.warp("aio500");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_CHAPITRE) == 10) && ((int32)g.var(kV_GIMD3211) == 0) && ((int32)g.var(kV_GIND3211) == 0) && ((int32)g.var(kV_GIMD3221) == 0) && ((int32)g.var(kV_GIND3221) == 0)) {
		g.zoneEnable(1);
		g.zoneEnable(2);
	}
	if (((int32)g.var(kV_entree_PPF) == 0) && ((int32)g.var(kV_MODE_VISITE) == 0)) {
		g.zoneDisable(3);
		g.zoneDisable(4);
	}
	if (((int32)g.var(kV_entree_PPF) == 0) && ((int32)g.var(kV_CHAPITRE) == 10)) {
		g.zoneEnable(5);
		g.zoneEnable(6);
	}
	if ((g.clickedZone() == 1) && ((int32)g.var(kV_CHAPITRE) == 10) && ((int32)g.var(kV_GIMD3211) == 0) && ((int32)g.var(kV_GIND3211) == 0)) {
		g.dialogue("GIMD3211", "B500GIA", "B500ANJ");
		g.setVar(kV_GIMD3211, 1);
		g.zoneDisable(1);
		g.zoneDisable(2);
	}
	if ((g.clickedZone() == 2) && ((int32)g.var(kV_CHAPITRE) == 10) && ((int32)g.var(kV_GIND3211) == 0) && ((int32)g.var(kV_GIMD3211) == 0)) {
		g.dialogue("GIND3211", "B500GIB", "B500ANJ");
		g.setVar(kV_GIND3211, 1);
		g.zoneDisable(1);
		g.zoneDisable(2);
	}
	if ((g.clickedZone() == 5) && ((g.heldObject() == kO_MANDAT4 ? 1 : 0) != 0) && ((int32)g.var(kV_GIMD3221) == 0) && ((int32)g.var(kV_GIND3221) == 0)) {
		g.dialogue("GIMD3221", "B500GIA", "B500ANJ");
		g.setVar(kV_GIMD3221, 1);
		g.setVar(kV_entree_PPF, 1);
		g.zoneDisable(1);
		g.zoneDisable(2);
		g.zoneDisable(5);
		g.zoneDisable(6);
		g.zoneEnable(4);
		g.zoneEnable(3);
	}
	if ((g.clickedZone() == 6) && ((g.heldObject() == kO_MANDAT4 ? 1 : 0) != 0) && ((int32)g.var(kV_GIND3221) == 0) && ((int32)g.var(kV_GIMD3221) == 0)) {
		g.dialogue("GIND3221", "B500GIB", "B500ANJ");
		g.setVar(kV_GIND3221, 1);
		g.setVar(kV_entree_PPF, 1);
		g.zoneDisable(1);
		g.zoneDisable(2);
		g.zoneDisable(6);
		g.zoneDisable(5);
		g.zoneEnable(3);
		g.zoneEnable(4);
	}
}

// lgaw100 (0x429730)
static void place_lgaw100(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(276, 1952, 620, 2047, 0, "pne150", 0, -1.0, -1.0);
		g.zoneGo(276, 0, 620, 100, 0, "pne150", 0, -1.0, -1.0);
		g.zoneLabel(419, 1565, 572, 1693, 0, "coffre");
		g.zoneLabel(327, 929, 446, 1108, 0, "bandeau_lga");
		g.warp("lgaw100");
	}
	g.zoneHandler();
}

// lgaw101 (0x429080)
static void place_lgaw101(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(276, 1952, 620, 2047, 0, nullptr, 0, 0.0, 0.0);
		g.zoneGo(276, 0, 620, 100, 0, nullptr, 0, 0.0, 0.0);
		g.zoneLook(419, 1565, 572, 1693, 1, "meuble1", 0);
		g.zoneDoc(414, 156, 651, 254, 0, "eunuques");
		g.zoneTalk(360, 191, 413, 220, 1);
		g.zoneDoc(487, 944, 640, 1104, 0, "chefs_eunuques");
		g.zoneTalk(402, 1001, 486, 1042, 1);
		g.zoneLabel(327, 929, 446, 1108, 0, "bandeau_lga");
		g.zoneLabel(278, 387, 574, 600, 0, "bibliotheque");
		g.warp("lgaw101");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((int32)g.var(kV_VAR_ANID1121) != 0) {
		g.zoneEnable(2);
	}
	if ((((int32)g.var(kV_CHAPITRE) == 1) && ((int32)g.var(kV_XNED1011) == 0)) || (((int32)g.var(kV_CHAPITRE) == 2) && (((int32)g.var(kV_ANXN1111) == 0) || ((int32)g.var(kV_ANXN1121) == 0) || (((int32)g.var(kV_BOUDDHA) != 0) && ((int32)g.var(kV_ANXN1151) == 0) && ((int32)g.var(kV_ANXN1121) != 0)))) || (((int32)g.var(kV_CHAPITRE) == 5) && ((int32)g.var(kV_ANXN2111) == 0))) {
		g.zoneEnable(6);
	}
	if (((int32)g.var(kV_CHAPITRE) == 1) && ((int32)g.var(kV_XNED1011) == 1) && ((int32)g.var(kV_ENED1011) == 0)) {
		g.zoneEnable(4);
	}
	if ((g.clickedZone() == 4) && ((int32)g.var(kV_XNED1011) == 1) && ((int32)g.var(kV_ENED1011) == 0)) {
		g.dialogue("ENED1011", "G101EGC", "I201ANJ");
		g.screenEffect();
		g.dialogue("ENED1021", "G101EGC", "I201ANJ");
		g.screenEffect();
		g.dialogue("ENED1031", "G101EGC", "I201ANJ");
		g.setVar(kV_ENED1011, 1);
		g.minutesAdd("MINPN102");
		g.zoneDisable(4);
	}
	if ((g.clickedZone() == 0) || (g.clickedZone() == 1)) {
		if (((int32)g.var(kV_CHAPITRE) == 4) && ((int32)g.var(kV_PNEH201) == 0)) {
			g.dialogue("GDCD2011", "D150GDC", "D150ANJ");
			g.video("PNEH202");
			g.minutesAdd("MINPN202");
			g.setVar(kV_PNEH201, 1);
			g.setAngles(6.25, 0.0);
			g.gotoPlace("pne150");
			return;
		}
		g.setAngles(4.7, 0.0);
		g.gotoPlace("pne150");
		return;
	}
	if (g.clickedZone() == 6) {
		if (((int32)g.var(kV_CHAPITRE) == 1) && ((int32)g.var(kV_XNED1011) == 0)) {
			g.dialogue("XNED1011", "G101XNE", "G101ANJ");
			g.setVar(kV_XNED1011, 1);
			g.zoneDisable(6);
		}
		if ((int32)g.var(kV_CHAPITRE) == 2) {
			if (((int32)g.var(kV_ANXN1111) == 1) && ((int32)g.var(kV_ANXN1121) == 0)) {
				g.dialogue("ANXN1121", "G101ANJ", "G101XNE");
				g.setVar(kV_ANXN1121, 1);
				g.zoneDisable(6);
			}
			if ((int32)g.var(kV_ANXN1111) == 0) {
				g.dialogue("ANXN1111", "G101ANJ", "G101XNE");
				g.setVar(kV_ANXN1111, 1);
				g.zoneDisable(6);
			}
			if (((int32)g.var(kV_BOUDDHA) != 0) && ((int32)g.var(kV_ANXN1151) == 0) && ((int32)g.var(kV_ANXN1121) != 0)) {
				g.dialogue("ANXN1151", "G101ANJ", "G101XNE");
				g.setVar(kV_ANXN1151, 1);
				g.zoneDisable(6);
			}
		}
		if (((int32)g.var(kV_CHAPITRE) == 5) && ((int32)g.var(kV_ANXN2111) == 0)) {
			g.dialogue("ANXN2111", "G101ANJ", "G101XNE");
			g.setVar(kV_ANXN2111, 1);
			g.zoneDisable(6);
		}
	}
	if (g.clickedZone() == 5) {
		if (((g.heldObject() == kO_CLE_WANG ? 1 : 0) != 0) && ((int32)g.var(kV_ANXN1131) == 0)) {
			g.dialogue("ANXN1131", "G101ANJ", "G101XNE");
			g.setVar(kV_ANXN1131, 1);
		}
		if (((g.heldObject() == kO_POSTHUME ? 1 : 0) != 0) && ((int32)g.var(kV_ANXN1141) == 0)) {
			g.dialogue("ANXN1141", "G101ANJ", "G101XNE");
			g.setVar(kV_ANXN1141, 1);
		}
		if (((g.heldObject() == kO_CONFES1 ? 1 : 0) != 0) && ((int32)g.var(kV_ANXN2011) == 0)) {
			g.dialogue("ANXN2011", "G101ANJ", "G101XNE");
			g.setVar(kV_ANXN2011, 1);
		}
		if (((g.heldObject() == kO_INDIC1 ? 1 : 0) != 0) && ((int32)g.var(kV_ANXN2021) == 0)) {
			g.dialogue("ANXN2021", "G101ANJ", "G101XNE");
			g.setVar(kV_ANXN2021, 1);
		}
		if (((int32)g.var(kV_ANXN2111) != 0) && ((g.heldObject() == kO_CONFES2 ? 1 : 0) != 0) && ((int32)g.var(kV_ANXN2121) == 0)) {
			g.dialogue("ANXN2121", "G101ANJ", "G101XNE");
			g.setVar(kV_ANXN2121, 1);
			g.minutesAdd("MINPN211");
		}
		if (((int32)g.var(kV_ANXN2111) != 0) && ((g.heldObject() == kO_INDIC2 ? 1 : 0) != 0) && ((int32)g.var(kV_ANXN2131) == 0)) {
			g.dialogue("ANXN2131", "G101ANJ", "G101XNE");
			g.setVar(kV_ANXN2131, 1);
		}
		if (((g.heldObject() == kO_LISTE_BOITES ? 1 : 0) != 0) && ((int32)g.var(kV_ANXN2141) == 0)) {
			g.dialogue("ANXN2141", "G101ANJ", "G101XNE");
			g.setVar(kV_ANXN2141, 1);
			return;
		}
	}
}

// lgaw102 (0x428ea0)
static void place_lgaw102(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(276, 1952, 620, 2047, 0, "pne150", 0, 4.7, 0.0);
		g.zoneGo(276, 0, 620, 100, 0, "pne150", 0, 4.7, 0.0);
		g.zoneLook(419, 1565, 572, 1693, 0, "meuble1", 0);
		g.zoneDoc(414, 156, 651, 254, 0, "eunuques");
		g.zoneTalk(360, 191, 413, 220, 1);
		g.zoneLabel(327, 929, 446, 1108, 0, "bandeau_lga");
		g.warp("lgaw102");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_CHAPITRE) == 9) && ((int32)g.var(kV_ANXI3111) != 0) && ((int32)g.var(kV_ANMW3111) != 0) && ((int32)g.var(kV_ANEN3121) == 0)) {
		g.zoneEnable(4);
	}
	if ((g.clickedZone() == 4) && ((int32)g.var(kV_CHAPITRE) == 9)) {
		if (((int32)g.var(kV_ANEN3121) == 0) && ((int32)g.var(kV_ENED3111) == 1)) {
			g.dialogue("ANEN3121", "G102ANJ", "G102ENE");
			g.setVar(kV_ANEN3121, 1);
			g.zoneDisable(4);
		}
		if ((int32)g.var(kV_ENED3111) == 0) {
			g.dialogue("ENED3111", "G102ENE", "G102ANJ");
			g.minutesAdd("MINPN311");
			g.setVar(kV_ENED3111, 1);
			g.zoneDisable(4);
		}
	}
}

// indice1 (0x428dd0)
static void place_indice1(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(430, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(161, 257, 366, 502, 0, nullptr);
		g.image("indice1");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) && ((int32)g.var(kV_Venant_de_BOUDDHA) == 1)) {
		g.setVar(kV_Venant_de_BOUDDHA, 2);
		g.minutesAdd("MINPN201");
		g.soundStop();
		g.gotoPlace("lgaw101");
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("indice1");
	}
}

// confess1 (0x428d20)
static void place_confess1(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(430, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(14, 165, 373, 622, 0, nullptr);
		g.image("confess1");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) && ((int32)g.var(kV_Venant_de_BOUDDHA) == 1)) {
		g.soundStop();
		g.gotoPlace("bouddha3");
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("confess1");
	}
}

// bouddha2 (0x428c90)
static void place_bouddha2(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(411, 259, 429, 302, 0, nullptr);
		g.zoneTake(388, 270, 410, 312, 0, nullptr);
		g.image("bouddha2");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) || (g.clickedZone() == 1)) {
		g.setVar(kV_Venant_de_BOUDDHA, 1);
		g.objectToInventory(kO_CONFES1);
		g.gotoPlace("confess1");
	}
}

// bouddha3 (0x428c00)
static void place_bouddha3(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(400, 303, 432, 350, 0, nullptr);
		g.zoneTake(388, 338, 413, 376, 0, nullptr);
		g.image("bouddha3");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) || (g.clickedZone() == 1)) {
		g.objectToInventory(kO_INDIC1);
		g.gotoPlace("indice1");
	}
}

// meuble1 (0x428b10)
static void place_meuble1(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(404, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(19, 147, 352, 503, 0, nullptr);
		g.image("meuble1");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("meuble1A");
		g.gotoPlace("meuble2");
		return;
	}
	if (g.clickedZone() == 0) {
		if ((((int32)g.var(kV_CHAPITRE) >= 0) && ((int32)g.var(kV_CHAPITRE) <= 8)) || ((int32)g.var(kV_MODE_VISITE) == 1)) {
			g.gotoPlace("lgaw101");
			return;
		}
		if ((int32)g.var(kV_CHAPITRE) >= 9) {
			g.gotoPlace("lgaw102");
		}
	}
}

// meuble2 (0x4287c0)
static void place_meuble2(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(398, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(24, 0, 366, 159, 0, nullptr);
		g.zoneTake(29, 480, 361, 639, 0, nullptr);
		g.zoneTake(47, 178, 136, 321, 0, nullptr);
		g.zoneTake(141, 177, 245, 320, 0, nullptr);
		g.zoneUse(283, 218, 313, 269, 1);
		g.zoneTake(259, 177, 353, 318, 1, nullptr);
		g.image("meuble2");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_ANXN1121) != 0) && ((int32)g.var(kV_Cle_Wang_Utilisee) == 0) && ((g.objectState(kO_TOURNEVIS) != 0 ? 1 : 0) != 0) && ((g.objectState(kO_MARTEAU) != 0 ? 1 : 0) != 0) && ((g.objectState(kO_BURIN) != 0 ? 1 : 0) != 0)) {
		g.zoneEnable(5);
	}
	if ((int32)g.var(kV_Cle_Wang_Utilisee) != 0) {
		g.zoneEnable(6);
	}
	if (g.clickedZone() == 0) {
		g.soundQueue("meuble1B");
		if ((((int32)g.var(kV_CHAPITRE) >= 0) && ((int32)g.var(kV_CHAPITRE) <= 8)) || ((int32)g.var(kV_MODE_VISITE) == 1)) {
			g.gotoPlace("lgaw101");
			return;
		}
		if ((int32)g.var(kV_CHAPITRE) >= 9) {
			g.gotoPlace("lgaw102");
			return;
		}
	}
	if ((g.clickedZone() == 1) || (g.clickedZone() == 2)) {
		g.soundQueue("meuble1B");
		g.gotoPlace("meuble1");
		return;
	}
	if (g.clickedZone() == 3) {
		g.soundQueue("meuble2A");
		if ((int32)g.var(kV_Marteau) == 0) {
			g.gotoPlace("meuble30");
			return;
		}
		g.gotoPlace("meuble31");
		return;
	}
	if (g.clickedZone() == 4) {
		g.soundQueue("meuble2A");
		if (((int32)g.var(kV_Burin) == 0) && ((int32)g.var(kV_Tournevis) == 0)) {
			g.gotoPlace("meuble40");
			return;
		}
		if (((int32)g.var(kV_Burin) == 1) && ((int32)g.var(kV_Tournevis) == 0)) {
			g.gotoPlace("meuble41");
			return;
		}
		if (((int32)g.var(kV_Burin) == 0) && ((int32)g.var(kV_Tournevis) == 1)) {
			g.gotoPlace("meuble42");
			return;
		}
		if (((int32)g.var(kV_Burin) == 1) && ((int32)g.var(kV_Tournevis) == 1)) {
			g.gotoPlace("meuble43");
			return;
		}
	}
	if (g.clickedZone() == 6) {
		g.soundQueue("meuble2A");
		g.gotoPlace("meuble5");
		return;
	}
	if ((g.clickedZone() == 5) && ((g.heldObject() == kO_CLE_WANG ? 1 : 0) != 0)) {
		g.soundQueue("meuble2B");
		g.setVar(kV_Cle_Wang_Utilisee, 1);
		g.soundQueue("meuble2B");
		g.objectDestroy(kO_CLE_WANG);
		g.gotoPlace("meuble5");
	}
}

// meuble30 (0x4286c0)
static void place_meuble30(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(436, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(248, 195, 291, 245, 0, nullptr);
		g.zoneTake(292, 230, 327, 259, 0, nullptr);
		g.zoneTake(328, 249, 374, 285, 0, nullptr);
		g.zoneTake(220, 128, 420, 316, 1, nullptr);
		g.image("meuble30");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.soundQueue("meuble2A");
		g.gotoPlace("meuble2");
		return;
	}
	if ((g.clickedZone() >= 1) && (g.clickedZone() <= 3)) {
		g.objectToInventory(kO_MARTEAU);
		g.setVar(kV_Marteau, 1);
		g.gotoPlace("meuble31");
	}
}

// meuble31 (0x428630)
static void place_meuble31(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(443, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(245, 123, 441, 316, 1, nullptr);
		g.image("meuble31");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.soundQueue("meuble2A");
		g.gotoPlace("meuble2");
	}
}

// meuble40 (0x4284e0)
static void place_meuble40(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(444, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(329, 207, 364, 234, 0, nullptr);
		g.zoneTake(302, 223, 328, 244, 0, nullptr);
		g.zoneTake(282, 234, 301, 251, 0, nullptr);
		g.zoneTake(342, 255, 394, 275, 0, nullptr);
		g.zoneTake(284, 260, 341, 272, 0, nullptr);
		g.zoneTake(264, 158, 438, 316, 1, nullptr);
		g.image("meuble40");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.soundQueue("meuble2A");
		g.gotoPlace("meuble2");
		return;
	}
	if ((g.clickedZone() >= 1) && (g.clickedZone() <= 3)) {
		g.objectToInventory(kO_BURIN);
		g.setVar(kV_Burin, 1);
		g.gotoPlace("meuble41");
		return;
	}
	if ((g.clickedZone() >= 4) && (g.clickedZone() <= 5)) {
		g.objectToInventory(kO_TOURNEVIS);
		g.setVar(kV_Tournevis, 1);
		g.gotoPlace("meuble42");
	}
}

// meuble41 (0x428400)
static void place_meuble41(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(443, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(342, 255, 394, 275, 0, nullptr);
		g.zoneTake(284, 260, 341, 272, 0, nullptr);
		g.zoneTake(268, 158, 439, 314, 1, nullptr);
		g.image("meuble41");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.soundQueue("meuble2A");
		g.gotoPlace("meuble2");
		return;
	}
	if ((g.clickedZone() >= 1) && (g.clickedZone() <= 2)) {
		g.objectToInventory(kO_TOURNEVIS);
		g.setVar(kV_Tournevis, 1);
		g.gotoPlace("meuble43");
	}
}

// meuble42 (0x428310)
static void place_meuble42(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(443, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(329, 207, 364, 234, 0, nullptr);
		g.zoneTake(302, 223, 328, 244, 0, nullptr);
		g.zoneTake(282, 234, 301, 251, 0, nullptr);
		g.zoneTake(267, 153, 436, 316, 0, nullptr);
		g.image("meuble42");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.soundQueue("meuble2A");
		g.gotoPlace("meuble2");
		return;
	}
	if ((g.clickedZone() >= 1) && (g.clickedZone() <= 3)) {
		g.objectToInventory(kO_BURIN);
		g.setVar(kV_Burin, 1);
		g.gotoPlace("meuble43");
	}
}

// meuble43 (0x428280)
static void place_meuble43(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(443, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(264, 157, 436, 315, 1, nullptr);
		g.image("meuble43");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.soundQueue("meuble2A");
		g.gotoPlace("meuble2");
	}
}

// meuble5 (0x428160)
static void place_meuble5(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(443, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(311, 206, 426, 280, 1, nullptr);
		g.zoneTake(293, 160, 438, 319, 1, nullptr);
		g.image("meuble5");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((int32)g.var(kV_Venant_de_BOUDDHA) == 0) {
		g.zoneEnable(1);
	}
	if (g.clickedZone() == 0) {
		g.soundQueue("meuble2A");
		g.gotoPlace("meuble2");
		return;
	}
	if ((g.clickedZone() == 1) && ((int32)g.var(kV_Venant_de_BOUDDHA) == 0)) {
		if (g.puzzle(2, 0) != 0) {
			g.setVar(kV_CHAPITRE, 4);
			g.setVar(kV_Venant_de_BOUDDHA, 1);
			g.objectDestroy(kO_POSTHUME);
			g.gotoPlace("bouddha2");
			return;
		}
		g.gotoPlace("meuble5");
	}
}

// espw100 (0x427fe0)
static void place_espw100(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(240, 1379, 542, 1611, 0, "cpc230", 0, -1.0, -1.0);
		g.zoneGo(300, 947, 536, 1097, 0, "espw201", 0, -1.0, -1.0);
		g.zoneLabel(242, 2038, 310, 2047, 0, "bandeau_esp_ouest");
		g.zoneLabel(242, 0, 310, 209, 0, "bandeau_esp_ouest");
		g.zoneLabel(311, 627, 345, 725, 0, "bandeau_esp_sud");
		g.zoneDoc(399, 677, 437, 698, 0, "porcelaines");
		g.zoneDoc(439, 624, 530, 643, 0, "mobilier");
		g.zoneDoc(435, 644, 452, 730, 0, "mobilier");
		g.zoneDoc(431, 731, 517, 750, 0, "mobilier");
		g.zoneDoc(427, 1068, 515, 1230, 0, "mobilier");
		g.zoneDoc(530, 75, 635, 208, 0, "jeu");
		g.zoneLabel(540, 1800, 660, 2047, 0, "kang");
		g.zoneLabel(540, 0, 660, 68, 0, "kang");
		g.zoneLabel(510, 184, 610, 320, 0, "kang");
		g.warp("espw100");
	}
	g.zoneHandler();
}

// espw101 (0x427dc0)
static void place_espw101(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(240, 1379, 542, 1611, 0, "cpc230", 0, -1.0, -1.0);
		g.zoneGo(300, 947, 536, 1097, 0, "espw201", 0, -1.0, -1.0);
		g.zoneLabel(242, 2038, 310, 2047, 0, "bandeau_esp_ouest");
		g.zoneLabel(242, 0, 310, 209, 0, "bandeau_esp_ouest");
		g.zoneLabel(311, 627, 345, 725, 0, "bandeau_esp_sud");
		g.zoneDoc(399, 677, 437, 698, 0, "porcelaines");
		g.zoneDoc(439, 624, 530, 643, 0, "mobilier");
		g.zoneLabel(435, 644, 452, 730, 0, "table");
		g.zoneDoc(431, 731, 517, 750, 0, "mobilier");
		g.zoneDoc(427, 1068, 515, 1230, 0, "mobilier");
		g.zoneLook(530, 75, 635, 208, 1, nullptr, 0);
		g.zoneLabel(540, 1800, 660, 2047, 0, "kang");
		g.zoneLabel(540, 0, 660, 68, 0, "kang");
		g.zoneDoc(510, 184, 610, 320, 0, "chauffage");
		g.zoneLabel(285, 430, 537, 549, 0, "bibliotheque");
		g.warp("espw101");
		if (((int32)g.var(kV_ANMW2031) == 1) && ((int32)g.var(kV_Venant_de_GO) == 0)) {
			g.zoneEnable(10);
		}
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 10) {
		if (g.puzzle(4, 0) == 1) {
			g.objectDestroy(kO_CONFES1);
			g.objectDestroy(kO_INDIC1);
			g.setVar(kV_CHAPITRE, 5);
			g.gotoPlace("go20");
			return;
		}
		g.setVar(kV_GO, ((int32)g.var(kV_GO) + 1));
	}
}

// espw200 (0x427c50)
static void place_espw200(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(229, 1904, 517, 2047, 0, "espw100", 0, -1.0, -1.0);
		g.zoneGo(229, 0, 517, 120, 0, "espw100", 0, -1.0, -1.0);
		g.zoneDoc(530, 670, 619, 714, 0, "calligraphie");
		g.zoneLabel(572, 549, 603, 568, 0, "cure_dents");
		g.zoneLabel(316, 263, 347, 349, 0, "bandeau_esp_sud");
		g.zoneDoc(429, 245, 503, 269, 0, "mobilier");
		g.zoneDoc(432, 270, 448, 339, 0, "mobilier");
		g.zoneDoc(436, 340, 512, 360, 0, "mobilier");
		g.zoneDoc(490, 1240, 680, 1909, 0, "mobilier");
		g.zoneDoc(184, 732, 538, 1260, 0, "bibliotheque");
		g.zoneLabel(591, 412, 616, 454, 0, "pierre_encre");
		g.zoneLabel(616, 491, 637, 549, 0, "pose_pinceau");
		g.zoneLabel(609, 595, 626, 630, 0, "pierre_eau");
		g.warp("espw200");
	}
	g.zoneHandler();
}

// espw201 (0x4272f0)
static void place_espw201(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(229, 1954, 517, 2047, 0, "espw101", 0, -1.0, -1.0);
		g.zoneGo(229, 0, 517, 120, 0, "espw101", 0, -1.0, -1.0);
		g.zoneDoc(425, 443, 538, 566, 0, "mandarins");
		g.zoneTalk(358, 483, 424, 511, 1);
		g.zoneDoc(530, 670, 619, 714, 0, "calligraphie");
		g.zoneDoc(405, 643, 549, 756, 0, "princes");
		g.zoneDoc(333, 675, 404, 717, 0, "princes");
		g.zoneTake(572, 549, 603, 568, 1, nullptr);
		g.zoneLabel(316, 263, 347, 349, 0, "bandeau_esp_sud");
		g.zoneDoc(429, 245, 503, 269, 0, "mobilier");
		g.zoneDoc(432, 270, 448, 339, 0, "mobilier");
		g.zoneDoc(436, 340, 512, 360, 0, "mobilier");
		g.zoneDoc(490, 1240, 680, 1909, 0, "mobilier");
		g.zoneDoc(184, 732, 538, 1260, 1, "bibliotheque");
		g.zoneLabel(591, 412, 616, 454, 0, "pierre_encre");
		g.zoneLabel(616, 491, 637, 549, 0, "pose_pinceau");
		g.zoneLabel(609, 595, 626, 630, 0, "pierre_eau");
		g.zoneLook(540, 310, 650, 720, 1, nullptr, 0);
		g.zoneLabel(466, 1280, 604, 1334, 0, "chandelier_jade");
		g.zoneDoc(470, 1854, 534, 1886, 0, "jade");
		if ((int32)g.var(kV_var221) == 2) {
			g.dialogue("MWED3011", "J201MWE", "J201ANJ");
			g.setVar(kV_var221, 3);
		}
		g.warp("espw201");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_ANXI2211) != 0) && ((int32)g.var(kV_T14) == 0) && ((g.objectState(kO_ORIGINAUX) == 3 ? 1 : 0) != 0)) {
		g.zoneEnable(17);
	}
	if (g.clickedZone() == 17) {
		if ((g.objectState(kO_PINCEAU_ESP) != 0 ? 1 : 0) == 0) {
			g.objectDestroy(kO_ORIGINAUX);
			g.gotoPlace("table");
			return;
		}
		g.gotoPlace("table10");
		return;
	}
	if ((int32)g.var(kV_T14) != 0) {
		g.zoneDisable(17);
	}
	if ((((int32)g.var(kV_CHAPITRE) == 9) && ((int32)g.var(kV_ANXI3151) == 1) && ((int32)g.var(kV_ANMW3151) == 0)) || (((int32)g.var(kV_CHAPITRE) == 4) && ((((int32)g.var(kV_MWED1011) == 0) && ((int32)g.var(kV_ANMW201A) == 0)) || (((int32)g.var(kV_MWED1011) == 1) && ((int32)g.var(kV_ANMW2011) == 0)))) || (((int32)g.var(kV_CHAPITRE) == 1) && ((int32)g.var(kV_Premiere_entree_ESP) == 0) && ((int32)g.var(kV_MWED1011) == 0))) {
		g.zoneEnable(3);
	}
	if (((int32)g.var(kV_Venant_de_GO) == 0) && ((((int32)g.var(kV_GO) == 1) && ((int32)g.var(kV_Aide_Go_1) == 0)) || ((int32)g.var(kV_GO) >= 2))) {
		g.zoneEnable(3);
	}
	if (((int32)g.var(kV_CHAPITRE) == 6) && ((int32)g.var(kV_ANXI2211) != 0) && ((int32)g.var(kV_MWED2131) == 0)) {
		g.zoneEnable(3);
	}
	if ((g.clickedZone() == 7) && (g.unknownCall(0x403520, 27) == 0)) {
		g.objectToInventory(kO_CURE_DENTS);
	}
	if (g.clickedZone() == 3) {
		if (((int32)g.var(kV_CHAPITRE) == 1) && ((int32)g.var(kV_Premiere_entree_ESP) == 0) && ((int32)g.var(kV_MWED1011) == 0)) {
			g.dialogue("MWED1011", "J201MWE", "J201ANJ");
			g.dialogue("PRND1011", "J201PRN", "J201MWE");
			g.setVar(kV_MWED1011, 1);
			g.setVar(kV_Premiere_entree_ESP, 1);
			g.zoneDisable(3);
		}
		if ((int32)g.var(kV_CHAPITRE) == 4) {
			if (((int32)g.var(kV_MWED1011) == 1) && ((int32)g.var(kV_ANMW2011) == 0)) {
				g.dialogue("ANMW2011", "J201ANJ", "J201MWE");
				g.setVar(kV_ANMW2011, 1);
				g.zoneDisable(3);
			}
			if (((int32)g.var(kV_MWED1011) == 0) && ((int32)g.var(kV_ANMW201A) == 0)) {
				g.dialogue("ANMW201A", "J201ANJ", "J201MWE");
				g.setVar(kV_ANMW201A, 1);
				g.zoneDisable(3);
			}
			if ((int32)g.var(kV_GO) == 1) {
				g.dialogue("ANMW2041", "J201ANJ", "J201MWE");
				g.setVar(kV_Aide_Go_1, 1);
				g.zoneDisable(3);
			}
			if ((int32)g.var(kV_GO) >= 2) {
				g.dialogue("ANMW2051", "J201ANJ", "J201MWE");
				g.zoneDisable(3);
			}
		}
		if (((int32)g.var(kV_CHAPITRE) == 6) && ((int32)g.var(kV_ANXI2211) != 0)) {
			g.dialogue("MWED2131", "J201MWE", "J201ANJ");
			g.setVar(kV_MWED2131, 1);
			g.zoneDisable(3);
		}
		if (((int32)g.var(kV_CHAPITRE) == 9) && ((int32)g.var(kV_ANXI3151) == 1) && ((int32)g.var(kV_ANMW3151) == 0)) {
			g.dialogue("ANMW3151", "J201ANJ", "J201MWE");
			g.setVar(kV_ANMW3151, 1);
			g.zoneDisable(3);
		}
	}
	if (g.clickedZone() == 2) {
		if (((int32)g.var(kV_ANXI2211) != 0) && ((int32)g.var(kV_MWED2131) != 0) && ((g.heldObject() == kO_ORIGINAUX ? 1 : 0) != 0) && ((g.objectState(kO_ORIGINAUX) == 3 ? 1 : 0) == 0)) {
			g.objectDestroy(kO_ORIGINAUX);
			g.setVar(kV_var221, 1);
			g.gotoPlace("table");
			return;
		}
		if (((g.heldObject() == kO_POSTHUME ? 1 : 0) != 0) && ((int32)g.var(kV_MWED1111) == 0)) {
			g.dialogue("MWED1111", "J201MWE", "J201ANJ");
			g.setVar(kV_MWED1111, 1);
		}
		if (((g.heldObject() == kO_CONFES1 ? 1 : 0) != 0) && ((int32)g.var(kV_ANMW2021) == 0)) {
			g.dialogue("ANMW2021", "J201ANJ", "J201MWE");
			g.setVar(kV_ANMW2021, 1);
		}
		if (((g.heldObject() == kO_INDIC1 ? 1 : 0) != 0) && ((int32)g.var(kV_ANMW2031) == 0)) {
			g.dialogue("ANMW2031", "J201ANJ", "J201MWE");
			if ((int32)g.var(kV_PRND1011) == 0) {
				g.dialogue("PRND2011", "J201PRN", "J201MWE");
				g.setVar(kV_ANMW2031, 1);
				g.setVar(kV_PRND2011, 1);
			}
		}
		if (((g.heldObject() == kO_CONFES2 ? 1 : 0) != 0) && ((int32)g.var(kV_ANMW2111) == 0)) {
			g.dialogue("ANMW2111", "J201ANJ", "J201MWE");
			g.setVar(kV_ANMW2111, 1);
		}
		if (((g.heldObject() == kO_INDIC2 ? 1 : 0) != 0) && ((int32)g.var(kV_ANMW2121) == 0)) {
			g.dialogue("ANMW2121", "J201ANJ", "J201MWE");
			g.setVar(kV_ANMW2121, 1);
		}
		if (((g.heldObject() == kO_CONFES3 ? 1 : 0) != 0) && ((int32)g.var(kV_ANMW3111) == 0)) {
			g.dialogue("ANMW3111", "J201ANJ", "J201MWE");
			g.setVar(kV_ANMW3111, 1);
		}
		if (((g.heldObject() == kO_INDIC3 ? 1 : 0) != 0) && ((int32)g.var(kV_ANMW3121) == 0)) {
			g.dialogue("ANMW3121", "J201ANJ", "J201MWE");
			g.setVar(kV_ANMW3121, 1);
		}
		if (((g.heldObject() == kO_INDICE_CACHETS2 ? 1 : 0) != 0) && ((int32)g.var(kV_ANMW3211) == 0)) {
			g.dialogue("ANMW3211", "J201ANJ", "J201MWE");
			g.setVar(kV_ANMW3211, 1);
		}
		if (((g.heldObject() == kO_CONFES4 ? 1 : 0) != 0) && ((int32)g.var(kV_ANMW4111) == 0)) {
			g.dialogue("ANMW4111", "J201ANJ", "J201MWE");
			g.setVar(kV_ANMW4111, 1);
		}
	}
}

// table (0x427210)
static void place_table(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(420, 0, 479, 639, 1, nullptr, 0, 0.0, 0.0);
		g.zoneTake(44, 133, 160, 357, 1, "table");
		g.zoneTake(141, 545, 266, 628, 0, nullptr);
		g.image("table");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 2) {
		g.soundQueue("pinceau2");
		g.objectToCursor(kO_PINCEAU_ESP);
		g.gotoPlace("table10");
		return;
	}
	if (g.clickedZone() == 0) {
		g.objectToInventory(kO_ORIGINAUX);
		g.setAngles(-1.0, -1.0);
		g.gotoPlace("espw201");
	}
}

// table10 (0x4270f0)
static void place_table10(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(398, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneUse(45, 137, 162, 346, 0);
		g.zoneUse(234, 370, 296, 445, 0);
		g.zoneUse(138, 542, 273, 632, 1);
		g.image("table10");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 2) {
		g.setVar(kV_Pinceau_mouille, 1);
		g.soundQueue("pinceau1");
	}
	if (g.clickedZone() == 0) {
		g.objectToInventory(kO_ORIGINAUX);
		g.setAngles(-1.0, -1.0);
		g.gotoPlace("espw201");
		return;
	}
	if ((g.clickedZone() == 1) && ((int32)g.var(kV_CHAPITRE) == 6) && ((g.heldObject() == kO_PINCEAU_ESP ? 1 : 0) != 0)) {
		g.gotoPlace("table11");
	}
}

// table11 (0x426f10)
static void place_table11(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneUse(116, 28, 373, 216, 0);
		g.zoneUse(156, 222, 321, 400, 0);
		g.zoneUse(105, 417, 338, 575, 0);
		g.zoneGo(400, 5, 475, 635, 0, "table10", 0, 0.0, 0.0);
		g.image("table11");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) && ((g.heldObject() == kO_PINCEAU_ESP ? 1 : 0) != 0)) {
		if ((int32)g.var(kV_Pinceau_mouille) == 1) {
			g.soundQueue("anjesp21");
			g.gotoPlace("table12");
			return;
		}
		g.soundQueue("anjesp22");
	}
	if ((g.clickedZone() == 1) && ((g.heldObject() == kO_PINCEAU_ESP ? 1 : 0) != 0)) {
		if ((int32)g.var(kV_Pinceau_mouille) == 1) {
			g.soundQueue("anjesp21");
			g.gotoPlace("table13");
			return;
		}
		g.soundQueue("anjesp22");
	}
	if ((g.clickedZone() == 2) && ((g.heldObject() == kO_PINCEAU_ESP ? 1 : 0) != 0)) {
		if ((int32)g.var(kV_Pinceau_mouille) == 1) {
			g.soundQueue("anjesp21");
			g.setVar(kV_CHAPITRE, 8);
			g.objectDestroy(kO_ORIGINAUX);
			g.minutesAdd("MINES299");
			g.video("ideo");
			g.soundPlayWait("origine");
			g.soundQueue("anjpoc");
			g.gotoPlace("table14");
			return;
		}
		g.soundQueue("anjesp22");
	}
}

// table12 (0x426d90)
static void place_table12(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(392, 5, 475, 635, 0, "table10", 0, -1.0, -1.0);
		g.zoneUse(158, 236, 330, 407, 0);
		g.zoneUse(98, 390, 346, 586, 0);
		g.image("table12");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 1) && ((g.heldObject() == kO_PINCEAU_ESP ? 1 : 0) != 0)) {
		if ((int32)g.var(kV_Pinceau_mouille) == 1) {
			g.soundQueue("anjesp21");
			g.gotoPlace("table13");
			return;
		}
		g.soundQueue("anjesp22");
	}
	if ((g.clickedZone() == 2) && ((g.heldObject() == kO_PINCEAU_ESP ? 1 : 0) != 0)) {
		if ((int32)g.var(kV_Pinceau_mouille) == 1) {
			g.setVar(kV_CHAPITRE, 8);
			g.soundQueue("anjesp21");
			g.objectDestroy(kO_ORIGINAUX);
			g.minutesAdd("MINES299");
			g.video("ideo");
			g.soundPlayWait("origine");
			g.soundQueue("anjpoc");
			g.gotoPlace("table14");
			return;
		}
		g.soundQueue("anjesp22");
	}
}

// table13 (0x426c10)
static void place_table13(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(417, 5, 475, 635, 0, "table10", 0, -1.0, -1.0);
		g.zoneUse(115, 27, 373, 234, 0);
		g.zoneUse(92, 387, 343, 584, 0);
		g.image("table13");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 1) && ((g.heldObject() == kO_PINCEAU_ESP ? 1 : 0) != 0)) {
		if ((int32)g.var(kV_Pinceau_mouille) == 1) {
			g.soundQueue("anjesp21");
			g.gotoPlace("table12");
			return;
		}
		g.soundQueue("anjesp22");
	}
	if ((g.clickedZone() == 2) && ((g.heldObject() == kO_PINCEAU_ESP ? 1 : 0) != 0)) {
		if ((int32)g.var(kV_Pinceau_mouille) == 1) {
			g.setVar(kV_CHAPITRE, 8);
			g.soundQueue("anjesp21");
			g.objectDestroy(kO_ORIGINAUX);
			g.minutesAdd("MINES299");
			g.video("ideo");
			g.soundPlayWait("origine");
			g.soundQueue("anjpoc");
			g.gotoPlace("table14");
			return;
		}
		g.soundQueue("anjesp22");
	}
}

// table14 (0x426b20)
static void place_table14(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(423, 5, 475, 635, 0, nullptr, 0, 0.0, 0.0);
		g.image("table14");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.objectDestroy(kO_PINCEAU_ESP);
		g.objectDestroy(kO_CONFES2);
		g.setVar(kV_T14, 1);
		g.objectToInventory(kO_ORIGINAUX);
		g.soundStop();
		if ((int32)g.var(kV_var221) == 1) {
			g.setVar(kV_var221, 2);
			g.gotoPlace("espw201");
			return;
		}
		g.gotoPlace("espw201");
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundPlayWait("origine");
		g.soundQueue("anjpoc");
	}
}

// espw102 (0x426940)
static void place_espw102(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(240, 1379, 542, 1611, 0, "cpc230", 0, -1.0, -1.0);
		g.zoneGo(300, 947, 536, 1097, 0, nullptr, 0, 0.0, 0.0);
		g.zoneLabel(242, 2038, 310, 2047, 0, "bandeau_esp_ouest");
		g.zoneLabel(242, 0, 310, 209, 0, "bandeau_esp_ouest");
		g.zoneLabel(311, 627, 345, 725, 0, "bandeau_esp_sud");
		g.zoneDoc(399, 677, 437, 698, 0, "porcelaines");
		g.zoneDoc(439, 624, 530, 643, 0, "mobilier");
		g.zoneDoc(435, 644, 452, 730, 0, "mobilier");
		g.zoneDoc(431, 731, 517, 750, 0, "mobilier");
		g.zoneDoc(427, 1068, 515, 1230, 0, "mobilier");
		g.zoneDoc(530, 75, 635, 208, 0, "jeu");
		g.zoneLabel(540, 1800, 660, 2047, 0, "kang");
		g.zoneLabel(540, 0, 660, 68, 0, "kang");
		g.zoneLabel(510, 184, 610, 320, 0, "kang");
		g.warp("espw102");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 1) {
		if (((int32)g.var(kV_MWED5011) == 0) && ((int32)g.var(kV_Venant_de_HORLOGE) != 0)) {
			g.dialogue("MWED5011", "J202MWE", "J202ANJ");
			g.setVar(kV_MWED5011, 1);
			g.setAngles(3.2, 0.0);
			g.gotoPlace("espw202");
			return;
		}
		g.setAngles(3.2, 0.0);
		g.gotoPlace("espw202");
	}
}

// espw202 (0x4267b0)
static void place_espw202(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(229, 1904, 517, 2047, 0, "espw102", 0, -1.0, -1.0);
		g.zoneGo(229, 0, 517, 120, 0, "espw102", 0, -1.0, -1.0);
		g.zoneDoc(425, 443, 538, 566, 0, "mandarins");
		g.zoneDoc(358, 483, 424, 511, 0, "mandarins");
		g.zoneDoc(530, 670, 619, 714, 0, "calligraphie");
		g.zoneLabel(572, 549, 603, 568, 0, "cure_dents");
		g.zoneLabel(316, 263, 347, 349, 0, "bandeau_esp_sud");
		g.zoneDoc(429, 245, 503, 269, 0, "mobilier");
		g.zoneDoc(432, 270, 448, 339, 0, "mobilier");
		g.zoneDoc(436, 340, 512, 360, 0, "mobilier");
		g.zoneDoc(490, 1240, 680, 1909, 0, "mobilier");
		g.zoneDoc(184, 732, 538, 1260, 0, "bibliotheque");
		g.zoneLabel(591, 412, 616, 454, 0, "pierre_encre");
		g.zoneLabel(616, 491, 637, 549, 0, "pose_pinceau");
		g.zoneLabel(609, 595, 626, 630, 0, "pierre_eau");
		g.warp("espw202");
	}
	g.zoneHandler();
}

// cgc110 (0x426530)
static void place_cgc110(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(248, 1487, 431, 1637, 0, "cgc120", 0, -1.0, -1.0);
		g.zoneGo(298, 948, 457, 1078, 0, nullptr, 0, 0.0, 0.0);
		g.zoneDoc(200, 800, 490, 1270, 0, "nwf");
		g.zoneDoc(270, 1271, 456, 1395, 0, "nwf");
		g.zoneDoc(322, 1396, 420, 1470, 0, "nwf");
		g.zoneLabel(59, 0, 530, 280, 0, "pag");
		g.zoneLabel(305, 1676, 354, 1776, 0, "shp");
		g.warp("cgc110");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 1) {
		if (((int32)g.var(kV_CHAPITRE) >= 16) || ((int32)g.var(kV_MODE_VISITE) == 1)) {
			g.setAngles(3.14, 0.0);
			g.gotoPlace("nwfw100");
			return;
		}
		if (((int32)g.var(kV_CHAPITRE) >= 0) && ((int32)g.var(kV_CHAPITRE) <= 10)) {
			g.setAngles(3.14, 0.0);
			g.gotoPlace("nwfw101");
			return;
		}
		if (((int32)g.var(kV_CHAPITRE) >= 11) && ((int32)g.var(kV_CHAPITRE) <= 15)) {
			if (((int32)g.var(kV_CHAPITRE) == 13) && ((int32)g.var(kV_MINXQ411) == 1) && ((int32)g.var(kV_NWFH401) == 0)) {
				g.video("NWFH431");
				g.dialogue("XQRD4091", "S102XQR", "AANJ102");
				g.dialogue("MPID4191", "I202MPI", nullptr);
				g.minutesAdd("MINNW419");
				g.setVar(kV_NWFH401, 1);
				g.setVar(kV_CHAPITRE, 14);
				g.objectDestroy(kO_LISTE_VICTIMES);
				g.setAngles(1.39, 0.0);
				g.gotoPlace("bpiw202");
				return;
			}
			g.setAngles(3.14, 0.0);
			g.gotoPlace("nwfw102");
			return;
		}
	}
}

// cgc120 (0x4263d0)
static void place_cgc120(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(323, 994, 419, 1049, 0, "aio100", 0, 1.57, 0.0);
		g.zoneGo(246, 1446, 447, 1630, 0, "cgc130", 0, -1.0, -1.0);
		g.zoneGo(299, 520, 424, 598, 0, "cgc110", 0, -1.0, -1.0);
		g.zoneLabel(278, 1728, 342, 1863, 0, "shp");
		g.zoneDoc(305, 638, 412, 887, 0, "nwf");
		g.zoneDoc(387, 1218, 423, 1251, 0, "jarres");
		g.zoneDoc(384, 1338, 413, 1362, 0, "jarres");
		g.zoneDoc(384, 1397, 404, 1413, 0, "jarres");
		g.zoneDoc(383, 1427, 395, 1439, 0, "jarres");
		g.zoneLabel(300, 315, 399, 511, 0, "pag");
		g.zoneDoc(372, 965, 426, 979, 0, "gardes");
		g.zoneDoc(371, 1066, 426, 1080, 0, "gardes");
		g.warp("cgc120");
	}
	g.zoneHandler();
}

// cgc130 (0x4262a0)
static void place_cgc130(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(249, 384, 448, 610, 0, "cgc120", 0, -1.0, -1.0);
		g.zoneGo(220, 1450, 452, 1640, 0, "cgc140", 0, -1.0, -1.0);
		g.zoneGo(224, 1336, 451, 1430, 0, "cgc310", 0, -1.0, -1.0);
		g.zoneDoc(270, 1180, 358, 1345, 0, "poc");
		g.zoneLabel(254, 1812, 334, 2032, 0, "shp");
		g.zoneDoc(385, 1227, 416, 1252, 0, "jarres");
		g.zoneDoc(389, 822, 428, 856, 0, "jarres");
		g.zoneDoc(385, 694, 412, 717, 0, "jarres");
		g.zoneDoc(383, 635, 406, 658, 0, "jarres");
		g.warp("cgc130");
	}
	g.zoneHandler();
}

// cgc140 (0x426120)
static void place_cgc140(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(250, 2011, 331, 2047, 0, "cgc230", 0, -1.0, -1.0);
		g.zoneGo(250, 0, 331, 44, 0, "cgc230", 0, -1.0, -1.0);
		g.zoneGo(263, 945, 458, 1077, 0, "cgc310", 0, -1.0, -1.0);
		g.zoneGo(257, 420, 440, 620, 0, "cgc130", 0, -1.0, -1.0);
		g.zoneGo(262, 1420, 440, 1620, 0, "cgc150", 0, -1.0, -1.0);
		g.zoneLabel(356, 912, 425, 942, 0, "lion_cgc");
		g.zoneLabel(356, 1080, 432, 1111, 0, "lion_cgc");
		g.zoneDoc(384, 769, 414, 797, 0, "jarres");
		g.zoneDoc(383, 1240, 413, 1268, 0, "jarres");
		g.zoneDoc(384, 1787, 409, 1810, 0, "jarres");
		g.zoneDoc(383, 239, 409, 262, 0, "jarres");
		g.zoneDoc(245, 860, 358, 1180, 0, "poc");
		g.warp("cgc140");
	}
	g.zoneHandler();
}

// cgc150 (0x425fe0)
static void place_cgc150(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(235, 370, 456, 572, 0, "cgc140", 0, -1.0, -1.0);
		g.zoneGo(238, 1426, 448, 1681, 0, "cgc160", 0, -1.0, -1.0);
		g.zoneGo(237, 597, 450, 707, 0, "cgc310", 0, -1.0, -1.0);
		g.zoneDoc(278, 690, 365, 810, 0, "poc");
		g.zoneLabel(250, 40, 330, 243, 0, "shp");
		g.zoneDoc(385, 743, 413, 768, 0, "jarres");
		g.zoneDoc(387, 1109, 436, 1155, 0, "jarres");
		g.zoneDoc(385, 1295, 416, 1323, 0, "jarres");
		g.zoneDoc(385, 1373, 409, 1393, 0, "jarres");
		g.zoneDoc(385, 185, 411, 208, 0, "jarres");
		g.warp("cgc150");
	}
	g.zoneHandler();
}

// cgc160 (0x425ec0)
static void place_cgc160(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(324, 980, 417, 1032, 0, "aie100", 0, 1.57, 0.0);
		g.zoneGo(248, 433, 438, 623, 0, "cgc150", 0, -1.0, -1.0);
		g.zoneLabel(275, 177, 338, 310, 0, "shp");
		g.zoneDoc(386, 788, 425, 824, 0, "jarres");
		g.zoneDoc(386, 687, 413, 710, 0, "jarres");
		g.zoneDoc(383, 629, 406, 645, 0, "jarres");
		g.zoneLabel(295, 1535, 398, 1710, 0, "pfs");
		g.zoneDoc(372, 951, 422, 964, 0, "gardes");
		g.zoneDoc(371, 1050, 424, 1063, 0, "gardes");
		g.warp("cgc160");
	}
	g.zoneHandler();
}

// cgc210 (0x425db0)
static void place_cgc210(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(384, 2016, 441, 2047, 0, "cth380", 0, 4.64, 0.0);
		g.zoneGo(384, 0, 441, 28, 0, "cth380", 0, 4.64, 0.0);
		g.zoneGo(325, 1525, 423, 1574, 0, "cgc230", 0, -1.0, -1.0);
		g.zoneLabel(365, 1127, 418, 1223, 0, "poc");
		g.zoneLabel(170, 1585, 423, 1802, 0, "shp");
		g.zoneLabel(367, 608, 435, 721, 0, "pag");
		g.zoneLabel(397, 749, 433, 867, 0, "gdc");
		g.zoneLabel(374, 1435, 405, 1488, 0, "pfs");
		g.warp("cgc210");
	}
	g.zoneHandler();
}

// cgc220 (0x425ca0)
static void place_cgc220(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(385, 2013, 442, 2047, 0, "cth350", 0, 4.65, 0.0);
		g.zoneGo(385, 0, 442, 27, 0, "cth350", 0, 4.65, 0.0);
		g.zoneGo(324, 474, 423, 524, 0, "cgc230", 0, -1.0, -1.0);
		g.zoneLabel(363, 822, 416, 915, 0, "poc");
		g.zoneDoc(172, 235, 431, 462, 0, "shp");
		g.zoneLabel(366, 1343, 430, 1437, 0, "pfs");
		g.zoneLabel(373, 555, 407, 621, 0, "pag");
		g.zoneLabel(391, 636, 414, 683, 0, "gdc");
		g.warp("cgc220");
	}
	g.zoneHandler();
}

// cgc230 (0x425b90)
static void place_cgc230(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(313, 467, 415, 518, 0, "cgc210", 0, -1.0, -1.0);
		g.zoneGo(310, 1529, 419, 1578, 0, "cgc220", 0, -1.0, -1.0);
		g.zoneGo(352, 953, 504, 1088, 0, "cgc140", 0, -1.0, -1.0);
		g.zoneDoc(100, 1640, 560, 2047, 0, "shp");
		g.zoneDoc(100, 0, 560, 380, 0, "shp");
		g.zoneLabel(371, 576, 414, 650, 0, "pag");
		g.zoneLabel(393, 670, 418, 743, 0, "gdc");
		g.zoneLabel(372, 1390, 412, 1474, 0, "pfs");
		g.warp("cgc230");
	}
	g.zoneHandler();
}

// cgc310 (0x425a00)
static void place_cgc310(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(200, 1902, 485, 2047, 0, "cgc140", 0, -1.0, -1.0);
		g.zoneGo(200, 0, 485, 133, 0, "cgc140", 0, -1.0, -1.0);
		g.zoneLabel(326, 782, 412, 836, 0, "lion_cgc");
		g.zoneLabel(323, 1186, 410, 1238, 0, "lion_cgc");
		g.zoneLook(302, 995, 376, 1039, 1, "porte", 0);
		g.zoneDoc(301, 911, 376, 1124, 0, "poc");
		g.zoneDoc(387, 649, 423, 682, 0, "jarres");
		g.zoneDoc(385, 1361, 424, 1395, 0, "jarres");
		g.zoneDoc(383, 180, 405, 197, 0, "jarres");
		g.zoneDoc(382, 1847, 407, 1863, 0, "jarres");
		g.zoneGo(265, 356, 458, 552, 0, "cgc130", 0, -1.0, -1.0);
		g.zoneGo(255, 1497, 444, 1688, 0, "cgc150", 0, -1.0, -1.0);
		g.warp("cgc310");
		if ((int32)g.var(kV_CHAPITRE) == 8) {
			g.zoneEnable(4);
		}
	}
	g.zoneHandler();
}

// porte (0x425980)
static void place_porte(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(404, 0, 479, 639, 0, "cgc310", 0, -1.0, -1.0);
		g.zoneLook(273, 190, 360, 265, 0, "porte1", 0);
		g.image("porte");
	}
	g.zoneHandler();
}

// porte1 (0x4258e0)
static void place_porte1(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(460, 0, 479, 639, 0, "porte", 0, -1.0, -1.0);
		g.zoneUse(412, 153, 457, 197, 0);
		g.image("porte1");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 1) && ((g.heldObject() == kO_BURIN ? 1 : 0) != 0)) {
		g.gotoPlace("porte2");
	}
}

// porte2 (0x425830)
static void place_porte2(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(459, 0, 479, 639, 0, "cgc310", 0, -1.0, -1.0);
		g.zoneUse(185, 208, 242, 269, 0);
		g.image("porte2");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 1) && ((g.heldObject() == kO_MARTEAU ? 1 : 0) != 0)) {
		g.soundQueue("burin");
		g.gotoPlace("bouton1");
	}
}

// bouton1 (0x425790)
static void place_bouton1(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(166, 196, 331, 445, 0, nullptr);
		g.image("bouton1");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		if (g.puzzle(7, 0) == 1) {
			g.objectDestroy(kO_INDIC2);
			g.objectDestroy(kO_CONFES2);
			g.gotoPlace("bouton20");
			return;
		}
		g.gotoPlace("porte");
	}
}

// bouton20 (0x425710)
static void place_bouton20(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(183, 249, 355, 314, 0, nullptr);
		g.image("bouton20");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.setVar(kV_Venant_de_BOUTON, 1);
		g.objectToInventory(kO_CONFES3);
		g.gotoPlace("confess3");
	}
}

// bouton21 (0x425690)
static void place_bouton21(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(183, 325, 340, 384, 0, nullptr);
		g.image("bouton21");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.objectToInventory(kO_INDIC3);
		g.setVar(kV_CHAPITRE, 9);
		g.gotoPlace("indice3");
	}
}

// confess3 (0x4255e0)
static void place_confess3(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(440, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(15, 468, 426, 611, 0, nullptr);
		g.image("confess3");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) && ((int32)g.var(kV_Venant_de_BOUTON) == 1)) {
		g.soundStop();
		g.gotoPlace("bouton21");
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("confess3");
	}
}

// confess2 (0x4254c0)
static void place_confess2(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(439, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(13, 326, 400, 621, 0, nullptr);
		g.image("confess2");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		if ((int32)g.var(kV_Venant_de_GO) == 1) {
			g.soundStop();
			g.gotoPlace("go21");
			return;
		}
		if ((int32)g.var(kV_Venant_de_GO) == 2) {
			g.soundStop();
			g.setVar(kV_Venant_de_GO, ((int32)g.var(kV_Venant_de_GO) + 1));
			g.minutesAdd("MINES209");
			g.screenEffect();
			g.soundPlayWait("anjbpi");
			g.setAngles(3.4, 0.0);
			g.gotoPlace("bpiw101");
			return;
		}
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("confess2");
	}
}

// indice2 (0x4253c0)
static void place_indice2(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(443, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(5, 40, 439, 261, 0, nullptr);
		g.image("indice2");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		if ((int32)g.var(kV_Venant_de_GO) == 1) {
			g.gotoPlace("go22");
			return;
		}
		if ((int32)g.var(kV_Venant_de_GO) == 2) {
			g.setVar(kV_Venant_de_GO, ((int32)g.var(kV_Venant_de_GO) + 1));
			g.minutesAdd("MINES209");
			g.soundStop();
			g.soundPlayWait("anjbpi");
			g.gotoPlace("espw101");
			return;
		}
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("indice2");
	}
}

// indice3 (0x4252b0)
static void place_indice3(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(448, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(23, 367, 206, 618, 1, nullptr);
		g.image("indice3");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) && ((int32)g.var(kV_Venant_de_BOUTON) == 1)) {
		g.setVar(kV_Venant_de_BOUTON, 2);
		g.minutesAdd("MINGC301");
		g.soundStop();
		g.video("poch301");
		g.dialogue("GDCD3011", "O301GDC", "O301ANJ");
		g.video("poch302");
		g.minutesAdd("MINGC302");
		g.gotoPlace("cgc310");
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("indice3");
	}
}

// go1 (0x4251d0)
static void place_go1(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(429, 0, 479, 639, 0, "espw101", 0, -1.0, -1.0);
		g.zoneTake(97, 116, 412, 483, 0, nullptr);
		g.image("go1");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 1) {
		if (g.puzzle(4, 0) == 1) {
			g.objectDestroy(kO_CONFES1);
			g.objectDestroy(kO_INDIC1);
			g.setVar(kV_CHAPITRE, 5);
			g.gotoPlace("go20");
			return;
		}
		g.setVar(kV_GO, ((int32)g.var(kV_GO) + 1));
	}
}

// go20 (0x425110)
static void place_go20(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(209, 298, 233, 333, 0, nullptr);
		g.image("go20");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		if ((int32)g.var(kV_Venant_de_GO) == 0) {
			g.setVar(kV_Venant_de_GO, ((int32)g.var(kV_Venant_de_GO) + 1));
			g.objectToInventory(kO_CONFES2);
			g.gotoPlace("confess2");
			return;
		}
		g.setVar(kV_Venant_de_GO, ((int32)g.var(kV_Venant_de_GO) + 1));
		g.objectToInventory(kO_INDIC2);
		g.gotoPlace("indice2");
	}
}

// go21 (0x425080)
static void place_go21(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(214, 261, 240, 289, 0, nullptr);
		g.image("go21");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.setVar(kV_Venant_de_GO, ((int32)g.var(kV_Venant_de_GO) + 1));
		g.objectToInventory(kO_INDIC2);
		g.gotoPlace("indice2");
	}
}

// go22 (0x425020)
static void place_go22(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(435, 5, 475, 635, 0, "bpiw101", 0, 0.0, 0.0);
		g.image("go22");
	}
	g.zoneHandler();
}

// fight (0x424e60)
static void place_fight(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.image("fight");
		g.zoneUse(187, 238, 475, 395, 0);
		g.setMem(0x530bf8, (int32)g.timeMs());
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	g.setMem(0, (int32)g.timeMs()); // esi
	if (((g.mem(0x48f2a0) == 0) && (g.mem(0x48f298) != 0)) || (g.unknownCall(0x414c70, 57) != 0)) {
		g.interfaceScreen();
	}
	if (g.clickedZone() == 0) {
		if ((g.heldObject() == kO_MARTEAU ? 1 : 0) != 0) {
			g.video("DAMH503");
			g.dialogue("ANDA5111", "A102ANJ", "A102DAM");
			g.minutesAdd("MINNW501");
			g.setVar(kV_DAMH501, 1);
			g.setAngles(1.58, 0.0);
			g.setVar(kV_FIGHTED, 3);
			g.video("DAMH502a");
			g.dialogue("ANJDAM61", "A102ANJ", nullptr);
			g.video("DAMH502g");
			g.gotoPlace("bdaw100");
			return;
		}
		if ((g.clickedZone() == 0) && ((g.heldObject() == kO_RUYI ? 1 : 0) != 0)) {
			g.video("DAMH504");
			g.dialogue("ANDA5111", "A102ANJ", "A102DAM");
			g.minutesAdd("MINNW501");
			g.setVar(kV_DAMH501, 1);
			g.setAngles(1.58, 0.0);
			g.setVar(kV_FIGHTED, 3);
			g.video("DAMH502a");
			g.dialogue("ANJDAM61", "A102ANJ", nullptr);
			g.video("DAMH502g");
			g.gotoPlace("bdaw100");
			return;
		}
	}
	if ((g.mem(0) - g.mem(0x530bf8)) > 3000) {
		g.video("DAMH505");
		g.setVar(kV_FIGHTED, 0);
		g.setAngles(1.6, 0.0);
		g.gotoPlace("nwfw100");
	}
}

// fight2 (0x424de0)
static void place_fight2(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.image("fight2");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((int32)g.var(kV_FIGHTED) == 1) {
		g.gotoPlace("fight");
		return;
	}
	if ((int32)g.var(kV_FIGHTED) == 0) {
		g.setVar(kV_FIGHTED, 1);
	}
}

// nwfw100 (0x424c20)
static void place_nwfw100(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(310, 1947, 621, 2047, 0, "cgc110", 0, -1.0, -1.0);
		g.zoneGo(310, 0, 621, 101, 0, "cgc110", 0, -1.0, -1.0);
		g.zoneGo(318, 434, 660, 582, 1, "banw100", 0, 4.67, 0.0);
		g.zoneGo(311, 1455, 670, 1619, 1, nullptr, 0, 0.0, 0.0);
		g.zoneLabel(411, 871, 561, 929, 0, "chandelier_celadon");
		g.zoneLabel(363, 974, 554, 1077, 0, "vase_celadon");
		g.zoneLabel(412, 1111, 555, 1164, 0, "chandelier_celadon");
		g.warp("nwfw100");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_MODE_VISITE) != 0) || ((int32)g.var(kV_CHAPITRE) >= 17)) {
		g.zoneEnable(3);
	}
	if ((int32)g.var(kV_MODE_VISITE) != 0) {
		g.zoneEnable(2);
	}
	if (g.clickedZone() == 3) {
		if (((int32)g.var(kV_CHAPITRE) != 17) || ((int32)g.var(kV_FIGHTED) != 0)) {
			g.setAngles(1.58, 0.0);
			g.gotoPlace("bdaw100");
			return;
		}
		if ((int32)g.var(kV_DAMH501) == 0) {
			g.video("DAMH501");
			g.setMem(0x48f2a4, 1);
			g.gotoPlace("fight2");
			return;
		}
	}
}

// nwfw101 (0x424af0)
static void place_nwfw101(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(310, 1947, 621, 2047, 0, "cgc110", 0, -1.0, -1.0);
		g.zoneGo(310, 0, 621, 101, 0, "cgc110", 0, -1.0, -1.0);
		g.zoneGo(318, 434, 660, 582, 0, nullptr, 0, 0.0, 0.0);
		g.zoneGo(311, 1455, 670, 1619, 0, "bdaw101", 0, -1.0, -1.0);
		g.warp("nwfw101");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 2) {
		if ((int32)g.var(kV_Ruyi) == 0) {
			g.setAngles(4.67, 0.0);
			g.gotoPlace("banw111");
			return;
		}
		if ((int32)g.var(kV_Ruyi) == 1) {
			g.setAngles(4.67, 0.0);
			g.gotoPlace("banw112");
		}
	}
}

// nwfw102 (0x424a40)
static void place_nwfw102(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(310, 1947, 621, 2047, 0, "cgc110", 0, -1.0, -1.0);
		g.zoneGo(310, 0, 621, 101, 0, "cgc110", 0, -1.0, -1.0);
		g.zoneGo(318, 434, 660, 582, 0, "banw121", 0, -1.0, -1.0);
		g.warp("nwfw102");
	}
	g.zoneHandler();
}

// banw100 (0x424920)
static void place_banw100(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(192, 1435, 700, 1640, 0, "nwfw100", 0, -1.0, -1.0);
		g.zoneLabel(340, 851, 403, 1025, 0, "bandeau_ban");
		g.zoneLabel(308, 307, 555, 390, 0, "bibliotheque");
		g.zoneLabel(556, 307, 580, 351, 0, "bibliotheque");
		g.zoneLabel(646, 468, 660, 502, 0, "pose_pinceau");
		g.zoneLabel(608, 547, 660, 611, 0, "chandelier_verre");
		g.zoneLabel(454, 830, 527, 872, 0, "vase_bronze");
		g.zoneLabel(468, 999, 550, 1053, 0, "vase_bronze");
		g.zoneLabel(417, 897, 559, 965, 0, "ecran_jade");
		g.zoneLabel(679, 1797, 739, 1888, 0, "crachoir");
		g.warp("banw100");
	}
	g.zoneHandler();
}

// banw111 (0x4247e0)
static void place_banw111(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(192, 1435, 700, 1640, 0, "nwfw101", 0, -1.0, -1.0);
		g.zoneLabel(340, 860, 403, 1033, 0, "bandeau_ban");
		g.zoneLook(308, 304, 555, 387, 1, "registre", 0);
		g.zoneLook(556, 304, 584, 344, 1, "registre", 0);
		g.zoneTake(531, 981, 568, 1020, 1, nullptr);
		g.warp("banw111");
		if ((int32)g.var(kV_CHAPITRE) == 5) {
			g.zoneEnable(4);
			if (((int32)g.var(kV_ANXI2111) != 0) && ((int32)g.var(kV_ANMW2111) != 0)) {
				g.zoneEnable(2);
				g.zoneEnable(3);
			}
		}
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 4) {
		g.objectToInventory(kO_RUYI);
		g.setVar(kV_Ruyi, 1);
		g.gotoPlace("banw112");
	}
}

// banw112 (0x4246f0)
static void place_banw112(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(192, 1435, 700, 1640, 0, "nwfw101", 0, -1.0, -1.0);
		g.zoneLabel(340, 860, 403, 1033, 0, "bandeau_ban");
		g.zoneLook(308, 304, 555, 387, 1, "registre", 0);
		g.zoneLook(556, 304, 584, 344, 1, "registre", 0);
		if (((int32)g.var(kV_CHAPITRE) == 5) && ((int32)g.var(kV_ANXI2111) != 0) && ((int32)g.var(kV_ANMW2111) != 0)) {
			g.zoneEnable(2);
			g.zoneEnable(3);
		}
		g.warp("banw112");
	}
	g.zoneHandler();
}

// registre (0x4245a0)
static void place_registre(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(449, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(11, 376, 440, 535, 1, nullptr);
		if ((int32)g.var(kV_Liste_Boites) == 0) {
			g.zoneEnable(1);
		}
		g.image("registre");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((int32)g.var(kV_Liste_Boites) == 0) {
		if (g.clickedZone() == 1) {
			g.minutesAdd("MINNW211");
			g.setVar(kV_PDC_ouvert, 1);
			g.gotoPlace("cahiers");
			return;
		}
		if (g.clickedZone() == 0) {
			if ((int32)g.var(kV_Ruyi) == 0) {
				g.gotoPlace("banw111");
				return;
			}
			if ((int32)g.var(kV_Ruyi) == 1) {
				g.gotoPlace("banw112");
				return;
			}
		}
	}
	if (((int32)g.var(kV_Liste_Boites) == 1) && (g.clickedZone() == 0)) {
		if ((int32)g.var(kV_Ruyi) == 0) {
			g.gotoPlace("banw111");
			return;
		}
		if ((int32)g.var(kV_Ruyi) == 1) {
			g.gotoPlace("banw112");
		}
	}
}

// cahiers (0x4244c0)
static void place_cahiers(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(439, 5, 475, 635, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(48, 112, 365, 426, 1, nullptr);
		g.image("cahiers");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((int32)g.var(kV_Liste_Boites) == 0) {
		g.zoneEnable(1);
	}
	if (g.clickedZone() == 0) {
		if ((int32)g.var(kV_Ruyi) == 0) {
			g.gotoPlace("banw111");
			return;
		}
		if ((int32)g.var(kV_Ruyi) == 1) {
			g.gotoPlace("banw112");
			return;
		}
	}
	if (g.clickedZone() == 1) {
		g.gotoPlace("lboites");
	}
}

// lboites (0x4243c0)
static void place_lboites(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(430, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(19, 286, 346, 630, 0, nullptr);
		g.image("lboites");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("lboites");
	}
	if ((g.clickedZone() == 0) && ((int32)g.var(kV_Liste_Boites) == 0)) {
		g.objectToInventory(kO_LISTE_BOITES);
		g.setVar(kV_Liste_Boites, 1);
		if ((int32)g.var(kV_Ruyi) == 0) {
			g.soundStop();
			g.gotoPlace("banw111");
			return;
		}
		if ((int32)g.var(kV_Ruyi) == 1) {
			g.soundStop();
			g.gotoPlace("banw112");
		}
	}
}

// banw121 (0x424320)
static void place_banw121(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(192, 1435, 700, 1640, 0, "nwfw102", 0, -1.0, -1.0);
		g.zoneLabel(343, 861, 403, 1035, 0, "bandeau_ban");
		g.zoneLabel(308, 300, 555, 383, 0, "registre");
		g.zoneLabel(556, 300, 584, 340, 0, "registre");
		g.warp("banw121");
	}
	g.zoneHandler();
}

// banw122 (0x424280)
static void place_banw122(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(192, 1435, 700, 1640, 0, "nwfw102", 0, -1.0, -1.0);
		g.zoneLabel(343, 861, 403, 1035, 0, "bandeau_ban");
		g.zoneLabel(308, 300, 555, 383, 0, "registre");
		g.zoneLabel(556, 300, 584, 340, 0, "registre");
		g.warp("banw122");
	}
	g.zoneHandler();
}

// bdaw100 (0x4240d0)
static void place_bdaw100(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(171, 400, 710, 620, 0, "nwfw100", 0, -1.0, -1.0);
		g.zoneLabel(391, 1426, 452, 1599, 0, "bandeau_bda");
		g.zoneTake(470, 1115, 547, 1166, 1, nullptr);
		g.zoneLook(660, 1643, 688, 1713, 0, "pinceau", 0);
		g.zoneLabel(550, 1733, 664, 1783, 0, "chandelier_bois");
		g.zoneLabel(663, 1573, 691, 1634, 0, "pierre_eau_corail");
		g.zoneLabel(664, 1483, 701, 1567, 0, "pierre_encre_lotus");
		g.zoneLabel(637, 1395, 696, 1447, 0, "pot_pinceau_ivoire");
		g.zoneLabel(632, 1451, 646, 1486, 0, "presse_papier");
		g.zoneLabel(497, 1031, 553, 1081, 0, "vase_dragon");
		g.zoneLabel(483, 1190, 531, 1230, 0, "vase_dragon");
		g.zoneLabel(691, 128, 716, 232, 0, "crachoir_email");
		g.warp("bdaw100");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_CHAPITRE) == 17) && ((int32)g.var(kV_Venant_de_HORLOGE) == 0)) {
		g.zoneEnable(2);
		if ((g.clickedZone() == 2) && (g.puzzle(6, 0) == 1)) {
			g.setVar(kV_Venant_de_HORLOGE, 1);
			g.setVar(kV_CHAPITRE, 18);
			g.gotoPlace("horloge2");
		}
	}
}

// edit (0x424000)
static void place_edit(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(425, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(10, 447, 473, 614, 0, nullptr);
		g.image("edit");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) && ((int32)g.var(kV_Venant_de_HORLOGE) == 1)) {
		g.soundStop();
		g.setVar(kV_Venant_de_HORLOGE, 2);
		g.minutesAdd("MINNW510");
		g.gotoPlace("bdaw100");
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("edit");
	}
}

// plbombe (0x423f80)
static void place_plbombe(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(440, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.image("plbombe");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) && ((int32)g.var(kV_Venant_de_HORLOGE) == 1)) {
		g.gotoPlace("horloge3");
	}
}

// horloge1 (0x423ef0)
static void place_horloge1(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(404, 0, 479, 639, 0, "bdaw100", 0, -1.0, -1.0);
		g.zoneLook(134, 246, 367, 439, 1, "horloge2", 0);
		g.image("horloge1");
		if ((int32)g.var(kV_Venant_de_HORLOGE) == 0) {
			g.zoneEnable(1);
		}
	}
	g.zoneHandler();
}

// horloge2 (0x423e70)
static void place_horloge2(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(124, 295, 284, 344, 0, nullptr);
		g.image("horloge2");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.setVar(kV_Venant_de_HORLOGE, 1);
		g.objectToInventory(kO_PLBOMB);
		g.gotoPlace("plbombe");
	}
}

// horloge3 (0x423e00)
static void place_horloge3(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(117, 349, 271, 407, 0, nullptr);
		g.image("horloge3");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.objectToInventory(kO_EDI);
		g.gotoPlace("edit");
	}
}

// pinceau (0x423d60)
static void place_pinceau(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(404, 0, 479, 639, 0, "bdaw100", 0, -1.0, -1.0);
		g.zoneLabel(221, 224, 324, 285, 0, "DATE_DAMING1");
		g.zoneLabel(220, 302, 325, 359, 0, "DATE_DAMING2");
		g.zoneLabel(214, 375, 327, 429, 0, "DATE_DAMING3");
		g.image("pinceau");
	}
	g.zoneHandler();
}

// bdaw101 (0x423950)
static void place_bdaw101(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(171, 400, 710, 620, 0, "nwfw101", 0, -1.0, -1.0);
		g.zoneLabel(470, 1090, 552, 1145, 0, "horloge");
		g.zoneTalk(397, 1525, 468, 1562, 1);
		g.zoneDoc(469, 1475, 625, 1600, 0, "surintendants");
		g.zoneDoc(550, 1631, 630, 1680, 0, "surintendants");
		g.zoneDoc(500, 1601, 590, 1630, 0, "surintendants");
		g.zoneLabel(390, 1420, 458, 1605, 0, "bandeau_bda");
		if (((int32)g.var(kV_CHAPITRE) == 5) && ((int32)g.var(kV_DAMD2131) == 0) && ((int32)g.var(kV_ANXN2121) == 1) && ((int32)g.var(kV_Liste_Boites) == 0)) {
			g.zoneEnable(2);
		}
		g.warp("bdaw101");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 2) && ((int32)g.var(kV_CHAPITRE) == 5) && ((int32)g.var(kV_ANXN2121) == 1) && ((int32)g.var(kV_DAMD2131) == 0)) {
		g.dialogue("DAMD2131", "A101DAM", "A101ANJ");
		g.setVar(kV_DAMD2131, 1);
		g.zoneDisable(2);
	}
	if ((g.clickedZone() >= 3) && (g.clickedZone() <= 5)) {
		if (((g.heldObject() == kO_POSTHUME ? 1 : 0) != 0) && ((int32)g.var(kV_DAMD1011) == 0)) {
			g.dialogue("DAMD1011", "A101DAM", "A101ANJ");
			g.setVar(kV_DAMD1011, 1);
		}
		if (((g.heldObject() == kO_CONFES1 ? 1 : 0) != 0) && ((int32)g.var(kV_DAMD2011) == 0)) {
			g.dialogue("DAMD2011", "A101DAM", "A101ANJ");
			g.setVar(kV_DAMD2011, 1);
		}
		if (((g.heldObject() == kO_INDIC1 ? 1 : 0) != 0) && ((int32)g.var(kV_DAMD2021) == 0)) {
			g.dialogue("DAMD2021", "A101DAM", "A101ANJ");
			g.setVar(kV_DAMD2021, 1);
		}
		if (((g.heldObject() == kO_CONFES2 ? 1 : 0) != 0) && ((int32)g.var(kV_DAMD2111) == 0)) {
			g.dialogue("DAMD2111", "A101DAM", "A101ANJ");
			g.setVar(kV_DAMD2111, 1);
		}
		if (((g.heldObject() == kO_INDIC2 ? 1 : 0) != 0) && ((int32)g.var(kV_DAMD2121) == 0)) {
			g.dialogue("DAMD2121", "A101DAM", "A101ANJ");
			g.setVar(kV_DAMD2121, 1);
		}
		if (((g.heldObject() == kO_LISTE_BOITES ? 1 : 0) != 0) && ((int32)g.var(kV_ANDA2141) == 0)) {
			g.dialogue("ANDA2141", "A101ANJ", "A101DAM");
			g.setVar(kV_ANDA2141, 1);
		}
		if (((g.heldObject() == kO_ORIGINAUX ? 1 : 0) != 0) && ((int32)g.var(kV_DAMD2121) == 0)) {
			g.dialogue("DAMD2121", "A101DAM", "A101ANJ");
			g.setVar(kV_DAMD2121, 1);
		}
		if (((g.heldObject() == kO_CONFES3 ? 1 : 0) != 0) && ((int32)g.var(kV_DAMD3111) == 0)) {
			g.dialogue("DAMD3111", "A101DAM", "A101ANJ");
			g.setVar(kV_DAMD3111, 1);
		}
		if (((g.heldObject() == kO_INDIC3 ? 1 : 0) != 0) && ((int32)g.var(kV_DAMD3121) == 0)) {
			g.dialogue("DAMD3121", "A101DAM", "A101ANJ");
			g.setVar(kV_DAMD3121, 1);
		}
		if (((g.heldObject() == kO_INDICE_CACHETS ? 1 : 0) != 0) && ((int32)g.var(kV_DAMD3211) == 0)) {
			g.dialogue("DAMD3211", "A101DAM", "A101ANJ");
			g.setVar(kV_DAMD3211, 1);
		}
	}
}

// spfw100 (0x4237d0)
static void place_spfw100(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(238, 1269, 610, 1800, 0, "ctp330", 0, 4.7, 0.0);
		g.zoneLabel(268, 471, 314, 552, 0, "bandeau_spf");
		g.zoneLabel(315, 368, 430, 385, 1, "bandeau_gauche_spf");
		g.zoneLabel(315, 638, 420, 654, 1, "bandeau_droite_spf");
		g.zoneLabel(469, 335, 565, 347, 0, "colonette");
		g.zoneLabel(471, 672, 564, 683, 0, "colonette");
		g.zoneLabel(324, 420, 417, 591, 1, "panneau_spf");
		g.zoneLabel(426, 482, 508, 535, 0, "trone_spf");
		g.zoneLabel(28, 177, 211, 649, 0, "plafond");
		g.zoneLabel(457, 392, 498, 411, 0, "brule");
		g.zoneLabel(460, 598, 493, 621, 0, "brule");
		g.zoneLabel(521, 200, 585, 258, 0, "brule");
		g.zoneLabel(525, 762, 583, 821, 0, "brule");
		g.zoneLabel(409, 667, 462, 790, 0, "coffret");
		g.zoneLabel(418, 237, 461, 360, 0, "coffret");
		g.warp("spfw100");
	}
	g.zoneHandler();
}

// spfw101 (0x4234d0)
static void place_spfw101(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(238, 1269, 610, 1800, 0, "ctp330", 0, 4.7, 0.0);
		g.zoneLabel(268, 471, 314, 552, 0, "bandeau_spf");
		g.zoneDoc(358, 349, 417, 376, 0, "eunuques");
		g.zoneDoc(418, 312, 560, 410, 0, "eunuques");
		g.zoneDoc(561, 324, 649, 400, 0, "eunuques");
		g.zoneTalk(353, 664, 408, 692, 1);
		g.zoneDoc(455, 615, 544, 740, 0, "chefs_eunuques");
		g.zoneDoc(545, 641, 646, 716, 0, "chefs_eunuques");
		g.zoneDoc(409, 631, 454, 725, 0, "chefs_eunuques");
		g.zoneLabel(315, 368, 430, 385, 0, "bandeau_gauche_spf");
		g.zoneLabel(315, 638, 420, 654, 0, "bandeau_droite_spf");
		g.zoneLabel(324, 420, 417, 591, 0, "panneau_spf");
		g.zoneLabel(426, 482, 508, 535, 0, "trone_spf");
		g.zoneLabel(66, 495, 140, 525, 0, "plafond");
		g.zoneLabel(460, 598, 493, 621, 0, "vase_spf");
		g.zoneLabel(521, 200, 585, 258, 0, "animal_spf");
		g.zoneLabel(525, 762, 583, 821, 0, "animal_spf");
		g.warp("spfw101");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((int32)g.var(kV_XPFD3211) == 0) {
		g.zoneEnable(5);
	}
	if ((g.clickedZone() == 5) && ((int32)g.var(kV_XPFD3211) == 0)) {
		g.dialogue("XPFD3211", "K101XPF", "K101ANJ");
		g.setVar(kV_XPFD3211, 1);
		g.zoneDisable(5);
	}
	if (((g.clickedZone() == 3) || (g.clickedZone() == 4)) && ((g.heldObject() == kO_INDICE_CACHETS ? 1 : 0) != 0) && ((int32)g.var(kV_EPFD3211) == 0) && ((int32)g.var(kV_XPFD3211) != 0)) {
		g.video("SPFH320");
		if (g.puzzle(3, 0) != 0) {
			g.setVar(kV_EPFD3211, 1);
			g.objectDestroy(kO_INDICE_CACHETS);
			g.objectToInventory(kO_INDICE_CACHETS2);
		}
		g.dialogue("ANEF3221", "K102ANJ", "K102EPF");
		g.screenEffect();
	}
	if ((g.clickedZone() >= 6) && (g.clickedZone() <= 8) && ((g.heldObject() == kO_CIRE ? 1 : 0) != 0) && ((int32)g.var(kV_ANXF3231) == 0)) {
		g.dialogue("ANXF3231", "K101ANJ", "K101XPF");
		g.setVar(kV_ANXF3231, 1);
		g.minutesAdd("MINSP321");
	}
}

// spfw102 (0x423300)
static void place_spfw102(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(238, 1269, 610, 1800, 0, "ctp330", 0, 4.7, 0.0);
		g.zoneLabel(268, 471, 314, 552, 0, "bandeau_spf");
		g.zoneTalk(358, 349, 417, 376, 0);
		g.zoneDoc(418, 312, 560, 410, 0, "eunuques");
		g.zoneDoc(561, 324, 649, 400, 0, "eunuques");
		g.zoneLabel(315, 368, 430, 385, 0, "bandeau_gauche_spf");
		g.zoneLabel(315, 638, 420, 654, 0, "bandeau_droite_spf");
		g.zoneLabel(471, 672, 564, 683, 0, "colonette");
		g.zoneLabel(324, 420, 417, 591, 0, "panneau_spf");
		g.zoneLabel(426, 482, 508, 535, 0, "trone_spf");
		g.zoneLabel(66, 495, 140, 525, 0, "plafond");
		g.zoneLabel(460, 598, 493, 621, 0, "vase_spf");
		g.zoneLabel(521, 200, 585, 258, 0, "animal_spf");
		g.zoneLabel(525, 762, 583, 821, 0, "animal_spf");
		g.warp("spfw102");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((int32)g.var(kV_ANEF3231) == 1) {
		g.zoneDisable(2);
	}
	if ((g.clickedZone() == 2) && ((int32)g.var(kV_ANEF3231) == 0)) {
		g.dialogue("ANEF3231", "K102ANJ", "K102EPF");
		g.minutesAdd("MINSP326");
		g.setVar(kV_ANEF3231, 1);
		g.zoneDisable(2);
	}
}

// jixw111 (0x423210)
static void place_jixw111(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(290, 1982, 460, 2047, 0, nullptr, 0, 0.0, 0.0);
		g.zoneGo(290, 0, 460, 62, 0, nullptr, 0, 0.0, 0.0);
		g.zoneGo(231, 860, 505, 1121, 0, nullptr, 0, 0.0, 0.0);
		g.warp("jixw111");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) || (g.clickedZone() == 1)) {
		g.video("jixh111");
		g.gotoPlace("jixw121");
		return;
	}
	if (g.clickedZone() == 2) {
		g.video("jixh007");
		g.setAngles(4.72, 0.0);
		g.gotoPlace("aie600b");
		return;
	}
}

// jixw121 (0x422f90)
static void place_jixw121(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(285, 958, 466, 1087, 0, "jixw111", 0, -1.0, -1.0);
		g.zoneTalk(388, 2037, 412, 2047, 1);
		g.zoneTalk(388, 0, 412, 12, 1);
		g.zoneLabel(371, 27, 480, 49, 0, "DID");
		g.zoneLabel(374, 1972, 516, 1994, 0, "EID");
		g.zoneLabel(382, 2012, 481, 2031, 0, "EID");
		g.zoneLabel(374, 1933, 544, 1966, 0, "DID");
		g.zoneLabel(377, 66, 512, 93, 0, "EID");
		g.zoneDoc(413, 2030, 460, 2047, 0, "IMD");
		g.zoneDoc(461, 2022, 518, 2047, 0, "IMD");
		g.zoneDoc(413, 0, 460, 15, 0, "IMD");
		g.zoneDoc(461, 0, 518, 24, 0, "IMD");
		g.warp("jixw121");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_VAR_ANID1121) == 0) || ((int32)g.var(kV_VAR_ANID1131) == 0)) {
		g.zoneEnable(1);
		g.zoneEnable(2);
	}
	if ((g.clickedZone() == 1) || (g.clickedZone() == 2)) {
		if ((int32)g.var(kV_VAR_ANID1121) == 0) {
			g.dialogue("ANID1011", "M111ANJ", "M111IMD");
			g.dialogue("ANID1121", "M111ANJ", "M111IMD");
			g.setVar(kV_VAR_ANID1121, 1);
			g.setVar(kV_CHAPITRE, 2);
			g.objectToInventory(kO_CLE_WANG);
			g.objectToInventory(kO_POSTHUME);
			g.minutesAdd("MINJI109");
			g.setMem(0x48f27c, -1);
		}
	}
	if (((g.clickedZone() == 1) || (g.clickedZone() == 2)) && ((int32)g.var(kV_VAR_ANID1121) == 1) && ((int32)g.var(kV_VAR_ANID1131) == 0)) {
		g.dialogue("ANID1131", "M111ANJ", "M111IMD");
		g.setVar(kV_VAR_ANID1131, 1);
		g.zoneDisable(1);
		g.zoneDisable(2);
	}
}

// jixw131 (0x422ee0)
static void place_jixw131(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(270, 948, 440, 1084, 0, "jixw121", 0, -1.0, -1.0);
		g.zoneGo(352, 2033, 443, 2047, 0, "jixw210", 0, 3.14, 0.0);
		g.zoneGo(352, 0, 443, 25, 0, "jixw210", 0, 3.14, 0.0);
		g.warp("jixw131");
	}
	g.zoneHandler();
}

// posthum (0x422e00)
static void place_posthum(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(438, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(10, 243, 310, 629, 0, nullptr);
		g.image("posthum");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) && ((int32)g.var(kV_VAR_Venant_de_JIX111) == 1)) {
		g.minutesAdd("MINJI109");
		g.setVar(kV_VAR_Venant_de_JIX111, 2);
		g.setVar(kV_CHAPITRE, 2);
		g.gotoPlace("jixw111");
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("POSTHUME");
	}
}

// confess4 (0x422d50)
static void place_confess4(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(441, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.zoneTake(14, 291, 409, 623, 0, nullptr);
		g.image("confess4");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) && ((int32)g.var(kV_VAR_Venant_de_PUZZLE4) == 1)) {
		g.soundStop();
		g.gotoPlace("puzzle43");
		return;
	}
	if (g.clickedZone() == 1) {
		g.soundQueue("confess4");
	}
}

// indice4 (0x422cc0)
static void place_indice4(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(440, 0, 479, 639, 0, nullptr, 0, 0.0, 0.0);
		g.image("indice4");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) && ((int32)g.var(kV_VAR_Venant_de_PUZZLE4) == 1)) {
		g.setVar(kV_VAR_Venant_de_PUZZLE4, 2);
		g.gotoPlace("jixw210");
	}
}

// jixw110 (0x422c10)
static void place_jixw110(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(236, 897, 509, 1150, 0, "aie600b", 0, 4.7, 0.0);
		g.zoneGo(278, 1987, 464, 2047, 0, "jixw120", 0, -1.0, -1.0);
		g.zoneGo(278, 0, 464, 62, 0, "jixw120", 0, -1.0, -1.0);
		g.warp("jixw110");
	}
	g.zoneHandler();
}

// jixw120 (0x422ac0)
static void place_jixw120(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(260, 939, 485, 1100, 0, "jixw110", 0, -1.0, -1.0);
		if (((int32)g.var(kV_LVICT) == 0) && ((int32)g.var(kV_MODE_VISITE) == 0)) {
			g.zoneGo(259, 1974, 477, 2047, 0, nullptr, 0, 0.0, 0.0);
			g.zoneGo(259, 0, 477, 63, 0, nullptr, 0, 0.0, 0.0);
		}
		g.warp("jixw120");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 1) || (g.clickedZone() == 2)) {
		g.video("JIXH401");
		g.dialogue("ANEB4091", "M401ANJ", "M401EU");
		g.video("JIXH402");
		g.dialogue("ANJIX41", "M137ANJ", nullptr);
		g.video("JIXH402b");
		g.dialogue("EN1JIX41", "M137ENB", "M137ANJ");
		g.minutesAdd("MINJI4090");
		g.gotoPlace("victime");
	}
}

// jixw130 (0x422a10)
static void place_jixw130(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(263, 937, 477, 1095, 0, "jixw120", 0, -1.0, -1.0);
		g.zoneGo(352, 2033, 443, 2047, 0, "jixw210", 0, 3.14, 0.0);
		g.zoneGo(352, 0, 443, 25, 0, "jixw210", 0, 3.14, 0.0);
		g.warp("jixw130");
	}
	g.zoneHandler();
}

// jixw112 (0x422930)
static void place_jixw112(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(236, 897, 509, 1150, 0, nullptr, 0, 0.0, 0.0);
		g.zoneGo(278, 1987, 464, 2047, 0, "jixw122", 0, -1.0, -1.0);
		g.zoneGo(278, 0, 464, 62, 0, "jixw122", 0, -1.0, -1.0);
		g.warp("jixw112");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.video("jixh007");
		g.setAngles(4.7, 0.0);
		g.gotoPlace("aie600b");
	}
}

// victime (0x422880)
static void place_victime(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.image("victime");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (((int32)g.var(kV_CHAPITRE) == 12) && ((int32)g.var(kV_LVICT) != 0)) {
		g.setVar(kV_CHAPITRE, 13);
		g.voice("victime");
		g.objectToInventory(kO_LISTE_VICTIMES);
		g.minutesAdd("MINJI409");
		g.gotoPlace("jixw210");
		return;
	}
	g.setVar(kV_LVICT, 1);
}

// jixw122 (0x4227d0)
static void place_jixw122(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(260, 939, 485, 1100, 0, "jixw112", 0, -1.0, -1.0);
		g.zoneGo(259, 1974, 477, 2047, 0, "jixw132", 0, -1.0, -1.0);
		g.zoneGo(259, 0, 477, 63, 0, "jixw132", 0, -1.0, -1.0);
		g.warp("jixw122");
	}
	g.zoneHandler();
}

// jixw132 (0x422530)
static void place_jixw132(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTalk(325, 49, 382, 91, 1);
		g.zoneGo(263, 937, 477, 1095, 0, "jixw122", 0, -1.0, -1.0);
		g.zoneGo(352, 2033, 443, 2047, 0, "jixw210", 0, 3.14, 0.0);
		g.zoneGo(352, 0, 443, 25, 0, "jixw210", 0, 3.14, 0.0);
		g.zoneTalk(290, 159, 396, 235, 1);
		g.warp("jixw132");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((((int32)g.var(kV_CHAPITRE) == 13) && ((int32)g.var(kV_ANXP4111) == 0)) || (((int32)g.var(kV_CHAPITRE) == 14) && ((int32)g.var(kV_ANXP4211) == 0))) {
		g.zoneEnable(0);
	}
	if (g.clickedZone() == 0) {
		if (((int32)g.var(kV_CHAPITRE) == 13) && ((int32)g.var(kV_ANXP4111) == 0)) {
			g.setVar(kV_ANXP4111, 1);
			g.dialogue("ANXP4111", "M132ANJ", "M132XPR");
			g.zoneDisable(0);
			g.minutesAdd("MINXQ411");
		}
		if (((int32)g.var(kV_CHAPITRE) == 14) && ((int32)g.var(kV_ANXP4211) == 0)) {
			g.dialogue("ANXP4211", "M132ANJ", "M132XPR");
			g.setVar(kV_ANXP4211, 1);
			g.zoneDisable(0);
		}
	}
	if (((int32)g.var(kV_CHAPITRE) == 13) && ((int32)g.var(kV_XPRD4101) == 0)) {
		g.dialogue("XPRD4101", "M132XPR", "M132ANJ");
		g.setVar(kV_XPRD4101, 1);
	}
	if (((int32)g.var(kV_CHAPITRE) == 13) && ((int32)g.var(kV_ANCP4101) == 0)) {
		g.zoneEnable(4);
	}
	if ((g.clickedZone() == 4) && ((int32)g.var(kV_CHAPITRE) == 13) && ((int32)g.var(kV_ANCP4101) == 0)) {
		g.dialogue("ANCP4101", "M132ANJ", "M131CPR");
		g.setVar(kV_ANCP4101, 1);
		g.zoneDisable(4);
	}
}

// jixw210 (0x422420)
static void place_jixw210(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneGo(316, 2010, 481, 2047, 1, nullptr, 0, 0.0, 0.0);
		g.zoneLook(327, 1493, 380, 1583, 1, "soupir", 0);
		g.warp("jixw210");
		if (((int32)g.var(kV_CHAPITRE) >= 13) && ((int32)g.var(kV_CHAPITRE) <= 15)) {
			g.zoneEnable(1);
		}
		if (((int32)g.var(kV_CHAPITRE) > 13) || ((int32)g.var(kV_VAR_Venant_de_PUZZLE4) != 0)) {
			g.zoneEnable(0);
		}
		if ((int32)g.var(kV_CHAPITRE) == 13) {
			g.setMem(0x48f2a8, ((int32)g.var(kV_VAR_Venant_de_PUZZLE4) == 0 ? 1 : 0));
		}
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.setAngles(6.2, 0.0);
		g.gotoPlace("jixw132");
	}
}

// soupir (0x422130)
static void place_soupir(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneUse(135, 174, 354, 195, 0);
		g.zoneUse(117, 190, 137, 380, 0);
		g.zoneUse(125, 380, 140, 524, 0);
		g.zoneUse(349, 191, 365, 363, 0);
		g.zoneUse(339, 359, 357, 522, 0);
		g.zoneUse(138, 522, 340, 540, 0);
		g.zoneGo(403, 5, 475, 635, 0, "jixw210", 0, 1.0, 1.0);
		g.image("soupir");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if ((g.clickedZone() == 0) && ((g.heldObject() == kO_TOURNEVIS ? 1 : 0) != 0) && (((int32)g.var(kV_crans) & 1) == 0)) {
		g.setVar(kV_crans, ((int32)g.var(kV_crans) | 1));
		if ((int32)g.var(kV_crans) == 15) {
			g.soundPlayWait("soupira");
			g.soundQueue("soupirb");
			g.zoneDisable(0);
		} else {
			g.soundQueue("soupira");
			g.zoneDisable(0);
		}
	}
	if (((g.clickedZone() == 1) || (g.clickedZone() == 2)) && ((g.heldObject() == kO_TOURNEVIS ? 1 : 0) != 0) && (((int32)g.var(kV_crans) & 2) == 0)) {
		g.setVar(kV_crans, ((int32)g.var(kV_crans) | 2));
		if ((int32)g.var(kV_crans) == 15) {
			g.soundPlayWait("soupira");
			g.soundQueue("soupirb");
			g.zoneDisable(1);
			g.zoneDisable(2);
		} else {
			g.soundQueue("soupira");
			g.zoneDisable(1);
			g.zoneDisable(2);
		}
	}
	if ((g.clickedZone() == 5) && ((g.heldObject() == kO_TOURNEVIS ? 1 : 0) != 0) && (((int32)g.var(kV_crans) & 4) == 0)) {
		g.setVar(kV_crans, ((int32)g.var(kV_crans) | 4));
		if ((int32)g.var(kV_crans) == 15) {
			g.soundPlayWait("soupira");
			g.soundQueue("soupirb");
			g.zoneDisable(5);
		} else {
			g.soundQueue("soupira");
			g.zoneDisable(5);
		}
	}
	if (((g.clickedZone() == 3) || (g.clickedZone() == 4)) && ((g.heldObject() == kO_TOURNEVIS ? 1 : 0) != 0) && (((int32)g.var(kV_crans) & 8) == 0)) {
		g.setVar(kV_crans, ((int32)g.var(kV_crans) | 8));
		if ((int32)g.var(kV_crans) == 15) {
			g.soundPlayWait("soupira");
			g.soundQueue("soupirb");
			g.zoneDisable(3);
			g.zoneDisable(4);
		} else {
			g.soundQueue("soupira");
			g.zoneDisable(3);
			g.zoneDisable(4);
		}
	}
	if ((int32)g.var(kV_crans) == 15) {
		g.gotoPlace("soupir2");
	}
}

// soupir2 (0x422030)
static void place_soupir2(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(300, 358, 346, 433, 0, nullptr);
		g.zoneGo(403, 5, 475, 635, 0, "jixw210", 0, 3.14, 0.0);
		g.image("soupir2");
		if ((int32)g.var(kV_VAR_Venant_de_PUZZLE4) != 0) {
			g.zoneDisable(0);
		}
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		if (g.puzzle(5, 0) != 0) {
			g.objectDestroy(kO_INDIC3);
			g.objectDestroy(kO_CONFES3);
			g.setVar(kV_VAR_Venant_de_PUZZLE4, 1);
			g.video("puzzl4");
			g.minutesAdd("MINJI411");
			g.gotoPlace("puzzle42");
			return;
		}
		g.setMem(0x48f2a8, 1);
	}
}

// puzzle41 (0x421ff0)
static void place_puzzle41(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.image("puzzle41");
	}
	g.zoneHandler();
}

// puzzle42 (0x421f70)
static void place_puzzle42(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(170, 271, 277, 322, 0, nullptr);
		g.image("puzzle42");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.objectToInventory(kO_CONFES4);
		g.setVar(kV_VAR_Venant_de_PUZZLE4, 1);
		g.gotoPlace("confess4");
	}
}

// puzzle43 (0x421f00)
static void place_puzzle43(CryOmni3DEngine_China &g, bool entry) {
	if (entry) {
		g.zonesReset();
		g.zoneTake(182, 325, 275, 373, 0, nullptr);
		g.image("puzzle43");
	}
	if ((g.zoneHandler() ? 1 : 0) != 0) {
		return;
	}
	if (g.clickedZone() == 0) {
		g.objectToInventory(kO_INDIC4);
		g.gotoPlace("indice4");
	}
}

const CryOmni3DEngine_China::PlaceDef CryOmni3DEngine_China::kPlaces[] = {
	{ "Script_Start", &place_Script_Start },
	{ "cth140", &place_cth140 },
	{ "cth210", &place_cth210 },
	{ "cth130", &place_cth130 },
	{ "cth230", &place_cth230 },
	{ "cth120", &place_cth120 },
	{ "cth110", &place_cth110 },
	{ "cth150", &place_cth150 },
	{ "cth170", &place_cth170 },
	{ "cth160", &place_cth160 },
	{ "cth290", &place_cth290 },
	{ "cth220", &place_cth220 },
	{ "cth280", &place_cth280 },
	{ "cth240", &place_cth240 },
	{ "cth250", &place_cth250 },
	{ "cth270", &place_cth270 },
	{ "cth260", &place_cth260 },
	{ "cth310", &place_cth310 },
	{ "cth330", &place_cth330 },
	{ "cth340", &place_cth340 },
	{ "cth320", &place_cth320 },
	{ "cth360", &place_cth360 },
	{ "cth350", &place_cth350 },
	{ "cth380", &place_cth380 },
	{ "cth370", &place_cth370 },
	{ "chs150", &place_chs150 },
	{ "chs110", &place_chs110 },
	{ "chs120", &place_chs120 },
	{ "chs130", &place_chs130 },
	{ "chs250", &place_chs250 },
	{ "chs240", &place_chs240 },
	{ "chs140", &place_chs140 },
	{ "chs010", &place_chs010 },
	{ "chs260", &place_chs260 },
	{ "chs210", &place_chs210 },
	{ "chs220", &place_chs220 },
	{ "chs230", &place_chs230 },
	{ "shs140", &place_shs140 },
	{ "shs270", &place_shs270 },
	{ "shs260", &place_shs260 },
	{ "shs250", &place_shs250 },
	{ "shs240", &place_shs240 },
	{ "trone", &place_trone },
	{ "bombe1", &place_bombe1 },
	{ "bombe2", &place_bombe2 },
	{ "shs230", &place_shs230 },
	{ "shs220", &place_shs220 },
	{ "shs210", &place_shs210 },
	{ "shs170", &place_shs170 },
	{ "shs160", &place_shs160 },
	{ "shs150", &place_shs150 },
	{ "shs130", &place_shs130 },
	{ "shs120", &place_shs120 },
	{ "shs110", &place_shs110 },
	{ "cpc330", &place_cpc330 },
	{ "cpc720", &place_cpc720 },
	{ "cpc350", &place_cpc350 },
	{ "cpc600", &place_cpc600 },
	{ "cpc340", &place_cpc340 },
	{ "cpc440", &place_cpc440 },
	{ "cpc320", &place_cpc320 },
	{ "cpc450", &place_cpc450 },
	{ "cpc310", &place_cpc310 },
	{ "cpc140", &place_cpc140 },
	{ "cpc130", &place_cpc130 },
	{ "cpc120", &place_cpc120 },
	{ "cpc110", &place_cpc110 },
	{ "cpc410", &place_cpc410 },
	{ "cpc420", &place_cpc420 },
	{ "cpc430", &place_cpc430 },
	{ "cpc540", &place_cpc540 },
	{ "cpc360", &place_cpc360 },
	{ "cpc550", &place_cpc550 },
	{ "cpc370", &place_cpc370 },
	{ "cpc230", &place_cpc230 },
	{ "cpc520", &place_cpc520 },
	{ "cpc530", &place_cpc530 },
	{ "cpc510", &place_cpc510 },
	{ "cpc210", &place_cpc210 },
	{ "cpc220", &place_cpc220 },
	{ "cpc710", &place_cpc710 },
	{ "cpc730", &place_cpc730 },
	{ "pne150", &place_pne150 },
	{ "pne140", &place_pne140 },
	{ "pne130", &place_pne130 },
	{ "pne120", &place_pne120 },
	{ "pne210", &place_pne210 },
	{ "pne220", &place_pne220 },
	{ "pne240", &place_pne240 },
	{ "pne230", &place_pne230 },
	{ "pne110", &place_pne110 },
	{ "pnew310", &place_pnew310 },
	{ "ctp110", &place_ctp110 },
	{ "ctp310", &place_ctp310 },
	{ "ctp320", &place_ctp320 },
	{ "ctp330", &place_ctp330 },
	{ "ctp340", &place_ctp340 },
	{ "ctp350", &place_ctp350 },
	{ "ctp210", &place_ctp210 },
	{ "lgew100", &place_lgew100 },
	{ "coffre", &place_coffre },
	{ "natte", &place_natte },
	{ "cachwen1", &place_cachwen1 },
	{ "cachwen2", &place_cachwen2 },
	{ "cachwen3", &place_cachwen3 },
	{ "pdc005", &place_pdc005 },
	{ "pdc010", &place_pdc010 },
	{ "pdc011", &place_pdc011 },
	{ "pdc012", &place_pdc012 },
	{ "pdc110", &place_pdc110 },
	{ "pdc140", &place_pdc140 },
	{ "pdc170", &place_pdc170 },
	{ "pdc178", &place_pdc178 },
	{ "pdc179", &place_pdc179 },
	{ "pdc130", &place_pdc130 },
	{ "pdc150", &place_pdc150 },
	{ "pdc120", &place_pdc120 },
	{ "pdc160", &place_pdc160 },
	{ "pdcw510", &place_pdcw510 },
	{ "pdcw520", &place_pdcw520 },
	{ "pdc112", &place_pdc112 },
	{ "pdc142", &place_pdc142 },
	{ "pdc172", &place_pdc172 },
	{ "bougies", &place_bougies },
	{ "lvierge", &place_lvierge },
	{ "rebus", &place_rebus },
	{ "pdc132", &place_pdc132 },
	{ "pdc152", &place_pdc152 },
	{ "pdc122", &place_pdc122 },
	{ "jarre2", &place_jarre2 },
	{ "pierre2", &place_pierre2 },
	{ "pierre21", &place_pierre21 },
	{ "pierre22", &place_pierre22 },
	{ "pdc162", &place_pdc162 },
	{ "pdc175", &place_pdc175 },
	{ "pdc176", &place_pdc176 },
	{ "pdc177", &place_pdc177 },
	{ "jarre1", &place_jarre1 },
	{ "arbre1", &place_arbre1 },
	{ "arbre2", &place_arbre2 },
	{ "arbre3", &place_arbre3 },
	{ "fsceaux", &place_fsceaux },
	{ "pierre1", &place_pierre1 },
	{ "pdcw171", &place_pdcw171 },
	{ "boite11", &place_boite11 },
	{ "boite12", &place_boite12 },
	{ "boite21", &place_boite21 },
	{ "boite22", &place_boite22 },
	{ "boite31", &place_boite31 },
	{ "boite32", &place_boite32 },
	{ "boite33", &place_boite33 },
	{ "boite331", &place_boite331 },
	{ "origine", &place_origine },
	{ "penjing", &place_penjing },
	{ "penjing2", &place_penjing2 },
	{ "proclam", &place_proclam },
	{ "pdcw512", &place_pdcw512 },
	{ "pdcw522", &place_pdcw522 },
	{ "aie100", &place_aie100 },
	{ "aie200", &place_aie200 },
	{ "aie300", &place_aie300 },
	{ "aie400", &place_aie400 },
	{ "aie500", &place_aie500 },
	{ "aie600b", &place_aie600b },
	{ "bpiw100", &place_bpiw100 },
	{ "bpiw200", &place_bpiw200 },
	{ "bpiw101", &place_bpiw101 },
	{ "bpiw201", &place_bpiw201 },
	{ "bpiw102", &place_bpiw102 },
	{ "bpiw202", &place_bpiw202 },
	{ "aio100", &place_aio100 },
	{ "aio200", &place_aio200 },
	{ "aio300", &place_aio300 },
	{ "aio400", &place_aio400 },
	{ "aio500", &place_aio500 },
	{ "lgaw100", &place_lgaw100 },
	{ "lgaw101", &place_lgaw101 },
	{ "lgaw102", &place_lgaw102 },
	{ "indice1", &place_indice1 },
	{ "confess1", &place_confess1 },
	{ "bouddha2", &place_bouddha2 },
	{ "bouddha3", &place_bouddha3 },
	{ "meuble1", &place_meuble1 },
	{ "meuble2", &place_meuble2 },
	{ "meuble30", &place_meuble30 },
	{ "meuble31", &place_meuble31 },
	{ "meuble40", &place_meuble40 },
	{ "meuble41", &place_meuble41 },
	{ "meuble42", &place_meuble42 },
	{ "meuble43", &place_meuble43 },
	{ "meuble5", &place_meuble5 },
	{ "espw100", &place_espw100 },
	{ "espw101", &place_espw101 },
	{ "espw200", &place_espw200 },
	{ "espw201", &place_espw201 },
	{ "table", &place_table },
	{ "table10", &place_table10 },
	{ "table11", &place_table11 },
	{ "table12", &place_table12 },
	{ "table13", &place_table13 },
	{ "table14", &place_table14 },
	{ "espw102", &place_espw102 },
	{ "espw202", &place_espw202 },
	{ "cgc110", &place_cgc110 },
	{ "cgc120", &place_cgc120 },
	{ "cgc130", &place_cgc130 },
	{ "cgc140", &place_cgc140 },
	{ "cgc150", &place_cgc150 },
	{ "cgc160", &place_cgc160 },
	{ "cgc210", &place_cgc210 },
	{ "cgc220", &place_cgc220 },
	{ "cgc230", &place_cgc230 },
	{ "cgc310", &place_cgc310 },
	{ "porte", &place_porte },
	{ "porte1", &place_porte1 },
	{ "porte2", &place_porte2 },
	{ "bouton1", &place_bouton1 },
	{ "bouton20", &place_bouton20 },
	{ "bouton21", &place_bouton21 },
	{ "confess3", &place_confess3 },
	{ "confess2", &place_confess2 },
	{ "indice2", &place_indice2 },
	{ "indice3", &place_indice3 },
	{ "go1", &place_go1 },
	{ "go20", &place_go20 },
	{ "go21", &place_go21 },
	{ "go22", &place_go22 },
	{ "fight", &place_fight },
	{ "fight2", &place_fight2 },
	{ "nwfw100", &place_nwfw100 },
	{ "nwfw101", &place_nwfw101 },
	{ "nwfw102", &place_nwfw102 },
	{ "banw100", &place_banw100 },
	{ "banw111", &place_banw111 },
	{ "banw112", &place_banw112 },
	{ "registre", &place_registre },
	{ "cahiers", &place_cahiers },
	{ "lboites", &place_lboites },
	{ "banw121", &place_banw121 },
	{ "banw122", &place_banw122 },
	{ "bdaw100", &place_bdaw100 },
	{ "edit", &place_edit },
	{ "plbombe", &place_plbombe },
	{ "horloge1", &place_horloge1 },
	{ "horloge2", &place_horloge2 },
	{ "horloge3", &place_horloge3 },
	{ "pinceau", &place_pinceau },
	{ "bdaw101", &place_bdaw101 },
	{ "spfw100", &place_spfw100 },
	{ "spfw101", &place_spfw101 },
	{ "spfw102", &place_spfw102 },
	{ "jixw111", &place_jixw111 },
	{ "jixw121", &place_jixw121 },
	{ "jixw131", &place_jixw131 },
	{ "posthum", &place_posthum },
	{ "confess4", &place_confess4 },
	{ "indice4", &place_indice4 },
	{ "jixw110", &place_jixw110 },
	{ "jixw120", &place_jixw120 },
	{ "jixw130", &place_jixw130 },
	{ "jixw112", &place_jixw112 },
	{ "victime", &place_victime },
	{ "jixw122", &place_jixw122 },
	{ "jixw132", &place_jixw132 },
	{ "jixw210", &place_jixw210 },
	{ "soupir", &place_soupir },
	{ "soupir2", &place_soupir2 },
	{ "puzzle41", &place_puzzle41 },
	{ "puzzle42", &place_puzzle42 },
	{ "puzzle43", &place_puzzle43 },
	{ nullptr, nullptr }
};

} // End of namespace China
} // End of namespace CryOmni3D
