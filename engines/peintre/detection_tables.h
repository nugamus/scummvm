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

namespace Peintre {

const PlainGameDescriptor peintreGames[] = {
	{ "sunlight", "Mission Sunlight" },
	{ 0, 0 }
};

const ADGameDescription gameDescriptions[] = {
	{
		"sunlight",
		nullptr,
		AD_ENTRY2s("Data/mission.___", "4ae6122f86f89ceb3a59779d33101b91", 1751552,
				   "Data/Scenes_3D/musee.BFG", "deaee53ddb989c7328c1ba57cb7166b2", 2768417),
		Common::EN_ANY,
		Common::kPlatformWindows,
		ADGF_UNSTABLE | ADGF_CD,
		GUIO8(GUIO_NOMIDI, GAMEOPTION_WIDESCREEN, GAMEOPTION_FILTER_TEXTURES, GAMEOPTION_MODERN_CONTROLS, GAMEOPTION_INVERT_Y,
			  GAMEOPTION_MOUSE_SENSITIVITY, GAMEOPTION_TURN_SPEED, GAMEOPTION_FOV)
	},

	AD_TABLE_END_MARKER
};

} // End of namespace Peintre
