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
#include "common/textconsole.h"

#include "graphics/font.h"
#include "graphics/fontman.h"
#include "graphics/surface.h"

#include "x3d/frame.h"
#include "x3d/renderer.h"

namespace X3D {

static Graphics::Surface *bitmap(const Common::String &name) {
	if (name.empty() || name == "0")
		return nullptr;
	// Data/2DBIT/<name>, ".bmp" added when the name has no '.'
	Graphics::Surface *s = loadBitmap(Common::Path("2dbit/" + (name.contains('.') ? name : name + ".bmp")));
	if (!s)
		warning("Missing bitmap 2dbit/%s", name.c_str());
	return s;
}

Frame::~Frame() {
	for (View &v : _views) {
		for (Graphics::Surface *s : { v.bitmap, v.hover }) {
			if (s) {
				s->free();
				delete s;
			}
		}
	}
}

bool Frame::load(const Common::String &name) {
	Common::File f;
	if (!f.open(Common::Path("2DFRA/" + name + ".fra"))) {
		warning("Missing frame %s", name.c_str());
		return false;
	}
	const uint32 count = f.readUint32LE();
	for (uint32 i = 0; i < count && !f.err(); i++) {
		View v;
		v.tag = f.readString(0, 4);
		v.id = f.readSint32LE();
		v.x = f.readSint32LE();
		v.y = f.readSint32LE();
		v.w = f.readSint32LE();
		v.h = f.readSint32LE();
		v.visible = f.readSint32LE() != 0;
		v.parent = f.readSint32LE();
		f.skip(8);

		// Class bodies (fra.ksy)
		if (v.tag == "TIB#" || v.tag == "POL#") {
			v.bitmapDx = f.readSint32LE();
			v.bitmapDy = f.readSint32LE();
			v.bitmap = bitmap(f.readString(0, 32));
			f.skip(v.tag == "POL#" ? 12 : 4);
			if (v.bitmap && !v.w)
				v.w = v.bitmap->w + v.bitmapDx;
			if (v.bitmap && !v.h)
				v.h = v.bitmap->h + v.bitmapDy;
		} else if (v.tag == "RCS#" || v.tag == "AOL#" || v.tag == "VAS#" || v.tag == "cSU#") {
			f.skip(108); // the scroll bar's bitmaps (Q-0061)
			v.list = v.tag != "RCS#";
		} else if (v.tag == "loV#" || v.tag == "BoV#" || v.tag == "AoV#" || v.tag == "nCC#" || v.tag == "nIC#") {
			f.skip(36);
		} else if (v.tag == "dEU#" || v.tag == "dES#" || v.tag == "idE#") {
			v.edit = true;
			v.maxLength = v.tag == "dES#" ? 40 : 30;
		}

		// Properties until tag 0
		for (;;) {
			const uint32 tag = f.readUint32LE();
			if (!tag || f.err())
				break;
			switch (tag) {
			case MKTAG('@', 'g', 'c', 'u'): // ucg@
				v.cursor = f.readUint32LE();
				break;
			case MKTAG('@', 'S', 'C', 'R'): // RCS@
				v.command = f.readString(0, 32);
				break;
			case MKTAG('@', 'H', 'I', 'L'): // LIH@
			case MKTAG('@', 'H', 'I', 'G'): { // GIH@ starts disabled
				Graphics::Surface *s = bitmap(f.readString(0, 32));
				v.hoverDx = f.readSint32LE();
				v.hoverDy = f.readSint32LE();
				if (tag == MKTAG('@', 'H', 'I', 'L'))
					v.hover = s;
				else if (s) {
					s->free();
					delete s;
				}
				break;
			}
			case MKTAG('@', 'C', 'U', 'R'): // RUC@
			case MKTAG('@', 'D', 'R', 'A'): // ARD@
				f.skip(4);
				break;
			case MKTAG('@', 'F', 'R', 'A'): // ARF@
				f.skip(32);
				break;
			case MKTAG('@', 'A', 'N', 'I'): // INA@
				f.skip(56);
				break;
			default:
				warning("Frame %s: unknown property %08x", name.c_str(), tag);
				return false;
			}
		}
		_views.push_back(v);
	}
	return !f.err();
}

int Frame::viewAt(const Common::Point &p) const {
	for (int i = _views.size() - 1; i >= 0; i--) {
		const View &v = _views[i];
		if (v.visible && Common::Rect(v.x, v.y, v.x + v.w, v.y + v.h).contains(p))
			return i;
	}
	return -1;
}

int Frame::cursorAt(int view) const {
	return view >= 0 ? _views[view].cursor : -1;
}

const Common::String &Frame::commandAt(int view) const {
	static const Common::String none;
	return view >= 0 ? _views[view].command : none;
}

int Frame::editView() const {
	for (uint i = 0; i < _views.size(); i++)
		if (_views[i].edit)
			return i;
	return -1;
}

void Frame::type(char c) {
	const int e = editView();
	if (e < 0)
		return;
	View &v = _views[e];
	if (v.placeholder) {
		v.text.clear();
		v.placeholder = false;
	}
	if (v.text.size() < v.maxLength)
		v.text += c;
}

void Frame::backspace() {
	const int e = editView();
	if (e >= 0 && !_views[e].text.empty()) {
		_views[e].text.deleteLastChar();
		_views[e].placeholder = false;
	}
}

void Frame::setText(const Common::String &text, bool placeholder) {
	const int e = editView();
	if (e >= 0) {
		_views[e].text = text;
		_views[e].placeholder = placeholder;
	}
}

int Frame::listView() const {
	for (uint i = 0; i < _views.size(); i++)
		if (_views[i].list)
			return i;
	return -1;
}

void Frame::setList(const Common::Array<Common::String> &rows, int selected) {
	_rows = rows;
	_selected = selected;
}

int Frame::listRowAt(const Common::Point &p) const {
	const int l = listView();
	if (l < 0)
		return -1;
	const View &v = _views[l];
	if (!Common::Rect(v.x, v.y, v.x + v.w - 38, v.y + v.h).contains(p))
		return -1;
	// ponytail: no scrolling (the scroll bar is Q-0061); rows beyond the view are not shown
	const int row = (p.y - v.y) / 32;
	return row < (int)_rows.size() ? row : -1;
}

Common::String Frame::text() const {
	const int e = editView();
	return e >= 0 && !_views[e].placeholder ? _views[e].text : Common::String();
}

void Frame::draw(Renderer &r, int xOffset, int hovered) {
	for (uint i = 0; i < _views.size(); i++) {
		View &v = _views[i];
		if (!v.visible)
			continue;
		if (v.bitmap)
			r.drawImage(*v.bitmap, xOffset + v.x + v.bitmapDx, v.y + v.bitmapDy, false);
		if ((int)i == hovered && v.hover)
			r.drawImage(*v.hover, xOffset + v.x + v.hoverDx, v.y + v.hoverDy, false);
		if (v.edit) {
			// The original draws edits with GDI Arial 12 pt (ui.md, Text): a sans-serif here
			if (v.text.empty() && !v.placeholder && v.tag == "dEU#") {
				v.text = "Player's name";
				v.placeholder = true;
			}
			const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
			Graphics::Surface s;
			s.create(v.w, v.h, Graphics::PixelFormat::createFormatRGBA32());
			s.fillRect(Common::Rect(v.w, v.h), s.format.ARGBToColor(255, 31, 33, 80));
			font->drawString(&s, v.text, 2, (v.h - font->getFontHeight()) / 2, v.w - 4, s.format.ARGBToColor(255, 255, 255, 255));
			r.drawImage(s, xOffset + v.x, v.y, false);
			s.free();
		}
		if (v.list && !_rows.empty()) {
			// Rows centred in (0, 32 row, w - 38, 32), keyed over the frame (save.md, Lists)
			const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
			Graphics::Surface s;
			s.create(v.w, v.h, Graphics::PixelFormat::createFormatRGBA32());
			s.fillRect(Common::Rect(v.w, v.h), s.format.ARGBToColor(255, 255, 255, 255));
			for (int row = 0; row < (int)_rows.size() && (row + 1) * 32 <= v.h; row++) {
				const uint32 color = row == _selected ? s.format.ARGBToColor(255, 247, 196, 90) : s.format.ARGBToColor(255, 135, 186, 235);
				font->drawString(&s, _rows[row], 0, row * 32 + (32 - font->getFontHeight()) / 2, v.w - 38, color, Graphics::kTextAlignCenter);
			}
			r.drawImage(s, xOffset + v.x, v.y, true);
			s.free();
		}
	}
}

} // End of namespace X3D
