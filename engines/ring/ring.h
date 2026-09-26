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

namespace Graphics {
class WinFont;
}

namespace Ring {

class Cursors;
class Resources;
class World;

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
	World &world() { return *_world; }
	Cursors &cursors() { return *_cursors; }

	/** `PuzSetAct` (games/ring/docs/sy.md): the puzzle becomes the current one. */
	void puzSetAct(int puzzle);
	/** `PuzSetMod`: refused (false) when mode 2 is asked of a puzzle already in mode 2. */
	bool puzSetMod(int puzzle, int mode, int object);
	/** `StartMenu` (sy.md, "Flow"). */
	void startMenu(bool fromGame);
	/** What `WM_CLOSE` does: the exit dialogue (spec/boot.md, "Input"). */
	void requestClose();

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

	/** `GetMultiLanMes` (spec/text.md): loads the key's title and text from aMes.ini. */
	void message(const char *key);
	const Common::String &messageTitle() const { return _messageTitle; }
	const Common::String &messageText() const { return _messageText; }

private:
	void showStartupScreens();
	void addCursors();
	/** One idle-loop frame (spec/boot.md, "Frame"). */
	void frame();
	/** Hot-spot tracking, 0x408dd0 (spec/cursor.md). */
	void track(int x, int y);
	/** `MouseLeftEvent` (spec/cursor.md). */
	void click(int x, int y);

	const ADGameDescription *_gameDescription;
	Graphics::ManagedSurface _screen;
	Common::ScopedPtr<Resources> _resources;
	Common::ScopedPtr<World> _world;
	Common::ScopedPtr<Cursors> _cursors;
	Common::ScopedPtr<Graphics::WinFont> _font;
	Common::String _messageTitle, _messageText;
	int _zone = 1;
	int _menuZone = 0;   ///< app+0x6f: the zone the menu returns to, 0 when the menu is down
	int _puzzle = 0;     ///< the current puzzle (app+0x81)
	Common::Point _mouse;
	bool _clicked = false; ///< a left button press at _clickPos not handled yet
	Common::Point _clickPos;
	Common::String _languageFolder;
	bool _escapeDown = false;
};

} // End of namespace Ring

#endif // RING_RING_H
