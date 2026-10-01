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

// The main menu: a parchment background (UI/001_Menu/Menu.jpg) with the item labels from
// Text.txt drawn in Grumpa.TTF. The menu actor is defined in UI/001_Menu/001_Menu.atx
// (type 31, E-0005): flags, then colours (normal 255,255,0; shadow 38,24,14; highlight
// 130,200,0), font size 56, then Splash.jpg / Menu.jpg / Grumpa.TTF / Text.txt / ... The
// item layout (positions) is CFXMenu's; we centre the list on the parchment for now.

#include "common/file.h"
#include "common/str.h"
#include "common/ustr.h"
#include "graphics/font.h"
#include "graphics/fonts/ttf.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"
#include "image/jpeg.h"

#include "common/debug.h"
#include "grumpa/grumpa.h"

namespace Grumpa {

// Text.txt line indices (Swedish/Nordic): 0 "Laddar Data...", 4..10 the main-menu items.
static const int kMenuFirst = 4, kMenuCount = 7;

static Common::U32String fromCp1252(const Common::String &s) {
	// Windows-1252 is Latin-1 for the bytes the game uses (å ä ö ...), so map 1:1.
	Common::U32String out;
	for (uint i = 0; i < s.size(); i++)
		out += (uint32)(byte)s[i];
	return out;
}

bool GrumpaEngine::loadMenuText(Common::Array<Common::U32String> &items) {
	Common::File f;
	if (!f.open(Common::Path("UI/001_Menu/Text.txt")) && !f.open(Common::Path("Local_Swedish/Text.txt")))
		return false;
	Common::Array<Common::U32String> lines;
	while (!f.eos()) {
		Common::String line = f.readLine();
		lines.push_back(fromCp1252(line));
	}
	for (int i = kMenuFirst; i < kMenuFirst + kMenuCount && i < (int)lines.size(); i++)
		items.push_back(lines[i]);
	return !items.empty();
}

bool GrumpaEngine::drawMenu(int selected) {
	Common::File bg;
	if (bg.open(Common::Path("UI/001_Menu/Menu.jpg"))) {
		Image::JPEGDecoder jpeg;
		jpeg.setOutputPixelFormat(_screen.format);
		if (jpeg.loadStream(bg)) {
			const Graphics::Surface *s = jpeg.getSurface();
			_screen.blitFrom(*s, Common::Point((kScreenWidth - s->w) / 2, (kScreenHeight - s->h) / 2));
		}
	}
	Common::Array<Common::U32String> items;
	if (!loadMenuText(items))
		return false;
	Common::File ff;
	Graphics::Font *font = nullptr;
	bool opened = ff.open(Common::Path("UI/001_Menu/Grumpa.TTF"));
	if (opened) {
		// Grumpa.TTF is a symbol font: its glyphs are at 0xF020..0xF0FF (the MS symbol PUA),
		// not Latin-1, so map each byte i to codepoint 0xF000+i (as Windows GDI does).
		uint32 mapping[256];
		for (int i = 0; i < 256; i++)
			mapping[i] = 0xF000 + i;
		Common::SeekableReadStream *buf = ff.readStream(ff.size());
		font = Graphics::loadTTFFont(buf, DisposeAfterUse::YES, 40, Graphics::kTTFSizeModeCharacter,
									 0, 0, Graphics::kTTFRenderModeLight, mapping);
	}
	if (!font)
		return false;
	uint32 normal = _screen.format.RGBToColor(255, 255, 0);
	uint32 shadow = _screen.format.RGBToColor(38, 24, 14);
	uint32 hi = _screen.format.RGBToColor(130, 200, 0);
	int lineH = font->getFontHeight() + 6;
	int total = lineH * (int)items.size();
	int y = (kScreenHeight - 120 - total) / 2 + 40;  // centred on the parchment map area
	for (uint i = 0; i < items.size(); i++) {
		uint32 col = ((int)i == selected) ? hi : normal;
		font->drawString(&_screen, items[i], 1, y + 2, kScreenWidth, shadow, Graphics::kTextAlignCenter);
		font->drawString(&_screen, items[i], 0, y, kScreenWidth, col, Graphics::kTextAlignCenter);
		y += lineH;
	}
	delete font;
	return true;
}

} // End of namespace Grumpa
