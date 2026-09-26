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
#include "peintre/detection.h"
#include "peintre/detection_tables.h"

const DebugChannelDef PeintreMetaEngineDetection::debugFlagList[] = {
	{ Peintre::kDebugLoad, "Load", "File loading" },
	{ Peintre::kDebugGraphics, "Graphics", "Graphics debug level" },
	{ Peintre::kDebugScript, "Script", "Game logic" },
	{ Peintre::kDebugInput, "Input", "Keys, clicks and picking" },
	{ Peintre::kDebugSound, "Sound", "Sound and music" },
	DEBUG_CHANNEL_END
};

static const char *const directoryGlobs[] = {
	"data",
	"scenes_3d",
	nullptr
};

PeintreMetaEngineDetection::PeintreMetaEngineDetection() : AdvancedMetaEngineDetection(
	Peintre::gameDescriptions, Peintre::peintreGames) {
	_flags = kADFlagMatchFullPaths;
	_maxScanDepth = 3;
	_directoryGlobs = directoryGlobs;
}

REGISTER_PLUGIN_STATIC(PEINTRE_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, PeintreMetaEngineDetection);
