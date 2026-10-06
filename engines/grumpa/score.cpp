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

#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/system.h"
#include "image/jpeg.h"

#include "grumpa/character.h"
#include "grumpa/dialogue.h"
#include "grumpa/score.h"

namespace Grumpa {

// The score display and the ambience, written from engines/grumpa/docs/spec/score.md.

// The tokens of the first `{ }` block of an .atx (comments after "//" dropped).
static bool readAtxBlock(const char *path, Common::Array<Common::String> &tok) {
	Common::File f;
	if (!f.open(path))
		return false;
	bool inBlock = false;
	while (!f.eos()) {
		Common::String line = f.readLine();
		int slash = line.find("//");
		if (slash >= 0)
			line = Common::String(line.c_str(), slash);
		line.trim();
		if (line == "{") {
			inBlock = true;
		} else if (line == "}") {
			if (inBlock)
				return true;
		} else if (inBlock) {
			Common::String w;
			for (uint i = 0; i <= line.size(); i++) {
				if (i == line.size() || line[i] == ' ' || line[i] == '\t') {
					if (!w.empty())
						tok.push_back(w);
					w.clear();
				} else {
					w += line[i];
				}
			}
		}
	}
	return false;
}

// ---- Score (actor 8) --------------------------------------------------------------------

static const int kFormMap[][2] = { { 10, 8 }, { 13, 9 }, { 11, 10 }, { 88, 11 }, { 12, 12 } };

// A picture's frames: "<stem>0000.jpg" animates while "<stem>%04d.jpg" exists (scene.md).
static bool loadFrames(const Common::String &name, Common::Array<Graphics::ManagedSurface> &out) {
	Graphics::PixelFormat fmt = g_system->getScreenFormat();
	bool animated = name.size() > 8 && name.hasSuffixIgnoreCase("0000.jpg");
	Common::String stem = animated ? Common::String(name.c_str(), name.size() - 8) : name;
	for (int i = 0; i < 100; i++) {
		Common::String file = "UI/008_Score/" + (animated ? stem + Common::String::format("%04d.jpg", i) : name);
		Common::File f;
		Image::JPEGDecoder jpeg;
		jpeg.setOutputPixelFormat(fmt);
		if (!f.open(Common::Path(file)) || !jpeg.loadStream(f))
			break;
		out.push_back(Graphics::ManagedSurface());
		out.back().copyFrom(*jpeg.getSurface());
		if (!animated)
			break;
	}
	return !out.empty();
}

bool Score::load() {
	Common::Array<Common::String> tok;
	if (!readAtxBlock("UI/008_Score/008_Score.atx", tok) || tok.size() < 9 + kElements + 6)
		return false;
	_active = atoi(tok[1].c_str()) != 0;
	_visible = atoi(tok[2].c_str()) != 0;
	Common::Point coin(atoi(tok[3].c_str()), atoi(tok[4].c_str()));
	Common::Point heart(atoi(tok[5].c_str()), atoi(tok[6].c_str()));
	_digitGap = atoi(tok[7].c_str());
	_digitSpacing = atoi(tok[8].c_str());
	for (int i = 0; i < kElements; i++) {
		Element &e = _el[i];
		e = Element();
		if (!loadFrames(tok[9 + i], e.frames))
			warning("Grumpa: score picture %s not found", tok[9 + i].c_str());
		else
			e.key = e.frames[0].getPixel(0, 0);
	}
	for (int i = 0; i < 6; i++)
		_sounds[i] = tok[9 + kElements + i];
	_slot[0] = 0;
	_slot[kCoins] = 0;
	_slot[kLife] = 99;
	_slot[kAir] = 99;
	_form = 0;

	// The layout (E-0703).
	_el[2].pos = coin;
	_el[3].pos = heart;
	for (int i = 8; i < kElements; i++) {
		_el[i].pos = Common::Point(heart.x - _el[i].width() - 8, heart.y);
		_el[i].visible = false;
	}
	int x = _el[8].pos.x;
	static const int kLeft[] = { 4, 6, 7 };  // air, poison, strength, right to left
	for (int k = 0; k < 3; k++) {
		Element &e = _el[kLeft[k]];
		x -= 8 + e.width();
		e.pos = Common::Point(x, heart.y);
		e.visible = false;
	}
	for (int i = 0; i < 2; i++) {
		_el[i].pos = Common::Point(687, 0);
		_el[i].visible = _el[i].active = false;
	}
	_el[formIcon()].visible = true;
	return true;
}

int Score::formIcon() const {
	return kFormMap[_form][1];
}

bool Score::stateOf(int slot, int32 &value) const {
	if (slot < 0 || slot > kAir)
		return false;
	value = _slot[slot];
	return true;
}

void Score::sound(int i) {
	Common::File *f = new Common::File();
	if (!f->open(Common::Path("UI/008_Score/" + _sounds[i]))) {
		delete f;
		return;
	}
	Audio::RewindableAudioStream *wav = Audio::makeWAVStream(f, DisposeAfterUse::YES);
	if (wav)
		g_system->getMixer()->playStream(Audio::Mixer::kSFXSoundType, &_handle[i], wav);
}

void Score::playStar(int i) {
	Element &e = _el[i];
	if (!e.running) {
		e.frame = 0;
		e.dir = 0;
		e.running = true;
	}
	e.visible = e.active = true;
}

// The heart or the air bar shows its value: frame count - value/9 - 1 (E-0703).
void Score::barFrame(int slot, bool added) {
	Element &e = _el[slot == kLife ? 3 : 4];
	int n = e.frames.size();
	int v = _slot[slot];
	int f = v == 99 ? (added ? 0 : n - 1) : n - v / 9 - 1;
	if (f >= 0 && f < n)
		e.frame = f;
}

void Score::add(int slot, int n) {
	_slot[slot] = MIN<int32>(_slot[slot] + n, 99);
	bool effect = _slot[slot] != 99 && n != 0;
	if (slot == kCoins) {
		sound(2);
	} else if (slot == kLife) {
		barFrame(slot, true);
		if (effect) {
			playStar(0);
			sound(0);
		}
	} else if (slot == kAir) {
		barFrame(slot, true);
		if (effect)
			sound(3);
	}
}

void Score::sub(int slot, int n) {
	int32 old = _slot[slot];
	_slot[slot] = MAX<int32>(old - n, 0);
	if (slot == kLife) {
		barFrame(slot, false);
		if (old != 0) {
			playStar(1);
			sound(1);
		}
	} else if (slot == kAir) {
		barFrame(slot, false);
		if (old != 0)
			sound(4);
	}
}

void Score::setForm(int charId, Characters &chars) {
	Character *c = chars.find(charId);
	if (!c)
		return;
	if (c->state.size() > 1)
		_slot[kLife] = c->state[1];
	for (int i = 8; i < kElements; i++)
		_el[i].visible = false;
	for (int i = 0; i < ARRAYSIZE(kFormMap); i++)
		if (kFormMap[i][0] == charId)
			_form = i;
	_el[formIcon()].visible = true;
	add(kLife, 0);
}

void Score::command(int op, int arg1, int arg2, Characters &chars) {
	const int formChar = kFormMap[_form][0];
	Character *c = arg2 > 0 ? chars.find(arg2) : nullptr;
	switch (op) {
	case 2: _visible = true; break;
	case 3: _visible = false; break;
	case 9: add(kCoins, arg1); break;
	case 10: sub(kCoins, arg1); break;
	case 11: _active = true; break;
	case 12: _active = false; break;
	case 50:
	case 51:
		if (arg2 == -10 || arg2 == -11) {
			// To actors 3 / 4, whose character sends it back with its own id (E-1223).
			const int held = chars.held(arg2 == -10 ? Characters::kPlayerActor : Characters::kFollowerActor);
			if (held > 0)
				command(op, arg1, held, chars);
		} else if (arg2 == 0) {
			if (op == 50)
				add(kLife, arg1);
			else
				sub(kLife, arg1);
			Character *form = chars.find(formChar);
			if (form && form->state.size() > 1)
				form->state[1] = _slot[kLife];
		} else if (arg2 > 0) {
			if (c && c->state.size() > 1)
				c->state[1] += op == 50 ? arg1 : -arg1;
			// ponytail: a character hit and still alive plays its animation 0x17; no
			// character animation state machine yet (Q-0403).
			if (arg2 == formChar) {
				if (op == 50)
					add(kLife, arg1);
				else
					sub(kLife, arg1);
			}
		}
		break;
	case 76: add(kAir, arg1); break;
	case 77: sub(kAir, arg1); break;
	case 78: _el[4].visible = true; break;
	case 79: _el[4].visible = false; break;
	case 80: _el[6].visible = true; break;
	case 81: _el[6].visible = false; break;
	case 82: _el[7].visible = true; break;
	case 83: _el[7].visible = false; break;
	case 85: setForm(arg1, chars); break;
	default: break;
	}
}

// The stars: one frame per update (fps 50, E-0701), forward and back, then hidden.
void Score::update() {
	if (!_active)
		return;
	for (int i = 0; i < 2; i++) {
		Element &e = _el[i];
		int n = e.frames.size();
		if (!e.active || !e.running || n == 0)
			continue;
		if (e.dir == 0) {
			if (++e.frame >= n) {
				e.dir = 1;
				e.frame = n - 2;
			}
		} else if (--e.frame < 0) {
			e.running = false;
			e.dir = 0;
			e.frame = 0;
			e.active = e.visible = false;
		}
	}
}

void Score::draw(Graphics::ManagedSurface &screen) const {
	if (!_visible)
		return;
	for (int i = 0; i < kElements; i++) {
		const Element &e = _el[i];
		if (i == kFont || !e.visible || e.frames.empty())
			continue;
		screen.transBlitFrom(e.frames[CLIP<int>(e.frame, 0, e.frames.size() - 1)], e.pos, e.key);
	}
	const Element &font = _el[kFont];
	if (font.frames.size() < 10)
		return;
	int v = _slot[kCoins];
	Common::Point p(_el[2].pos.x + _el[2].width() + _digitGap, _el[2].pos.y);
	if (v > 9) {
		screen.transBlitFrom(font.frames[v / 10], p, font.key);
		p.x += font.width() + _digitSpacing;
	}
	screen.transBlitFrom(font.frames[v % 10], p, font.key);
}

void Score::syncState(Common::Serializer &s) {
	byte f = (_active ? 1 : 0) | (_visible ? 2 : 0) | (_el[4].visible ? 4 : 0) |
			 (_el[6].visible ? 8 : 0) | (_el[7].visible ? 16 : 0);
	s.syncAsByte(f);
	int32 form = _form;
	s.syncAsSint32LE(form);
	for (int i = 0; i < 4; i++)
		s.syncAsSint32LE(_slot[i]);
	if (s.isLoading()) {
		_active = f & 1;
		_visible = f & 2;
		_el[4].visible = f & 4;
		_el[6].visible = f & 8;
		_el[7].visible = f & 16;
		_form = CLIP<int32>(form, 0, ARRAYSIZE(kFormMap) - 1);
		for (int i = 8; i < kElements; i++)
			_el[i].visible = false;
		_el[formIcon()].visible = true;
		barFrame(kLife, true);
		barFrame(kAir, true);
	}
}

// ---- Ambience (actor 180) -----------------------------------------------------------------

enum {
	kAmbientVolume = -500,   // hundredths of a dB (E-0702)
	kSilent = -5000,
	kFadeStep = 50           // 5000 / 100 updates
};

bool Ambience::load() {
	stop();
	Common::Array<Common::String> tok;
	_names.clear();
	_cur = _prev = -1;
	if (!readAtxBlock("Actors/global2.atx", tok) || tok.size() < 5)
		return false;
	_start = atoi(tok[3].c_str());
	int n = atoi(tok[4].c_str());
	for (int i = 0; i < n && 5 + i < (int)tok.size(); i++)
		_names.push_back(tok[5 + i]);
	_snd.clear();
	_snd.resize(_names.size());
	_pending = _start;
	return true;
}

void Ambience::setVolume(Sound &snd, int32 v) {
	snd.volume = v;
	g_system->getMixer()->setChannelVolume(snd.handle, (byte)CLIP<double>(255.0 * pow(10.0, v / 2000.0), 0, 255));
}

void Ambience::play(int i) {
	if (i < 0 || i >= (int)_names.size())
		return;
	Sound &snd = _snd[i];
	const bool restart = i == _cur;  // the current one plays again only if it stopped
	if (restart && g_system->getMixer()->isSoundHandleActive(snd.handle))
		return;
	Common::SeekableReadStream *f = openSound(_names[i]);
	Audio::RewindableAudioStream *wav = f ? Audio::makeWAVStream(f, DisposeAfterUse::YES) : nullptr;
	if (!wav) {
		debug(1, "Grumpa: ambience %s not found", _names[i].c_str());
		return;
	}
	if (restart) {
		g_system->getMixer()->playStream(Audio::Mixer::kMusicSoundType, &snd.handle, Audio::makeLoopingAudioStream(wav, 0));
		setVolume(snd, snd.volume);
		return;
	}
	// Bug fix: the original forgets a sound still fading out when two changes come within its
	// fade, leaving it looping; stop it.
	if (_prev >= 0 && _prev != i)
		g_system->getMixer()->stopHandle(_snd[_prev].handle);
	g_system->getMixer()->stopHandle(snd.handle);
	_prev = _cur;
	_cur = i;
	g_system->getMixer()->playStream(Audio::Mixer::kMusicSoundType, &snd.handle, Audio::makeLoopingAudioStream(wav, 0));
	snd.fadeOut = false;
	snd.fadeIn = i != _start;
	setVolume(snd, snd.fadeIn ? kSilent : kAmbientVolume);
	if (_prev >= 0) {
		_snd[_prev].fadeIn = false;
		_snd[_prev].fadeOut = true;
		_snd[_prev].volume = 0;
	}
	debug(1, "Grumpa: ambience %d %s", i, _names[i].c_str());
}

void Ambience::command(int op, int arg1) {
	if (op == 73) {
		play(arg1);
	} else if (op == 74) {
		if (_cur >= 0)
			g_system->getMixer()->stopHandle(_snd[_cur].handle);
		if (_prev >= 0)
			g_system->getMixer()->stopHandle(_snd[_prev].handle);
	}
}

void Ambience::enterScene() {
	if (_pending >= 0)
		play(_pending);
	_pending = -1;
}

void Ambience::update() {
	if (_prev >= 0) {
		Sound &p = _snd[_prev];
		if (p.fadeOut) {
			if (p.volume - kFadeStep < kSilent) {
				p.fadeOut = false;
				g_system->getMixer()->stopHandle(p.handle);
			} else {
				setVolume(p, p.volume - kFadeStep);
			}
		}
		if (!g_system->getMixer()->isSoundHandleActive(p.handle))
			_prev = -1;
	}
	if (_cur >= 0 && _snd[_cur].fadeIn) {
		Sound &c = _snd[_cur];
		if (c.volume + kFadeStep > kAmbientVolume) {
			c.fadeIn = false;
			setVolume(c, kAmbientVolume);
		} else {
			setVolume(c, c.volume + kFadeStep);
		}
	}
}

void Ambience::stop() {
	for (uint i = 0; i < _snd.size(); i++)
		g_system->getMixer()->stopHandle(_snd[i].handle);
}

void Ambience::syncState(Common::Serializer &s) {
	int32 cur = _cur;
	s.syncAsSint32LE(cur);
	if (s.isLoading()) {
		stop();
		_cur = _prev = -1;
		_pending = cur;
	}
}

} // End of namespace Grumpa
