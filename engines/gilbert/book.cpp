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

#include "graphics/font.h"

#include "gilbert/book.h"
#include "gilbert/dialog.h"
#include "gilbert/gilbert.h"
#include "gilbert/logic.h"
#include "gilbert/menu.h"
#include "gilbert/room.h"
#include "gilbert/sound.h"

namespace Gilbert {

namespace {

// The bar (screens.md "Tabs"): item, place, book (-1: not a book), title picture.
const struct Tab {
	int item, x, y, book, title;
} kTabs[] = {
	{ 0x4e, 88, 354, 1, 0x7b },   // Ordliste
	{ 0x52, 206, 354, 0, 0x7a },  // Tips
	{ 0x53, 88, 387, 2, 0x79 },   // Gilberts venner
	{ 0x51, 206, 387, 3, 0x78 },  // Eksperimenter
	{ 0x50, 325, 387, -1, -1 },   // Udskriv
	{ 0x4f, 443, 387, -1, -1 },   // Tilbage
};

enum {
	kPrint = 0x50, kBackToRoom = 0x4f, kToList = 0x69, kUp = 0x7e, kDown = 0x81
};

const uint32 kListColour = 0x802D19;
const uint32 kNewColour = 0x1E46A0; ///< a topic not opened yet (the mark_new_topics option)
const uint32 kPageColour = 0x402E1A;
// The heights of the original's surfaces for the list and a page.
const int kListHeight = 320, kPageHeight = 1000;

// The text style of the markup's \f formats (screens.md "A topic page").
struct Style {
	int size = 8;
	bool bold = false;
	bool underline = false;

	void format(int n) {
		switch (n) {
		case 0: size = 8; bold = false; underline = false; break;
		case 1: size = 8; bold = true; underline = false; break;
		case 2: size = 8; bold = false; underline = true; break;
		case 3: size = 8; bold = true; underline = true; break;
		case 4: size = 10; bold = false; underline = false; break;
		case 5: size = 10; bold = true; underline = false; break;
		default: break;
		}
	}
};

} // End of anonymous namespace

int Book::tabFor(int book) const {
	for (const Tab &t : kTabs)
		if (t.book == book)
			return t.item;
	return -1;
}

// The book button (rooms.md "Actions in the room", screens.md "Opening and leaving").
void Book::open() {
	_images.load("maps/!global/bookimages.wxi");
	_tab = tabFor(1);
	_pressedButton = _tab;
	buildList(1, 0);
	_hoverTab = _hoverArrow = _pressedArrow = -1;
	_vm->room()->clearNewTopic();
	_vm->setMode(GilbertEngine::kModeBook);
}

// book::BuildList: up to 20 titles from rank `first` (screens.md "The list").
void Book::buildList(int book, int first) {
	Logic *logic = _vm->logic();
	_book = book;
	_first = first;
	_page = false;
	_scroll = 0;
	_links.clear();
	_surface.clear();
	const int count = logic->bookTopicCount(book);
	// The line and the font run on from title to title (E-0512).
	int y = 0;
	Style style;
	for (int i = 0; i < 20 && first + i < count; i++) {
		const int rank = first + i;
		const uint32 topic = logic->bookTopicFromIndex(book, rank);
		const uint32 colour = _vm->options().newTopics && !_vm->isSeen(GilbertEngine::topicKey(book, topic)) ? kNewColour : kListColour;
		int x = 10;
		const int x0 = x, y0 = y;
		BookParser p;
		for (int t = p.first(logic->bookTopicTitle(book, rank)); t != kTokenEnd; t = p.next()) {
			if (t == kTokenText) {
				const Common::U32String s = GilbertEngine::fromWindows1252(p.token);
				_surface.text(s, x, y, style.size, colour, style.bold);
				x += _vm->textWidth(s, style.size, style.bold);
			} else if (t == kTokenFormat) {
				style.format(p.format);
			} else if (t == kTokenTab) {
				x = (x / 20 + 1) * 20;
			} else if (t == kTokenLineFeed) {
				y += 12;
				x = 10;
			}
		}
		Link l;
		l.rect = Common::Rect(x0 + 135, y0 + 103, x + 135, y + 115);
		l.book = book;
		l.topic = topic;
		_links.push_back(l);
		y += 12;
	}
}

// book::BuildPage (screens.md "A topic page").
void Book::buildPage(int book, uint32 topic) {
	Logic *logic = _vm->logic();
	const int rank = logic->bookIndexFromTopic(book, topic);
	_vm->markSeen(GilbertEngine::topicKey(book, topic));
	_book = book;
	_page = true;
	_scroll = 0;
	_links.clear();
	_surface.clear();
	int x = 10, y = 10, lineHeight = 12;
	Style style;
	uint32 colour = kPageColour;
	bool inLink = false;
	Link link;
	Common::Point linkStart;
	int formatNumber = 0;
	auto newLine = [&]() {
		y += lineHeight;
		x = 10;
		lineHeight = 12;
	};
	BookParser p;
	for (int t = p.first(logic->bookTopicText(book, rank)); t != kTokenEnd; t = p.next()) {
		switch (t) {
		case kTokenText: {
			const Common::U32String s = GilbertEngine::fromWindows1252(p.token);
			const int w = _vm->textWidth(s, style.size, style.bold);
			if (x + w > 344)
				newLine();
			if (inLink && linkStart.x < 0)
				linkStart = Common::Point(x, y);
			_surface.text(s, x, y, style.size, colour, style.bold);
			if (style.underline || inLink) {
				const Graphics::Font *f = _vm->font(style.size, style.bold);
				const int uy = y + f->getFontAscent() + 1;
				if (uy < kPageHeight)
					_surface.underlineLast(uy, MIN(w, 355 - x));
			}
			x += w;
			break;
		}
		case kTokenFormat:
			formatNumber = p.format;
			style.format(p.format);
			break;
		case kTokenLink:
			inLink = true;
			colour = kListColour;
			linkStart = Common::Point(-1, -1);
			link.book = p.linkBook;
			link.topic = p.linkTopic;
			break;
		case kTokenLinkEnd:
			if (inLink) {
				if (linkStart.x < 0)
					linkStart = Common::Point(x, y);
				link.rect = Common::Rect(linkStart.x + 135, linkStart.y + 103, x + 135, y + 115);
				_links.push_back(link);
				style.format(formatNumber);
				colour = kPageColour;
				inLink = false;
			}
			break;
		case kTokenPicture: {
			Picture *pic = _images[p.picture];
			if (!pic)
				break;
			if (x + pic->surface.w > 354)
				newLine();
			// Drawn opaque: fuchsia stays transparent through the page's own key (E-0512).
			_surface.picture(pic, x, y);
			x += pic->surface.w;
			lineHeight = MAX<int>(lineHeight, pic->surface.h);
			break;
		}
		case kTokenTab:
			x = (x / 50 + 1) * 50;
			if (x > 344)
				newLine();
			break;
		case kTokenLineFeed:
			newLine();
			break;
		default:
			break;
		}
	}
}

// The mode 4 tick: flipped on every second tick (screens.md "Ticks by mode").
void Book::tick() {
	draw();
	DialogBox *dialog = _vm->dialog();
	if (dialog->isOpen()) {
		dialog->handleMouse();
		dialog->draw();
	} else {
		handleMouse();
	}
	_vm->drawCursor(_cursor);
	_flip = !_flip;
	if (_flip)
		_vm->present();
}

// book::Draw (screens.md "Frame").
void Book::draw() {
	PictureCollection &i2 = _vm->interface2();
	_vm->clear();
	_vm->drawPicture(i2[0x00], 64, 50);
	_vm->drawPicture(i2[0x6f], 64, 50);
	_vm->drawPage(_surface, Common::Rect(0, _scroll, 355, MIN(_scroll + 320, _page ? kPageHeight : kListHeight)), 135, 95);
	_vm->drawPicture(_vm->interface1()[0], 64, 50);
	_vm->drawPicture(i2[0x41], 64, 337);
	for (const Tab &t : kTabs) {
		_vm->drawPicture(i2[t.item], t.x, t.y);
		if (_hoverTab == t.item)
			_vm->drawPicture(i2[t.item + 6], t.x, t.y);
		if (_pressedButton == t.item)
			_vm->drawPicture(i2[t.item + 12], t.x, t.y);
		if (_tab == t.item && t.title >= 0)
			_vm->drawPicture(i2[t.title], 192, 56);
	}
	// The pressed bar button acts while the mouse is on it, once per press (E-0513).
	if (_pressedButton >= 0 && _pressedButton == _hoverTab && _pressedButton != _vm->menu()->lastAction())
		tabAction(_pressedButton);
	static const struct {
		int item, x, y;
	} arrows[] = { { kToList, 115, 93 }, { kUp, 503, 93 }, { kDown, 503, 343 } };
	for (const auto &a : arrows) {
		_vm->drawPicture(i2[a.item], a.x, a.y);
		if (_hoverArrow == a.item)
			_vm->drawPicture(i2[a.item + 1], a.x, a.y);
		if (_pressedArrow == a.item) {
			_vm->drawPicture(i2[a.item + 2], a.x, a.y);
			arrowAction(a.item);
			_pressedArrow = -1;
		}
	}
}

// book::HandleMouse (screens.md "Mouse").
void Book::handleMouse() {
	// The keyboard shortcuts: Escape is the bar's back button.
	if (_vm->shortcut() == kActionBack) {
		_vm->takeShortcut();
		tabAction(kBackToRoom);
		return;
	}
	PictureCollection &i2 = _vm->interface2();
	const Common::Point m = _vm->mouse();
	const Common::Rect mr(m.x - 3, m.y - 3, m.x + 3, m.y + 3);
	const int dy = _page ? _scroll : 0;
	int linkHit = -1;
	for (uint i = 0; i < _links.size(); i++) {
		Common::Rect r = _links[i].rect;
		r.translate(0, -dy);
		if (r.intersects(mr))
			linkHit = i;
	}
	_cursor = linkHit >= 0 && !_vm->leftHeld() ? 7 : 0;

	auto firstHit = [&](const int *items, int n) {
		for (int i = 0; i < n; i++) {
			Picture *p = i2[items[i]];
			if (p && p->last.intersects(mr))
				return items[i];
		}
		return -1;
	};
	static const int tabs[] = { 0x4e, 0x52, 0x53, 0x51, 0x50, 0x4f };
	static const int arrows[] = { kUp, kDown, kToList };
	_hoverTab = Common::Rect(64, 360, 576, 430).contains(m) ? firstHit(tabs, 6) : -1;
	const bool arrowArea = Common::Rect(500, 90, 525, 360).contains(m) || Common::Rect(114, 90, 130, 125).contains(m);
	_hoverArrow = arrowArea ? firstHit(arrows, 3) : -1;

	if (!_vm->press(GilbertEngine::kScreenBook))
		return;
	if (linkHit >= 0) {
		const Link l = _links[linkHit];
		if (_page) {
			// Links to books other than 0..3 change nothing (E-0513).
			const int tab = tabFor(l.book);
			if (tab < 0)
				return;
			_tab = tab;
		}
		buildPage(l.book, l.topic);
		return;
	}
	if (_hoverTab >= 0)
		_pressedButton = _hoverTab;
	if (_hoverArrow >= 0)
		_pressedArrow = _hoverArrow;
}

void Book::tabAction(int item) {
	_vm->menu()->setLastAction(item);
	for (const Tab &t : kTabs) {
		if (t.item != item)
			continue;
		if (t.book >= 0) {
			_tab = item;
			if (t.book != _book) {
				_vm->sound()->playWave(1, 2);
				buildList(t.book, 0);
			}
		} else if (item == kPrint) {
			_vm->dialog()->openPrintMessage();
		} else if (item == kBackToRoom) {
			_vm->sound()->playWave(1, 4);
			_scroll = _first = 0;
			_hoverTab = _pressedButton = -1;
			_vm->setMode(GilbertEngine::kModeRoom);
		}
	}
}

// The arrows (screens.md "Arrows").
void Book::arrowAction(int item) {
	_vm->menu()->setLastAction(item);
	Sound *snd = _vm->sound();
	snd->playWave(1, 1);
	const int count = _vm->logic()->bookTopicCount(_book);
	if (item == kToList) {
		buildList(_book, 0);
	} else if (!_page) {
		const int first = item == kUp ? MAX(_first - 10, 0) : MAX(MIN(_first + 10, count - 20), 0);
		buildList(_book, first);
	} else {
		_scroll = item == kUp ? MAX(_scroll - 24, 0) : MIN(_scroll + 24, 999);
	}
}

} // End of namespace Gilbert
