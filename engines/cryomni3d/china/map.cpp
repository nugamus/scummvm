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

// China's map (spec/china-interface.md Map, E-1105, E-1150..E-1152), opened by the compass.

#include "common/system.h"

#include "cryomni3d/china/engine.h"
#include "cryomni3d/china/map_tables.h"

namespace CryOmni3D {
namespace China {

static const int kMapWindowWidth = 373; // the big map's window; the small map starts there
static const int kMapBigWidth = 846, kMapBigHeight = 1228;

// The big map's scroll offsets for a point on the small map (E-1105)
static Common::Point mapOffsets(const Common::Point &p) {
	return Common::Point(CLIP((p.x - 435) * kMapBigWidth / 267, 0, kMapBigWidth - kMapWindowWidth),
	                     CLIP((p.y - 79) * kMapBigHeight / 391, 0, 747)); // the clamps of E-1105
}

static bool mapTravelAllowed(const CryOmni3DEngine_China &g, const MapSpot &spot) {
	if (g.var(0)) {
		return true;
	}
	const Common::String target(spot.place);
	if (target == "cpc600") {
		return g.var(0x18) || g.var(0x19);
	}
	if (target == "ctp330") {
		return g.var(0xcc);
	}
	if (target == "pdc010") {
		return g.var(0x7c);
	}
	return true;
}

void CryOmni3DEngine_China::mapPresent() {
	g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, 640, 480);
	g_system->updateScreen();
	waitFrame();
}

// The building list (E-1152): rises over the map, a click picks a building or closes it.
// Returns the picked building's point on the small map, or (-1, -1).
Common::Point CryOmni3DEngine_China::mapBuildings() {
	_fontManager.setCurrentFont(0); // Q-1154
	Common::Array<Common::String> names;
	int widest = 0;
	for (uint i = 0; i < ARRAYSIZE(kMapBuildings); i++) {
		const Common::String text = label(kMapBuildings[i].place);
		if (text.empty()) {
			break;
		}
		names.push_back(text);
		widest = MAX(widest, (int)_fontManager.getStrWidth(text));
	}
	if (names.empty()) {
		return Common::Point(-1, -1);
	}
	const int w = (widest + 3) & ~3;
	const int rowHeight = _fontManager.getFontMaxHeight() + 5;
	const int n = names.size();
	Graphics::ManagedSurface list(w + 10, rowHeight * n, _format);
	list.clear(textColor(0xffff));
	_fontManager.setSurface(&list);
	_fontManager.setForeColor(textColor(0x7000));
	for (int i = 0; i < n; i++) {
		_fontManager.displayStr(5, rowHeight * (n - 1 - i), names[i]); // the first key at the bottom
	}
	_fontManager.setSurface(&_screen);

	Graphics::ManagedSurface saved;
	saved.copyFrom(_screen);
	const int left = 501 - w / 2, bottom = 440;
	const int top = bottom - list.h;
	// Rises and sinks 4 rows per frame (Q-1150)
	auto drawAt = [&](int y) {
		_screen.blitFrom(saved);
		const int rows = MIN<int>(bottom - y, list.h);
		if (rows > 0) {
			_screen.blitFrom(list, Common::Rect(0, 0, list.w, rows), Common::Point(left, y));
		}
	};
	for (int y = bottom - 4; y > top && !shouldAbort(); y -= 4) {
		drawAt(y);
		mapPresent();
		pollEvents();
	}
	PuzzleInput in;
	puzzleStart(in);
	Common::Point picked(-1, -1);
	while (!shouldAbort()) {
		puzzlePoll(in);
		const Common::Point m = cursorTopLeft();
		int hovered = -1;
		if (m.x >= left + 1 && m.x <= left + w + 10 && m.y >= top && m.y < bottom) {
			hovered = n - 1 - (m.y - top) / rowHeight;
		}
		drawAt(top);
		if (hovered >= 0) {
			const int y = top + rowHeight * (n - 1 - hovered);
			_screen.fillRect(Common::Rect(left, y, left + list.w, y + rowHeight), textColor(0x7000));
			_fontManager.setForeColor(textColor(0xffff));
			_fontManager.displayStr(left + 5, y, names[hovered]);
		}
		mapPresent();
		if (in.press) {
			if (hovered >= 0) {
				picked = Common::Point(kMapBuildings[hovered].x, kMapBuildings[hovered].y);
			}
			break;
		}
	}
	for (int y = top + 4; y < bottom && !shouldAbort(); y += 4) {
		drawAt(y);
		mapPresent();
		pollEvents();
	}
	return picked;
}

// The map loop (E-1105, E-1151): true when the player travelled to another place.
bool CryOmni3DEngine_China::map() {
	Graphics::ManagedSurface big, small;
	Sprite frame, point, exitButton, buildingsButton;
	if (!loadTga("INVENT/GRANPLAN.TGA", big) || !loadTga("INVENT/PETIPLAN.TGA", small) ||
	        !loadSprite("INVENT/CADRE.SPR", frame) || !loadSprite("INVENT/POINT.SPR", point) ||
	        !loadSprite("INVENT/SPIRS.SPR", exitButton) || !loadSprite("INVENT/ICO_BAT.SPR", buildingsButton) ||
	        big.w < kMapBigWidth || big.h < kMapBigHeight) {
		warning("China: the map cannot be loaded");
		return false;
	}

	// You are here (E-1150): the prefix table (last match wins), then the full names
	bool here = false;
	Common::Point herePos;
	const Common::String placeName(_place ? _place->name : "");
	for (uint i = 0; i < ARRAYSIZE(kMapPrefixPoints); i++) {
		if (placeName.size() >= 3 && !scumm_strnicmp(placeName.c_str(), kMapPrefixPoints[i].place, 3)) {
			here = true;
			herePos = Common::Point(kMapPrefixPoints[i].x + kMapWindowWidth, kMapPrefixPoints[i].y);
		}
	}
	for (uint i = 0; i < ARRAYSIZE(kMapPlacePoints); i++) {
		if (placeName.equalsIgnoreCase(kMapPlacePoints[i].place)) {
			here = true;
			herePos = Common::Point(kMapPlacePoints[i].x + kMapWindowWidth, kMapPlacePoints[i].y);
		}
	}
	// The window starts scrolled as if the cursor were on the marker; with no marker (the
	// original's position is uninitialised) at the top-left
	Common::Point framePos(435, 79);
	if (here) {
		framePos = Common::Point(CLIP<int16>(herePos.x, 435, 580), CLIP<int16>(herePos.y, 79, 312));
	}
	Common::Point offsets = mapOffsets(framePos);

	PuzzleInput in;
	puzzleStart(in);
	bool travelled = false;
	while (!shouldAbort()) {
		puzzlePoll(in);
		const Common::Point m = cursorTopLeft();
		// The cursor over the small map scrolls the big one (E-1105)
		if (m.x >= kMapWindowWidth && m.y <= 386) {
			framePos = Common::Point(CLIP<int16>(m.x, 435, 580), CLIP<int16>(m.y, 79, 312));
			offsets = mapOffsets(framePos);
		}

		_screen.blitFrom(big, Common::Rect(offsets.x, offsets.y, offsets.x + kMapWindowWidth, offsets.y + 480),
		                 Common::Point(0, 0));
		_screen.blitFrom(small, Common::Point(kMapWindowWidth, 0));
		_screen.transBlitFrom(frame.surface, Common::Point(framePos.x - 62, framePos.y - 79), frame.keyColor);
		if (here) {
			_screen.transBlitFrom(point.surface, herePos - Common::Point(2, 2), point.keyColor);
		}
		_screen.transBlitFrom(exitButton.surface, exitButton.pos, exitButton.keyColor);
		_screen.transBlitFrom(buildingsButton.surface, buildingsButton.pos, buildingsButton.keyColor);

		// The first hot spot under the cursor: its label and cursor (E-1151)
		int spot = -1;
		if (m.x <= kMapWindowWidth) {
			const int gx = m.x + offsets.x, gy = m.y + offsets.y;
			for (uint i = 0; i < ARRAYSIZE(kMapSpots) && spot < 0; i++) {
				const MapSpot &s = kMapSpots[i];
				if (gx >= s.left && gx <= s.right && gy >= s.top && gy <= s.bottom) {
					spot = i;
				}
			}
		}
		int cursor = 11;
		if (spot >= 0) {
			const MapSpot &s = kMapSpots[spot];
			Common::String text = label(s.key);
			if (text.empty()) {
				text = Common::String(" pas de label ") + s.key;
			}
			_fontManager.setCurrentFont(0); // Q-1154
			_fontManager.setForeColor(textColor(0xffff));
			_fontManager.displayStr(380, 425, text);
			if (s.type == 0 && mapTravelAllowed(*this, s)) {
				cursor = 8;
			} else if (s.type == 8) {
				cursor = 15;
			}
		}
		setCursorSprite(cursor);
		mapPresent();

		if (!in.press) {
			continue;
		}
		if (spot >= 0 && kMapSpots[spot].type == 0 && mapTravelAllowed(*this, kMapSpots[spot])) {
			if (findPlace(kMapSpots[spot].place)) {
				setAngles(kMapSpots[spot].alpha, 0.);
				gotoPlace(kMapSpots[spot].place);
				travelled = true;
				break;
			}
		} else if (spot >= 0 && kMapSpots[spot].type == 8) {
			// The documentation on the spot's key, then the current place again (E-1151)
			documentFiche(kMapSpots[spot].key);
			if (_place) {
				gotoPlace(_place->name);
			}
			puzzleStart(in);
			_cursorId = -1;
		} else if (spriteRect(exitButton).contains(m)) {
			break;
		} else if (spriteRect(buildingsButton).contains(m)) {
			const Common::Point p = mapBuildings();
			if (p.x >= 0) {
				// No clamping here (E-1152)
				framePos = p;
				offsets = mapOffsets(p);
			}
			puzzleStart(in);
		}
	}
	_cursorId = -1;
	return travelled;
}

} // End of namespace China
} // End of namespace CryOmni3D
