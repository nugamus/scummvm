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

#include "common/debug.h"

#include "graphics/font.h"
#include "graphics/managed_surface.h"

#include "ring/detection.h"
#include "ring/resources.h"
#include "ring/world.h"

namespace Ring {

void World::setUp(Sounds *sounds, int lr) {
	_sounds = sounds;
	_lr = lr;
	for (int zone = kZoneSY; zone <= kZoneN2; zone++) {
		uint count;
		const SetupCall *calls = zoneSetup(zone, count);
		for (uint i = 0; i < count; i++)
			apply(zone, calls[i]);
	}
}

Puzzle *World::puzzle(int id) {
	for (auto &p : _puzzles)
		if (p->id == id)
			return p.get();
	return nullptr;
}

Rotation *World::rotation(int id) {
	for (auto &r : _rotations)
		if (r->id == id)
			return r.get();
	return nullptr;
}

Object *World::object(int id) {
	for (auto &o : _objects)
		if (o->id == id)
			return o.get();
	return nullptr;
}

static float asFloat(int32 bits) {
	float f;
	memcpy(&f, &bits, 4);
	return f;
}

// A hot spot's rectangle as declared; some are empty or inverted and never match (0x4238b0).
static Common::Rect rectOf(const int32 *a) {
	Common::Rect r;
	r.left = a[0];
	r.top = a[1];
	r.right = a[2];
	r.bottom = a[3];
	return r;
}

void World::apply(int zone, const SetupCall &c) {
	const int32 *a = c.args;
	auto str = [&](int i) { return Common::String(c.strs[a[i]] ? c.strs[a[i]] : ""); };
	switch (c.call) {
	case kAddPuz: {
		Common::SharedPtr<Puzzle> p(new Puzzle());
		p->id = a[0];
		p->zone = zone;
		_puzzles.push_back(p);
		break;
	}
	case kAddRot: {
		Common::SharedPtr<Rotation> r(new Rotation());
		r->id = a[0];
		r->zone = zone;
		r->name = str(1);
		r->paused = a[2] != 0;
		r->layers = a[3];
		_rotations.push_back(r);
		break;
	}
	case kObjAddRotAcc: {
		Object *o = object(a[0]);
		Rotation *r = rotation(a[1]);
		if (!o || !r)
			break;
		Common::SharedPtr<Accessibility> acc(new Accessibility());
		acc->object = o->id;
		acc->hotSpot.rect = rectOf(a + 2);
		acc->hotSpot.enabled = a[6] != 0;
		acc->hotSpot.cursor = a[7];
		acc->hotSpot.value = a[8];
		o->accessibilities.push_back(acc);
		r->accessibilities.push_back(acc);
		break;
	}
	case kRotAddMovToRot:
	case kRotAddMovToPuz:
	case kPuzAddMovToRot:
	case kPuzAddMovToPuz: {
		int kind = c.call == kRotAddMovToRot ? 0 : c.call == kRotAddMovToPuz ? 1 : c.call == kPuzAddMovToRot ? 2 : 3;
		Common::Array<Movability> *list = nullptr;
		if (kind < 2) {
			if (Rotation *r = rotation(a[0]))
				list = &r->movabilities;
		} else if (Puzzle *p = puzzle(a[0])) {
			list = &p->movabilities;
		}
		if (!list)
			break;
		Movability m;
		m.target = a[1];
		m.kind = kind;
		m.ride = str(2);
		m.hotSpot.rect = rectOf(a + 3);
		m.hotSpot.enabled = a[7] != 0;
		m.hotSpot.cursor = a[8];
		m.hotSpot.value = a[9];
		list->push_back(m);
		break;
	}
	case kRotSetMovToRot:
	case kRotSetMovToPuz:
	case kPuzSetMovToRot: {
		Rotation *r = c.call == kPuzSetMovToRot ? nullptr : rotation(a[0]);
		Puzzle *p = c.call == kPuzSetMovToRot ? puzzle(a[0]) : nullptr;
		Common::Array<Movability> *list = r ? &r->movabilities : p ? &p->movabilities : nullptr;
		if (!list || (uint)a[1] >= list->size())
			break;
		Movability &m = (*list)[a[1]];
		if (c.call == kPuzSetMovToRot) {
			m.alpha2 = asFloat(a[2]);
			m.beta2 = asFloat(a[3]);
			m.ran2 = asFloat(a[4]);
			break;
		}
		m.alpha1 = asFloat(a[2]);
		m.beta1 = asFloat(a[3]);
		m.ran1 = asFloat(a[4]);
		m.turn = (byte)a[6];
		if (c.call == kRotSetMovToRot) {
			m.alpha2 = asFloat(a[7]);
			m.beta2 = asFloat(a[8]);
			m.ran2 = asFloat(a[9]);
		}
		break;
	}
	case kRotSetMovOff:
		if (Rotation *r = rotation(a[0]))
			for (int i = a[1]; i <= a[2] && i >= 0 && i < (int)r->movabilities.size(); i++)
				r->movabilities[i].hotSpot.enabled = false;
		break;
	case kPuzAddBgrImg:
		if (Puzzle *p = puzzle(a[0])) {
			p->background = str(1);
			p->bgX = a[2];
			p->bgY = a[3];
		}
		break;
	case kAddObj: {
		// Name and icon come from aObj.ini when present (spec/api.md); not needed yet.
		Common::SharedPtr<Object> o(new Object());
		o->id = a[0];
		o->icon = str(2);
		o->flags = (byte)a[3];
		_objects.push_back(o);
		break;
	}
	case kObjSetPasDraCur:
	case kObjSetActDraCur:
		if (Object *o = object(a[0])) {
			DragCursor &d = o->dragCursors[c.call == kObjSetActDraCur];
			d.offsetX = a[1];
			d.offsetY = a[2];
			d.frames = a[3];
			d.kind = a[4];
			d.fps = asFloat(a[5]);
		}
		break;
	case kObjAddPuzAcc: {
		Object *o = object(a[0]);
		Puzzle *p = puzzle(a[1]);
		if (!o || !p)
			break;
		Common::SharedPtr<Accessibility> acc(new Accessibility());
		acc->object = o->id;
		acc->hotSpot.rect = rectOf(a + 2);
		acc->hotSpot.enabled = a[6] != 0;
		acc->hotSpot.cursor = a[7];
		acc->hotSpot.value = a[8];
		o->accessibilities.push_back(acc);
		p->accessibilities.push_back(acc);
		break;
	}
	case kObjSetPuzAccKey:
		if (Object *o = object(a[0]))
			if ((uint)a[1] < o->accessibilities.size())
				o->accessibilities[a[1]]->hotSpot.key = a[2];
		break;
	case kObjAddPre:
		if (Object *o = object(a[0]))
			o->presentations.push_back(Presentation());
		break;
	case kObjPreAddImgToPuz: {
		Puzzle *p = puzzle(a[2]);
		if (!p || !object(a[0]))
			break;
		Common::SharedPtr<PuzzleImage> img(new PuzzleImage());
		img->object = a[0];
		img->presentation = a[1];
		img->zone = zone;
		img->file = str(3);
		img->x = a[4];
		img->y = a[5];
		img->active = a[6] != 0;
		img->drawType = (byte)a[7];
		img->priority = a[8];
		// Inserted before the first picture of higher priority (aPuzzle::AddPreImg).
		uint i = 0;
		while (i < p->images.size() && p->images[i]->priority <= img->priority)
			i++;
		p->images.insert_at(i, img);
		break;
	}
	case kObjPreAddTxtToPuz: {
		Puzzle *p = puzzle(a[2]);
		Object *o = object(a[0]);
		if (!p || !o || (uint)a[1] >= o->presentations.size())
			break;
		Common::SharedPtr<PuzzleText> t(new PuzzleText());
		t->object = a[0];
		t->presentation = a[1];
		t->text = str(3);
		t->x = a[4];
		t->y = a[5];
		t->font = a[6];
		for (int i = 0; i < 3; i++) {
			t->color[i] = (byte)a[7 + i];
			t->background[i] = (byte)a[10 + i];
		}
		t->opaque = !(a[10] == -1 && a[11] == -1 && a[12] == -1);
		o->presentations[a[1]].texts.push_back(t);
		p->texts.push_back(t);
		break;
	}
	case kObjPreSho:
		showPresentation(a[0], c.argc > 1 ? a[1] : -1, true);
		break;
	case kSouAdd:
		if (_sounds)
			_sounds->add(a[0], a[1], str(2));
		break;
	case kSouSet:
		if (_sounds)
			_sounds->setVolume(a[0], a[1]);
		break;
	case kPuzAddAmbSou:
	case kRotAddAmbSou:
	case kPuzAdd3DSou:
	case kRotAdd3DSou: {
		// (owner, sound, volume, pan, same, leave, fade) or (owner, sound, same, leave, fade, volume, angle, amplitude)
		bool puz = c.call == kPuzAddAmbSou || c.call == kPuzAdd3DSou;
		Puzzle *p = puz ? puzzle(a[0]) : nullptr;
		Rotation *r = puz ? nullptr : rotation(a[0]);
		if ((!p && !r) || a[c.call == kPuzAddAmbSou || c.call == kRotAddAmbSou ? 6 : 4] < 2)
			break;
		Common::SharedPtr<SoundItem> item(new SoundItem());
		item->sound = a[1];
		if (c.call == kPuzAddAmbSou || c.call == kRotAddAmbSou) {
			item->volume = a[2];
			item->pan = a[3];
			item->sameMode = a[4];
			item->leaveMode = a[5];
			item->fade = a[6] - 1;
		} else {
			item->sameMode = a[2];
			item->leaveMode = a[3];
			item->fade = a[4] - 1;
			item->volume = a[5];
			float angle = asFloat(a[6]);
			if (a[7] >= 0 && a[7] <= 100)
				item->amplitude = a[7];
			if (angle >= -360.0f && angle <= 360.0f)
				item->offset = _lr * angle * 0.0174532889f;
			// A rotation's pan from its current angle (unset before the zone sets it), a puzzle's from 0.
			item->pan = item->pan3D(r ? r->alpha + 135.0f : 0.0f, _lr);
		}
		(p ? p->sounds : r->sounds).push_back(item);
		break;
	}
	case kPuzSetAmbSouOff:
	case kRotSetAmbSouOff:
	case kPuzSet3DSouOff:
	case kRotSet3DSouOff: {
		bool puz = c.call == kPuzSetAmbSouOff || c.call == kPuzSet3DSouOff;
		SoundItems *list = puz ? (puzzle(a[0]) ? &puzzle(a[0])->sounds : nullptr) : (rotation(a[0]) ? &rotation(a[0])->sounds : nullptr);
		if (list)
			for (auto &i : *list)
				if (i->sound == a[1])
					i->active = false;
		break;
	}
	case kObjSetAccOff:
		setAccessibilities(a[0], false, a[1], a[2]);
		break;
	default:
		break; // ponytail: rotations, animations, sounds and variables come with their specs
	}
}

bool World::shown(int id, int presentation) {
	Object *o = object(id);
	return o && (uint)presentation < o->presentations.size() && o->presentations[presentation].shown;
}

PuzzleText *World::text(int id, int presentation, int index) {
	Object *o = object(id);
	if (!o || (uint)presentation >= o->presentations.size())
		return nullptr;
	Presentation &pr = o->presentations[presentation];
	return (uint)index < pr.texts.size() ? pr.texts[index].get() : nullptr;
}

void World::showPresentation(int id, int presentation, bool shown) {
	Object *o = object(id);
	if (!o)
		return;
	for (uint i = 0; i < o->presentations.size(); i++)
		if (presentation < 0 || (int)i == presentation)
			o->presentations[i].shown = shown;
}

PuzzleImage *World::image(int id, int presentation, int index) {
	for (auto &p : _puzzles)
		for (auto &img : p->images)
			if (img->object == id && img->presentation == presentation && index-- == 0)
				return img.get();
	return nullptr;
}

void World::hideAndFree(int id) {
	showPresentation(id, -1, false);
	for (auto &p : _puzzles)
		for (auto &img : p->images)
			if (img->object == id)
				img->image.reset();
}

void World::setAccessibilities(int id, bool on, int from, int to) {
	Object *o = object(id);
	if (!o)
		return;
	for (uint i = 0; i < o->accessibilities.size(); i++)
		if (from < 0 || ((int)i >= from && (int)i <= to))
			o->accessibilities[i]->hotSpot.enabled = on;
}

void World::draw(Puzzle &p, Resources &res, Graphics::ManagedSurface &dst) {
	if (!p.background.empty()) {
		if (!p.bgImage)
			p.bgImage.reset(res.loadImage(p.zone, p.background, true));
		if (p.bgImage)
			p.bgImage->draw(dst, p.bgX, p.bgY, 1);
	}
	for (auto &img : p.images) {
		if (!img->active || !shown(img->object, img->presentation))
			continue;
		if (!img->image)
			img->image.reset(res.loadImage(img->zone, img->file, true));
		if (img->image)
			img->image->draw(dst, img->x, img->y, img->drawType);
	}
	for (auto &t : p.texts) {
		// Only font 1 exists (spec/text.md); GDI's TextOutA from the cell's top left.
		if (t->font != 1 || !_font || t->text.empty() || !shown(t->object, t->presentation))
			continue;
		if (t->opaque)
			dst.fillRect(Common::Rect(t->x, t->y, t->x + _font->getStringWidth(t->text), t->y + _font->getFontHeight()),
						 dst.format.RGBToColor(t->background[0], t->background[1], t->background[2]));
		_font->drawString(&dst, t->text, t->x, t->y, dst.w - t->x, dst.format.RGBToColor(t->color[0], t->color[1], t->color[2]));
	}
}

const Accessibility *World::hit(const Common::Array<Common::SharedPtr<Accessibility> > &list, int x, int y) {
	for (auto &acc : list)
		if (acc->hotSpot.contains(x, y))
			return acc.get();
	return nullptr;
}

const Movability *World::hit(const Common::Array<Movability> &list, int x, int y) {
	for (auto &m : list)
		if (m.hotSpot.contains(x, y))
			return &m;
	return nullptr;
}

const Accessibility *World::hit(const Puzzle &p, int x, int y) const {
	for (auto &acc : p.accessibilities) {
		if (!acc->hotSpot.contains(x, y))
			continue;
		if (p.mode == 2 && acc->object != p.modeObject)
			return nullptr;
		return acc.get();
	}
	return nullptr;
}

} // End of namespace Ring
