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
#include "common/file.h"
#include "common/formats/winexe_pe.h"
#include "common/memstream.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "graphics/cursorman.h"
#include "graphics/surface.h"

#include "image/bmp.h"

#include "x3d/interaction.h"
#include "x3d/scene.h"
#include "x3d/sound.h"
#include "x3d/talk.h"

namespace X3D {

// A NUL-terminated string in a fixed-size field (the whole field is consumed)
static Common::String readString(Common::SeekableReadStream &s, uint size) {
	return s.readString(0, size);
}

// A BMP image as a surface in the screen format, or nullptr
static Graphics::Surface *decodeBitmap(Common::SeekableReadStream &s) {
	Image::BitmapDecoder bmp;
	if (!bmp.loadStream(s))
		return nullptr;
	return bmp.getSurface()->convertTo(g_system->getScreenFormat(), bmp.getPalette().data());
}

// Cursor kinds 0..5 (interaction.md, Cursors): EXE bitmap resources and their hotspots
static const char *const kCursorNames[] = {
	"CUR_DEFAULT.BMP", "CUR_WAIT.BMP", "CUR_CLIC.BMP", "CUR_VOICE.BMP", "CUR_TAKE.BMP", "CUR_USE.BMP"
};
static const int kCursorHotspots[][2] = { { 0, 0 }, { 9, 2 }, { 9, 2 }, { 10, 10 }, { 10, 4 }, { 10, 10 } };

Interaction::Interaction(Scene &scene, Sound &sound, Talk &talk) : _scene(scene), _sound(sound), _talk(talk) {
	// The cursors are resources of the game's EXE: next to Data/ in an installed copy,
	// under INSTALL/02_PR/ on the CD
	Common::PEResources exe;
	if (!exe.loadFromEXE("MissionMonet.exe") && !exe.loadFromEXE("INSTALL/02_PR/MissionMonet.exe")) {
		warning("MissionMonet.exe not found: no game cursors");
		return;
	}
	for (int i = 0; i < ARRAYSIZE(kCursorNames); i++) {
		Common::SeekableReadStream *res = exe.getResource(Common::kWinBitmap, Common::WinResourceID(kCursorNames[i]));
		if (!res)
			continue;
		// A bitmap resource lacks the 14-byte file header: add it
		const uint32 size = res->size();
		byte *data = (byte *)malloc(size + 14);
		res->read(data + 14, size);
		delete res;
		const uint32 infoSize = READ_LE_UINT32(data + 14);
		const uint16 bpp = READ_LE_UINT16(data + 14 + 14);
		const uint32 colors = bpp <= 8 ? (READ_LE_UINT32(data + 14 + 32) ? READ_LE_UINT32(data + 14 + 32) : 1u << bpp) : 0;
		data[0] = 'B';
		data[1] = 'M';
		WRITE_LE_UINT32(data + 2, size + 14);
		WRITE_LE_UINT32(data + 6, 0);
		WRITE_LE_UINT32(data + 10, 14 + infoSize + colors * 4);
		Common::MemoryReadStream stream(data, size + 14, DisposeAfterUse::YES);
		_cursors[i] = decodeBitmap(stream);
	}
	setCursor(0);
	CursorMan.showMouse(true);
}

Interaction::~Interaction() {
	for (Graphics::Surface *s : _cursors) {
		if (s) {
			s->free();
			delete s;
		}
	}
	if (_heldImage) {
		_heldImage->free();
		delete _heldImage;
	}
}

void Interaction::load(const Common::String &unitDir) {
	_soundDir = unitDir + "Sound/";

	// Hotspots and their initial state (E-0072)
	if (Common::SeekableReadStream *s = openBinChunk(Common::Path(unitDir + "INFOOBJ.BIN"), "#OBJECTS#")) {
		const uint32 count = s->readUint32LE();
		for (uint32 i = 0; i < count && !s->err(); i++) {
			Hotspot h;
			h.name = readString(*s, 40);
			h.type = s->readUint32LE();
			h.cursor = s->readUint32LE();
			const bool visible = s->readUint32LE() != 0;
			const float frame = s->readFloatLE();
			const bool paused = s->readUint32LE() != 0;
			const float fps = s->readUint32LE();
			const bool loop = s->readUint32LE() != 0;
			if (!visible)
				_scene.hideObject(h.name);
			_scene.setAnimationState(h.name, frame, paused, fps, loop);
			_hotspots.push_back(h);
		}
		delete s;
	}

	debug(1, "%u hotspots in %sINFOOBJ.BIN", _hotspots.size(), unitDir.c_str());

	// Click actions (E-0071)
	if (Common::SeekableReadStream *s = openBinChunk(Common::Path(unitDir + "INFOACT.BIN"), "#ACTIONS#")) {
		const uint32 count = s->readUint32LE();
		for (uint32 i = 0; i < count && !s->err(); i++) {
			Action a;
			a.id = s->readUint32LE();
			a.name = readString(*s, 30);
			a.condition = readString(*s, 258);
			a.maxRuns = s->readSint32LE();
			a.trigger = s->readUint32LE();
			a.item = readString(*s, 32);
			a.hotspotType = s->readUint32LE();
			a.hotspot = readString(*s, 32);
			a.targetType = s->readUint32LE();
			a.target = readString(*s, 32);
			const uint32 steps = s->readUint32LE();
			for (uint32 k = 0; k < 10; k++) {
				const uint32 op = s->readUint32LE();
				const Common::String arg = readString(*s, 64);
				if (k < steps) {
					a.ops.push_back(op);
					a.args.push_back(arg);
				}
			}
			_actions.push_back(a);
		}
		delete s;
	}
}

int Interaction::findHotspot(const Common::String &name) const {
	for (uint i = 0; i < _hotspots.size(); i++) {
		const Common::String &h = _hotspots[i].name;
		if (h.equalsIgnoreCase(name) || (h.hasPrefix("*") && h.substr(1).equalsIgnoreCase(name)))
			return i;
	}
	return -1;
}

int Interaction::hotspotFor(const Common::StringArray &names) const {
	// The first name with a '*' names the hotspot from the '*' on (equal, else contained)
	for (const Common::String &n : names) {
		const size_t star = n.findFirstOf('*');
		if (star == Common::String::npos)
			continue;
		Common::String text = n.substr(star);
		text.toLowercase();
		for (uint i = 0; i < _hotspots.size(); i++)
			if (_hotspots[i].name.equalsIgnoreCase(text))
				return i;
		for (uint i = 0; i < _hotspots.size(); i++) {
			Common::String h = _hotspots[i].name;
			h.toLowercase();
			if (h.contains(text))
				return i;
		}
	}
	return -1;
}

void Interaction::setCursor(uint kind) {
	if (_shownCursor == (int)kind || kind >= ARRAYSIZE(_cursors) || !_cursors[kind])
		return;
	const Graphics::Surface *s = _cursors[kind];
	CursorMan.replaceCursor(*s, kCursorHotspots[kind][0], kCursorHotspots[kind][1], s->format.RGBToColor(255, 255, 255));
	_shownCursor = kind;
}

void Interaction::hover(int hotspot, uint32 millis) {
	if (_heldImage) {
		// Over a "use" hotspot the held item blinks at 6 frames per second
		const bool blinkOff = hotspot >= 0 && _hotspots[hotspot].cursor == 5 && (millis * 6 / 1000) % 2;
		const int shown = blinkOff ? -3 : -1;
		if (shown != _shownCursor) {
			CursorMan.showMouse(!blinkOff);
			if (!blinkOff)
				CursorMan.replaceCursor(*_heldImage, 16, 16, _heldImage->format.RGBToColor(255, 255, 255));
			_shownCursor = shown;
		}
		return;
	}
	setCursor(hotspot >= 0 ? _hotspots[hotspot].cursor : 0);
}

void Interaction::holdItem(const Common::String &item) {
	if (_heldImage) {
		_heldImage->free();
		delete _heldImage;
		_heldImage = nullptr;
	}
	_heldItem = item;
	CursorMan.showMouse(true);
	_shownCursor = -2;
	if (item.empty())
		return;
	Common::File f;
	if (f.open(Common::Path("2dbit/" + item + "C.BMP")))
		_heldImage = decodeBitmap(f);
	if (!_heldImage)
		warning("No cursor image for item %s", item.c_str());
}

void Interaction::setCursorKind(const Common::String &hotspot, uint kind) {
	const int h = findHotspot(hotspot);
	if (h >= 0)
		_hotspots[h].cursor = kind;
}

bool Interaction::evaluate(const Common::String &condition) const {
	// true / false, mNN or NN (action NN is exhausted), !, & and | left to right, ( )
	Common::String c = condition;
	c.toLowercase();
	uint i = 0;
	struct Parser {
		const Interaction &in;
		const Common::String &c;
		uint &i;
		void skip() {
			while (i < c.size() && c[i] == ' ')
				i++;
		}
		bool term() {
			skip();
			if (i < c.size() && c[i] == '!') {
				i++;
				return !term();
			}
			if (i < c.size() && c[i] == '(') {
				i++;
				const bool v = expr();
				skip();
				if (i < c.size() && c[i] == ')')
					i++;
				return v;
			}
			if (c.substr(i, 4) == "true") {
				i += 4;
				return true;
			}
			if (c.substr(i, 5) == "false") {
				i += 5;
				return false;
			}
			if (i < c.size() && c[i] == 'm')
				i++;
			uint32 id = 0;
			while (i < c.size() && Common::isDigit(c[i]))
				id = id * 10 + (c[i++] - '0');
			for (const Action &a : in._actions)
				if (a.id == id)
					return a.exhausted;
			return false;
		}
		bool expr() {
			bool v = term();
			for (;;) {
				skip();
				if (i < c.size() && c[i] == '&') {
					i++;
					v = term() && v;
				} else if (i < c.size() && c[i] == '|') {
					i++;
					v = term() || v;
				} else {
					return v;
				}
			}
		}
	} parser = { *this, c, i };
	return parser.expr();
}

bool Interaction::runnable(const Action &a, uint32 trigger) const {
	return a.trigger == trigger && !a.exhausted && evaluate(a.condition) &&
	       (trigger != 7 || a.item.equalsIgnoreCase(_heldItem));
}

void Interaction::click(int hotspot, Common::StringArray &unitActions) {
	if (hotspot < 0 || !actionsEnabled)
		return;
	const Hotspot &h = _hotspots[hotspot];
	const uint32 trigger = _heldItem.empty() ? 8 : 7;
	for (Action &a : _actions) {
		if (a.hotspotType != h.type || findHotspot(a.hotspot) != hotspot || !runnable(a, trigger))
			continue;
		run(a, unitActions);
		return;
	}
}

// Sound/<name>.WAV, or Sound/<name> when the name has the extension (sound.md)
Common::Path Interaction::soundPath(const Common::String &name) const {
	Common::String lower = name;
	lower.toLowercase();
	return Common::Path(_soundDir + (lower.contains(".wav") ? name : name + ".WAV"));
}

Common::String Interaction::hotspotName(const Action &a) const {
	const int h = findHotspot(a.hotspot);
	return h >= 0 ? _hotspots[h].name : a.hotspot;
}

void Interaction::run(Action &a, Common::StringArray &unitActions) {
	debug(1, "action %s on %s", a.name.c_str(), a.hotspot.c_str());
	const int target = findHotspot(a.target);
	const Common::String targetName = target >= 0 ? _hotspots[target].name : a.target;
	for (uint k = 0; k < a.ops.size(); k++) {
		const Common::String &arg = a.args[k];
		switch (a.ops[k]) {
		case 1: // voice: a character talks, anything else speaks from the hotspot (sound.md)
			if (a.targetType == 6 || (target >= 0 && _hotspots[target].type == 6)) {
				const Common::String character = targetName.hasPrefix("*") ? targetName.substr(1) : targetName;
				if (!_talk.say(character, arg))
					_sound.emit(Sound::kVoiceEmitter, soundPath(arg), eye, false);
			} else {
				_sound.emit(Sound::kVoiceEmitter, soundPath(arg), _scene.objectPosition(hotspotName(a)), false);
			}
			break;
		case 2: // take the target
			if (target >= 0)
				_hotspots[target].cursor = 0;
			_scene.hideObject(targetName);
			holdItem(targetName.hasPrefix("*") ? targetName.substr(1) : targetName);
			break;
		case 3: // use up the held item
			holdItem("");
			if (target >= 0)
				_hotspots[target].cursor = 0;
			break;
		case 4:
			_scene.startAnimation(arg);
			break;
		case 7:
			if (target >= 0)
				_hotspots[target].cursor = atoi(arg.c_str());
			break;
		case 9: // 0 shows, anything else hides (E-0088)
			_scene.hideObject(targetName, atoi(arg.c_str()) != 0);
			break;
		case 10:
			unitActions.push_back(arg);
			break;
		case 12:
			_sound.play(soundPath(arg), Sound::kAmbient, 85, true);
			break;
		case 13:
			_sound.emit(Sound::kEffectsEmitter, soundPath(arg), _scene.objectPosition(hotspotName(a)), false);
			break;
		case 101:
			_sound.stopGroup(Sound::kVoice);
			_sound.detach(Sound::kVoiceEmitter);
			_sound.play(soundPath(arg), Sound::kVoice, 100, false);
			break;
		case 14:
			for (Action &b : _actions)
				if (b.name.equalsIgnoreCase(arg) && runnable(b, b.trigger)) {
					run(b, unitActions);
					break;
				}
			break;
		case 15:
		case 16:
			for (Action &b : _actions)
				if (b.name.equalsIgnoreCase(arg))
					b.condition = a.ops[k] == 15 ? "TRUE" : "FALSE";
			break;
		default: // 6 (wait) does not occur in the corpus
			warning("Action %s: unhandled step %u", a.name.c_str(), a.ops[k]);
			break;
		}
	}
	a.runs++;
	if (a.maxRuns < 100 && a.runs >= a.maxRuns)
		a.exhausted = true;
}

} // End of namespace X3D
