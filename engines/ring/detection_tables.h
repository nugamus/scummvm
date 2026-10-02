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


namespace Ring {

const PlainGameDescriptor ringGames[] = {
	{ "ring", "Ring: The Legend of the Nibelungen" },
	{ nullptr, nullptr }
};

// DVD edition: RING.EXE and AS.AT2 are the same in every language; the language is the
// SY archive in DATA/<LAN>/ (engines/ring E-0007, E-0008 in the research repository).
#define RING_DVD(lang, sy, symd5, sysize) \
	{ \
		"ring", "DVD", \
		AD_ENTRY3s("RING.EXE", "10e21ce9cf937c56c5891113ac1cfcc2", 618496, \
				   "DATA/AS.AT2", "5f65ee721fdf50bc074dd25bb28592fb", 1533273, \
				   sy, symd5, sysize), \
		lang, Common::kPlatformWindows, ADGF_UNSTABLE | ADGF_DVD, GUIO1(GUIO_NOMIDI) \
	}

#define RING_CD(lang, sy, symd5, sysize) \
	{ \
		"ring", "CD", \
		AD_ENTRY3s("RING.EXE", "daa9454d0a6383d8c69172a3873713a5", 712192, \
				   "DATA/AS.AT2", "5f65ee721fdf50bc074dd25bb28592fb", 1533273, \
				   sy, symd5, sysize), \
		lang, Common::kPlatformWindows, ADGF_UNSTABLE | ADGF_CD, GUIO1(GUIO_NOMIDI) \
	}

#define RING_ISO(lang, sy, symd5, sysize) \
	{ \
		"ring", "ISO", \
		AD_ENTRY3s("RING.EXE", "88a6962191f6c5aa35c93d49115a59ce", 663552, \
				   "DATA/AS.AT2", "5f65ee721fdf50bc074dd25bb28592fb", 1533273, \
				   sy, symd5, sysize), \
		lang, Common::kPlatformWindows, ADGF_UNSTABLE | ADGF_CD, GUIO1(GUIO_NOMIDI) \
	}

const ADGameDescription gameDescriptions[] = {
	RING_DVD(Common::EN_ANY, "DATA/ENG/SY.AT2", "26dad59f1c5f374172042848d6ac0c95", 12211188),
	RING_DVD(Common::FR_FRA, "DATA/FRA/SY.AT2", "50c8c8eb1fb5020b433eefe830f89fcf", 12247853),
	RING_DVD(Common::DE_DEU, "DATA/GER/SY.AT2", "9e458bb847b75ec2eba4c4bb270a988e", 12248323),
	RING_DVD(Common::NL_NLD, "DATA/HOL/SY.AT2", "df1ba9a4d70bd2440db66c25f1f45a0f", 12241075),
	RING_DVD(Common::IT_ITA, "DATA/ITA/SY.AT2", "499b2b38d7a64e7241ec54d607944c87", 12243837),
	RING_DVD(Common::ES_ESP, "DATA/SPA/SY.AT2", "8f7585ec1969d8e6ac9c6a9a5f2ef9cd", 12248009),
	RING_DVD(Common::SV_SWE, "DATA/SWE/SY.AT2", "499b2b38d7a64e7241ec54d607944c87", 12250280),

	// The CD version (6 discs) and the ISO version (4 discs): every disc copied into one folder
	// (spec/editions.md, "Discs and folders"); English, French and German only.
	RING_CD(Common::EN_ANY, "DATA/ENG/SY.AT2", "26dad59f1c5f374172042848d6ac0c95", 12211188),
	RING_CD(Common::FR_FRA, "DATA/FRA/SY.AT2", "50c8c8eb1fb5020b433eefe830f89fcf", 12247853),
	RING_CD(Common::DE_DEU, "DATA/GER/SY.AT2", "9e458bb847b75ec2eba4c4bb270a988e", 12248323),
	RING_ISO(Common::EN_ANY, "DATA/ENG/SY.AT2", "611266f1e782c344c180d39dcda170e4", 13146193),
	RING_ISO(Common::FR_FRA, "DATA/FRA/SY.AT2", "73f21eaa50a4f6ffa8bf65f78e66b850", 13189839),
	RING_ISO(Common::DE_DEU, "DATA/GER/SY.AT2", "73f21eaa50a4f6ffa8bf65f78e66b850", 13191355),

	AD_TABLE_END_MARKER
};

#undef RING_DVD
#undef RING_CD
#undef RING_ISO

} // End of namespace Ring
