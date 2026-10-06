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
#include "grumpa/detection.h"
#include "grumpa/detection_tables.h"

const DebugChannelDef GrumpaMetaEngineDetection::debugFlagList[] = {
	{ Grumpa::kDebugLoad, "Load", "File loading" },
	{ Grumpa::kDebugGraphics, "Graphics", "Graphics debug level" },
	{ Grumpa::kDebugScript, "Script", "Game logic" },
	{ Grumpa::kDebugSound, "Sound", "Sound and music" },
	{ Grumpa::kDebugCoverage, "coverage", "Which opcodes and scenes run (cov lines for test coverage)" },
	DEBUG_CHANNEL_END
};

// The game data lives in subdirectories (Actors, Bitmaps, Scenes, ...) beside the programs.
static const char *const directoryGlobs[] = {
	"Actors",
	nullptr
};

GrumpaMetaEngineDetection::GrumpaMetaEngineDetection() : AdvancedMetaEngineDetection(
	Grumpa::gameDescriptions, Grumpa::grumpaGames) {
	_flags = kADFlagMatchFullPaths;
	_maxScanDepth = 2;
	_directoryGlobs = directoryGlobs;
}

REGISTER_PLUGIN_STATIC(GRUMPA_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, GrumpaMetaEngineDetection);
