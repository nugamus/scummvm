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

#ifndef RING_RING_H
#define RING_RING_H

#include "common/ptr.h"
#include "common/rect.h"
#include "common/str.h"

#include "engines/advancedDetector.h"
#include "engines/engine.h"

#include "graphics/managed_surface.h"

namespace Ring {

class Resources;

/**
 * The Ring engine (Arxel Tribe, 1999). Behaviour follows the specs in the research
 * repository (engines/ring/docs/spec/): boot.md for the start-up, video.md, drawing.md,
 * resources.md.
 */
class RingEngine : public Engine {
public:
	RingEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~RingEngine() override;

	Common::Error run() override;
	bool hasFeature(EngineFeature f) const override;

	Graphics::ManagedSurface &screen() { return _screen; }
	Resources &resources() { return *_resources; }

	/** Copies the screen to the backend and updates it. */
	void present();
	/** Handles pending events, then sleeps `ms`. */
	void pollEvents(uint32 ms = 0);
	/** True while Escape is held (the original polls GetAsyncKeyState(VK_ESCAPE)). */
	bool escapePressed() const { return _escapeDown; }
	/** Waits for `ms` milliseconds or until Escape is held (0x402890). */
	void wait(uint32 ms);

	/** `DisFad` (spec/video.md): fades picture `from` into `to` over `frames` at 25 fps, holds `holdMs`. */
	bool fade(const Common::String &from, const Common::String &to, uint frames, uint32 holdMs);

	const Common::String &languageFolder() const { return _languageFolder; }

private:
	void showStartupScreens();

	const ADGameDescription *_gameDescription;
	Graphics::ManagedSurface _screen;
	Common::ScopedPtr<Resources> _resources;
	Common::String _languageFolder;
	bool _escapeDown = false;
};

} // End of namespace Ring

#endif // RING_RING_H
