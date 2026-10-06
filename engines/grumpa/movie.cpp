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

// MPEG-1 film playback (intro, death, outro): the films are standard MPEG-1 system streams
// (E-0007), played with ScummVM's MPEGPSDecoder (needs the mpeg2 and mad components). A film
// is scaled to the 800x600 page. The boot intro skips on Escape or a click; a cut scene
// (type 0x07, E-1806) only on Space, with the game stopped, and leaves the screen black.

#include "common/events.h"
#include "common/file.h"
#include "common/system.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"
#include "video/mpegps_decoder.h"

#include "grumpa/grumpa.h"

namespace Grumpa {

bool GrumpaEngine::playMovie(const Common::String &film, bool cutScene) {
	Common::String name = film;
	if (name.hasSuffixIgnoreCase(".mpg"))
		name = Common::String(name.c_str(), name.size() - 4);
	Common::File *f = new Common::File();
	bool opened = false;
	// The CD holds the Swedish intro and the shared films in Movies/, the other languages'
	// intros in the cabinet (E-1000, E-1770).
	const Common::String dirs[] = { Common::String("Movies_") + _langFolder + "/", "Movies/", "" };
	for (uint i = cutScene ? 1 : 0; i < (cutScene ? 2 : ARRAYSIZE(dirs)); i++) {  // cut scenes: Movies/ only
		if (f->open(Common::Path(dirs[i] + name + ".mpg"))) {
			opened = true;
			break;
		}
	}
	if (!opened) {
		delete f;
		debug(1, "Grumpa: film %s not found", name.c_str());
		return false;
	}
	Video::MPEGPSDecoder dec;
	if (!dec.loadStream(f)) {   // the decoder takes ownership of the stream
		debug(1, "Grumpa: could not open film %s", name.c_str());
		return false;
	}
	dec.start();
	bool skipped = false;
	int frames = 0;
	while (!dec.endOfVideo() && !skipped && !shouldQuit()) {
		Common::Event event;
		while (g_system->getEventManager()->pollEvent(event)) {
			bool skip = cutScene
				? event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_SPACE
				: (event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_ESCAPE)
				  || event.type == Common::EVENT_LBUTTONUP;
			skipped |= skip;
		}
		if (dec.needsUpdate()) {
			const Graphics::Surface *frame = dec.decodeNextFrame();
			if (frame) {
				frames++;
				Graphics::Surface *conv = frame->convertTo(_screen.format);
				int x = (kScreenWidth - conv->w) / 2, y = (kScreenHeight - conv->h) / 2;
				if (conv->w == kScreenWidth && conv->h == kScreenHeight)
					_screen.blitFrom(*conv);
				else {
					_screen.clear();
					// Nearest-neighbour scale to fill the page keeping aspect.
					float s = MIN((float)kScreenWidth / conv->w, (float)kScreenHeight / conv->h);
					int dw = (int)(conv->w * s), dh = (int)(conv->h * s);
					Graphics::Surface *scaled = conv->scale(dw, dh, false);
					_screen.blitFrom(*scaled, Common::Point((kScreenWidth - dw) / 2, (kScreenHeight - dh) / 2));
					scaled->free();
					delete scaled;
					(void)x; (void)y;
				}
				conv->free();
				delete conv;
			}
		}
		g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, kScreenWidth, kScreenHeight);
		g_system->updateScreen();
		g_system->delayMillis(10);
	}
	debug(1, "Grumpa: film %s: %d frames%s", name.c_str(), frames, skipped ? ", skipped" : "");
	if (cutScene) {  // the back surface is filled black (E-1806)
		_screen.clear();
		g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, kScreenWidth, kScreenHeight);
		g_system->updateScreen();
	}
	return true;
}

} // End of namespace Grumpa
