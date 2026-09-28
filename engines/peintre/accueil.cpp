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
#include "common/formats/winexe_pe.h"
#include "common/ptr.h"
#include "common/system.h"

#include "graphics/cursorman.h"
#include "graphics/font.h"
#include "graphics/fonts/ttf.h"
#include "graphics/wincursor.h"

#include "peintre/peintre.h"

namespace Peintre {

// The player-name screen (ui.md "Player-name screen", E-0402, E-0403). The original is a
// desktop-sized popup with the 640x480 bitmap centred; here the bitmap is the screen.

static uint16 rgb(byte r, byte g, byte b) {
	return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

/** Reads an RT_BITMAP resource (a DIB without file header) of mission.___ as RGB565. */
static bool loadBitmapResource(Common::PEResources &exe, const char *name, Graphics::Surface &out) {
	Common::ScopedPtr<Common::SeekableReadStream> s(exe.getResource(Common::kWinBitmap, Common::WinResourceID(name)));
	if (!s)
		return false;
	const uint32 hdr = s->readUint32LE();
	const int32 w = s->readSint32LE();
	int32 h = s->readSint32LE();
	s->readUint16LE();
	const uint16 bpp = s->readUint16LE();
	const uint32 compression = s->readUint32LE();
	s->skip(12);
	uint32 colours = s->readUint32LE();
	if (bpp != 8 || compression != 0 || w <= 0)
		return false;
	s->seek(hdr);
	if (colours == 0)
		colours = 256;
	uint16 pal[256];
	memset(pal, 0, sizeof(pal));
	for (uint32 i = 0; i < colours && i < 256; i++) {
		const byte b = s->readByte(), g = s->readByte(), r = s->readByte();
		s->readByte();
		pal[i] = rgb(r, g, b);
	}
	const bool bottomUp = h > 0;
	h = ABS(h);
	out.create(w, h, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));
	const uint32 stride = (w + 3) & ~3;
	Common::Array<byte> row(stride);
	for (int32 y = 0; y < h; y++) {
		s->read(row.data(), stride);
		uint16 *dst = (uint16 *)out.getBasePtr(0, bottomUp ? h - 1 - y : y);
		for (int32 x = 0; x < w; x++)
			dst[x] = pal[row[x]];
	}
	return true;
}

// Windows' I-beam, shown over the edit (E-0445); white, as the original's inverts the
// dark field.
static const char *const kIBeam[] = {
	"XXX.XXX", "...X...", "...X...", "...X...", "...X...", "...X...", "...X...", "...X...",
	"...X...", "...X...", "...X...", "...X...", "...X...", "...X...", "...X...", "XXX.XXX"
};

static void setCursor(bool ibeam) {
	if (!ibeam) {
		Graphics::Cursor *arrow = Graphics::makeDefaultWinCursor();
		CursorMan.replaceCursor(arrow);
		delete arrow;
		return;
	}
	uint16 pixels[16 * 7];
	for (int y = 0; y < 16; y++)
		for (int x = 0; x < 7; x++)
			pixels[y * 7 + x] = kIBeam[y][x] == 'X' ? 0xFFFF : 0;
	const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
	CursorMan.replaceCursor(pixels, 7, 16, 3, 8, 0, &format);
}

static void fill(Graphics::Surface &dst, const Common::Rect &r, uint16 colour) {
	Common::Rect c(r);
	c.clip(Common::Rect(dst.w, dst.h));
	dst.fillRect(c, colour);
}

bool PeintreEngine::runPlayerScreen(uint &player, bool &known) {
	Common::PEResources exe;
	Graphics::Surface background, background2, buttons;
	if (!exe.loadFromEXE(Common::Path("mission.___")) ||
		!loadBitmapResource(exe, "ACCUEIL_BMP", background) ||
		!loadBitmapResource(exe, "ACCUEIL2_BMP", background2) ||
		!loadBitmapResource(exe, "BOUTONS_BMP", buttons)) {
		warning("Cannot read the player screen's bitmaps from mission.___");
		return false;
	}
	// The font "Trobo" is FONTS\TROBO.TTF, drawn 16 px high.
	Common::ScopedPtr<Graphics::Font> font;
	Common::File *ttf = new Common::File();
	if (ttf->open("FONTS/TROBO.TTF"))
		font.reset(Graphics::loadTTFFont(ttf, DisposeAfterUse::YES, 16, Graphics::kTTFSizeModeCell));
	else
		delete ttf;

	const uint16 kFieldBack = rgb(0, 84, 120), kWhite = rgb(255, 255, 255), kNameText = rgb(181, 198, 214);
	const Common::Rect kField(344, 206, 344 + 163, 206 + 26);
	const Common::Rect kOk(407, 396, 407 + 36, 396 + 23), kQuit(468, 396, 468 + 72, 396 + 27);
	auto playerRect = [](uint i) { return Common::Rect(90, 217 + 34 * i, 90 + 162, 217 + 34 * i + 27); };

	// The field is a Windows EDIT (E-0445): centred text, opening as "player's name", all
	// selected; the selection runs from `anchor` to the caret.
	Common::String field = "player's name";
	uint anchor = 0, caret = field.size();
	bool selecting = false;   // a press in the field, dragging the selection
	uint32 caretMoved = _system->getMillis();
	auto textWidth = [&](const Common::String &t) { return font ? font->getStringWidth(t) : 0; };
	auto textLeft = [&]() { return kField.left + (kField.width() - textWidth(field)) / 2; };
	auto prefix = [&](uint n) { return Common::String(field.c_str(), n); };
	auto charAt = [&](int x) {
		uint best = 0;
		for (uint i = 1; i <= field.size(); i++)
			if (ABS(x - textLeft() - textWidth(prefix(i))) < ABS(x - textLeft() - textWidth(prefix(best))))
				best = i;
		return best;
	};
	auto replaceSelection = [&](const Common::String &with) {
		const uint lo = MIN(anchor, caret), hi = MAX(anchor, caret);
		if (field.size() - (hi - lo) + with.size() > 20)
			return;
		field = prefix(lo) + with + (field.c_str() + hi);
		anchor = caret = lo + with.size();
	};
	struct HideCursor {
		~HideCursor() { CursorMan.showMouse(false); }
	} hideCursor;
	int shownCursor = -1;

	bool replacing = false;   // five players and a new name: pick the slot to replace
	uint selected = 0;
	int pressed = -1;         // control id being pressed: 0 OK, 1 Quit, 2 + i player i
	bool wasDown = false;

	while (!shouldQuit()) {
		pollInput();
		const Common::Point m = mouse();
		const bool down = buttonDown();
		int action = -1;
		if (down && !wasDown) {
			if (!replacing && kField.contains(m)) {
				anchor = caret = charAt(m.x);
				selecting = true;
				caretMoved = _system->getMillis();
			} else if (kOk.contains(m))
				pressed = 0;
			else if (kQuit.contains(m))
				pressed = 1;
			else
				for (uint i = 0; i < _players.size(); i++)
					if (playerRect(i).contains(m))
						pressed = 2 + i;
		} else if (!down && wasDown && pressed >= 0) {
			// A button acts when released over it.
			const Common::Rect r = pressed == 0 ? kOk : pressed == 1 ? kQuit : playerRect(pressed - 2);
			if (r.contains(m))
				action = pressed;
			pressed = -1;
		}
		if (selecting && down && caret != charAt(m.x)) {
			caret = charAt(m.x);
			caretMoved = _system->getMillis();
		}
		if (!down)
			selecting = false;
		wasDown = down;
		if (keyFired(Common::KEYCODE_RETURN) || keyFired(Common::KEYCODE_KP_ENTER))
			action = 0;
		if (keyFired(Common::KEYCODE_ESCAPE))
			action = 1;
		if (!replacing) {
			for (uint k = 0; k < typed().size(); k++) {
				const char c = typed()[k];
				caretMoved = _system->getMillis();
				// Backspace and Delete take the selection, else the character beside the caret
				if (c == '\b' && anchor == caret && caret > 0)
					anchor = caret - 1;
				else if (c == kTypedDelete && anchor == caret && caret < field.size())
					anchor = caret + 1;
				if (c == '\b' || c == kTypedDelete)
					replaceSelection("");
				else if (c == kTypedLeft)
					anchor = caret = anchor != caret ? MIN(anchor, caret) : caret - (caret > 0);
				else if (c == kTypedRight)
					anchor = caret = anchor != caret ? MAX(anchor, caret) : caret + (caret < field.size());
				else if (c == kTypedHome)
					anchor = caret = 0;
				else if (c == kTypedEnd)
					anchor = caret = field.size();
				else if ((byte)c >= 32)
					replaceSelection(Common::String(c));
			}
		}

		if (action == 1)
			return false;
		if (action >= 2) {
			if (replacing)
				selected = action - 2;
			else {
				field = _players[action - 2].name;
				anchor = 0;
				caret = field.size();
			}
		}
		if (action == 0 && !field.empty()) {
			if (replacing) {
				player = selected;
				_players[selected].name = field;
				_players[selected].volume = 0;
				_players[selected].viewSize = 0;
				known = false;
				return true;
			}
			for (uint i = 0; i < _players.size(); i++) {
				if (_players[i].name.equalsIgnoreCase(field)) {
					player = i;
					known = true;
					return true;
				}
			}
			if (_players.size() < kMaxPlayers) {
				PlayerRecord p;
				p.name = field;
				_players.push_back(p);
				player = _players.size() - 1;
				known = false;
				return true;
			}
			replacing = true;
			selected = 0;
		}

		// Draw.
		_screen.copyRectToSurface(replacing ? background2 : background, 0, 0, Common::Rect(640, 480));
		if (!replacing) {
			fill(_screen, kField, kFieldBack);
			const int x = textLeft(), top = kField.top + 3, bottom = top + (font ? font->getFontHeight() : 16);
			const uint lo = MIN(anchor, caret), hi = MAX(anchor, caret);
			// The selection in the system highlight colour: Windows' classic navy
			if (lo != hi)
				fill(_screen, Common::Rect(x + textWidth(prefix(lo)), top, x + textWidth(prefix(hi)), bottom), rgb(0, 0, 128));
			if (font)
				font->drawString(&_screen, field, x, top, kField.width(), kWhite);
			// The caret blinks at Windows' 530 ms, shown at once after a move
			if (lo == hi && (_system->getMillis() - caretMoved) % 1060 < 530) {
				const int cx = x + textWidth(prefix(caret));
				fill(_screen, Common::Rect(cx, top, cx + 1, bottom), kWhite);
			}
		}
		const int want = !replacing && kField.contains(m) ? 1 : 0;
		if (want != shownCursor) {
			setCursor(want);
			shownCursor = want;
		}
		CursorMan.showMouse(true);
		for (uint i = 0; i < _players.size(); i++) {
			const Common::Rect r = playerRect(i);
			fill(_screen, r, kFieldBack);
			const bool white = (int)(2 + i) == pressed || (replacing && i == selected);
			if (font)
				font->drawString(&_screen, _players[i].name, r.left + 2, r.top + 2, r.width() - 4, white ? kWhite : kNameText);
		}
		_screen.copyRectToSurface(buttons, kOk.left, kOk.top,
			Common::Rect(0, pressed == 0 ? 28 : 0, 36, (pressed == 0 ? 28 : 0) + 23));
		_screen.copyRectToSurface(buttons, kQuit.left, kQuit.top,
			Common::Rect(40, pressed == 1 ? 28 : 0, 40 + 72, (pressed == 1 ? 28 : 0) + 27));
		present();
		waitTick(20);
	}
	return false;
}

} // End of namespace Peintre
