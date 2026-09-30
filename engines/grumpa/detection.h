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

#ifndef GRUMPA_DETECTION_H
#define GRUMPA_DETECTION_H

#include "engines/advancedDetector.h"

namespace Grumpa {

enum GrumpaDebugChannels {
	kDebugLoad = 1,
	kDebugGraphics,
	kDebugScript,
	kDebugSound
};

extern const PlainGameDescriptor grumpaGames[];

extern const ADGameDescription gameDescriptions[];

} // End of namespace Grumpa

class GrumpaMetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
	static const DebugChannelDef debugFlagList[];

public:
	GrumpaMetaEngineDetection();
	~GrumpaMetaEngineDetection() override {}

	const char *getName() const override {
		return "grumpa";
	}

	const char *getEngineName() const override {
		return "Grumpa";
	}

	const char *getOriginalCopyright() const override {
		return "Grumpa (C) 2002 Idol FX / Vision Park";
	}

	const DebugChannelDef *getDebugChannels() const override {
		return debugFlagList;
	}
};

#endif // GRUMPA_DETECTION_H
