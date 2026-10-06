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

namespace Grumpa {

const PlainGameDescriptor grumpaGames[] = {
	{ "grumpa", "Grumpa" },
	{ 0, 0 }
};

// The installed game's folder: the game data dirs (Actors, Bitmaps, Scenes, ...) beside
// the programs. Detected on two language-independent game-data files (the Actors tables are
// identical across the Danish/Finnish/Norwegian/Swedish editions of the Nordic release).
// One entry, any language: an install's languages are not known (E-1770); the game options'
// language picks one.
#define GRUMPA_FOLDER(lang) \
	{ \
		"grumpa", nullptr, \
		AD_ENTRY2s("Actors/Characters.abi", "7b9d320c70317994ea55c1016d560bc4", 51645, \
				   "Actors/Items.abi",      "293eee0f0b4109eb1302c42b449efd9b", 10528), \
		lang, Common::kPlatformWindows, ADGF_UNSTABLE, GUIO1(GUIO_NOMIDI) \
	}

// The CD as shipped: the same data inside its InstallShield cabinet (E-1000). Items.abi
// is named alone in the cabinet; Characters.abi is not (Scenes has one too). One disc holds
// all four languages (E-0002): one entry each, the player picks one (E-1770).
#define GRUMPA_CD(lang) \
	{ \
		"grumpa", nullptr, \
		AD_ENTRY2s("data1.hdr",              "0fb9940d7f99ecbc5ed5766b0a8d6115", 685933, \
				   "is:data1.hdr:Items.abi", "A:293eee0f0b4109eb1302c42b449efd9b", 10528), \
		lang, Common::kPlatformWindows, ADGF_CD | ADGF_UNSTABLE, GUIO1(GUIO_NOMIDI) \
	}

const ADGameDescription gameDescriptions[] = {
	GRUMPA_FOLDER(Common::UNK_LANG),
	GRUMPA_CD(Common::SV_SWE),
	GRUMPA_CD(Common::DA_DNK),
	GRUMPA_CD(Common::FI_FIN),
	GRUMPA_CD(Common::NB_NOR),

	AD_TABLE_END_MARKER
};

#undef GRUMPA_FOLDER
#undef GRUMPA_CD

} // End of namespace Grumpa
