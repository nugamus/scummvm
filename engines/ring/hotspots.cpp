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

// The hotspot overlay (H), a development view, not the original's: every enabled
// accessibility and movability of the current place and of the dialogue layer (puzzle 1)
// is tinted and outlined, with ScummVM's marker and the object's name at its centre. The
// screen is sampled through the same search as the cursor tracking (spec/cursor.md), so a
// rotation's hot spots show where the panorama puts them.

#include "common/config-manager.h"
#include "common/hashmap.h"
#include "common/system.h"

#include "ring/bag.h"
#include "ring/ring.h"
#include "ring/world.h"

namespace Ring {

enum {
	kCell = 4, kGridW = 640 / kCell, kGridH = 480 / kCell,
	kAccessibility = 1, kMovability = 1000, kDialogue = 2000
};

int RingEngine::hotSpotAt(int x, int y) {
	if (Puzzle *p1 = _world->puzzle(1)) {
		for (uint i = 0; i < p1->accessibilities.size(); i++) {
			const Accessibility &acc = *p1->accessibilities[i];
			if (acc.hotSpot.contains(x, y))
				return p1->mode == 2 && acc.object != p1->modeObject ? 0 : kDialogue + i;
		}
		if (p1->mode == 2)
			return 0;
	}
	const Common::Array<Common::SharedPtr<Accessibility> > *accessibilities;
	const Common::Array<Movability> *movabilities;
	Rotation *r = _mode == 1 ? _world->rotation(_rotation) : nullptr;
	if (r && r->panorama && !r->paused) {
		if (y < 16 || y >= 16 + RotationView::kHeight)
			return 0;
		Common::Point pt = _view.toPanorama(*r->panorama, x, y);
		x = pt.x;
		y = pt.y;
		accessibilities = &r->accessibilities;
		movabilities = &r->movabilities;
	} else if (Puzzle *p = _world->puzzle(_puzzle)) {
		accessibilities = &p->accessibilities;
		movabilities = &p->movabilities;
	} else {
		return 0;
	}
	for (uint i = 0; i < accessibilities->size(); i++)
		if ((*accessibilities)[i]->hotSpot.contains(x, y))
			return kAccessibility + i;
	for (uint i = 0; i < movabilities->size(); i++)
		if ((*movabilities)[i].hotSpot.contains(x, y))
			return kMovability + i;
	return 0;
}

Graphics::HotspotInfo RingEngine::hotSpotInfo(int id, const Common::Point &at) {
	const Accessibility *acc = nullptr;
	if (id >= kDialogue) {
		acc = _world->puzzle(1)->accessibilities[id - kDialogue].get();
	} else if (id < kMovability) {
		Rotation *r = _mode == 1 ? _world->rotation(_rotation) : nullptr;
		acc = r ? r->accessibilities[id - kAccessibility].get() : _world->puzzle(_puzzle)->accessibilities[id - kAccessibility].get();
	}
	if (!acc)
		return Graphics::HotspotInfo(at, Common::U32String(), Graphics::kHotspotExit);
	Object *o = _world->object(acc->object);
	Common::String name = o ? o->name : "";
	return Graphics::HotspotInfo(at, Common::U32String(name, Common::kWindows1252), Graphics::kHotspotObject);
}

void RingEngine::drawHotspotRegions() {
	_hotspotList.clear();
	if (!_showHotspots || _bag->shown())
		return;
	static int ids[kGridH][kGridW];
	for (int gy = 0; gy < kGridH; gy++)
		for (int gx = 0; gx < kGridW; gx++)
			ids[gy][gx] = hotSpotAt(gx * kCell + kCell / 2, gy * kCell + kCell / 2);
	struct Sum {
		int n = 0, x = 0, y = 0;
	};
	Common::HashMap<int, Sum> sums;
	Common::Array<int> order;
	const uint16 yellow = (uint16)_screen.format.RGBToColor(255, 255, 0);
	auto other = [&](int gx, int gy, int id) {
		return gx < 0 || gy < 0 || gx >= kGridW || gy >= kGridH || ids[gy][gx] != id;
	};
	for (int gy = 0; gy < kGridH; gy++) {
		for (int gx = 0; gx < kGridW; gx++) {
			int id = ids[gy][gx];
			if (!id)
				continue;
			int x0 = gx * kCell, y0 = gy * kCell;
			// A quarter of yellow over the picture (RGB565).
			for (int y = y0; y < y0 + kCell; y++) {
				uint16 *p = (uint16 *)_screen.getBasePtr(x0, y);
				for (int x = 0; x < kCell; x++) {
					uint16 c = p[x];
					int r = (c >> 11) * 3 + 31, g = ((c >> 5) & 63) * 3 + 63, b = (c & 31) * 3;
					p[x] = (uint16)(((r / 4) << 11) | ((g / 4) << 5) | (b / 4));
				}
			}
			if (other(gx, gy - 1, id))
				_screen.hLine(x0, y0, x0 + kCell - 1, yellow);
			if (other(gx, gy + 1, id))
				_screen.hLine(x0, y0 + kCell - 1, x0 + kCell - 1, yellow);
			if (other(gx - 1, gy, id))
				_screen.vLine(x0, y0, y0 + kCell - 1, yellow);
			if (other(gx + 1, gy, id))
				_screen.vLine(x0 + kCell - 1, y0, y0 + kCell - 1, yellow);
			if (!sums.contains(id))
				order.push_back(id);
			Sum &s = sums[id];
			s.n++;
			s.x += x0 + kCell / 2;
			s.y += y0 + kCell / 2;
		}
	}
	for (int id : order) {
		const Sum &s = sums[id];
		_hotspotList.push_back(hotSpotInfo(id, Common::Point(s.x / s.n, s.y / s.n)));
	}
}

void RingEngine::toggleHotspots() {
	if (ConfMan.hasKey("enable_hotspots") && !ConfMan.getBool("enable_hotspots"))
		return;
	showHotspots(!_showHotspots);
	_shownHotspots.clear();
}

void RingEngine::getHotspotPositions(Common::Array<Graphics::HotspotInfo> &hotspots) {
	hotspots = _hotspotList;
}

void RingEngine::drawHotspots() {
	// The overlay is drawn again only when the markers change.
	if (!_showHotspots)
		return;
	bool same = _hotspotList.size() == _shownHotspots.size();
	for (uint i = 0; same && i < _hotspotList.size(); i++)
		same = _hotspotList[i].position == _shownHotspots[i].position && _hotspotList[i].name == _shownHotspots[i].name;
	if (same && !_hotspotForceRedraw)
		return;
	_shownHotspots = _hotspotList;
	if (_hotspotList.empty()) {
		_hotspotForceRedraw = false;
		if (_system->isOverlayVisible())
			_system->hideOverlay();
		return;
	}
	Engine::drawHotspots();
}

} // End of namespace Ring
