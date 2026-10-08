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

#include "common/file.h"
#include "common/system.h"

#include "graphics/cursorman.h"

#include "cryomni3d/china/engine.h"

namespace CryOmni3D {
namespace China {

// Waits up to ms, or until Escape; true when Escape ended it.
bool CryOmni3DEngine_China::waitOrEscape(uint32 ms) {
	const uint32 end = g_system->getMillis() + ms;
	while (g_system->getMillis() < end && !shouldAbort()) {
		pollEvents();
		if (checkKeysPressed(1, Common::KEYCODE_ESCAPE)) {
			return true;
		}
		g_system->updateScreen();
		g_system->delayMillis(10);
	}
	return false;
}

// The epilogue (spec/china-zones.md Endings, E-0954): four stills, each with its
// LABELS.TXT text in the subtitle band, 10 s each or until Escape.
void CryOmni3DEngine_China::epilogue() {
	static const char *const stills[4] = { "ANJFR000", "GEN_DAFR", "CONCUFR0", "JONGFR00" };
	static const char *const texts[4] = { "FIN_ANJING", "FIN_DAMING", "FIN_SHOUXIU", "FIN_PRINCE" };
	CursorMan.showMouse(false);
	for (uint i = 0; i < 4 && !shouldAbort(); i++) {
		image(stills[i]);
		drawView();
		drawSubtitle(label(texts[i]));
		g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, 640, 480);
		waitOrEscape(10000);
	}
	CursorMan.showMouse(true);
	_display = kDisplayNone;
}

// Credits (E-0205): one page every 5 s; a page ends at a line starting (or with a second
// byte) `/`; `#` lines are headings in 0xfb20, the others in white; each line is centred
// on x = 320, 20 pixels apart, the block centred on y = 240.
// Q-1001: the background (black), the font (slot 1) and Escape skipping a page are assumed.
void CryOmni3DEngine_China::credits() {
	Common::File file;
	if (!file.open("LOC/CREDITS.TXT")) {
		return;
	}
	Common::Array<Common::String> page;
	CursorMan.showMouse(false);
	_fontManager.setCurrentFont(1);
	while (!shouldAbort()) {
		const bool end = file.eos();
		Common::String line = end ? Common::String("/") : file.readLine();
		if (line.size() && line.lastChar() == '\r') {
			line.deleteLastChar();
		}
		const bool pageBreak = (line.size() > 0 && line[0] == '/') || (line.size() > 1 && line[1] == '/');
		if (!pageBreak) {
			if (page.size() < 30) {
				page.push_back(line);
			}
			continue;
		}
		if (!page.empty()) {
			_screen.clear(_format.RGBToColor(0, 0, 0));
			const int top = 240 - (int)page.size() * 20 / 2;
			for (uint i = 0; i < page.size(); i++) {
				const bool heading = page[i].hasPrefix("#");
				const Common::String text = heading ? Common::String(page[i].c_str() + 1) : page[i];
				_fontManager.setForeColor(textColor(heading ? 0xfb20 : 0xffff));
				_fontManager.displayStr(320 - _fontManager.getStrWidth(text) / 2, top + 20 * i, text);
			}
			g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, 640, 480);
			waitOrEscape(5000);
			page.clear();
		}
		// Blank lines after a break vanish (E-0205)
		while (!file.eos()) {
			const int32 pos = file.pos();
			Common::String next = file.readLine();
			next.trim();
			if (!next.empty() && next[0] != '/') {
				file.seek(pos);
				break;
			}
		}
		if (end || file.eos()) {
			break;
		}
	}
	CursorMan.showMouse(true);
	clearKeys();
}

} // End of namespace China
} // End of namespace CryOmni3D
