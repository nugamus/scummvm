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

namespace Gilbert {

const PlainGameDescriptor gilbertGames[] = {
	{ "gilbert", "Gilbert og den kemystiske \xC3\xB8" },
	{ 0, 0 }
};

// The installed game's folder (the CD's Program folder: Gilbert.exe beside Data), or the
// CD itself.
const ADGameDescription gameDescriptions[] = {
	{
		"gilbert",
		nullptr,
		AD_ENTRY2s("Gilbert.exe", "1285f64f4cf377ed95bf039623fcd561", 582144,
				   "Data/game/default.dat", "93069a4704dcc9d36b1093840ec2f60f", 702971),
		Common::DA_DNK,
		Common::kPlatformWindows,
		ADGF_UNSTABLE | ADGF_CD,
		GUIO8(GUIO_NOMIDI, GAMEOPTION_ALWAYS_RUN, GAMEOPTION_SHORTCUTS, GAMEOPTION_WHEEL,
		      GAMEOPTION_MARK_CHOICES, GAMEOPTION_NEW_TOPICS, GAMEOPTION_AUTOSAVE, GAMEOPTION_FULLSCREEN_FILMS)
	},
	{
		"gilbert",
		"CD",
		AD_ENTRY2s("Program/Gilbert.exe", "1285f64f4cf377ed95bf039623fcd561", 582144,
				   "Program/Data/game/default.dat", "93069a4704dcc9d36b1093840ec2f60f", 702971),
		Common::DA_DNK,
		Common::kPlatformWindows,
		ADGF_UNSTABLE | ADGF_CD,
		GUIO8(GUIO_NOMIDI, GAMEOPTION_ALWAYS_RUN, GAMEOPTION_SHORTCUTS, GAMEOPTION_WHEEL,
		      GAMEOPTION_MARK_CHOICES, GAMEOPTION_NEW_TOPICS, GAMEOPTION_AUTOSAVE, GAMEOPTION_FULLSCREEN_FILMS)
	},

	AD_TABLE_END_MARKER
};

} // End of namespace Gilbert
