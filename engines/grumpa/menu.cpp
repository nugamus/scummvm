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

#include "common/debug.h"
#include "common/events.h"
#include "common/file.h"
#include "common/rect.h"
#include "common/str.h"
#include "common/system.h"
#include "common/ustr.h"
#include "graphics/font.h"
#include "graphics/fonts/ttf.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"
#include "image/jpeg.h"

#include "grumpa/grumpa.h"

namespace Grumpa {

// Text.txt line indices (Swedish/Nordic): 0 "Laddar Data...", 4..10 the main-menu items.
static const int kMenuFirst = 4, kMenuCount = 7;

Common::U32String GrumpaEngine::fromCp1252(const Common::String &s) {
	// Windows-1252 is Latin-1 for the bytes the game uses (å ä ö ...), so map 1:1.
	Common::U32String out;
	for (uint i = 0; i < s.size(); i++)
		out += (uint32)(byte)s[i];
	return out;
}

// Grumpa.TTF is a symbol font: its glyphs are at 0xF020..0xF0FF (the MS symbol PUA), not
// Latin-1, so map each byte i to codepoint 0xF000+i (as Windows GDI does). Cached.
const Graphics::Font *GrumpaEngine::menuFont(int size) {
	if (_menuFont && _menuFontSize == size)
		return _menuFont;
	delete _menuFont;
	_menuFont = nullptr;
	_menuFontSize = size;
	Common::File ff;
	if (!ff.open(Common::Path("UI/001_Menu/Grumpa.TTF")))
		return nullptr;
	uint32 mapping[256];
	for (int i = 0; i < 256; i++)
		mapping[i] = 0xF000 + i;
	Common::SeekableReadStream *buf = ff.readStream(ff.size());
	_menuFont = Graphics::loadTTFFont(buf, DisposeAfterUse::YES, size, Graphics::kTTFSizeModeCharacter,
									  0, 0, Graphics::kTTFRenderModeLight, mapping);
	return _menuFont;
}

bool GrumpaEngine::loadTextFile(const Common::String &rel, Common::Array<Common::U32String> &lines) {
	Common::File f;
	// The language's texts (Local_<language>/) before the menu folder's Swedish copies (E-1770).
	const Common::String menuDir = "UI/001_Menu/";
	if (rel.hasPrefix(menuDir))
		f.open(Common::Path(Common::String("Local_") + _langFolder + "/" + rel.substr(menuDir.size())));
	if (!f.isOpen() && !f.open(Common::Path(rel)))
		return false;
	while (!f.eos())
		lines.push_back(fromCp1252(f.readLine()));
	return true;
}

bool GrumpaEngine::loadMenuText(Common::Array<Common::U32String> &items) {
	Common::Array<Common::U32String> lines;
	if (!loadTextFile("UI/001_Menu/Text.txt", lines))
		return false;
	for (int i = kMenuFirst; i < kMenuFirst + kMenuCount && i < (int)lines.size(); i++)
		items.push_back(lines[i]);
	return !items.empty();
}

static void drawBackground(Graphics::ManagedSurface &screen, const Common::String &jpg) {
	Common::File bg;
	if (!bg.open(Common::Path(jpg)))
		return;
	Image::JPEGDecoder jpeg;
	jpeg.setOutputPixelFormat(screen.format);
	if (jpeg.loadStream(bg)) {
		const Graphics::Surface *s = jpeg.getSurface();
		screen.blitFrom(*s, Common::Point((screen.w - s->w) / 2, (screen.h - s->h) / 2));
	}
}

bool GrumpaEngine::drawMenu(int selected) {
	drawBackground(_screen, "UI/001_Menu/Menu.jpg");
	Common::Array<Common::U32String> items;
	if (!loadMenuText(items))
		return false;
	const Graphics::Font *font = menuFont(40);
	if (!font)
		return false;
	uint32 normal = _screen.format.RGBToColor(255, 255, 0);
	uint32 shadow = _screen.format.RGBToColor(38, 24, 14);
	uint32 hi = _screen.format.RGBToColor(130, 200, 0);
	int lineH = font->getFontHeight() + 6;
	int total = lineH * (int)items.size();
	int y = (kScreenHeight - 120 - total) / 2 + 40;  // centred on the parchment map area
	_menuRects.clear();
	for (uint i = 0; i < items.size(); i++) {
		int wdt = font->getStringWidth(items[i]);
		_menuRects.push_back(Common::Rect((kScreenWidth - wdt) / 2, y, (kScreenWidth + wdt) / 2, y + font->getFontHeight()));
		uint32 col = ((int)i == selected) ? hi : normal;
		font->drawString(&_screen, items[i], 1, y + 2, kScreenWidth, shadow, Graphics::kTextAlignCenter);
		font->drawString(&_screen, items[i], 0, y, kScreenWidth, col, Graphics::kTextAlignCenter);
		y += lineH;
	}
	return true;
}

int GrumpaEngine::menuItemAt(const Common::Point &p) const {
	for (uint i = 0; i < _menuRects.size(); i++)
		if (_menuRects[i].contains(p))
			return (int)i;
	return -1;
}

// A scrolling text screen (Credits "Medverkande", Help "Hjälp") over the parchment.
void GrumpaEngine::showTextScreen(const Common::String &textFile) {
	Common::Array<Common::U32String> lines;
	loadTextFile(textFile, lines);
	const Graphics::Font *font = menuFont(28);
	int lineH = font ? font->getFontHeight() + 4 : 30;
	int scroll = 0;
	int maxScroll = MAX(0, (int)lines.size() * lineH - (kScreenHeight - 160));
	bool done = false;
	while (!done && !shouldQuit()) {
		Common::Event e;
		while (g_system->getEventManager()->pollEvent(e)) {
			if (e.type == Common::EVENT_KEYDOWN) {
				if (e.kbd.keycode == Common::KEYCODE_ESCAPE || e.kbd.keycode == Common::KEYCODE_RETURN)
					done = true;
				else if (e.kbd.keycode == Common::KEYCODE_DOWN)
					scroll = MIN(scroll + lineH, maxScroll);
				else if (e.kbd.keycode == Common::KEYCODE_UP)
					scroll = MAX(scroll - lineH, 0);
			} else if (e.type == Common::EVENT_LBUTTONUP) {
				done = true;
			} else if (e.type == Common::EVENT_WHEELUP) {
				scroll = MAX(scroll - lineH, 0);
			} else if (e.type == Common::EVENT_WHEELDOWN) {
				scroll = MIN(scroll + lineH, maxScroll);
			}
		}
		drawBackground(_screen, "UI/001_Menu/Menu.jpg");
		if (font) {
			uint32 col = _screen.format.RGBToColor(255, 255, 0);
			uint32 sh = _screen.format.RGBToColor(38, 24, 14);
			int y = 90 - scroll;
			for (uint i = 0; i < lines.size(); i++, y += lineH) {
				if (y < 70 || y > kScreenHeight - 80)
					continue;
				font->drawString(&_screen, lines[i], 1, y + 1, kScreenWidth, sh, Graphics::kTextAlignCenter);
				font->drawString(&_screen, lines[i], 0, y, kScreenWidth, col, Graphics::kTextAlignCenter);
			}
		}
		g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, kScreenWidth, kScreenHeight);
		g_system->updateScreen();
		g_system->delayMillis(10);
	}
}

} // End of namespace Grumpa
