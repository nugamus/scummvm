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


#ifndef RING_DETECTION_H
#define RING_DETECTION_H

#include "engines/advancedDetector.h"

namespace Ring {

enum RingDebugChannels {
	kDebugLoad = 1,
	kDebugGraphics,
	kDebugVideo,
	kDebugSound,
	kDebugScript,
	kDebugInput
};

extern const PlainGameDescriptor ringGames[];
extern const ADGameDescription gameDescriptions[];

} // End of namespace Ring

class RingMetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
	static const DebugChannelDef debugFlagList[];

public:
	RingMetaEngineDetection();
	~RingMetaEngineDetection() override {}

	const char *getName() const override {
		return "ring";
	}

	const char *getEngineName() const override {
		return "Ring";
	}

	const char *getOriginalCopyright() const override {
		return "Ring (C) 1999 Arxel Tribe";
	}

	const DebugChannelDef *getDebugChannels() const override {
		return debugFlagList;
	}
};

#endif // RING_DETECTION_H
