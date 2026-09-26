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

#ifndef RING_API_H
#define RING_API_H

#include "common/scummsys.h"

namespace Ring {

/** The zone API calls the set-ups make (spec/api.md, "Declarations"). */
enum ApiCall {
	kAddObj, kAddPuz, kAddRot, kObjAddBagAni, kObjAddPre, kObjAddPuzAcc, kObjAddRotAcc,
	kObjPreAddAniToPuz, kObjPreAddAniToRot, kObjPreAddImgToPuz, kObjPreAddImgToRot,
	kObjPreAddTxtToPuz, kObjPreAniSetStaFra, kObjPrePauAni, kObjPreSetAniCooOnPuz,
	kObjPreSetAniIdeOnPuz, kObjPreSetAniIdeOnRot, kObjPreSho, kObjSetAccOff, kObjSetActCur,
	kObjSetActDraCur, kObjSetPasCur, kObjSetPasDraCur, kObjSetPuzAccKey, kPuzAdd3DSou,
	kPuzAddAmbSou, kPuzAddBgrImg, kPuzAddMovToPuz, kPuzAddMovToRot, kPuzSet3DSouOff,
	kPuzSetAmbSouOff, kPuzSetMovToRot, kRotAdd3DSou, kRotAddAmbSou, kRotAddMovToPuz,
	kRotAddMovToRot, kRotSet3DSouOff, kRotSetAmbSouOff, kRotSetJugOn, kRotSetMovOff,
	kRotSetMovToPuz, kRotSetMovToRot, kSetComBufLen, kSetZone, kSouAdd, kSouSet, kVarDefByte,
	kVarDefDwrd, kVarDefFloa, kVarDefStrg, kVarDefWord, kVarSetByte
};

/**
 * One call of a zone set-up, with its constant arguments in order. A string argument
 * holds its index into `strs`; a float argument holds its 32-bit pattern.
 */
struct SetupCall {
	ApiCall call;
	int argc;
	int32 args[13];
	const char *strs[2];
};

/** The set-up calls of zone 1..8 (ring/setup.cpp, generated). */
const SetupCall *zoneSetup(int zone, uint &count);

} // End of namespace Ring

#endif // RING_API_H
