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

// Agent adapter for scummvm-agent-bridge (BRIDGE commit, agent-bridge branch only): a file
// mailbox that reports the engine's own state as JSON (schema/observation.json). Active
// only with the game key agent_mailbox=<folder> (set by the bridge; not a metaengine option).
// Reads existing state; changes nothing.
//   req-<n>.json {"cmd": "describe"}  ->  rep-<n>.json
//   req-<n>.json {"cmd": "plan", "verb": ...}  ->  what to hold during the next valid_ms (tier 2)
//   req-<n>.json {"cmd": "save" | "load", "slot": n}  ->  the engine's own save / load
// ScummVM has no file delete: the client removes answered requests, the adapter skips
// numbers up to the last one it answered.

#include "common/algorithm.h"
#include "common/array.h"
#include "common/config-manager.h"
#include "common/file.h"
#include "common/formats/json.h"
#include "common/fs.h"
#include "common/system.h"

#include "grumpa/events.h"
#include "grumpa/grumpa.h"

namespace Grumpa {

namespace {

// Per-verb state of the plan command (tier 2); reset when the request differs or the verb ended.
struct PlanState {
	Common::String key;
	bool init = false;
	int phase = 0;           // 0 approach, 1 stop, 2 equip, 3 aim, 4 click, 5 verify
	int scene = -1;
	uint32 start = 0, progress = 0, phaseT = 0;
	float best = 1e9f;
	int retries = 0;
	int lines = 0;           // voice lines started when the verb began
	bool pressed = false;    // the last plan held the left button
	bool rightLast = false;  // the last plan held the right button
	bool fight = false;      // attack: in the stance, swinging
	bool pulse = false;
	bool sceneOk = false;    // a scene change is the goal
	bool trigWas = false;    // the target trigger's spent flag when the click was made
	bool existed = false;
	bool clicked = false;    // a click was made
	uint32 sig = 0;          // the scene's switches when the click was made
};

struct Snapshot {
	PlanState plan;
	int lineCount = 0;       // voice lines seen start (for talk)
	bool valid = false;
	int scene = -1;
	Common::Array<int> itemState;   // by index of Inventory::items()
	Common::Array<int> life;        // by index of Characters::all(), 0x7fffffff: no Life slot
	Common::Array<int> speaking;    // the scene sounds of a speaker that play
	int lastHandled = 0;            // requests are numbered 1, 2, ... and sent one at a time
	Common::Array<Common::JSONValue *> events;  // queued, oldest first

	~Snapshot() {
		for (uint i = 0; i < events.size(); i++)
			delete events[i];
	}
};

Snapshot *g_snap = nullptr;  // allocated on first use, freed by agentFree() with the engine

typedef Common::JSONObject Obj;
typedef Common::JSONArray Arr;
typedef Common::JSONValue JV;

JV *num(double v) { return new JV(v); }
JV *str(const Common::String &s) { return new JV(s); }
JV *flag(bool b) { return new JV(b); }

JV *box(const Common::Rect &r) {
	Arr a;
	a.push_back(num(r.left));
	a.push_back(num(r.top));
	a.push_back(num(r.right));
	a.push_back(num(r.bottom));
	return new JV(a);
}

bool onScreen(const Common::Rect &r) {
	return r.width() > 0 && r.height() > 0 && r.right > 0 && r.bottom > 0 &&
		   r.left < kScreenWidth && r.top < kScreenHeight;
}

void queueEvent(const char *type, int id, const char *text) {
	if (g_snap->events.size() >= 256) {
		delete g_snap->events.front();
		g_snap->events.remove_at(0);
	}
	if (!strcmp(type, "line"))
		g_snap->lineCount++;
	Obj o;
	o["t"] = num(g_system->getMillis());
	o["type"] = str(type);
	o["id"] = num(id);
	if (text)
		o["text"] = str(text);
	g_snap->events.push_back(new JV(o));
}

Common::String stem(Common::String s) {
	if (s.size() > 4 && s[s.size() - 4] == '.')
		s = Common::String(s.c_str(), s.size() - 4);
	return s;
}

// Compare the live state with the last snapshot and queue the events (item gained or lost,
// scene change, voice line started, damage, death).
void diffState(GrumpaEngine &vm, int scene, bool inScene) {
	Snapshot &s = *g_snap;
	if (scene != s.scene) {
		if (s.valid && scene >= 0)
			queueEvent("scene", scene, nullptr);
		s.scene = scene;
	}
	Common::Array<Inventory::Item> &items = vm.inventory().items();
	s.itemState.resize(items.size());
	for (uint i = 0; i < items.size(); i++) {
		const int st = items[i].state;
		const bool had = s.itemState[i] == Inventory::kCarried || s.itemState[i] == Inventory::kHeld;
		const bool has = st == Inventory::kCarried || st == Inventory::kHeld;
		if (s.valid && has != had)
			queueEvent("item", items[i].id, has ? "gained" : "lost");
		s.itemState[i] = st;
	}
	const Common::Array<Character> &all = vm.characters().all();
	s.life.resize(all.size());
	for (uint i = 0; i < all.size(); i++) {
		const int life = all[i].state.size() > 1 ? all[i].state[1] : 0x7fffffff;  // slot 1 is Life (E-0407)
		if (s.valid && s.life[i] != 0x7fffffff && life < s.life[i] && vm.characters().present(all[i]))
			queueEvent(life <= 0 ? "death" : "damage", all[i].id, nullptr);
		s.life[i] = life;
	}
	Common::Array<int> now;
	if (inScene) {
		const Common::Array<SceneSound> &sounds = vm.sceneData().sounds;
		for (uint i = 0; i < sounds.size(); i++)
			if (sounds[i].speaker && vm.voices().playing(sounds[i].id)) {
				now.push_back(sounds[i].id);
				bool was = false;
				for (uint j = 0; j < s.speaking.size(); j++)
					was |= (uint)s.speaking[j] == sounds[i].id;
				if (!was)
					queueEvent("line", sounds[i].id, sounds[i].name.c_str());
			}
	}
	s.speaking = now;
	s.valid = true;
}

// The observation of the schema; the caller owns the value.
JV *describe(GrumpaEngine &vm, bool inScene, int sceneNum, const Common::String &cursor, bool stance) {
	Obj root;
	root["schema"] = num(1);
	root["engine"] = str("grumpa");
	root["tier"] = num(2);
	Obj scr;
	scr["w"] = num(kScreenWidth);
	scr["h"] = num(kScreenHeight);
	root["screen"] = new JV(scr);
	root["mode"] = str(inScene ? "scene" : "menu");

	Arr entities, hotspots, invItems, subs, events;
	Obj inv;
	SceneData &sd = vm.sceneData();
	if (inScene && !sd.views.empty()) {
		Obj sc;
		sc["id"] = num(sceneNum);
		sc["view"] = num(sd.view);
		root["scene"] = new JV(sc);
		const Camera &cam = sd.views[sd.view].cam;

		// Characters present in the scene; the box is the body rectangle:
		// placeCharacter + characterRect.
		Character *pl = vm.characters().player();
		Common::Array<Character> &cs = vm.characters().list();
		for (uint i = 0; i < cs.size(); i++) {
			Character &c = cs[i];
			if (!vm.characters().present(c))
				continue;
			Common::Rect body;
			if (const Mesh *clip = vm.characters().mesh(c)) {
				Mesh placed;
				GrumpaEngine::placeCharacter(c, *clip, CLIP(c.frame, 0, clip->frames - 1), placed);
				body = vm.characterRect(c, placed);
			}
			Obj e;
			e["id"] = num(c.id);
			e["name"] = str(c.anims.empty() ? Common::String::format("character%u", c.id) : stem(c.anims[0]));
			const int role = c.role();  // 1 the player's, 2 the follower's, 3..7 a fighter's (E-1530)
			e["kind"] = str(&c == pl ? "player" : role == 2 ? "companion" : role >= 3 ? "enemy" : "character");
			Arr p;
			p.push_back(num(c.pos.x));
			p.push_back(num(c.pos.y));
			p.push_back(num(c.pos.z));
			e["pos"] = new JV(p);
			e["heading"] = num(c.yaw);
			e["box"] = box(body);
			e["visible"] = flag(onScreen(body));
			if (c.state.size() > 1)
				e["health"] = num(c.state[1]);
			e["action"] = str(Common::String::format("clip %d", c.clip));
			if (&c == pl)
				root["player"] = new JV(e);
			else
				entities.push_back(new JV(e));
		}
		// Mesh actors of the scene (platforms, snake, boulder ...): props.
		for (uint i = 0; i < sd.meshes.size(); i++) {
			const SceneMesh &m = sd.meshes[i];
			if (!m.visible || m.mesh.empty())
				continue;
			Common::Rect r = screenRect(m.mesh, cam);
			Obj e;
			e["id"] = num(m.id);
			e["name"] = str(stem(m.anb));
			e["kind"] = str("prop");
			e["box"] = box(r);
			e["visible"] = flag(onScreen(r));
			entities.push_back(new JV(e));
		}
		// Items lying in the scene (inventory.md): entities and "take" hotspots, on the
		// rectangle the last draw left (the click rectangle widens a thin one).
		Common::Array<Inventory::Item> &items = vm.inventory().items();
		for (uint i = 0; i < items.size(); i++) {
			const Inventory::Item &it = items[i];
			if (!it.visible || it.scene != sceneNum || it.state != Inventory::kInScene)
				continue;
			Obj e;
			e["id"] = num(it.id);
			e["name"] = str(stem(it.mesh));
			e["kind"] = str("item");
			Arr p;
			for (int k = 0; k < 3; k++)
				p.push_back(num(it.pos[k]));
			e["pos"] = new JV(p);
			e["box"] = box(it.rect);
			e["visible"] = flag(onScreen(it.rect));
			entities.push_back(new JV(e));
			Obj h;
			h["id"] = num(it.id);
			h["name"] = str(stem(it.mesh));
			h["kind"] = str("take");
			h["box"] = box(Inventory::clickRect(it.rect));
			h["enabled"] = flag(it.rect.width() > 0);
			hotspots.push_back(new JV(h));
		}
		// Triggers of the current view, as the H overlay draws them.
		for (uint i = 0; i < sd.triggers.size(); i++) {
			const SceneTrigger &tr = sd.triggers[i];
			if (tr.poly.size() < 2 || (tr.view >= 0 && tr.view != sd.view))
				continue;
			Obj h;
			h["id"] = num(tr.id);
			h["name"] = str(Common::String::format("trigger %u", tr.id));
			const char *kind = "trigger";
			for (uint k = 0; k < tr.cmds.size(); k++)
				if (tr.cmds[k].targetId == 185 && tr.cmds[k].opcode == 31) {  // go to scene (E-0116)
					kind = "exit";
					h["target"] = num(tr.cmds[k].arg1);
				}
			h["kind"] = str(kind);
			Arr poly;
			Common::Rect r(tr.poly[0].x, tr.poly[0].y, tr.poly[0].x + 1, tr.poly[0].y + 1);
			for (uint k = 0; k < tr.poly.size(); k++) {
				Arr pt;
				pt.push_back(num(tr.poly[k].x));
				pt.push_back(num(tr.poly[k].y));
				poly.push_back(new JV(pt));
				r.extend(Common::Rect(tr.poly[k].x, tr.poly[k].y, tr.poly[k].x + 1, tr.poly[k].y + 1));
			}
			h["poly"] = new JV(poly);
			h["box"] = box(r);
			h["enabled"] = flag(vm.events().triggerClickable(tr));
			hotspots.push_back(new JV(h));
		}
		// Voice lines playing now.
		for (uint i = 0; i < sd.sounds.size(); i++) {
			if (!sd.sounds[i].speaker || !vm.voices().playing(sd.sounds[i].id))
				continue;
			Obj o;
			o["speaker"] = num(sd.sounds[i].speaker);
			o["sound"] = str(sd.sounds[i].name);
			subs.push_back(new JV(o));
		}
	}
	Common::Array<Inventory::Item> &items = vm.inventory().items();
	for (uint i = 0; i < items.size(); i++) {
		const Inventory::Item &it = items[i];
		if (it.state != Inventory::kCarried && it.state != Inventory::kHeld)
			continue;
		Obj o;
		o["id"] = num(it.id);
		o["name"] = str(stem(it.mesh));
		o["state"] = num(it.state);
		invItems.push_back(new JV(o));
	}
	inv["items"] = new JV(invItems);
	inv["held"] = vm.inventory().held() >= 0 ? num(vm.inventory().held()) : new JV();
	inv["open"] = flag(vm.inventory().shown());
	for (uint i = 0; i < g_snap->events.size(); i++)
		events.push_back(g_snap->events[i]);  // now owned by the observation
	g_snap->events.clear();

	// Variables: the non-zero global flags (g240..g279, Actors/global.atx) and the player's
	// attributes (state slots 1 life, 2 attack, 3 defence; combat.md).
	Obj vars;
	for (int g = 240; g <= 279; g++) {
		Common::Array<SceneCond> c;  // "global g != 0" through the VM's own condition test
		SceneCond sc;
		sc.id = g;
		sc.mode = 3;
		c.push_back(sc);
		if (vm.events().conditionsHold(c))
			vars[Common::String::format("g%d", g)] = num(1);
	}
	if (Character *pc = vm.characters().player()) {
		if (pc->state.size() > 3) {
			vars["life"] = num(pc->state[1]);
			vars["attack"] = num(pc->state[2]);
			vars["defence"] = num(pc->state[3]);
		}
	}
	root["vars"] = new JV(vars);
	root["entities"] = new JV(entities);
	root["hotspots"] = new JV(hotspots);
	root["inventory"] = new JV(inv);
	root["subtitles"] = new JV(subs);
	root["events"] = new JV(events);
	Obj es;
	es["cursor"] = str(cursor);
	es["cursor_state"] = num(vm.cursorState());
	es["stance"] = flag(stance);
	root["engine_state"] = new JV(es);
	return new JV(root);
}

// ---- Tier 2: verbs as input plans ------------------------------------------------------------
// The adapter only says what to hold during the next valid_ms; the bridge presses it through the
// real input path and asks again, so every plan comes from the current state.

struct Out {
	int cx = -1, cy = -1;          // the cursor (-1: leave it)
	bool left = false, right = false, ctrl = false, shift = false, space = false;
	int clickX = -1, clickY = -1;  // a one-shot left click
	int valid = 60;
};

JV *pt2(int x, int y) {
	Arr a;
	a.push_back(num(x));
	a.push_back(num(y));
	return new JV(a);
}

JV *screenArr() { return pt2(kScreenWidth, kScreenHeight); }

JV *emit(const Out &o, const char *reason) {
	if (strcmp(reason, "walking"))
		debug(1, "Grumpa agent: t=%u %s", g_system->getMillis(), reason);
	Obj r;
	r["done"] = flag(false);
	r["screen"] = screenArr();
	if (o.cx >= 0)
		r["cursor"] = pt2(o.cx, o.cy);
	Arr b, k;
	if (o.left)
		b.push_back(str("left"));
	if (o.right)
		b.push_back(str("right"));
	if (o.ctrl)
		k.push_back(str("ctrl"));
	if (o.shift)
		k.push_back(str("shift"));
	if (o.space)
		k.push_back(str("space"));
	r["buttons"] = new JV(b);
	r["keys"] = new JV(k);
	if (o.clickX >= 0)
		r["click"] = pt2(o.clickX, o.clickY);
	r["valid_ms"] = num(o.valid);
	r["reason"] = str(reason);
	return new JV(r);
}

JV *finish(PlanState &st, bool ok, const char *reason) {
	if (!ok)
		warning("Grumpa agent: verb failed: %s", reason);
	debug(1, "Grumpa agent: verb ended ok=%d (%s)", ok, reason);
	st = PlanState();  // the next request starts afresh
	Obj r;
	r["done"] = flag(true);
	r["ok"] = flag(ok);
	r["reason"] = str(reason);
	r["screen"] = screenArr();
	return new JV(r);
}

double argNum(JV *req, const char *name, double def) {
	return req->hasChild(name) && req->child(name)->isNumber() ? req->child(name)->asNumber() : def;
}

bool inPoly(const Common::Array<Common::Point> &poly, int x, int y) {
	bool in = false;
	for (uint i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
		if ((poly[i].y > y) != (poly[j].y > y) &&
			x < (poly[j].x - poly[i].x) * (y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)
			in = !in;
	return in;
}

// A point inside the polygon, nearest its vertex mean.
Common::Point insidePoint(const Common::Array<Common::Point> &poly) {
	int sx = 0, sy = 0;
	for (uint i = 0; i < poly.size(); i++)
		sx += poly[i].x, sy += poly[i].y;
	Common::Point c(sx / (int)poly.size(), sy / (int)poly.size());
	if (inPoly(poly, c.x, c.y))
		return c;
	Common::Point best = poly[0];
	int bd = 1 << 30;
	for (int y = 0; y < kScreenHeight; y += 4)
		for (int x = 0; x < kScreenWidth; x += 4)
			if (inPoly(poly, x, y) && (x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) < bd) {
				bd = (x - c.x) * (x - c.x) + (y - c.y) * (y - c.y);
				best = Common::Point(x, y);
			}
	return best;
}

// The player's steering point: the centre of the body rectangle (walk.cpp, E-1850).
Common::Point bodyPoint(GrumpaEngine &vm, Character &c) {
	Common::Rect body;
	if (const Mesh *clip = vm.characters().mesh(c)) {
		Mesh placed;
		GrumpaEngine::placeCharacter(c, *clip, CLIP(c.frame, 0, clip->frames - 1), placed);
		body = vm.characterRect(c, placed);
	}
	return Common::Point((body.left + body.right) / 2, (body.top + body.bottom) / 2);
}

// The cursor that makes the player head for world (tx, tz): from the body centre along the
// heading (walking.md turn), kept off hotspots, which swallow a press.
void aimCursor(GrumpaEngine &vm, Character &p, float tx, float tz, Out &o) {
	const Common::Point b = bodyPoint(vm, p);
	const float phi = vm.viewYaw() - atan2f(tx - p.pos.x, tz - p.pos.z);
	const float ux = sinf(phi), uy = cosf(phi);
	float r = 200;
	if (ux > 1e-4f) r = MIN(r, (kScreenWidth - 2 - b.x) / ux);
	if (ux < -1e-4f) r = MIN(r, (1 - b.x) / ux);
	if (uy > 1e-4f) r = MIN(r, (kScreenHeight - 2 - b.y) / uy);
	if (uy < -1e-4f) r = MIN(r, (1 - b.y) / uy);
	r = MAX(r, 30.0f);
	for (int tries = 0; tries < 4; tries++) {
		o.cx = CLIP((int)(b.x + ux * r), 0, kScreenWidth - 1);
		o.cy = CLIP((int)(b.y + uy * r), 0, kScreenHeight - 1);
		if (!vm.overHotspot(Common::Point(o.cx, o.cy)))
			return;
		r = MAX(r * 0.6f, 30.0f);
	}
}

// What a verb points at: a position, a sphere to reach and a click point.
struct Target {
	bool found = false;
	int kind = 0;              // 1 item, 2 character, 3 trigger, 4 mesh
	Vec3 pos;                  // where to walk (kinds 1..3)
	Vec3 centre;               // trigger sphere
	float radius = 0;
	bool hasClick = false;
	Common::Point click;
	const Character *c = nullptr;
	const SceneTrigger *trig = nullptr;
	int exitScene = -1;
};

Target resolve(GrumpaEngine &vm, int sceneNum, int id) {
	Target t;
	Common::Array<Inventory::Item> &items = vm.inventory().items();
	for (uint i = 0; i < items.size(); i++) {
		const Inventory::Item &it = items[i];
		if (it.id != id || it.scene != sceneNum || it.state != Inventory::kInScene)
			continue;
		t.found = true;
		t.kind = 1;
		t.pos = Vec3(it.pos[0], it.pos[1], it.pos[2]);
		if (it.rect.width() > 0) {
			t.hasClick = true;
			const Common::Rect r = Inventory::clickRect(it.rect);
			t.click = Common::Point((r.left + r.right) / 2, (r.top + r.bottom) / 2);
		}
		return t;
	}
	Common::Array<Character> &cs = vm.characters().list();
	for (uint i = 0; i < cs.size(); i++) {
		if ((int)cs[i].id != id || !vm.characters().present(cs[i]))
			continue;
		t.found = true;
		t.kind = 2;
		t.c = &cs[i];
		t.pos = cs[i].pos;
		t.hasClick = true;
		t.click = bodyPoint(vm, cs[i]);
		return t;
	}
	SceneData &sd = vm.sceneData();
	for (uint i = 0; i < sd.triggers.size(); i++) {
		const SceneTrigger &tr = sd.triggers[i];
		if ((int)tr.id != id || tr.poly.size() < 3 || (tr.view >= 0 && tr.view != sd.view))
			continue;  // a trigger of another view is not on the screen
		t.found = true;
		t.kind = 3;
		t.trig = &tr;
		t.centre = tr.centre;
		t.radius = tr.radius;
		t.pos = tr.centre;
		t.hasClick = vm.events().triggerClickable(tr);
		t.click = insidePoint(tr.poly);
		for (uint k = 0; k < tr.cmds.size(); k++)
			if (tr.cmds[k].targetId == 185 && tr.cmds[k].opcode == 31)
				t.exitScene = tr.cmds[k].arg1;
		return t;
	}
	for (uint i = 0; i < sd.meshes.size(); i++)
		if ((int)sd.meshes[i].id == id && sd.meshes[i].visible && !sd.meshes[i].mesh.empty()) {
			const Common::Rect r = screenRect(sd.meshes[i].mesh, sd.views[sd.view].cam);
			t.found = true;
			t.kind = 4;
			t.hasClick = r.width() > 0;
			t.click = Common::Point((r.left + r.right) / 2, (r.top + r.bottom) / 2);
			return t;
		}
	return t;
}

float dist2(const Vec3 &a, const Vec3 &b) { return sqrtf((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z)); }
float dist3(const Vec3 &a, const Vec3 &b) { return sqrtf((a - b).dot(a - b)); }

// Close enough to act on the target: `r` > 0 is a horizontal distance, else the verb's own
// range (item reach 160 with a margin, spheres touching, the reaction spheres for talk).
bool arrived(const Character &p, const Target &t, float r, bool reaction) {
	if (t.kind == 4)
		return true;
	if (r > 0)
		return dist2(p.pos, t.pos) < r;
	switch (t.kind) {
	case 1:
		return dist3(p.pos, t.pos) <= 140.0f;  // inventory.md: 160
	case 2:
		if (reaction && p.reach > 0 && t.c->reach > 0)
			return dist3(p.pos, t.c->pos) < (p.reach + t.c->reach) * 0.9f;
		return dist3(p.pos, t.c->pos) < (p.radius + t.c->radius) * 0.95f;
	default:
		return !t.trig->proximity || Characters::touches(p, t.centre, t.radius);
	}
}

// One round of walking to world (tx, tz): the left button, steered; re-pressed when a fidget
// swallowed the press.
void walkStep(GrumpaEngine &vm, Character &p, float tx, float tz, bool run, PlanState &st, Out &o) {
	aimCursor(vm, p, tx, tz, o);
	const bool walking = p.clip == 1 || p.clip == 2 || p.clip == 4 || p.clip == 5 || p.clip == 6;
	o.left = walking || !st.pressed;
	st.pressed = o.left;
	o.shift = run;
	// Far from the target the steer barely changes: hold the plan longer (fewer rounds, each of
	// which costs the bridge a mailbox exchange and a frame).
	if (walking)
		o.valid = CLIP((int)(4.0f * dist2(p.pos, Vec3(tx, 0, tz))), 60, 300);
}

// Progress watch: false after ~3 s of game time without getting closer.
bool progressing(PlanState &st, float d, uint32 now) {
	if (d < st.best - 3.0f) {
		st.best = d;
		st.progress = now;
	}
	return now - st.progress < 3000;
}

// What the scene shows of its switches (active, visible, playing, spent flags, the held item):
// a change after a click means the engine reacted.
uint32 sceneSig(GrumpaEngine &vm) {
	uint32 h = 17;
	SceneData &sd = vm.sceneData();
	for (uint i = 0; i < sd.sprites.size(); i++)
		h = h * 31 + sd.sprites[i].active + 2 * sd.sprites[i].visible;
	for (uint i = 0; i < sd.meshes.size(); i++)
		h = h * 31 + sd.meshes[i].active + 2 * sd.meshes[i].visible + 4 * (vm.events().spriteFrame(sd.meshes[i].id) > 0);  // started playing
	for (uint i = 0; i < sd.triggers.size(); i++)
		h = h * 31 + sd.triggers[i].active + 2 * sd.triggers[i].spent;
	return h * 31 + (uint32)(vm.inventory().held() + 1);
}

// The verbs. `req` is the request, `key` its canonical text.
JV *planVerb(GrumpaEngine &vm, bool inScene, int sceneNum, JV *req, const Common::String &key) {
	Snapshot &sn = *g_snap;
	PlanState &st = sn.plan;
	const uint32 now = g_system->getMillis();
	Common::String verb = req->hasChild("verb") && req->child("verb")->isString() ? req->child("verb")->asString() : "";
	if (key != st.key) {
		st = PlanState();
		st.key = key;
		st.scene = sceneNum;
		st.start = st.progress = now;
		st.lines = sn.lineCount;
	}
	if (verb == "choose")  // dialogue.md: voice lines only, no menus
		return finish(st, false, "Grumpa has no dialogue choices (dialogue.md)");
	if (verb != "walk_to" && verb != "take" && verb != "use" && verb != "talk" && verb != "attack")
		return finish(st, false, "unknown verb");
	if (!inScene)
		return finish(st, false, "not in a scene");
	Character *pl = vm.characters().player();
	if (!pl || !vm.characters().present(*pl))
		return finish(st, false, "no player character");
	if (pl->state.size() > 1 && pl->state[1] <= 0)
		return finish(st, false, "the player died");

	const bool hasItem = req->hasChild("item");
	const bool hasId = req->hasChild("target");
	const int id = (int)argNum(req, "target", -1);
	const int itemId = (int)argNum(req, "item", -1);
	const float r = (float)argNum(req, "r", 0);
	Target t;
	if (hasId)
		t = resolve(vm, sceneNum, id);
	const int legScene = (int)argNum(req, "scene", -1);  // a walk_to leg planned for that scene: skipped in another, done when it is left
	if (!st.init && legScene >= 0 && legScene != sceneNum)
		return finish(st, true, "leg for another scene, skipped");
	if (!st.init) {
		st.init = true;
		st.existed = t.found;
		// a scene change is the goal when the target is an exit (walk_to) or a trigger used bare
		st.sceneOk = (verb == "walk_to" && ((t.kind == 3 && t.exitScene >= 0) || (req->hasChild("exit") && req->child("exit")->isBool() && req->child("exit")->asBool()))) || (verb == "use" && !hasItem && t.kind == 3);
	}
	if (sceneNum != st.scene) {  // the goal for an exit, and the reaction of a use that clicked
		const bool goal = st.sceneOk || legScene >= 0 || (verb == "use" && st.clicked);
		return finish(st, goal, goal ? "scene changed (the goal)" : "scene changed");
	}

	Out o;
	Inventory &inv = vm.inventory();
	const Common::Point body = bodyPoint(vm, *pl);
	auto idle = [&](const char *why) {  // nothing held, the cursor on the body: no turn, no stray click
		o.cx = body.x;
		o.cy = body.y;
		st.pressed = false;
		o.valid = 400;  // a voice line or fade lasts seconds: ask again less often
		return emit(o, why);
	};
	const bool wasRight = st.rightLast;
	st.rightLast = false;
	auto toggle = [&](const char *why) {  // a right press, then a release if it did not take
		if (!wasRight) {
			o.right = true;
			st.rightLast = true;
		}
		return emit(o, why);
	};
	const bool busy = !sn.speaking.empty() || !vm.events().fadeIdle();

	if (verb == "attack") {
		// combat.md: hit test at < 140 and u.f < -0.8 (facing it), stance on Ctrl, swing on a left press.
		if (!hasId)
			return finish(st, false, "attack needs a target");
		if (!t.found)
			return finish(st, st.existed, st.existed ? "target gone" : "target not in the scene");
		if (t.kind != 2)
			return finish(st, false, "target is not a character");
		if (t.c->state.size() > 1 && t.c->state[1] <= 0)
			return finish(st, true, "target dead");
		// an extension of the contract: "until_health": n ends the verb once the target is down to n
		if (req->hasChild("until_health") && t.c->state.size() > 1 && t.c->state[1] <= (int)argNum(req, "until_health", 0))
			return finish(st, true, "target hurt enough");
		if (inv.shown())
			return toggle("closing the panel");
		const float d = dist3(pl->pos, t.c->pos);
		if (d < 110.0f)
			st.fight = true;
		else if (d > 160.0f)
			st.fight = false;
		if (!st.fight) {
			if (!progressing(st, d, now))
				return finish(st, false, "stuck");
			walkStep(vm, *pl, t.c->pos.x, t.c->pos.z, false, st, o);
			return emit(o, "approaching");
		}
		st.progress = now;
		st.best = 1e9f;
		aimCursor(vm, *pl, t.c->pos.x, t.c->pos.z, o);
		const float ux = pl->pos.x - t.c->pos.x, uz = pl->pos.z - t.c->pos.z;
		const float n = sqrtf(ux * ux + uz * uz);
		const bool facing = n > 1e-3f && (ux * sinf(pl->yaw) + uz * cosf(pl->yaw)) / n < -0.85f;
		o.ctrl = true;
		st.pulse = !st.pulse;
		o.left = facing && st.pulse;
		o.valid = 100;
		return emit(o, facing ? "swinging" : "turning to face it");
	}

	if (verb == "walk_to" && req->hasChild("jump") && req->child("jump")->isBool() && req->child("jump")->asBool()) {
		// A running jump (combat.md, E-1406/E-1407; scenario jump_pool): aim (cursor only), hold the
		// left button and Shift for a run-up, tap Space, keep steering through the flight, let go and
		// wait for the landing. Towards (x, z) or along "yaw" (radians, as the engine's headings).
		float tx, tz;
		if (req->hasChild("yaw")) {
			tx = pl->pos.x + 1000.0f * sinf((float)argNum(req, "yaw", 0));
			tz = pl->pos.z + 1000.0f * cosf((float)argNum(req, "yaw", 0));
		} else {
			tx = (float)argNum(req, "x", 0);
			tz = (float)argNum(req, "z", 0);
		}
		const int aim = (int)argNum(req, "aim_ms", 600), runup = (int)argNum(req, "runup_ms", 240);
		const int el = (int)(now - st.start);
		const bool walkJump = req->hasChild("walk") && req->child("walk")->isBool() && req->child("walk")->asBool();  // W2J2N: a jump from the walk, shorter
		aimCursor(vm, *pl, tx, tz, o);
		// Space from a walk gives the short jump W2J2N; the run jump R2J2N (clip 5) needs the run
		// clip, facing the way (combat.md): the phases wait for both.
		if (st.phase == 0 && el >= aim) {
			st.phase = 1;
			st.phaseT = now;
		}
		if (st.phase == 1) {
			float d = pl->yaw - atan2f(tx - pl->pos.x, tz - pl->pos.z);
			d = atan2f(sinf(d), cosf(d));
			if (now - st.phaseT >= (uint32)runup && ((pl->clip == (walkJump ? 2 : 5) && fabsf(d) < 0.2f) || now - st.phaseT > 3000)) {
				st.phase = 2;
				st.phaseT = now;
			}
		}
		if (st.phase == 2 && now - st.phaseT >= 80) {
			st.phase = 3;
			st.phaseT = now;
		}
		if (st.phase == 3 && now - st.phaseT >= 800) {
			st.phase = 4;
			st.phaseT = now;
		}
		if (st.phase == 4 && now - st.phaseT >= 800)
			return finish(st, true, "jumped");
		o.left = st.phase >= 1 && st.phase <= 3;
		o.shift = o.left && !walkJump;
		o.space = st.phase == 2;
		if (st.phase == 4) {
			o.cx = body.x;
			o.cy = body.y;
		}
		o.valid = st.phase == 2 ? 80 : st.phase == 1 ? 40 : 160;
		return emit(o, "jumping");
	}

	if (verb == "walk_to") {
		float tx, tz;
		bool done;
		if (hasId) {
			if (!t.found || t.kind == 4)
				return finish(st, false, "unknown target");
			tx = t.pos.x;
			tz = t.pos.z;
			done = arrived(*pl, t, r, false);
		} else {
			tx = (float)argNum(req, "x", 0);
			tz = (float)argNum(req, "z", 0);
			done = dist2(pl->pos, Vec3(tx, 0, tz)) < (r > 0 ? r : 30.0f);
		}
		if (done)
			return finish(st, true, "arrived");
		if (busy) {
			st.progress = now;
			return idle("waiting for a voice line or fade");
		}
		if (inv.shown())
			return toggle("closing the panel");
		if (!progressing(st, dist2(pl->pos, Vec3(tx, 0, tz)), now)) {
			warning("Grumpa agent: stuck at (%.0f, %.0f) going to (%.0f, %.0f), clip %d", pl->pos.x, pl->pos.z, tx, tz, pl->clip);
			return finish(st, false, "stuck");
		}
		walkStep(vm, *pl, tx, tz, req->hasChild("run") && req->child("run")->isBool() && req->child("run")->asBool(), st, o);
		return emit(o, "walking");
	}

	// take, talk, use: walk up, stop, (use: pick the item up onto the cursor), aim, click, watch.
	if (!hasId)
		return finish(st, false, "needs a target");
	if (verb == "use" && hasItem && !inv.item(itemId))
		return finish(st, false, "unknown item");
	if (verb == "take") {
		const Inventory::Item *it = inv.item(id);
		if (it && (it->state == Inventory::kCarried || it->state == Inventory::kHeld))
			return finish(st, true, "carried");
	}
	if (verb == "talk" && sn.lineCount > st.lines)
		return finish(st, true, "a line started");
	// a view change fades for a second: the target may show up in it (grace of 2.5 s)
	if ((!t.found || (st.phase < 3 && !t.hasClick)) && now - st.start < 2500) {
		st.progress = now;
		return idle("waiting for the target to show");
	}
	if (!t.found)
		return finish(st, false, st.phase >= 4 ? "target gone" : "unknown target");
	if (verb == "take" && t.kind != 1)
		return finish(st, false, "target is not an item lying here");
	if (verb == "talk" && t.kind != 2)
		return finish(st, false, "target is not a character");

	switch (st.phase) {
	case 0:  // approach
		if (arrived(*pl, t, r, verb == "talk")) {
			st.phase = 1;
			return idle("arrived, stopping");
		}
		if (busy) {
			st.progress = now;
			return idle("waiting for a voice line or fade");
		}
		if (inv.held() >= 0)  // a held item is no walk arrow (inventory.md)
			return finish(st, false, "an item is on the cursor, cannot walk");
		if (inv.shown())
			return toggle("closing the panel");
		if (!progressing(st, dist3(pl->pos, t.pos), now))
			return finish(st, false, "stuck");
		walkStep(vm, *pl, t.pos.x, t.pos.z, false, st, o);
		return emit(o, "walking");
	case 1:
		st.phase = 2;
		return idle("stopped");
	case 2:  // equip (use with an item): panel open, slot click, panel closed
		if (verb == "use" && hasItem) {
			if (inv.held() == itemId && inv.shown())
				return toggle("closing the panel");
			if (inv.held() != itemId) {
				if (inv.held() >= 0)
					return finish(st, false, "another item is held");
				const Inventory::Item *it = inv.item(itemId);
				if (!it || it->state != Inventory::kCarried)
					return finish(st, false, "the item is not carried");
				if (!inv.shown())
					return toggle("opening the panel");
				const Common::Rect s = inv.slotRect(itemId);
				if (s.isEmpty())
					return finish(st, false, "item has no panel slot");
				o.cx = o.clickX = (s.left + s.right) / 2;
				o.cy = o.clickY = (s.top + s.bottom) / 2;
				o.valid = 100;
				return emit(o, "picking the item");
			}
		}
		st.phase = 3;
		st.phaseT = now;
		return idle("ready");
	case 3:  // aim: the hover registers on an update
		if (!t.hasClick && now - st.phaseT < 3000)  // a script may still be switching it on
			return idle("waiting for the target to be clickable");
		if (!t.hasClick)
			return finish(st, false, "target is not on screen or not clickable");
		o.cx = t.click.x;
		o.cy = t.click.y;
		st.phase = 4;
		return emit(o, "aiming");
	case 4:  // click
		if (!t.hasClick)
			return finish(st, false, "target is not on screen or not clickable");
		o.cx = o.clickX = t.click.x;
		o.cy = o.clickY = t.click.y;
		o.valid = 100;
		st.phase = 5;
		st.phaseT = now;
		st.retries++;
		st.clicked = true;
		st.sig = sceneSig(vm);
		st.trigWas = t.trig ? (t.trig->spent || !t.trig->active) : false;
		return emit(o, "clicking");
	default: {  // verify
		o.cx = t.hasClick ? t.click.x : body.x;
		o.cy = t.hasClick ? t.click.y : body.y;
		const bool trigChanged = (t.trig && (t.trig->spent || !t.trig->active) != st.trigWas) || sceneSig(vm) != st.sig;
		bool ok = false;
		if (verb == "use" && hasItem) {
			const Inventory::Item *it = inv.item(itemId);
			ok = (it && it->state == Inventory::kGone) || inv.held() != itemId || trigChanged;  // consumed, or taken off the cursor (returned by the trigger's list), or the trigger changed
		} else if (verb == "use") {
			ok = trigChanged || sn.lineCount > st.lines;
		}
		if (ok)
			return finish(st, true, "the engine reacted");
		if (!vm.events().fadeIdle()) {  // a fade started: a scene or view change is on its way
			st.phaseT = now;
			return emit(o, "waiting for the fade");
		}
		if (now - st.phaseT < 3000)
			return emit(o, "waiting for the reaction");
		if (st.retries >= 3)
			return finish(st, false, "nothing reacted");
		// the item was dropped or the gate missed: start again (an item still on the cursor cannot walk)
		st.phase = inv.held() >= 0 ? 3 : 0;
		st.phaseT = now;
		st.best = 1e9f;
		st.progress = now;
		return idle("retrying");
	}
	}
}

} // namespace

// The panel rectangle of the slot holding item `id` (inventory.md layout); empty if none.
Common::Rect Inventory::slotRect(int id) const {
	const int left = _pos.x + (_panel.w - 258) / 2, top = _pos.y + _panel.h;
	for (int i = 0; i < kSlots; i++)
		if (_slot[i] == id) {
			const int x = left + (i % 3) * 86, y = top + (i / 3) * 86;
			return Common::Rect(x, y, x + 86, y + 86);
		}
	return Common::Rect();
}

void GrumpaEngine::agentUpdate(bool inScene) {
	if (!ConfMan.hasKey("agent_mailbox"))
		return;
	Common::FSNode dir(Common::Path(ConfMan.get("agent_mailbox"), '/'));
	if (!g_snap) {
		g_snap = new Snapshot();
		// Announce the adapter: a client can then tell "busy" (a film blocks this loop) from
		// "no adapter".
		Common::DumpFile hello;
		if (hello.open(dir.getChild("adapter.json")))
			hello.writeString("{\"engine\":\"grumpa\",\"tier\":2,\"schema\":1}\n");
	}
	diffState(*this, inScene ? _sceneNum : -1, inScene);

	Common::FSList files;
	if (!dir.getChildren(files, Common::FSNode::kListFilesOnly))
		return;
	Common::Array<int> todo;
	for (uint i = 0; i < files.size(); i++) {
		int n = -1;
		const Common::String name = files[i].getName();
		if (sscanf(name.c_str(), "req-%d.json", &n) != 1 || name != Common::String::format("req-%d.json", n))
			continue;
		if (n > g_snap->lastHandled)
			todo.push_back(n);
	}
	Common::sort(todo.begin(), todo.end());
	for (uint i = 0; i < todo.size(); i++) {
		const int n = todo[i];
		Common::File in;
		if (!in.open(dir.getChild(Common::String::format("req-%d.json", n))))
			continue;  // still being written: next iteration
		Common::String text = in.readString(0, in.size());
		in.close();
		JV *req = Common::JSON::parse(text.c_str());
		if (!req)
			continue;  // incomplete JSON: retry next iteration
		g_snap->lastHandled = n;
		Common::String cmd;
		if (req->isObject() && req->hasChild("cmd") && req->child("cmd")->isString())
			cmd = req->child("cmd")->asString();

		JV *rep;
		if (cmd == "plan") {
			rep = planVerb(*this, inScene, _sceneNum, req, Common::JSON::stringify(req));
		} else if (cmd == "describe") {
			rep = describe(*this, inScene, _sceneNum, _cursorName, _stance);
		} else if (cmd == "save") {
			// A replay's snapshot (export_saves): the engine's own save, as the save dialog does.
			Obj o;
			const int slot = (int)argNum(req, "slot", 1);
			Common::U32String why;
			if (!canSaveGameStateCurrently(&why))
				o["error"] = str("cannot save now: " + why.encode());
			else if (saveGameState(slot, "agent bridge").getCode() != Common::kNoError)
				o["error"] = str(Common::String::format("saving to slot %d failed", slot));
			else
				o["saved"] = num(slot);
			rep = new JV(o);
		} else if (cmd == "load") {
			// A rewind to a snapshot: the save is entered on the loop's next iteration.
			Obj o;
			const int slot = (int)argNum(req, "slot", 1);
			Common::U32String why;
			if (!canLoadGameStateCurrently(&why))
				o["error"] = str("cannot load now: " + why.encode());
			else if (loadGameState(slot).getCode() != Common::kNoError)
				o["error"] = str(Common::String::format("loading slot %d failed", slot));
			else
				o["loaded"] = num(slot);
			rep = new JV(o);
		} else {
			Obj o;
			o["error"] = str("unknown command " + cmd);
			rep = new JV(o);
		}
		delete req;
		Common::DumpFile out;
		if (out.open(dir.getChild(Common::String::format("rep-%d.json", n)))) {
			Common::String json = Common::JSON::stringify(rep);
			out.write(json.c_str(), json.size());
			out.finalize();
		} else {
			warning("Grumpa agent: cannot write the reply %d", n);
		}
		delete rep;
	}
}

void GrumpaEngine::agentFree() {
	delete g_snap;
	g_snap = nullptr;
}

} // End of namespace Grumpa
