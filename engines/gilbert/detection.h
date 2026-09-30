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

#ifndef GILBERT_DETECTION_H
#define GILBERT_DETECTION_H

#include "engines/advancedDetector.h"

namespace Gilbert {

enum GilbertDebugChannels {
	kDebugLoad = 1,
	kDebugGraphics,
	kDebugScript,
	kDebugSound
};

// Keymapper actions: the original's only keys (boot.md, rooms.md).
enum GilbertAction {
	kActionNone,
	kActionSkip, ///< Escape: skips a film
	kActionRun   ///< Ctrl: Gilbert runs while it is held
};

extern const PlainGameDescriptor gilbertGames[];

extern const ADGameDescription gameDescriptions[];

} // End of namespace Gilbert

class GilbertMetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
	static const DebugChannelDef debugFlagList[];

public:
	GilbertMetaEngineDetection();
	~GilbertMetaEngineDetection() override {}

	const char *getName() const override {
		return "gilbert";
	}

	const char *getEngineName() const override {
		return "Gilbert";
	}

	const char *getOriginalCopyright() const override {
		return "Gilbert (C) 1999 Pir New World Media";
	}

	const DebugChannelDef *getDebugChannels() const override {
		return debugFlagList;
	}
};

#endif // GILBERT_DETECTION_H
