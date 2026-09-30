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
#include "common/debug.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/system.h"
#include "graphics/pixelformat.h"

#include "engines/util.h"

#include "grumpa/grumpa.h"

namespace Grumpa {

GrumpaEngine::GrumpaEngine(OSystem *syst, const ADGameDescription *gameDesc)
	: Engine(syst), _gameDesc(gameDesc) {
}

GrumpaEngine::~GrumpaEngine() {
}

Common::Error GrumpaEngine::run() {
	// The original draws to one 800x600 16-bit (RGB555) page; use that format (E-0010).
	// Scene rendering (2D background + .fxi depth + 3D actors) comes once the scene formats
	// are specced (games/grumpa docs, Q-0005..Q-0007).
	Graphics::PixelFormat format = Graphics::PixelFormat(2, 5, 5, 5, 0, 10, 5, 0, 0);
	initGraphics(kScreenWidth, kScreenHeight, &format);
	_screen.create(kScreenWidth, kScreenHeight, g_system->getScreenFormat());

	debug(1, "Grumpa: booted, %dx%d %d-bit surface", kScreenWidth, kScreenHeight,
		  _screen.format.bytesPerPixel * 8);

	// The game data sits in subdirectories (Bitmaps, Actors, Scenes, ...) of the game dir.
	SearchMan.addDirectory("gamedir", ConfMan.getPath("path"), 0, 2);

	_screen.clear();
	while (!shouldQuit()) {
		Common::Event event;
		while (g_system->getEventManager()->pollEvent(event)) {
			// Engine::shouldQuit() picks up quit/return-to-launcher events.
		}
		g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0,
								   kScreenWidth, kScreenHeight);
		g_system->updateScreen();
		g_system->delayMillis(10);
	}
	return Common::kNoError;
}

} // End of namespace Grumpa
