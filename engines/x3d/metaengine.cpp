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

#include "common/translation.h"

#include "x3d/metaengine.h"
#include "x3d/detection.h"
#include "x3d/x3d.h"

static const ADExtraGuiOptionsMap optionsList[] = {
	{
		GAMEOPTION_WIDESCREEN,
		{
			_s("Widescreen"),
			_s("Show more of the scene to the sides on a wide display"),
			"widescreen",
			false,
			0,
			0
		}
	},
	AD_EXTRA_GUI_OPTIONS_TERMINATOR
};

const ADExtraGuiOptionsMap *X3DMetaEngine::getAdvancedExtraGuiOptions() const {
	return optionsList;
}

const char *X3DMetaEngine::getName() const {
	return "x3d";
}

Common::Error X3DMetaEngine::createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const {
	*engine = new X3D::X3DEngine(syst, desc);
	return Common::kNoError;
}

#if PLUGIN_ENABLED_DYNAMIC(X3D)
REGISTER_PLUGIN_DYNAMIC(X3D, PLUGIN_TYPE_ENGINE, X3DMetaEngine);
#else
REGISTER_PLUGIN_STATIC(X3D, PLUGIN_TYPE_ENGINE, X3DMetaEngine);
#endif
