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

#include "monet/metaengine.h"
#include "monet/detection.h"
#include "monet/monet.h"

const char *MonetMetaEngine::getName() const {
	return "monet";
}

Common::Error MonetMetaEngine::createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const {
	*engine = new Monet::MonetEngine(syst, desc);
	return Common::kNoError;
}

#if PLUGIN_ENABLED_DYNAMIC(MONET)
REGISTER_PLUGIN_DYNAMIC(MONET, PLUGIN_TYPE_ENGINE, MonetMetaEngine);
#else
REGISTER_PLUGIN_STATIC(MONET, PLUGIN_TYPE_ENGINE, MonetMetaEngine);
#endif
