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

// China's documentation base: the contents screen and the fiche viewer
// (spec/china-documentation.md, E-1300..E-1305).

#include "common/debug.h"
#include "common/file.h"
#include "common/system.h"

#include "cryomni3d/china/engine.h"

namespace CryOmni3D {
namespace China {

enum DocSprite {
	kDsTheme0 = 0, // som_ying, som_tron, som_the, som_pinc, som_boul, som_arch, som_pers, som_lieu
	kDsSomSpir = 8,
	kDsUp, kDsDown, kDsUpLit, kDsDownLit,
	kDsIndex, kDsIndexLit, kDsExitLit,
	kDsNext, kDsPrev, kDsNextLit, kDsPrevLit,
	kDsIcoSpir,
	kDsBadge0 // ico_ying, ico_tron, ico_thei, ico_pinc, ico_boul, ico_arch, ico_pers, ico_lieu
};

static const char *const kDocSpriteNames[] = {
	"SOM_YING", "SOM_TRON", "SOM_THE", "SOM_PINC", "SOM_BOUL", "SOM_ARCH", "SOM_PERS", "SOM_LIEU",
	"SOM_SPIR", "FL_HAJAU", "FL_BAJAU", "FL_HABLC", "FL_BABLC", "ICO_INDX", "I_INDINV", "I_SPRINV",
	"FL_DRROU", "FL_GCROU", "FL_DRBLC", "FL_GCBLC", "ICO_SPIR",
	"ICO_YING", "ICO_TRON", "ICO_THEI", "ICO_PINC", "ICO_BOUL", "ICO_ARCH", "ICO_PERS", "ICO_LIEU"
};
// The theme backgrounds of the viewer (E-1302)
static const char *const kDocBackgrounds[8] = {
	"FONDBEIG", "FONDBRUN", "FONDGRIS", "FONDORAN", "FONDROSE", "FONDTURK", "FONDVERT", "FONDVIOL"
};
// Badge offsets added to the badge sprite's own position (E-1302, Q-1350)
static const int kDocBadgeOffsets[8][2] = {
	{ 25, 15 }, { 10, 5 }, { -5, -5 }, { 20, 10 }, { 15, 15 }, { 15, 5 }, { 20, 10 }, { 20, 10 }
};

static const uint16 kColorLink = 0x7020;
static const uint16 kColorWhite = 0xffff;
static const int kIndexRows = 20;
static const int kCursorLink = 8;
static const int kCursorDefault = 11;

namespace {

// A cursor over the text files' bytes
struct TextCursor {
	const Common::String &s;
	uint p;

	explicit TextCursor(const Common::String &str) : s(str), p(0) {}
	bool eof() const { return p >= s.size(); }
	char at(uint k = 0) const { return p + k < s.size() ? s[p + k] : 0; }
	void ws() {
		while (!eof() && (byte)s[p] <= ' ') {
			p++;
		}
	}
	// Reads `open ... close` and moves past it
	bool delim(char open, char close, Common::String &body) {
		if (at() != open) {
			return false;
		}
		const size_t e = s.findFirstOf(close, p + 1);
		if (e == Common::String::npos) {
			return false;
		}
		body = Common::String(s.c_str() + p + 1, e - p - 1);
		p = e + 1;
		return true;
	}
	// The `$link$` labels up to a byte of `stop`; any other byte on the way is passed over (E-0204)
	void links(Common::StringArray &out, const char *stop) {
		for (;;) {
			ws();
			if (eof() || strchr(stop, at())) {
				return;
			}
			Common::String body;
			if (at() == '$' && delim('$', '$', body)) {
				body.trim();
				body.toLowercase();
				out.push_back(body);
			} else {
				p++;
			}
		}
	}
};

bool readFile(const char *path, Common::String &out) {
	Common::File file;
	if (!file.open(path)) {
		warning("China: no %s", path);
		return false;
	}
	const int64 size = file.size();
	if (size < 0) {
		return false;
	}
	Common::Array<char> buf(size + 1);
	buf[size] = 0;
	if (file.read(buf.data(), size) != (uint32)size) {
		return false;
	}
	out = Common::String(buf.data(), size);
	return true;
}

} // End of anonymous namespace

// Fichetxt.txt (formats README, E-0204): `##theme##` `<title>`, fiches `#label#` `<title>` `!picture!`
// with a caption and text and `$link$` labels, or a table of `<a>` `<b>` rows, each closed by `##`;
// the theme closes with `###`. LISTE.TXT (E-0203, E-1301): rows `text/label`, group headers.
void CryOmni3DEngine_China::docLoad() {
	if (_docLoaded) {
		return;
	}
	_docLoaded = true;
	Common::String text;
	if (readFile("LOC/Fichetxt.txt", text)) {
		TextCursor c(text);
		for (;;) {
			c.ws();
			if (c.eof()) {
				break;
			}
			if (c.at() != '#' || c.at(1) != '#' || c.at(2) == '#') {
				warning("China: Fichetxt.txt: expected ##theme## at %u", c.p);
				break;
			}
			const size_t e = text.findFirstOf('#', c.p + 2);
			c.p = e == Common::String::npos ? text.size() : e + 2;
			DocTheme theme;
			c.ws();
			c.delim('<', '>', theme.title);
			for (;;) {
				c.ws();
				if (c.at() == '#' && c.at(1) == '#' && c.at(2) == '#') {
					c.p += 3;
					break;
				}
				DocFiche fiche;
				if (c.eof() || !c.delim('#', '#', fiche.label)) {
					warning("China: Fichetxt.txt: bad fiche at %u", c.p);
					break;
				}
				fiche.label.trim();
				fiche.label.toLowercase();
				c.ws();
				c.delim('<', '>', fiche.title);
				c.ws();
				c.delim('!', '!', fiche.picture);
				if (!fiche.picture.empty()) {
					while (!c.eof() && c.at() != '<' && c.at() != '#' && c.at() != '>') {
						c.p++;
					}
					c.delim('<', '>', fiche.caption);
					c.ws();
					c.delim('<', '>', fiche.text);
					c.links(fiche.links, "#");
				} else {
					for (;;) {
						c.ws();
						DocTableRow row;
						if (!c.delim('<', '>', row.a)) {
							break;
						}
						c.ws();
						c.delim('<', '>', row.b);
						c.links(row.links, "<#");
						fiche.rows.push_back(row);
					}
				}
				c.ws();
				if (c.at() == '#' && c.at(1) == '#') {
					c.p += 2;
				}
				theme.fiches.push_back(fiche);
			}
			_docThemes.push_back(theme);
		}
	}

	if (readFile("LOC/LISTE.TXT", text)) {
		TextCursor c(text);
		Common::String header; // a group header counts only when an entry follows it (Q-1353)
		for (;;) {
			c.ws();
			if (c.eof()) {
				break;
			}
			DocIndexRow row;
			if (c.at() == '#' && c.at(1) == '#') {
				size_t e = text.findFirstOf('\r', c.p);
				if (e == Common::String::npos) {
					e = text.size();
				}
				const Common::String line(text.c_str() + c.p, e - c.p);
				c.p = e;
				if (line.size() > 4 && line.hasSuffix("##")) {
					// A `##name##` section: its `<title>` line is skipped, the group's header is `-`
					c.ws();
					Common::String skipped;
					c.delim('<', '>', skipped);
					header = "-";
				} else if (line.size() == 3) {
					header = Common::String::format("-%c-", line[2]);
				} else {
					warning("China: LISTE.TXT: bad header %s", line.c_str());
				}
			} else if (c.at() == '<') {
				Common::String skipped; // text with no #id# is ignored
				if (!c.delim('<', '>', skipped)) {
					break;
				}
			} else {
				Common::String id;
				if (!c.delim('#', '#', id)) {
					c.p++;
					continue;
				}
				c.ws();
				c.delim('<', '>', row.text);
				if (!header.empty()) {
					DocIndexRow h;
					h.text = header;
					_docIndex.push_back(h);
					header.clear();
				}
				row.label = id;
				row.label.trim();
				row.label.toLowercase();
				_docIndex.push_back(row);
			}
		}
	}

	// W: the widest row text in font 10, headers count 0 (0x4095a0, E-1300)
	_fontManager.setCurrentFont(10);
	_docIndexWidth = 0;
	for (uint i = 0; i < _docIndex.size(); i++) {
		if (!_docIndex[i].label.empty()) {
			_docIndexWidth = MAX<int>(_docIndexWidth, _fontManager.getStrWidth(_docIndex[i].text));
		}
	}
	uint fiches = 0;
	for (uint t = 0; t < _docThemes.size(); t++) {
		fiches += _docThemes[t].fiches.size();
	}
	debug(1, "China: documentation: %u themes, %u fiches, %u index rows", _docThemes.size(), fiches, _docIndex.size());
}

void CryOmni3DEngine_China::docLoadSprites() {
	if (!_docSprites[0].surface.empty()) {
		return;
	}
	for (uint i = 0; i < kDocSpriteCount; i++) {
		loadSprite(Common::Path(Common::String::format("INTERF/%s.SPR", kDocSpriteNames[i])), _docSprites[i]);
	}
}

// The key is a fiche label, matched case-insensitively over all themes (E-1300)
bool CryOmni3DEngine_China::docFind(const Common::String &label, int &theme, int &fiche) const {
	Common::String key(label);
	key.trim();
	key.toLowercase();
	for (uint t = 0; t < _docThemes.size(); t++) {
		for (uint f = 0; f < _docThemes[t].fiches.size(); f++) {
			if (_docThemes[t].fiches[f].label == key) {
				theme = t;
				fiche = f;
				return true;
			}
		}
	}
	return false;
}

// The history (E-1302): written at the end position, which becomes current.
void CryOmni3DEngine_China::docHistoryPush(const Common::String &label) {
	if (_docEnd >= (int)kDocHistorySize) {
		// Original bug: the 51st write lands one slot past the 50-label array before the list
		// restarts (E-1302); here the list restarts at slot 0 without writing past the array
		_docEnd = 0;
		_docWrapped = true;
	}
	_docHistory[_docEnd] = label;
	_docCur = _docEnd;
	_docEnd++;
}

Common::Rect CryOmni3DEngine_China::docRect(int sprite) const {
	const Sprite &s = _docSprites[sprite];
	return Common::Rect(s.pos.x, s.pos.y, s.pos.x + s.surface.w, s.pos.y + s.surface.h);
}

void CryOmni3DEngine_China::docBlit(int sprite) {
	const Sprite &s = _docSprites[sprite];
	_screen.transBlitFrom(s.surface, s.pos, s.keyColor);
}

// Each channel divided by 2 (contents) or 8 (viewer): (c + t) >> n with the tint t = 0 (E-1300)
void CryOmni3DEngine_China::docDarken(const Common::Rect &area, int shift) {
	Common::Rect r(area);
	r.clip(Common::Rect(640, 480));
	for (int y = r.top; y < r.bottom; y++) {
		uint16 *p = (uint16 *)_screen.getBasePtr(r.left, y);
		for (int x = r.left; x < r.right; x++, p++) {
			byte cr, cg, cb;
			_format.colorToRGB(*p, cr, cg, cb);
			*p = _format.RGBToColor(cr >> shift, cg >> shift, cb >> shift);
		}
	}
}

// The index panel of both screens (E-1300). Draws it, scrolls on arrow hover (one row per frame)
// and returns the entry row under the mouse, or -1.
int CryOmni3DEngine_China::docIndexPanel(int shift, int &scroll, const Common::Point &mouse) {
	const int w = _docIndexWidth;
	const Sprite &up = _docSprites[kDsUp], &down = _docSprites[kDsDown];
	docDarken(Common::Rect(638 - w, 50, 640, 50 + up.surface.h + 315), shift);

	const int maxScroll = MAX<int>(0, (int)_docIndex.size() - kIndexRows);
	const Common::Rect upRect(640 - w / 2, 50, 640 - w / 2 + up.surface.w, 50 + up.surface.h);
	const Common::Rect downRect(640 - w / 2, 365, 640 - w / 2 + down.surface.w, 365 + down.surface.h);
	const bool upLit = upRect.contains(mouse) && scroll > 0;
	const bool downLit = downRect.contains(mouse) && scroll < maxScroll;
	if (upLit) {
		scroll--;
	}
	if (downLit) {
		scroll++;
	}
	const Sprite &upS = upLit ? _docSprites[kDsUpLit] : up;
	const Sprite &downS = downLit ? _docSprites[kDsDownLit] : down;
	_screen.transBlitFrom(upS.surface, upRect.origin(), upS.keyColor);
	_screen.transBlitFrom(downS.surface, downRect.origin(), downS.keyColor);

	int hovered = -1;
	_fontManager.setCurrentFont(10);
	for (int i = 0; i < kIndexRows && scroll + i < (int)_docIndex.size(); i++) {
		const DocIndexRow &row = _docIndex[scroll + i];
		const int width = _fontManager.getStrWidth(row.text);
		const Common::Rect hot(640 - w, 65 + 15 * i, 640 - w + width, 78 + 15 * i);
		const bool over = !row.label.empty() && hot.contains(mouse);
		if (over) {
			hovered = scroll + i;
		}
		_fontManager.setForeColor(textColor(over ? kColorLink : kColorWhite));
		_fontManager.displayStr(640 - w, 65 + 15 * i, row.text);
	}
	return hovered;
}

// INTERF\<name>.tga (0x416510), converted to the screen format
bool CryOmni3DEngine_China::docLoadPicture(const Common::String &name, Graphics::ManagedSurface &dst) {
	return loadTga(Common::Path(Common::String("INTERF/") + name + ".TGA"), dst);
}

// Word-wrapped text in the current font, 15 px lines (E-1304). `$` signs count no width; the
// words of a `$...$` link are drawn in 0x7020 and get a hot rectangle. A line is justified to
// `width` when at least two more lines follow it. With hardBreaks a newline ends the line (the
// special fiche and table 1, Text::drawBox 0x40e120); otherwise newlines are spaces (Q-1352).
void CryOmni3DEngine_China::docDrawText(const Common::String &text, int x, int y, int width, int bottom,
                                        bool justify, bool hardBreaks, Common::Array<DocLink> *links) {
	const int space = _fontManager.getStrWidth(" ");
	Common::Array<Common::Array<DocWord> > lines;
	Common::Array<bool> lineEnds; // the line ends its paragraph
	Common::Array<DocWord> line;
	int lineWidth = 0;
	bool inLink = false, wordLink = false;
	int linkNo = -1;
	Common::String word;
	for (uint i = 0; i <= text.size(); i++) {
		const char c = i < text.size() ? text[i] : '\n';
		if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
			if (c == '$') {
				inLink = !inLink;
				if (inLink) {
					linkNo++;
				}
				wordLink = true;
			} else {
				word += c;
				wordLink = wordLink || inLink;
			}
			continue;
		}
		if (!word.empty()) {
			DocWord w;
			w.text = word;
			w.width = _fontManager.getStrWidth(word);
			w.link = wordLink ? linkNo : -1;
			if (!line.empty() && lineWidth + space + w.width > width) {
				lines.push_back(line);
				lineEnds.push_back(false);
				line.clear();
				lineWidth = 0;
			}
			lineWidth += (line.empty() ? 0 : space) + w.width;
			line.push_back(w);
			word.clear();
		}
		wordLink = false;
		if (c == '\n' && (hardBreaks || i == text.size()) && !line.empty()) {
			lines.push_back(line);
			lineEnds.push_back(true);
			line.clear();
			lineWidth = 0;
		}
	}
	for (uint l = 0; l < lines.size(); l++, y += 15) {
		if (y + 15 > bottom) {
			break;
		}
		const Common::Array<DocWord> &words = lines[l];
		int total = 0;
		for (uint k = 0; k < words.size(); k++) {
			total += words[k].width;
		}
		const int gaps = words.size() - 1;
		int spare = 0;
		if (justify && !lineEnds[l] && l + 2 < lines.size() && gaps > 0) {
			spare = MAX(0, width - total - gaps * space);
		}
		int cx = x;
		for (uint k = 0; k < words.size(); k++) {
			const DocWord &w = words[k];
			_fontManager.setForeColor(textColor(w.link >= 0 ? kColorLink : kColorWhite));
			_fontManager.displayStr(cx, y, w.text);
			if (w.link >= 0 && links && links->size() < 10) {
				DocLink dl;
				dl.rect = Common::Rect(cx, y - 2, cx + w.width, y + 11);
				dl.number = w.link;
				links->push_back(dl);
			}
			cx += w.width + space + (spare > 0 ? spare / gaps + ((int)k < spare % gaps ? 1 : 0) : 0);
		}
	}
}

// The body of a fiche below the title (0x40c100, 0x40bfe0; E-1303..E-1305)
void CryOmni3DEngine_China::docDrawFiche(const DocFiche &fiche, const Graphics::ManagedSurface *pic, int tableSel,
                                         Common::Array<DocLink> &links) {
	const uint32 white = textColor(kColorWhite);
	if (fiche.picture == "speciale") {
		// The Chinese sign: two columns (caption, text), Text::drawBox in font 1
		_fontManager.setCurrentFont(1);
		docDrawText(fiche.caption, 100, 150, 500, 400, false, true, nullptr);
		docDrawText(fiche.text, 200, 150, 400, 400, false, true, nullptr);
		return;
	}
	if (fiche.picture.empty()) {
		_fontManager.setCurrentFont(1);
		for (uint i = 0; i < fiche.rows.size(); i++) {
			_fontManager.setForeColor(textColor((int)i == tableSel ? kColorWhite : kColorLink));
			_fontManager.displayStr(50, 50 + 20 * i, fiche.rows[i].a);
		}
		if (tableSel >= 0 && tableSel < (int)fiche.rows.size()) {
			const DocTableRow &row = fiche.rows[tableSel];
			const int y = 50 + 20 * tableSel;
			if (fiche.label == "fiche 2") {
				// Table 0: box x 300..630 down to 400, with links like the text column (Q-1351)
				docDrawText(row.b, 300, y, 330, 400, false, false, &links);
			} else {
				// Table 1: Text::drawBox from x = 60 + width of <a>, right 630, bottom 480
				const int x = 60 + _fontManager.getStrWidth(row.a);
				docDrawText(row.b, x, y, 630 - x, 480, false, true, nullptr);
			}
		}
		return;
	}
	if (pic) {
		const int w = pic->w, h = pic->h;
		if (w > 330 || h > 330) {
			_fontManager.setCurrentFont(0);
			_fontManager.setForeColor(white);
			_fontManager.displayStr(100, 100, w > 330 ? "Image trop large" : "Image trop haute");
		} else {
			const int px = 176 - (w + 1) / 2, py = 216 - (h + 1) / 2;
			_screen.blitFrom(*pic, Common::Point(px, py));
			Common::String caption(fiche.caption);
			for (uint i = 0; i < caption.size(); i++) {
				if (caption[i] == '\r' || caption[i] == '\n') {
					caption.setChar(' ', i);
				}
			}
			Common::Array<Common::String> lines;
			_fontManager.setCurrentFont(8);
			_fontManager.setForeColor(white);
			wrapText(caption, w < 165 ? 330 : w, lines);
			for (uint i = 0; i < lines.size(); i++) {
				_fontManager.displayStr(w < 165 ? 10 : px, py + h + 1 + 10 * i, lines[i]);
			}
		}
	}
	_fontManager.setCurrentFont(1);
	docDrawText(fiche.text, 350, 60, 280, 480, true, false, &links);
}

// The contents screen (0x4085d0, E-1300)
void CryOmni3DEngine_China::documentation() {
	docLoad();
	docLoadSprites();
	Graphics::ManagedSurface background;
	if (!loadStill("INTERF/FOND_SOM.HNM", background)) {
		return;
	}
	int openTheme = -1;
	bool indexOpen = false;
	int scroll = 0;
	int indexRow = -1; // the index entry under the mouse in the last frame
	PuzzleInput in;
	puzzleStart(in);
	setCursorSprite(kCursorDefault);
	while (!shouldAbort()) {
		puzzlePoll(in);
		if (in.escape) {
			break;
		}
		const Common::Point m = getMousePos();
		const uint themeCount = MIN<uint>(8, _docThemes.size());

		// Geometry: theme text origins and hot areas, the open theme's fiche list
		Common::Rect hot[8];
		Common::Point origin[8];
		_fontManager.setCurrentFont(10);
		for (uint k = 0; k < themeCount; k++) {
			const Common::Rect t = docRect(kDsTheme0 + k);
			origin[k] = Common::Point(t.left + t.width() / 2 + 40, t.top + t.height() / 2 - 5);
			hot[k] = Common::Rect(origin[k].x, origin[k].y, t.right + 30 + _fontManager.getStrWidth(_docThemes[k].title),
			                      t.bottom);
		}
		Common::Rect titleHot[50];
		int listX = 0, listY = 0;
		uint listCount = 0;
		if (openTheme >= 0) {
			const Common::Rect t = docRect(kDsTheme0 + openTheme);
			const DocTheme &theme = _docThemes[openTheme];
			listCount = MIN<uint>(50, theme.fiches.size());
			listX = t.left + 220;
			listY = t.top + t.height() / 2 - 5;
			int shift = 0;
			if (listY + 15 * (int)listCount > 480) {
				shift = listY - (480 - 15 * (int)listCount);
				listY -= shift;
			}
			_fontManager.setCurrentFont(7);
			for (uint j = 0; j < listCount; j++) {
				titleHot[j] = Common::Rect(listX, t.top + 2 + 15 * j - shift,
				                           listX + _fontManager.getStrWidth(theme.fiches[j].title),
				                           t.top + 13 + 15 * j - shift);
			}
		}
		int hoveredTheme = -1;
		for (uint k = 0; k < themeCount; k++) {
			if (hot[k].contains(m)) {
				hoveredTheme = k;
			}
		}
		int hoveredTitle = -1;
		for (uint j = 0; j < listCount; j++) {
			if (titleHot[j].contains(m)) {
				hoveredTitle = j;
			}
		}
		const bool overIndex = docRect(kDsIndex).contains(m);
		const bool overExit = docRect(kDsSomSpir).contains(m);

		if (in.press) {
			Common::String open;
			if (overExit) {
				break;
			}
			if (indexOpen && indexRow >= 0) {
				open = _docIndex[indexRow].label;
			} else if (hoveredTitle >= 0) {
				open = _docThemes[openTheme].fiches[hoveredTitle].label;
			} else if (overIndex) {
				indexOpen = !indexOpen;
				openTheme = -1;
			} else if (hoveredTheme >= 0) {
				openTheme = openTheme == hoveredTheme ? -1 : hoveredTheme;
				indexOpen = false;
			}
			if (!open.empty()) {
				// The viewer returns to the contents with the same theme open and the index closed
				documentFiche(open.c_str());
				indexOpen = false;
				indexRow = -1;
				puzzleStart(in);
				setCursorSprite(kCursorDefault);
				continue;
			}
		}

		_screen.blitFrom(background);
		const bool lighting = openTheme < 0 && !indexOpen;
		for (uint k = 0; k < themeCount; k++) {
			docBlit(kDsTheme0 + k);
			_fontManager.setCurrentFont(10);
			_fontManager.setForeColor(textColor(((int)k == hoveredTheme && lighting) || (int)k == openTheme ? kColorWhite
			                                                                                                  : kColorLink));
			_fontManager.displayStr(origin[k].x, origin[k].y, _docThemes[k].title);
		}
		if (openTheme >= 0) {
			_fontManager.setCurrentFont(7);
			for (uint j = 0; j < listCount; j++) {
				_fontManager.setForeColor(textColor((int)j == hoveredTitle ? kColorLink : kColorWhite));
				_fontManager.displayStr(listX, listY + 15 * j, _docThemes[openTheme].fiches[j].title);
			}
		}
		docBlit(overIndex || indexOpen ? kDsIndexLit : kDsIndex);
		docBlit(overExit ? kDsExitLit : kDsSomSpir);
		indexRow = indexOpen ? docIndexPanel(1, scroll, m) : -1;
		puzzleFlip();
	}
	_cursorId = -1;
}

// The fiche viewer (0x40c5a0, E-1302..E-1305): 0 on leaving, 3 for an unknown label
int32 CryOmni3DEngine_China::documentFiche(const char *key) {
	docLoad();
	docLoadSprites();
	int t, f;
	if (!docFind(key, t, f)) {
		warning("China: documentation: unknown fiche %s", key);
		return 3;
	}
	docHistoryPush(_docThemes[t].fiches[f].label);
	Graphics::ManagedSurface background;
	int backgroundTheme = -1;
	bool indexOpen = false;
	int scroll = 0;
	int tableSel = -1; // the initial selection is not specified (Q-1352): none
	Common::Array<DocLink> links;
	int indexRow = -1;
	Graphics::ManagedSurface picture;
	bool pictureOk = false;
	const DocFiche *pictureFor = nullptr;

	PuzzleInput in;
	puzzleStart(in);
	setCursorSprite(kCursorDefault);
	while (!shouldAbort()) {
		puzzlePoll(in);
		if (in.escape) {
			break;
		}
		const Common::Point m = getMousePos();
		if (backgroundTheme != t) {
			loadStill(Common::Path(Common::String("INTERF/") + kDocBackgrounds[MIN(t, 7)] + ".HNM"), background);
			backgroundTheme = t;
		}
		const DocTheme &theme = _docThemes[t];
		const DocFiche &fiche = theme.fiches[f];
		if (pictureFor != &fiche) {
			pictureFor = &fiche;
			pictureOk = !fiche.picture.empty() && fiche.picture != "speciale" && docLoadPicture(fiche.picture, picture);
		}
		const bool canPrev = f > 0, canNext = f + 1 < (int)theme.fiches.size();
		const bool canBack = _docCur != _docEnd && (_docCur != 0 || _docWrapped);
		const int next = _docWrapped ? (_docCur + 1) % (int)kDocHistorySize : _docCur + 1;
		const bool canForward = next != _docEnd;

		// Hover (nothing happens while the index is open): links and table rows
		int hoveredLink = -1;
		if (!indexOpen) {
			for (uint i = 0; i < links.size(); i++) {
				if (links[i].rect.contains(m)) {
					hoveredLink = links[i].number;
				}
			}
			if (fiche.picture.empty()) {
				_fontManager.setCurrentFont(1);
				for (uint i = 0; i < fiche.rows.size(); i++) {
					const Common::Rect r(50, 52 + 20 * i, 50 + _fontManager.getStrWidth(fiche.rows[i].a), 63 + 20 * i);
					if (r.contains(m) && !fiche.rows[i].b.empty() && tableSel != (int)i) {
						tableSel = i;
						links.clear();
						hoveredLink = -1;
					}
				}
			}
		}
		// A table row's links are the selected row's list (E-1305)
		const Common::StringArray *linkLabels = &fiche.links;
		if (fiche.picture.empty() && tableSel >= 0 && tableSel < (int)fiche.rows.size()) {
			linkLabels = &fiche.rows[tableSel].links;
		}
		setCursorSprite(hoveredLink >= 0 ? kCursorLink : kCursorDefault);

		const bool overIndex = docRect(kDsIndex).contains(m);
		const bool overExit = docRect(kDsIcoSpir).contains(m);
		const bool overUp = canBack && docRect(kDsUp).contains(m);
		const bool overDown = canForward && docRect(kDsDown).contains(m);
		const bool overPrev = canPrev && docRect(kDsPrev).contains(m);
		const bool overNext = canNext && docRect(kDsNext).contains(m);

		// Presses
		Common::String open; // a fiche to show
		bool push = true;
		if (in.press) {
			if (indexOpen) {
				// Any press closes the index; one on an entry row first opens that fiche
				if (indexRow >= 0) {
					open = _docIndex[indexRow].label;
				}
				indexOpen = false;
			} else if (hoveredLink >= 0 && hoveredLink < (int)linkLabels->size()) {
				open = (*linkLabels)[hoveredLink];
			} else if (overPrev) {
				open = theme.fiches[f - 1].label;
			} else if (overNext) {
				open = theme.fiches[f + 1].label;
			} else if (overUp) {
				_docCur = _docCur == 0 ? kDocHistorySize - 1 : _docCur - 1;
				open = _docHistory[_docCur];
				push = false;
			} else if (overDown) {
				_docCur = (_docCur + 1) % kDocHistorySize;
				open = _docHistory[_docCur];
				push = false;
			} else if (overExit) {
				break;
			} else if (overIndex) {
				indexOpen = true;
			}
		}
		if (!open.empty()) {
			int nt, nf;
			if (docFind(open, nt, nf)) {
				t = nt;
				f = nf;
				if (push) {
					docHistoryPush(_docThemes[t].fiches[f].label);
				}
				tableSel = -1;
				links.clear();
				indexRow = -1;
				continue; // the next frame shows the new page (and its theme's background)
			}
			warning("China: documentation: unknown link %s", open.c_str());
		}

		// The page (0x40c100)
		_screen.blitFrom(background);
		docBlit(overUp ? kDsUpLit : kDsUp);
		docBlit(overDown ? kDsDownLit : kDsDown);
		docBlit(overPrev ? kDsPrevLit : kDsPrev);
		docBlit(overNext ? kDsNextLit : kDsNext);
		docBlit(overExit ? kDsExitLit : kDsIcoSpir);
		{
			// Q-1350: the badge sprite's own position plus the theme's offset (the safest reading)
			const int tb = MIN(t, 7);
			const Sprite &b = _docSprites[kDsBadge0 + tb];
			_screen.transBlitFrom(b.surface, b.pos + Common::Point(kDocBadgeOffsets[tb][0], kDocBadgeOffsets[tb][1]),
			                      b.keyColor);
		}
		_fontManager.setCurrentFont(3);
		_fontManager.setForeColor(textColor(kColorWhite));
		_fontManager.displayStr(320 - _fontManager.getStrWidth(fiche.title) / 2, 10, fiche.title);
		links.clear();
		docDrawFiche(fiche, pictureOk ? &picture : nullptr, tableSel, links);
		docBlit(overIndex || indexOpen ? kDsIndexLit : kDsIndex);
		indexRow = -1;
		if (indexOpen) {
			indexRow = docIndexPanel(3, scroll, m);
		} else if (hoveredLink >= 0 && hoveredLink < (int)linkLabels->size()) {
			int lt, lf;
			if (docFind((*linkLabels)[hoveredLink], lt, lf)) {
				_fontManager.setCurrentFont(10);
				_fontManager.setForeColor(textColor(kColorLink));
				_fontManager.displayStr(355, 435, _docThemes[lt].fiches[lf].title);
			}
		}
		puzzleFlip();
	}
	_cursorId = -1;
	return 0;
}

} // End of namespace China
} // End of namespace CryOmni3D
