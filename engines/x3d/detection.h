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

#ifndef X3D_DETECTION_H
#define X3D_DETECTION_H

#include "engines/advancedDetector.h"

namespace X3D {

enum X3DDebugChannels {
	kDebugLoad = 1,
	kDebugGraphics,
	kDebugScript,
	kDebugInput,
	kDebugSound,
	kDebugMenu
};

extern const PlainGameDescriptor monetGames[];

extern const ADGameDescription gameDescriptions[];

#define GAMEOPTION_WIDESCREEN GUIO_GAMEOPTIONS1

} // End of namespace X3D

class X3DMetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
	static const DebugChannelDef debugFlagList[];

public:
	X3DMetaEngineDetection();
	~X3DMetaEngineDetection() override {}

	const char *getName() const override {
		return "x3d";
	}

	const char *getEngineName() const override {
		return "X3D";
	}

	const char *getOriginalCopyright() const override {
		return "X3D (C) 1998-2000 4X Technologies";
	}

	const DebugChannelDef *getDebugChannels() const override {
		return debugFlagList;
	}
};

#endif // X3D_DETECTION_H
