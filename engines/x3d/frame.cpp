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
#include "common/textconsole.h"

#include "graphics/font.h"
#include "graphics/fontman.h"
#include "graphics/fonts/ttf.h"
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

// The original draws edits and lists with GDI Arial 12 pt at 96 dpi, 16 px characters
// (ui.md, Text): Liberation Sans has Arial's metrics; the GUI font without FreeType
const Graphics::Font *Frame::textFont() const {
#ifdef USE_FREETYPE2
	if (!_font)
		_font.reset(Graphics::loadTTFFontFromArchive("LiberationSans-Regular.ttf", 12, Graphics::kTTFSizeModeCharacter, 96, 96));
	if (_font)
		return _font.get();
#endif
	return FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
}

int Frame::textWidth(const Common::String &text, int chars) const {
	return textFont()->getStringWidth(Common::String(text.c_str(), chars));
}

Frame::~Frame() {
	for (View &v : _views) {
		for (Graphics::Surface *s : { v.bitmap, v.hover, v.knob, v.scrollBar, v.scrollThumb, v.caption }) {
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
			v.bitmapName = f.readString(0, 32);
			v.bitmap = bitmap(v.bitmapName);
			f.skip(v.tag == "POL#" ? 12 : 4);
			if (v.bitmap && !v.w)
				v.w = v.bitmap->w + v.bitmapDx;
			if (v.bitmap && !v.h)
				v.h = v.bitmap->h + v.bitmapDy;
		} else if (v.tag == "RCS#" || v.tag == "AOL#" || v.tag == "VAS#" || v.tag == "cSU#") {
			// The scroll bar: arrows-and-track bitmap, thumb (fra.ksy scroll, E-0600)
			v.scrollBar = bitmap(f.readString(0, 32));
			f.skip(8);
			v.scrollThumb = bitmap(f.readString(0, 32));
			f.skip(36);
			v.list = v.tag != "RCS#";
		} else if (v.tag == "loV#" || v.tag == "BoV#" || v.tag == "AoV#") {
			v.slider = true;
			v.margin = f.readSint32LE();
			v.knob = bitmap(f.readString(0, 32));
		} else if (v.tag == "nCC#" || v.tag == "nIC#") {
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
			case MKTAG('@', 'H', 'I', 'G'): { // GIH@: a caption at absolute (dx, dy) (E-0603)
				Graphics::Surface *s = bitmap(f.readString(0, 32));
				const int dx = f.readSint32LE(), dy = f.readSint32LE();
				if (tag == MKTAG('@', 'H', 'I', 'L')) {
					v.hover = s;
					v.hoverDx = dx;
					v.hoverDy = dy;
				} else {
					v.caption = s;
					v.captionX = dx;
					v.captionY = dy;
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
				_views.push_back(v); // the destructor frees its bitmaps
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
	return view >= 0 && _views[view].enabled ? _views[view].cursor : -1;
}

const Common::String &Frame::commandAt(int view) const {
	return view >= 0 && _views[view].enabled ? _views[view].command : _noCommand;
}

int Frame::editView() const {
	for (uint i = 0; i < _views.size(); i++)
		if (_views[i].edit)
			return i;
	return -1;
}

void Frame::setCaret(int pos) {
	// Every caret move shows the caret and restarts its blink (E-0610)
	const int e = editView();
	_caret = _selEnd = _anchor = CLIP<int>(pos, 0, e >= 0 ? _views[e].text.size() : 0);
	_caretTime = g_system->getMillis();
}

void Frame::deleteSelection() {
	Common::String &t = _views[editView()].text;
	t = Common::String(t.c_str(), _caret) + (t.c_str() + _selEnd);
	setCaret(_caret);
}

void Frame::type(char c) {
	const int e = editView();
	// The length test comes before the insert, so one character more than the maximum fits
	if (e < 0 || _views[e].text.size() > _views[e].maxLength)
		return;
	deleteSelection();
	_views[e].text.insertChar(c, _caret);
	setCaret(_caret + 1);
	textChanged();
}

void Frame::backspace() {
	const int e = editView();
	if (e < 0)
		return;
	if (_caret != _selEnd) {
		deleteSelection();
	} else if (_caret > 0) {
		_views[e].text.deleteChar(_caret - 1);
		setCaret(_caret - 1);
	}
	textChanged();
}

void Frame::moveCaret(int delta) {
	if (editView() >= 0)
		setCaret(delta < 0 ? _caret - 1 : _selEnd + 1);
}

int Frame::charAt(int x) const {
	// The first boundary from the left after which the distance stops shrinking
	const Common::String &t = _views[editView()].text;
	int best = 0, distance = 10000;
	for (uint i = 0; i <= t.size(); i++) {
		const int d = ABS(textWidth(t, i) - x);
		if (d >= distance)
			break;
		best = i;
		distance = d;
	}
	return best;
}

void Frame::textChanged() {
	// Players list (cSU#, E-0610): OK dims and stops reacting on an empty name; otherwise
	// the player of that exact name is selected and scrolled to
	const int l = listView();
	const int e = editView();
	if (l < 0 || e < 0 || _views[l].tag != "cSU#")
		return;
	const Common::String &t = _views[e].text;
	setBitmap(2, t.empty() ? "UserOKN" : "UserOKM");
	setEnabled(2, !t.empty());
	_selected = -1;
	for (uint i = 0; i < _names.size() && !t.empty(); i++) {
		if (_names[i] == t) {
			_selected = i;
			break;
		}
	}
	if (_selected >= 0 && scrollMax() > 0)
		_scroll = MIN(_selected, scrollMax());
}

int Frame::indexOf(int id) const {
	for (uint i = 0; i < _views.size(); i++)
		if (_views[i].id == id)
			return i;
	return -1;
}

void Frame::setBitmap(int id, const Common::String &name) {
	const int i = indexOf(id);
	if (i < 0)
		return;
	View &v = _views[i];
	if (v.bitmapName == name)
		return;
	if (v.bitmap) {
		v.bitmap->free();
		delete v.bitmap;
	}
	v.bitmapName = name;
	v.bitmap = bitmap(name);
}

void Frame::setVisible(int id, bool visible) {
	const int i = indexOf(id);
	if (i >= 0)
		_views[i].visible = visible;
}

void Frame::setEnabled(int id, bool enabled) {
	const int i = indexOf(id);
	if (i >= 0)
		_views[i].enabled = enabled;
}

int Frame::sliderMax(int id) const {
	const int i = indexOf(id);
	return i >= 0 ? _views[i].w - 2 * _views[i].margin : 0;
}

int Frame::sliderValue(int id) const {
	const int i = indexOf(id);
	return i >= 0 ? _views[i].value : 0;
}

void Frame::setSliderValue(int id, int value) {
	const int i = indexOf(id);
	if (i >= 0)
		_views[i].value = CLIP(value, 0, sliderMax(id));
}

bool Frame::press(const Common::Point &p, bool shift) {
	if (pressScroll(p))
		return true;
	// The edit: caret to the nearest character boundary, Shift extends, a drag selects
	const int e = editView();
	if (e >= 0 && _views[e].visible && Common::Rect(_views[e].x, _views[e].y, _views[e].x + _views[e].w, _views[e].y + _views[e].h).contains(p)) {
		const int i = charAt(p.x - _views[e].x);
		if (!shift)
			setCaret(i);
		else if (_caret < i)
			_selEnd = i;
		else
			_caret = i;
		_anchor = i;
		_editDrag = true;
		return true;
	}
	// A margin steps by 5, the knob starts a drag, the rest of the track does nothing
	for (uint i = 0; i < _views.size(); i++) {
		View &v = _views[i];
		if (!v.slider || !v.visible || !Common::Rect(v.x, v.y, v.x + v.w, v.y + v.h).contains(p))
			continue;
		const int max = v.w - 2 * v.margin;
		if (p.x < v.x + v.margin)
			v.value = MAX(0, v.value - 5);
		else if (p.x >= v.x + v.w - v.margin)
			v.value = MIN(max, v.value + 5);
		else if (ABS(p.x - (v.x + v.margin + v.value)) <= 4)
			_dragging = i;
		return true;
	}
	return false;
}

void Frame::drag(const Common::Point &p) {
	const int l = listView();
	if (_scrollDrag && l >= 0 && scrollMax() > 0) {
		const View &v = _views[l];
		const int d = p.y - v.y - 33, len = v.h - 66, max = scrollMax();
		const int step = MAX(1, len / max);
		int s = d / step;
		if (d % step > step / 2)
			s++;
		_scroll = CLIP(s, 0, max);
	}
	if (_editDrag) {
		const int i = charAt(p.x - _views[editView()].x);
		_caret = MIN(_anchor, i);
		_selEnd = MAX(_anchor, i);
	}
	if (_dragging < 0)
		return;
	View &v = _views[_dragging];
	v.value = CLIP(p.x - v.margin - v.x, 0, v.w - 2 * v.margin);
}

void Frame::setText(const Common::String &text) {
	const int e = editView();
	if (e < 0)
		return;
	_views[e].text = text;
	setCaret(text.size());
	textChanged();
}

int Frame::listView() const {
	for (uint i = 0; i < _views.size(); i++)
		if (_views[i].list)
			return i;
	return -1;
}

void Frame::setList(const Common::Array<Common::String> &rows, int selected, const Common::Array<Common::String> &names) {
	_rows = rows;
	_names = names;
	_selected = selected;
	_scroll = CLIP(selected, 0, scrollMax()); // the list scrolls to the selected row
	textChanged(); // the players list selects the edit's name
}

void Frame::selectRow(int row) {
	_selected = row;
	// Players list: the row's name goes to the edit (E-0610)
	if (row >= 0 && row < (int)_names.size())
		setText(_names[row]);
}

int Frame::scrollMax() const {
	return MAX(0, scrollRange());
}

int Frame::scrollRange() const {
	const int l = listView();
	return l < 0 ? -1 : (int)_rows.size() - _views[l].h / 32;
}

int Frame::listRowAt(const Common::Point &p) const {
	const int l = listView();
	if (l < 0)
		return -1;
	const View &v = _views[l];
	const int bar = v.scrollBar ? v.scrollBar->w : 38;
	if (!Common::Rect(v.x, v.y, v.x + v.w - bar, v.y + v.h).contains(p))
		return -1;
	const int row = (p.y - v.y) / 32 + _scroll;
	return row < (int)_rows.size() ? row : -1;
}

bool Frame::pressScroll(const Common::Point &p) {
	// The bar column: 33 px arrows at both ends, the thumb drags, the track pages (E-0600)
	const int l = listView();
	if (l < 0 || !_views[l].scrollBar || scrollMax() <= 0)
		return false;
	const View &v = _views[l];
	const int right = v.x + v.w, left = right - v.scrollBar->w;
	if (p.x < left || p.x >= right || p.y < v.y || p.y >= v.y + v.h)
		return false;
	const int max = scrollMax(), page = v.h / 32;
	const float f = (v.h - 66) / (float)max;
	const int thumb = (int)(v.y + 33 + _scroll * f);
	const int half = v.scrollThumb ? v.scrollThumb->h / 2 : 7;
	if (p.y < v.y + 33)
		_scroll = MAX(0, _scroll - 1);
	else if (p.y >= v.y + v.h - 33)
		_scroll = MIN(max, _scroll + 1);
	else if (ABS(p.y - thumb) <= half)
		_scrollDrag = true;
	else
		_scroll = CLIP(_scroll + (p.y < thumb ? -page : page), 0, max);
	return true;
}

Common::String Frame::text() const {
	const int e = editView();
	return e >= 0 ? _views[e].text : Common::String();
}

void Frame::draw(Renderer &r, int xOffset, int hovered) {
	for (uint i = 0; i < _views.size(); i++) {
		View &v = _views[i];
		if (!v.visible)
			continue;
		if (v.bitmap)
			r.drawImage(*v.bitmap, xOffset + v.x + v.bitmapDx, v.y + v.bitmapDy, false);
		if ((int)i == hovered && v.hover && v.enabled)
			r.drawImage(*v.hover, xOffset + v.x + v.hoverDx, v.y + v.hoverDy, false);
		if (v.slider && v.knob)
			r.drawImage(*v.knob, xOffset + v.x + v.margin + v.value - 4, v.y, false);
		if (v.edit) {
			// White on (32, 32, 80) from the left, the selection inverted, a blinking
			// inverted caret (ui.md, Players screen)
			const Graphics::Font *font = textFont();
			Graphics::Surface s;
			s.create(v.w, v.h, Graphics::PixelFormat::createFormatRGBA32());
			const uint32 back = s.format.ARGBToColor(255, 32, 32, 80), white = s.format.ARGBToColor(255, 255, 255, 255);
			s.fillRect(Common::Rect(v.w, v.h), back);
			const int ty = (v.h - font->getFontHeight()) / 2;
			font->drawString(&s, v.text, 0, ty, v.w, white);
			const int x0 = textWidth(v.text, _caret);
			if (_selEnd > _caret) {
				const Common::String sel(v.text.c_str() + _caret, _selEnd - _caret);
				s.fillRect(Common::Rect(x0, MAX(0, ty), MIN(v.w, x0 + font->getStringWidth(sel)), MIN(v.h, ty + font->getFontHeight())), white);
				font->drawString(&s, sel, x0, ty, v.w - x0, back);
			} else if ((g_system->getMillis() - _caretTime) / kCaretBlink % 2 == 0 && x0 < v.w) {
				for (int y = 0; y < v.h; y++) {
					uint32 *px = (uint32 *)s.getBasePtr(x0, y);
					uint8 a, cr, cg, cb;
					s.format.colorToARGB(*px, a, cr, cg, cb);
					*px = s.format.ARGBToColor(255, 255 - cr, 255 - cg, 255 - cb);
				}
			}
			r.drawImage(s, xOffset + v.x, v.y, false);
			s.free();
		}
		if (v.list) {
			// Rows centred in (0, 32 row, w - 38, 32) on an opaque (32, 32, 80) box
			// (save.md, Lists)
			const Graphics::Font *font = textFont();
			Graphics::Surface s;
			s.create(v.w - 38, v.h, Graphics::PixelFormat::createFormatRGBA32());
			s.fillRect(Common::Rect(s.w, s.h), s.format.ARGBToColor(255, 32, 32, 80));
			for (int shown = 0; shown + _scroll < (int)_rows.size() && (shown + 1) * 32 <= v.h; shown++) {
				const int row = shown + _scroll;
				const uint32 color = row == _selected ? s.format.ARGBToColor(255, 247, 196, 90) : s.format.ARGBToColor(255, 135, 186, 235);
				font->drawString(&s, _rows[row], 0, shown * 32 + (32 - font->getFontHeight()) / 2, s.w, color, Graphics::kTextAlignCenter);
			}
			r.drawImage(s, xOffset + v.x, v.y, false);
			s.free();
			// The players list draws its bar from max 0 on, the thumb only above (E-0600)
			const int max = scrollRange();
			if (v.scrollBar && (max > 0 || (max == 0 && v.tag == "cSU#"))) {
				const int bx = xOffset + v.x + v.w - v.scrollBar->w;
				r.drawImage(*v.scrollBar, bx, v.y, false);
				if (v.scrollThumb && max > 0) {
					const float f = (v.h - 66) / (float)max;
					r.drawImage(*v.scrollThumb, bx, (int)(v.y + 33 + _scroll * f) - v.scrollThumb->h / 2, false);
				}
			}
		}
		if ((int)i == hovered && v.caption)
			r.drawImage(*v.caption, xOffset + v.captionX, v.captionY, false);
	}
}

} // End of namespace X3D
