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

// China's map tables (spec/china-interface.md Map, E-1150..E-1152), included by map.cpp.
// Generated from the game's data tables; regenerate rather than edit.

#ifndef CRYOMNI3D_CHINA_MAP_TABLES_H
#define CRYOMNI3D_CHINA_MAP_TABLES_H

namespace CryOmni3D {
namespace China {

// You are here: place name prefix (3 letters) or full name, point on petiplan
struct MapPoint {
	const char *place;
	int16 x, y;
};

static const MapPoint kMapPrefixPoints[] = {
	{ "shs", 132, 205 },
	{ "shm", 132, 118 },
	{ "shp", 132, 160 },
	{ "poc", 132, 127 },
	{ "ptt", 132, 66 },
	{ "spf", 132, 79 },
	{ "ppc", 132, 91 },
	{ "nwf", 100, 135 },
	{ "ban", 100, 135 },
	{ "esp", 144, 127 },
	{ "lga", 86, 116 },
	{ "bda", 100, 135 },
	{ "pdc", 164, 50 },
	{ "bpi", 115, 122 },
};

static const MapPoint kMapPlacePoints[] = {
	{ "pne310", 96, 105 },
	{ "pne210", 96, 113 },
	{ "pne220", 91, 111 },
	{ "pne230", 101, 111 },
	{ "pne240", 96, 111 },
	{ "pne110", 107, 115 },
	{ "pne120", 107, 117 },
	{ "pne130", 104, 117 },
	{ "pne140", 96, 117 },
	{ "pne150", 89, 117 },
	{ "jixw120", 143, 31 },
	{ "jixw121", 143, 31 },
	{ "jixw122", 143, 31 },
	{ "jixw130", 143, 28 },
	{ "jixw131", 143, 28 },
	{ "jixw132", 143, 28 },
	{ "jixw110", 143, 35 },
	{ "jixw111", 143, 35 },
	{ "jixw112", 143, 35 },
	{ "jixw210", 143, 23 },
	{ "cpc110", 115, 155 },
	{ "cpc120", 115, 120 },
	{ "cpc130", 115, 125 },
	{ "cpc140", 121, 125 },
	{ "cpc210", 150, 115 },
	{ "cpc220", 150, 125 },
	{ "cpc230", 147, 125 },
	{ "cpc310", 127, 125 },
	{ "cpc320", 127, 122 },
	{ "cpc330", 129, 122 },
	{ "cpc340", 132, 122 },
	{ "cpc350", 136, 122 },
	{ "cpc360", 139, 122 },
	{ "cpc370", 139, 125 },
	{ "cpc410", 119, 115 },
	{ "cpc420", 119, 120 },
	{ "cpc430", 123, 115 },
	{ "cpc440", 129, 120 },
	{ "cpc450", 129, 110 },
	{ "cpc510", 147, 115 },
	{ "cpc520", 147, 120 },
	{ "cpc530", 142, 115 },
	{ "cpc540", 136, 120 },
	{ "cpc550", 136, 110 },
	{ "cpc600", 132, 115 },
	{ "cpc710", 129, 100 },
	{ "cpc720", 132, 100 },
	{ "cpc730", 136, 100 },
	{ "aio100", 112, 127 },
	{ "aio200", 112, 115 },
	{ "aio300", 112, 104 },
	{ "aio400", 112, 91 },
	{ "aio500", 112, 77 },
	{ "aie100", 152, 127 },
	{ "aie200", 152, 115 },
	{ "aie300", 152, 104 },
	{ "aie400", 152, 91 },
	{ "aie500", 152, 77 },
	{ "aie600b", 152, 60 },
	{ "ctp110", 121, 77 },
	{ "ctp310", 128, 77 },
	{ "ctp320", 128, 82 },
	{ "ctp330", 132, 82 },
	{ "ctp340", 137, 82 },
	{ "ctp350", 137, 77 },
	{ "ctp210", 143, 77 },
	{ "cgc110", 100, 135 },
	{ "cgc120", 115, 140 },
	{ "cgc130", 123, 140 },
	{ "cgc140", 132, 140 },
	{ "cgc150", 141, 140 },
	{ "cgc160", 152, 137 },
	{ "cgc210", 117, 153 },
	{ "cgc220", 147, 153 },
	{ "cgc230", 132, 153 },
	{ "cgc310", 132, 135 },
	{ "chs010", 132, 214 },
	{ "chs110", 117, 211 },
	{ "chs120", 124, 214 },
	{ "chs130", 132, 214 },
	{ "chs140", 141, 214 },
	{ "chs150", 148, 211 },
	{ "chs240", 124, 218 },
	{ "chs210", 124, 222 },
	{ "chs220", 132, 222 },
	{ "chs230", 141, 222 },
	{ "chs250", 132, 218 },
	{ "chs260", 141, 218 },
	{ "cth110", 147, 203 },
	{ "cth120", 147, 198 },
	{ "cth130", 142, 197 },
	{ "cth140", 133, 197 },
	{ "cth150", 123, 197 },
	{ "cth160", 132, 197 },
	{ "cth170", 118, 203 },
	{ "cth210", 132, 191 },
	{ "cth220", 132, 188 },
	{ "cth230", 139, 188 },
	{ "cth240", 139, 182 },
	{ "cth250", 139, 174 },
	{ "cth260", 132, 174 },
	{ "cth270", 126, 174 },
	{ "cth280", 126, 182 },
	{ "cth290", 126, 188 },
	{ "cth310", 132, 174 },
	{ "cth320", 132, 168 },
	{ "cth330", 139, 168 },
	{ "cth340", 149, 168 },
	{ "cth350", 149, 164 },
	{ "cth360", 125, 168 },
	{ "cth370", 117, 168 },
	{ "cth380", 117, 164 },
	{ "cth410", 139, 182 },
	{ "cth420", 151, 182 },
	{ "cth430", 155, 182 },
	{ "cth440", 161, 182 },
	{ "cth450", 161, 175 },
	{ "cth510", 126, 182 },
	{ "cth520", 115, 182 },
	{ "cth530", 111, 182 },
	{ "cth540", 105, 182 },
	{ "cth550", 105, 175 },
};

// Hot spots on granplan (inclusive rectangles); type 0 travel, 7 label, 8 documentation
struct MapSpot {
	int16 top, left, bottom, right;
	const char *key;
	byte type;
	const char *place; // type 0: the travel target
	float alpha;
};

static const MapSpot kMapSpots[] = {
	{ 416, 295, 433, 305, "NWF", 0, "cgc110", 3.130f },
	{ 394, 459, 412, 477, "ESP", 0, "cpc230", 4.730f },
	{ 378, 355, 395, 372, "BPI", 0, "cpc120", 2.400f },
	{ 694, 410, 709, 434, "CHS", 0, "chs220", 1.510f },
	{ 647, 407, 671, 444, "SHS", 0, "shs140", 1.560f },
	{ 594, 413, 617, 435, "SHM", 0, "cth220", 1.560f },
	{ 525, 413, 547, 435, "SHP", 0, "cth310", 1.510f },
	{ 429, 407, 453, 438, "POC", 0, "cgc140", 3.130f },
	{ 355, 287, 378, 321, "PNE", 0, "pne140", 1.570f },
	{ 348, 410, 370, 435, "CPC", 0, "cpc600", 1.500f },
	{ 255, 413, 274, 436, "SPF", 0, "ctp330", 1.600f },
	{ 100, 443, 141, 476, "JIX", 0, "aie600b", 1.730f },
	{ 171, 519, 190, 541, "PDC", 0, "pdc010", 0.070f },
	{ 416, 305, 433, 317, "NWF", 8, nullptr, 0.f },
	{ 1054, 378, 1091, 463, "CRE", 8, nullptr, 0.f },
	{ 1054, 350, 1178, 378, "PDM", 7, nullptr, 0.f },
	{ 1054, 464, 1178, 492, "PDM", 7, nullptr, 0.f },
	{ 870, 391, 902, 453, "PHS", 8, nullptr, 0.f },
	{ 869, 311, 1049, 532, "CRE", 7, nullptr, 0.f },
	{ 741, 534, 796, 558, "PBA", 7, nullptr, 0.f },
	{ 742, 286, 795, 308, "PNR", 7, nullptr, 0.f },
	{ 623, 385, 671, 461, "SHS", 8, nullptr, 0.f },
	{ 557, 408, 588, 438, "SHM", 8, nullptr, 0.f },
	{ 490, 394, 521, 453, "SHP", 8, nullptr, 0.f },
	{ 430, 293, 466, 312, "PAG", 7, nullptr, 0.f },
	{ 431, 535, 467, 553, "PFS", 7, nullptr, 0.f },
	{ 393, 405, 412, 441, "POC", 8, nullptr, 0.f },
	{ 418, 313, 427, 346, "GDC", 8, nullptr, 0.f },
	{ 313, 261, 380, 344, "PNE", 8, nullptr, 0.f },
	{ 272, 396, 302, 453, "PPC", 8, nullptr, 0.f },
	{ 234, 413, 255, 436, "SPF", 8, nullptr, 0.f },
	{ 197, 396, 225, 453, "PTT", 8, nullptr, 0.f },
	{ 63, 350, 161, 500, "JIX", 8, nullptr, 0.f },
	{ 0, 399, 35, 453, "PGM", 7, nullptr, 0.f },
	{ 131, 503, 186, 556, "PPF", 7, nullptr, 0.f },
	{ 189, 503, 243, 556, "PRC", 7, nullptr, 0.f },
	{ 247, 503, 306, 556, "PBS", 7, nullptr, 0.f },
	{ 131, 564, 186, 617, "PYS", 7, nullptr, 0.f },
	{ 189, 564, 243, 617, "PHE", 7, nullptr, 0.f },
	{ 247, 564, 306, 617, "PBE", 7, nullptr, 0.f },
	{ 65, 503, 128, 663, "CLN", 7, nullptr, 0.f },
	{ 257, 620, 306, 658, "MDS", 7, nullptr, 0.f },
	{ 200, 620, 253, 658, "MDS", 7, nullptr, 0.f },
	{ 136, 632, 153, 667, "PSMC", 7, nullptr, 0.f },
	{ 313, 501, 391, 554, "PDJ", 7, nullptr, 0.f },
	{ 313, 594, 379, 657, "SCA", 7, nullptr, 0.f },
	{ 129, 231, 183, 285, "PBU", 7, nullptr, 0.f },
	{ 188, 231, 248, 285, "PPE", 7, nullptr, 0.f },
	{ 249, 231, 302, 285, "SPA", 7, nullptr, 0.f },
	{ 130, 293, 189, 346, "PEA", 7, nullptr, 0.f },
	{ 191, 293, 242, 346, "PAE", 7, nullptr, 0.f },
	{ 247, 293, 305, 346, "PLE", 7, nullptr, 0.f },
	{ 256, 178, 282, 199, "PFP", 8, nullptr, 0.f },
	{ 387, 261, 397, 344, "OCI", 7, nullptr, 0.f },
	{ 194, 177, 210, 199, "SFP", 7, nullptr, 0.f },
	{ 140, 210, 153, 230, "JFG", 7, nullptr, 0.f },
	{ 62, 158, 130, 231, "JJFG", 7, nullptr, 0.f },
	{ 64, 275, 124, 308, "PFP", 7, nullptr, 0.f },
	{ 64, 241, 124, 273, "SVP", 7, nullptr, 0.f },
	{ 43, 12, 122, 51, "TRC", 7, nullptr, 0.f },
	{ 79, 98, 100, 134, "SFE", 7, nullptr, 0.f },
	{ 196, 100, 219, 131, "PLP", 7, nullptr, 0.f },
	{ 374, 86, 394, 117, "PSE", 7, nullptr, 0.f },
	{ 397, 152, 427, 200, "PTC", 7, nullptr, 0.f },
	{ 310, 72, 656, 246, "PJID", 7, nullptr, 0.f },
	{ 908, 0, 962, 26, "PFO", 7, nullptr, 0.f },
	{ 852, 155, 880, 198, "SBM", 7, nullptr, 0.f },
	{ 1023, 97, 1039, 205, "MDL", 7, nullptr, 0.f },
	{ 986, 85, 1000, 110, "SPS", 7, nullptr, 0.f },
	{ 223, 787, 242, 811, "PSA", 8, nullptr, 0.f },
	{ 200, 729, 255, 774, "SNC", 8, nullptr, 0.f },
	{ 141, 729, 174, 774, "HLJ", 7, nullptr, 0.f },
	{ 111, 733, 133, 771, "CHP", 7, nullptr, 0.f },
	{ 81, 734, 96, 770, "PFA", 7, nullptr, 0.f },
	{ 66, 789, 75, 818, "PFB", 7, nullptr, 0.f },
	{ 64, 775, 77, 788, "PSB", 7, nullptr, 0.f },
	{ 209, 700, 220, 715, "CFP", 7, nullptr, 0.f },
	{ 168, 697, 181, 717, "HIA", 7, nullptr, 0.f },
	{ 92, 705, 111, 725, "PER", 7, nullptr, 0.f },
	{ 65, 685, 254, 729, "JQL", 7, nullptr, 0.f },
	{ 384, 734, 400, 769, "POT", 7, nullptr, 0.f },
	{ 306, 723, 334, 780, "SPI", 7, nullptr, 0.f },
	{ 274, 725, 295, 779, "PLT", 7, nullptr, 0.f },
	{ 1018, 579, 1036, 597, "GDS", 7, nullptr, 0.f },
	{ 1018, 615, 1036, 669, "AGS", 7, nullptr, 0.f },
	{ 1018, 671, 1036, 724, "MDA", 7, nullptr, 0.f },
	{ 910, 611, 929, 648, "PFL", 7, nullptr, 0.f },
	{ 856, 608, 882, 650, "SFL", 7, nullptr, 0.f },
	{ 825, 609, 846, 650, "SPS", 7, nullptr, 0.f },
	{ 782, 607, 801, 651, "PEL", 7, nullptr, 0.f },
	{ 986, 793, 1016, 810, "LGI", 7, nullptr, 0.f },
	{ 908, 818, 963, 844, "PFE", 7, nullptr, 0.f },
	{ 537, 684, 659, 796, "TLS", 7, nullptr, 0.f },
	{ 541, 643, 658, 656, "OTC", 7, nullptr, 0.f },
	{ 544, 579, 564, 614, "KTC", 7, nullptr, 0.f },
};

// The building list (ico_bat), bottom row first: label key, point on the screen
static const MapPoint kMapBuildings[] = {
	{ "SHS", 506, 252 },
	{ "ESP", 520, 157 },
	{ "PNE", 468, 135 },
	{ "SPF", 506, 95 },
	{ "JIX", 517, 79 },
	{ "PPF", 539, 100 },
	{ "CPC", 506, 140 },
	{ "BPI", 487, 150 },
	{ "POC", 506, 157 },
	{ "NWF", 470, 165 },
};

} // End of namespace China
} // End of namespace CryOmni3D

#endif
