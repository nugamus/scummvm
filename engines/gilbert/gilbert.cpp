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

#include "common/config-manager.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/system.h"

#include "engines/util.h"

#include "graphics/pixelformat.h"

#include "gilbert/gilbert.h"

namespace Gilbert {

GilbertEngine::GilbertEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst), _gameDesc(gameDesc) {
	// The game reads everything below Data (boot.md).
	const Common::FSNode gameDir(ConfMan.getPath("path"));
	SearchMan.addSubDirectoryMatching(gameDir, "data", 0, 4);
}

Common::Error GilbertEngine::run() {
	// One 640x480 16-bit page (E-0004).
	const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
	initGraphics(640, 480, &format);

	while (!shouldQuit()) {
		Common::Event event;
		while (_eventMan->pollEvent(event)) {
		}
		_system->updateScreen();
		_system->delayMillis(10);
	}
	return Common::kNoError;
}

} // End of namespace Gilbert
