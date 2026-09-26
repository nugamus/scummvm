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

#include "base/plugins.h"
#include "x3d/detection.h"
#include "x3d/detection_tables.h"

const DebugChannelDef X3DMetaEngineDetection::debugFlagList[] = {
	{ X3D::kDebugLoad, "Load", "File loading" },
	{ X3D::kDebugGraphics, "Graphics", "Graphics debug level" },
	{ X3D::kDebugScript, "Script", "Unit code and INFOACT actions" },
	{ X3D::kDebugInput, "Input", "Keys, clicks and picking" },
	{ X3D::kDebugSound, "Sound", "Voices and talkers" },
	{ X3D::kDebugMenu, "Menu", "Frames and menus" },
	DEBUG_CHANNEL_END
};

static const char *const directoryGlobs[] = {
	"data",
	"2dbit",
	nullptr
};

X3DMetaEngineDetection::X3DMetaEngineDetection() : AdvancedMetaEngineDetection(
	X3D::gameDescriptions, X3D::monetGames) {
	_flags = kADFlagMatchFullPaths;
	_maxScanDepth = 3;
	_directoryGlobs = directoryGlobs;
}

REGISTER_PLUGIN_STATIC(X3D_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, X3DMetaEngineDetection);
