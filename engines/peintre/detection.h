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

#ifndef PEINTRE_DETECTION_H
#define PEINTRE_DETECTION_H

#include "engines/advancedDetector.h"

namespace Peintre {

enum PeintreDebugChannels {
	kDebugLoad = 1,
	kDebugGraphics,
	kDebugScript,
	kDebugInput,
	kDebugSound
};

// Enhancements, all off by default (the original).
#define GAMEOPTION_WIDESCREEN GUIO_GAMEOPTIONS1
#define GAMEOPTION_FILTER_TEXTURES GUIO_GAMEOPTIONS2
#define GAMEOPTION_MODERN_CONTROLS GUIO_GAMEOPTIONS3
#define GAMEOPTION_INVERT_Y GUIO_GAMEOPTIONS4
#define GAMEOPTION_MOUSE_SENSITIVITY GUIO_GAMEOPTIONS5
#define GAMEOPTION_TURN_SPEED GUIO_GAMEOPTIONS6
#define GAMEOPTION_FOV GUIO_GAMEOPTIONS7

extern const PlainGameDescriptor peintreGames[];

extern const ADGameDescription gameDescriptions[];

} // End of namespace Peintre

class PeintreMetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
	static const DebugChannelDef debugFlagList[];

public:
	PeintreMetaEngineDetection();
	~PeintreMetaEngineDetection() override {}

	const char *getName() const override {
		return "peintre";
	}

	const char *getEngineName() const override {
		return "Peintre";
	}

	const char *getOriginalCopyright() const override {
		return "Mission Sunlight (C) 1998 index+, Media Factory, Cryo Interactive";
	}

	const DebugChannelDef *getDebugChannels() const override {
		return debugFlagList;
	}
};

#endif // PEINTRE_DETECTION_H
