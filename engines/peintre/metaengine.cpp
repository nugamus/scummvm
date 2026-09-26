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

#include "engines/advancedDetector.h"

#include "peintre/detection.h"
#include "peintre/peintre.h"

class PeintreMetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
	const char *getName() const override {
		return "peintre";
	}

	Common::Error createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const override {
		*engine = new Peintre::PeintreEngine(syst, desc);
		return Common::kNoError;
	}
};

#if PLUGIN_ENABLED_DYNAMIC(PEINTRE)
REGISTER_PLUGIN_DYNAMIC(PEINTRE, PLUGIN_TYPE_ENGINE, PeintreMetaEngine);
#else
REGISTER_PLUGIN_STATIC(PEINTRE, PLUGIN_TYPE_ENGINE, PeintreMetaEngine);
#endif
