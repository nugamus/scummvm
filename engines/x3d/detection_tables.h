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

namespace X3D {

const PlainGameDescriptor monetGames[] = {
	{ "monet", "Monet: The Mystery of the Orangerie Museum" },
	{ 0, 0 }
};

const ADGameDescription gameDescriptions[] = {
	{
		"monet",
		nullptr,
		AD_ENTRY2s("Data/App.bin", "0963102a249b8d930cc02a13aad2fd0c", 440,
				   "Data/2dbit/Intro1.bmp", "11104526a28e99fb42b1c8c4538e777b", 921656),
		Common::EN_ANY,
		Common::kPlatformWindows,
		ADGF_UNSTABLE | ADGF_CD,
		// (two options share an argument: there is no GUIO13)
		GUIO12(GUIO_NOMIDI, GAMEOPTION_HIGH_FPS GAMEOPTION_HIGH_RES, GAMEOPTION_WIDESCREEN, GAMEOPTION_MAX_DETAIL, GAMEOPTION_FILTER_TEXTURES, GAMEOPTION_RUN_TOGGLE, GAMEOPTION_CROUCH_TOGGLE, GAMEOPTION_MODERN_CONTROLS, GAMEOPTION_INVERT_Y, GAMEOPTION_MOUSE_SENSITIVITY, GAMEOPTION_TURN_SPEED, GAMEOPTION_FOV)
	},

	AD_TABLE_END_MARKER
};

} // End of namespace X3D
