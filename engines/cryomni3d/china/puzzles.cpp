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

// China's eight puzzles (games/china/docs/puzzles.md, E-1200..E-1253), entered through puzzle(n, m).

#include "common/file.h"
#include "common/system.h"

#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "image/tga.h"

#include "cryomni3d/china/engine.h"

namespace CryOmni3D {
namespace China {

// Cursor sprite ids (E-0901) used here
enum {
	kPuzzleCursorTake = 10,
	kPuzzleCursorDefault = 11,
	kPuzzleCursorUse = 14
};

static Common::Rect spriteRect(const Sprite &s) {
	return Common::Rect(s.pos.x, s.pos.y, s.pos.x + s.surface.w, s.pos.y + s.surface.h);
}

// Pixel hit (E-1250): inside the sprite and not the key colour.
// Original bug: the bottom test accepts y = top + height, a row past the sprite (E-1250); it does not hit here.
static bool pixelHit(const Sprite &s, const Common::Point &p) {
	const int x = p.x - s.pos.x, y = p.y - s.pos.y;
	if (s.surface.empty() || x < 0 || y < 0 || x >= s.surface.w || y >= s.surface.h) {
		return false;
	}
	return *(const uint16 *)s.surface.getBasePtr(x, y) != s.keyColor;
}

Common::Path CryOmni3DEngine_China::puzzlePath(const Common::String &file) const {
	return Common::Path("PUZZLES/" + _puzzleDir + "/" + file);
}

// setImage of the puzzle's TGA (E-1200): it becomes the still shown.
bool CryOmni3DEngine_China::puzzleBackground(const Common::String &name) {
	Common::File file;
	if (!file.open(puzzlePath(name + ".TGA"))) {
		warning("China: cannot open puzzle picture %s", name.c_str());
		return false;
	}
	Image::TGADecoder tga;
	if (!tga.loadStream(file) || !tga.getSurface()) {
		warning("China: cannot decode puzzle picture %s", name.c_str());
		return false;
	}
	Graphics::Surface *conv = tga.getSurface()->convertTo(_format);
	_still.copyFrom(*conv);
	conv->free();
	delete conv;
	_display = kDisplayStill;
	_fadePending = false;
	return true;
}

bool CryOmni3DEngine_China::puzzleSprite(const Common::String &name, Sprite &sprite) {
	return loadSprite(puzzlePath(name + ".SPR"), sprite);
}

// RAW masks: one byte per pixel, 640x480 (formats README).
bool CryOmni3DEngine_China::puzzleMask(const Common::String &name, Common::Array<byte> &mask) {
	Common::File file;
	mask.resize(640 * 480);
	if (!file.open(puzzlePath(name)) || file.read(mask.data(), mask.size()) != mask.size()) {
		warning("China: cannot read puzzle mask %s", name.c_str());
		return false;
	}
	return true;
}

// Plays a sound on a channel, restarting what the channel played (E-1200). A missing sound
// file is skipped silently (Q-1251).
void CryOmni3DEngine_China::puzzleSound(const char *file, uint channel) {
	Audio::SoundHandle &handle = _puzzleChannels[channel];
	_mixer->stopHandle(handle);
	Common::File *f = new Common::File();
	if (!f->open(puzzlePath(file))) {
		delete f;
		return;
	}
	Audio::RewindableAudioStream *stream = Audio::makeWAVStream(f, DisposeAfterUse::YES);
	if (stream) {
		_mixer->playStream(Audio::Mixer::kSFXSoundType, &handle, stream);
	}
}

// A button already down when the puzzle opens is the click that opened it: the latch starts set.
void CryOmni3DEngine_China::puzzleStart(PuzzleInput &in) {
	in = PuzzleInput();
	in.latch = getCurrentMouseButton() == 1;
	in.rightLatch = getCurrentMouseButton() == 2;
	_cursorId = -1;
	setCursorSprite(kPuzzleCursorDefault);
	clearKeys();
}

// The per-frame input of the common frame (E-1200): one press per button-down, Escape, Space.
void CryOmni3DEngine_China::puzzlePoll(PuzzleInput &in) {
	pollEvents();
	in.escape = in.space = false;
	while (!_keysPressed.empty()) {
		const Common::KeyCode key = _keysPressed.pop().keycode;
		if (key == Common::KEYCODE_ESCAPE) {
			in.escape = true;
		} else if (key == Common::KEYCODE_SPACE) {
			in.space = true;
		}
	}
	const uint button = getCurrentMouseButton();
	in.held = button == 1;
	in.press = in.held && !in.latch;
	in.latch = in.held;
	in.rightPress = button == 2 && !in.rightLatch;
	in.rightLatch = button == 2;
}

// Present the composed frame; a hover label goes after the cursor, at cursor + 20 (E-1201).
void CryOmni3DEngine_China::puzzleFlip(const Common::String &label) {
	if (!label.empty()) {
		drawLabelBox(label, cursorTopLeft() + Common::Point(20, 20));
	}
	g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, 640, 480);
	g_system->updateScreen();
	waitFrame();
}

// A held object goes back to the inventory (E-0953).
void CryOmni3DEngine_China::puzzleReturnHeld() {
	if (_heldObject != kNoObject) {
		_objects[_heldObject].state = 2;
		_objects[_heldObject].slot = -1;
		_heldObject = kNoObject;
	}
	_cursorId = -1;
}

// puzzle(n, m) (E-0953, E-1200): held object back, puzzle mode, the blocking run, then the
// current place is entered again. A puzzle that cannot load returns the previous result.
int32 CryOmni3DEngine_China::puzzle(int32 number, int32 arg) {
	puzzleReturnHeld();
	_puzzleMode = true;
	int32 result;
	switch (number) {
	case 2:
		_puzzleDir = "BOUDDHA";
		result = puzzleBouddha();
		break;
	case 3:
		_puzzleDir = "SCEAUX";
		result = puzzleSceaux(arg);
		break;
	case 4:
		_puzzleDir = "PUZZLEGO";
		result = puzzleGo();
		break;
	case 5:
		_puzzleDir = "PUZZLE4";
		result = puzzleRings();
		break;
	case 6:
		_puzzleDir = "HORLOGE";
		result = puzzleHorloge();
		break;
	case 7:
		_puzzleDir = "PORTE";
		result = puzzleBoutons();
		break;
	case 8:
		_puzzleDir = "BOMBE";
		result = puzzleBombe();
		break;
	default: // 1 and any other
		_puzzleDir = "PENJING";
		result = puzzlePenjing();
		break;
	}
	_puzzleMode = false;
	_cursorId = -1;
	for (uint i = 0; i < ARRAYSIZE(_puzzleChannels); i++) {
		_mixer->stopHandle(_puzzleChannels[i]);
	}
	// A puzzle that failed to load may have shown its picture already: the place comes back too
	if (_place) {
		gotoPlace(_place->name);
	}
	if (result < 0) {
		return _puzzleResult;
	}
	_puzzleResult = result;
	return result;
}

// 1 Penjing (E-1201)
int32 CryOmni3DEngine_China::puzzlePenjing() {
	static const struct {
		const char *stem;
		const char *key;
	} animals[12][2] = {
		{ { "hiro", "HIRONDELLE" }, { "serp", "SERPENT" } },
		{ { "grue", "GRUE" }, { "coq", "COQ" } },
		{ { "tigr", "TIGRE" }, { "lion", "LION" } },
		{ { "chin", "CHIEN" }, { "poul", "POULE" } },
		{ { "chau", "CHAUVE_SOURIS" }, { "chev", "CHEVAL" } },
		{ { "pois", "POISSON" }, { "sang", "SANGLIER" } },
		{ { "liev", "LI\xC8VRE" }, { "rena", "RENARD" } },
		{ { "cerf", "CERF" }, { "beuf", "BOEUF" } },
		{ { "sing", "SINGE" }, { "tort", "TORTUE" } },
		{ { "chat", "CHAT" }, { "rat", "RAT" } },
		{ { "drag", "DRAGON" }, { "fais", "FAISAN" } },
		{ { "chvr", "CH\xC8VRE" }, { "bich", "BICHE" } }
	};
	// Win: the first animal shown in these slots, the second in the others
	const uint winFirst = (1 << 2) | (1 << 3) | (1 << 6) | (1 << 8) | (1 << 10) | (1 << 11);

	Sprite sprites[12][2];
	if (!puzzleBackground("penjing")) {
		return -1;
	}
	for (uint i = 0; i < 12; i++) {
		for (uint j = 0; j < 2; j++) {
			if (!puzzleSprite(Common::String(animals[i][j].stem) + "_spr", sprites[i][j])) {
				return -1;
			}
		}
	}
	PuzzleInput in;
	puzzleStart(in);
	uint firstShown = 0xfff;
	int32 result = 0;
	while (!shouldAbort()) {
		puzzlePoll(in);
		const Common::Point p = cursorTopLeft();
		if (in.press) {
			for (uint i = 0; i < 12; i++) {
				if (spriteRect(sprites[i][0]).contains(p)) {
					firstShown ^= 1 << i;
					puzzleSound("penjinga.wav", 0);
				}
			}
		}
		_screen.blitFrom(_still);
		Common::String hover;
		bool hovered = false;
		for (uint i = 0; i < 12; i++) {
			const uint j = (firstShown >> i) & 1 ? 0 : 1;
			_screen.transBlitFrom(sprites[i][j].surface, sprites[i][j].pos, sprites[i][j].keyColor);
			if (!hovered && spriteRect(sprites[i][j]).contains(p)) {
				hovered = true;
				hover = label(animals[i][j].key);
			}
		}
		puzzleFlip(hover);
		if (firstShown == winFirst) {
			result = 1;
			break;
		}
		if (in.escape) {
			break;
		}
	}
	return result;
}

// 2 Bouddha (E-1202)
int32 CryOmni3DEngine_China::puzzleBouddha() {
	// Pieces in draw order: B1, B2, R1, R2, R3
	static const char *const names[5] = { "b1_spr", "b2_spr", "r1_spr", "r2_spr", "r3_spr" };
	Sprite sprites[5];
	if (!puzzleBackground("bouddha")) {
		return -1;
	}
	for (uint i = 0; i < 5; i++) {
		if (!puzzleSprite(names[i], sprites[i])) {
			return -1;
		}
	}
	PuzzleInput in;
	puzzleStart(in);
	bool shown[5] = { true, true, true, true, true };
	bool armed = false, cleared = false;
	int32 result = 0;
	while (!shouldAbort()) {
		puzzlePoll(in);
		const Common::Point p = cursorTopLeft();
		if (in.press) {
			for (uint i = 0; i < 5; i++) {
				if (spriteRect(sprites[i]).contains(p)) {
					shown[i] = !shown[i];
					puzzleSound("bouddhaa.wav", 0);
				}
			}
		}
		_screen.blitFrom(_still);
		for (uint i = 0; i < 5; i++) {
			if (shown[i]) {
				_screen.transBlitFrom(sprites[i].surface, sprites[i].pos, sprites[i].keyColor);
			}
		}
		puzzleFlip();
		if (shown[0] || shown[1]) {
			armed = false;
		} else if (shown[2] && shown[3] && shown[4]) {
			armed = true;
		}
		cleared = !(shown[2] || shown[3] || shown[4]);
		if (armed && cleared) {
			result = 1;
			break;
		}
		if (in.escape) {
			break;
		}
	}
	return result;
}

// 3 Sceaux (E-1203); variant 0 is spfw101 (with the wax CIRE and the bar), 1 is arbre3.
int32 CryOmni3DEngine_China::puzzleSceaux(int32 variant) {
	if (variant != 0 && variant != 1) {
		warning("China: Sceaux variant %d does not exist", variant);
		return -1;
	}
	const bool withBar = variant == 0;
	const uint kCire = 21; // the wax (E-1203)
	Sprite seal[3], shadow[3], demat[3], debrill[3], target[3], pad;
	if (!puzzleBackground(withBar ? "fond" : "fond2") || !puzzleSprite("tampons", pad)) {
		return -1;
	}
	for (uint i = 0; i < 3; i++) {
		const Common::String n = Common::String::format("%u", i + 1);
		if (!puzzleSprite("sceau" + n, seal[i]) || !puzzleSprite("ombre" + n, shadow[i]) ||
		        !puzzleSprite("demat" + n, demat[i]) || !puzzleSprite("debrill" + n, debrill[i]) ||
		        !puzzleSprite("empr" + n + (withBar ? "" : "f"), target[i])) {
			return -1;
		}
	}
	PuzzleInput in;
	puzzleStart(in);
	int step[3] = { 0, 0, 0 };
	bool held[3] = { false, false, false };
	bool imprinted[3] = { false, false, false };
	bool cireGiven = false;
	int32 result = 0;

	auto drawScene = [&]() {
		_screen.blitFrom(_still);
		for (uint i = 0; i < 3; i++) {
			if (!held[i]) {
				if (step[i] == 1) {
					_screen.transBlitFrom(demat[i].surface, Common::Point(demat[i].pos.x, debrill[i].pos.y), demat[i].keyColor);
				} else if (step[i] == 3) {
					_screen.transBlitFrom(debrill[i].surface, debrill[i].pos, debrill[i].keyColor);
				} else {
					_screen.transBlitFrom(shadow[i].surface, shadow[i].pos, shadow[i].keyColor);
					_screen.transBlitFrom(seal[i].surface, seal[i].pos, seal[i].keyColor);
				}
			}
			if (imprinted[i]) {
				_screen.transBlitFrom(target[i].surface, target[i].pos, target[i].keyColor);
			}
		}
		_screen.transBlitFrom(pad.surface, pad.pos, pad.keyColor);
		const Common::Point p = cursorTopLeft();
		for (uint i = 0; i < 3; i++) {
			if (held[i]) {
				_screen.transBlitFrom(seal[i].surface, Common::Point(p.x - seal[i].surface.w / 2, p.y - seal[i].surface.h / 2),
				                      seal[i].keyColor);
			}
		}
	};

	while (!shouldAbort()) {
		puzzlePoll(in);
		// Step 3 (variant 0): a one second flash, then CIRE in the hand the first time
		for (uint i = 0; i < 3; i++) {
			if (step[i] != 3) {
				continue;
			}
			const uint32 start = g_system->getMillis();
			while (g_system->getMillis() - start < 1000 && !shouldAbort()) {
				puzzlePoll(in); // input only polled, no clicks
				drawScene();
				puzzleFlip();
			}
			in.press = false;
			if (!cireGiven) {
				cireGiven = true;
				// CIRE in the hand whatever its state (E-1203)
				_objects[kCire].state = 1;
				_objects[kCire].slot = -1;
				_heldObject = kCire;
				loadSprite(Common::Path(Common::String::format("SPRITES/OBJETS/R_%s.SPR", objectStem(kCire))), _heldCursor);
				_cursorId = -1;
			}
			step[i] = 4;
		}
		setCursorSprite(_heldObject != kNoObject ? (int)kCursorHeld : (int)kPuzzleCursorDefault);
		if (in.press && _heldObject == kNoObject) {
			const Common::Point p = cursorTopLeft();
			for (uint i = 0; i < 3; i++) {
				if (!spriteRect(seal[i]).contains(p)) {
					continue;
				}
				bool otherHeld = false;
				for (uint j = 0; j < 3; j++) {
					if (j != i && step[j] == 1) {
						step[j] = 0;
					}
					otherHeld = otherHeld || (j != i && held[j]);
				}
				if (otherHeld) {
					continue;
				}
				if (step[i] == 4 || step[i] == 5) {
					held[i] = !held[i];
				} else if (step[i] == 1) {
					step[i] = withBar ? 3 : 4;
				} else if (step[i] == 0) {
					step[i] = 1;
				}
			}
			if (spriteRect(pad).contains(p)) {
				for (uint i = 0; i < 3; i++) {
					if (held[i] && step[i] == 4) {
						puzzleSound("tmpn.wav", 0);
						step[i] = 5;
					}
				}
			}
			// Target t takes the seal (t + 1) % 3
			for (uint t = 0; t < 3; t++) {
				const uint s = (t + 1) % 3;
				if (spriteRect(target[t]).contains(p) && held[s] && step[s] == 5) {
					puzzleSound("tmpn.wav", 0);
					imprinted[t] = true;
				}
			}
		}
		// Variant 0 opens the bar on Space or a right press while CIRE is not in the inventory
		if ((in.space || in.rightPress) && withBar && objectState(kCire) != 2) {
			Graphics::ManagedSurface background;
			background.copyFrom(_still);
			_still.copyFrom(_screen);
			const int32 stay = interfaceScreen();
			_still.copyFrom(background);
			in.latch = getCurrentMouseButton() == 1;
			in.rightLatch = getCurrentMouseButton() == 2;
			if (!stay) {
				break;
			}
		}
		drawScene();
		puzzleFlip();
		if (imprinted[0] && imprinted[1] && imprinted[2]) {
			result = 1;
			break;
		}
	}
	return result;
}

// 4 Go (E-1204). Sprite numbers are the original's: BOUT<5r+5-c>, BARRE<4r+4-c> (horizontal),
// BARRE<37+r-4c> (vertical), row r from the back, column c from the left.
int32 CryOmni3DEngine_China::puzzleGo() {
	Sprite stone[26], bar[41];
	if (!puzzleBackground("go1")) {
		return -1;
	}
	for (uint i = 1; i <= 25; i++) {
		if (!puzzleSprite(Common::String::format("bout%u", i), stone[i])) {
			return -1;
		}
	}
	for (uint i = 1; i <= 40; i++) {
		if (!puzzleSprite(Common::String::format("barre%u", i), bar[i])) {
			return -1;
		}
	}
	// The two stones each bar joins
	uint barA[41], barB[41];
	for (int r = 0; r < 5; r++) {
		for (int c = 0; c < 4; c++) {
			const uint n = 4 * r + 4 - c;
			barA[n] = 5 * r + 5 - c;
			barB[n] = 5 * r + 5 - (c + 1);
		}
	}
	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 5; c++) {
			const uint n = 37 + r - 4 * c;
			barA[n] = 5 * r + 5 - c;
			barB[n] = 5 * (r + 1) + 5 - c;
		}
	}
	static const uint solution[9] = { 3, 7, 8, 9, 12, 13, 14, 18, 23 };

	PuzzleInput in;
	puzzleStart(in);
	bool placed[26] = {}, barOn[41] = {};
	const Audio::SoundHandle &click = _puzzleChannels[0];
	int32 result = 0;
	while (!shouldAbort()) {
		puzzlePoll(in);
		if (in.press) {
			const Common::Point p = cursorTopLeft();
			for (uint i = 1; i <= 25; i++) {
				if (spriteRect(stone[i]).contains(p)) {
					placed[i] = !placed[i];
					puzzleSound("go1a.wav", 0);
				}
			}
			while (_mixer->isSoundHandleActive(click) && !shouldAbort()) {
				pollEvents();
				g_system->delayMillis(10);
			}
			for (uint n = 1; n <= 40; n++) {
				const bool on = placed[barA[n]] && placed[barB[n]];
				if (on && !barOn[n] && (n <= 20 || !_mixer->isSoundHandleActive(click))) {
					puzzleSound("go1b.wav", 1);
				}
				barOn[n] = on;
			}
		}
		_screen.blitFrom(_still);
		for (uint i = 1; i <= 25; i++) {
			if (placed[i]) {
				_screen.transBlitFrom(stone[i].surface, stone[i].pos, stone[i].keyColor);
			}
		}
		for (uint n = 21; n <= 40; n++) {
			if (barOn[n]) {
				_screen.transBlitFrom(bar[n].surface, bar[n].pos, bar[n].keyColor);
			}
		}
		for (uint n = 1; n <= 20; n++) {
			if (barOn[n]) {
				_screen.transBlitFrom(bar[n].surface, bar[n].pos, bar[n].keyColor);
			}
		}
		puzzleFlip();
		bool solved = true;
		for (uint i = 1; i <= 25; i++) {
			bool wanted = false;
			for (uint k = 0; k < ARRAYSIZE(solution); k++) {
				wanted = wanted || solution[k] == i;
			}
			solved = solved && placed[i] == wanted;
		}
		if (solved) {
			result = 1;
			break;
		}
		if (in.escape) {
			break;
		}
	}
	return result;
}

// 5 Puzzle4 (E-1250): three rings of sprites, then four sun/moon/sea/sky switches.
int32 CryOmni3DEngine_China::puzzleRings() {
	static const char *const stems[3] = { "couleur", "animaux", "direct" };
	static const char *const labelRings[3][8] = {
		{ "rouge", "jaune", "blanc", "bleuc", "noir", "gris", "vert", "bleuf" },
		{ "OISEAU", "rien", "TIGRE", "rien", "TORTUE", "rien", "DRAGON", "rien" },
		{ "SUD", "rien", "OUEST", "rien", "NORD", "rien", "EST", "rien" }
	};
	static const char *const phase2[4] = { "soleil", "lune", "mer", "ciel" };

	Sprite ring[3][9]; // by file number 1..8; entry i of a ring is file 8 - i
	Sprite plain[4], alt[4];
	Common::Array<byte> mask;
	if (!puzzleBackground("fond") || !puzzleMask("mask.raw", mask)) {
		return -1;
	}
	for (uint r = 0; r < 3; r++) {
		for (uint f = 1; f <= 8; f++) {
			if (!puzzleSprite(Common::String::format("%s%u", stems[r], f), ring[r][f])) {
				return -1;
			}
		}
	}
	for (uint i = 0; i < 4; i++) {
		if (!puzzleSprite(phase2[i], plain[i]) || !puzzleSprite(Common::String(phase2[i]) + "2", alt[i])) {
			return -1;
		}
	}
	PuzzleInput in;
	puzzleStart(in);
	const char *labels[3][8];
	memcpy(labels, labelRings, sizeof(labels));
	uint entry[3] = { 3, 3, 3 };
	bool on[4] = { false, false, false, false };
	uint chain = 0;
	bool phaseTwo = false;
	int32 result = 0;
	while (!shouldAbort()) {
		puzzlePoll(in);
		const Common::Point p = cursorTopLeft(); // E-1254
		const int m = p.x >= 0 && p.x < 640 && p.y >= 0 && p.y < 480 ? (int)mask[p.y * 640 + p.x] - 231 : 24;
		if (in.press) {
			if (!phaseTwo) {
				if (m >= 0 && m < 24) {
					const uint r = m / 8;
					const char *last = labels[r][7];
					for (uint k = 7; k > 0; k--) {
						labels[r][k] = labels[r][k - 1];
					}
					labels[r][0] = last;
					puzzleSound("puzzle4a.wav", 0);
				}
				for (uint r = 0; r < 3; r++) {
					if (pixelHit(ring[r][8 - entry[r]], p)) {
						entry[r] = (entry[r] + 7) % 8;
					}
				}
			} else {
				for (uint i = 0; i < 4; i++) {
					if (spriteRect(plain[i]).contains(p)) {
						on[i] = !on[i];
						puzzleSound("puzzle4b.wav", 1);
					}
				}
			}
		}
		_screen.blitFrom(_still);
		if (!phaseTwo) {
			for (uint r = 0; r < 3; r++) {
				const Sprite &s = ring[r][8 - entry[r]];
				_screen.transBlitFrom(s.surface, s.pos, s.keyColor);
			}
		} else {
			for (uint i = 0; i < 4; i++) {
				const Sprite &s = on[i] ? plain[i] : alt[i];
				_screen.transBlitFrom(s.surface, s.pos, s.keyColor);
			}
		}
		puzzleFlip(m >= 0 && m < 24 ? label(labels[m / 8][m % 8]) : Common::String());
		if (!phaseTwo) {
			if (entry[0] == 7 && entry[1] == 7 && entry[2] == 7) {
				phaseTwo = true;
				puzzleBackground("puzenter");
			}
		} else {
			// The chain: switch on in the order soleil, lune, mer, ciel, each while the later ones are off
			for (uint k = 0; k < chain; k++) {
				if (!on[k]) {
					chain = k;
					break;
				}
			}
			while (chain < 4 && on[chain]) {
				bool laterOff = true;
				for (uint k = chain + 1; k < 4; k++) {
					laterOff = laterOff && !on[k];
				}
				if (!laterOff) {
					break;
				}
				chain++;
			}
			if (chain == 4) {
				result = 1;
				break;
			}
		}
		if (in.escape) {
			break;
		}
	}
	return result;
}

// 6 Horloge (E-1251): four hands dragged over a mask.
int32 CryOmni3DEngine_China::puzzleHorloge() {
	static const char kLetters[4] = { 'a', 'b', 'd', 'c' };
	static const uint kCounts[4] = { 12, 12, 12, 30 };
	static const uint kStart[4] = { 8, 4, 5, 18 };
	static const uint kSolved[4] = { 6, 9, 10, 8 };
	// Mask values p of a hand: A and B 1..12, D 13..24, C 25..54
	static const int kMaskBase[4] = { 0, 0, 12, 24 };

	Sprite hand[4][31];
	Common::Array<byte> mask;
	if (!puzzleBackground("hrlgfnd") || !puzzleMask("mask.raw", mask)) {
		return -1;
	}
	for (uint h = 0; h < 4; h++) {
		for (uint n = 1; n <= kCounts[h]; n++) {
			if (!puzzleSprite(Common::String::format("aig%02u%c", n, kLetters[h]), hand[h][n])) {
				return -1;
			}
		}
	}
	PuzzleInput in;
	puzzleStart(in);
	uint cur[4] = { kStart[0], kStart[1], kStart[2], kStart[3] };
	int grabbed = -1;
	int previous = 0;
	int32 result = 0;
	while (!shouldAbort()) {
		puzzlePoll(in);
		const Common::Point p = cursorTopLeft(); // E-1254
		if (!in.held) {
			grabbed = -1;
		} else if (grabbed < 0) {
			for (int h = 0; h < 4 && grabbed < 0; h++) {
				if (pixelHit(hand[h][cur[h]], p)) {
					grabbed = h;
					previous = 0;
				}
			}
		}
		if (grabbed >= 0) {
			const int v = p.x >= 0 && p.x < 640 && p.y >= 0 && p.y < 480 ? mask[p.y * 640 + p.x] : 0;
			const int base = kMaskBase[grabbed];
			if (v > base && v <= base + 12 + (grabbed == 3 ? 18 : 0)) {
				const uint pos = v - base;
				cur[grabbed] = pos + 1 > kCounts[grabbed] ? 1 : pos + 1;
			}
			if (v != previous && previous != 0 && v >= 1 && v <= 54) {
				puzzleSound("clicaig.wav", 0);
			}
			previous = v;
		}
		_screen.blitFrom(_still);
		for (uint h = 0; h < 4; h++) {
			_screen.transBlitFrom(hand[h][cur[h]].surface, hand[h][cur[h]].pos, hand[h][cur[h]].keyColor);
		}
		puzzleFlip();
		if (grabbed < 0 && cur[0] == kSolved[0] && cur[1] == kSolved[1] && cur[2] == kSolved[2] &&
		        cur[3] == kSolved[3]) {
			video("puzhorlo");
			result = 1;
			break;
		}
		if (in.escape) {
			break;
		}
	}
	return result;
}

// 7 Boutons (E-1252): three columns of six symbols, each pressed back to APPOSER.
int32 CryOmni3DEngine_China::puzzleBoutons() {
	static const struct {
		const char *stem;
		const char *key;
	} columns[3][6] = {
		{ { "app1", "APPOSER" }, { "lett", "LETTR\xC9" }, { "peti", "PETIT" }, { "parl", "PARLER" },
		  { "homm", "HOMME" }, { "gran", "GRAND" } },
		{ { "app2", "APPOSER" }, { "ruis", "RUISSEAU" }, { "jade", "JADE" }, { "voya", "VOYAGER" },
		  { "coeu", "COEUR" }, { "femm", "FEMME" } },
		{ { "app3", "APPOSER" }, { "lune", "LUNE" }, { "sole", "SOLEIL" }, { "mont", "MONTAGNE" },
		  { "chef", "CHEF" }, { "forc", "FORCE" } }
	};
	Sprite sprites[3][6];
	if (!puzzleBackground("porte")) {
		return -1;
	}
	for (uint c = 0; c < 3; c++) {
		for (uint i = 0; i < 6; i++) {
			if (!puzzleSprite(Common::String(columns[c][i].stem) + "_spr", sprites[c][i])) {
				return -1;
			}
		}
	}
	PuzzleInput in;
	puzzleStart(in);
	uint shown[3] = { 2, 4, 0 };
	int32 result = 0;
	while (!shouldAbort()) {
		puzzlePoll(in);
		const Common::Point p = cursorTopLeft(); // E-1254
		Common::String hover;
		for (uint c = 0; c < 3; c++) {
			if (!spriteRect(sprites[c][shown[c]]).contains(p)) {
				continue;
			}
			hover = label(columns[c][shown[c]].key);
			if (in.press) {
				const bool wrap = shown[c] == 0;
				shown[c] = wrap ? 5 : shown[c] - 1;
				puzzleSound("bouton1a.wav", 0);
				if (wrap) {
					puzzleSound("bouton1a.wav", 0);
				}
			}
		}
		_screen.blitFrom(_still);
		for (uint c = 0; c < 3; c++) {
			const Sprite &s = sprites[c][shown[c]];
			_screen.transBlitFrom(s.surface, s.pos, s.keyColor);
		}
		puzzleFlip(hover);
		if (!shown[0] && !shown[1] && !shown[2]) {
			video("puzporte");
			result = 1;
			break;
		}
		if (in.escape) {
			break;
		}
	}
	return result;
}

// 8 Bombe (E-1253): the bomb under the throne, step by step.
int32 CryOmni3DEngine_China::puzzleBombe() {
	enum {
		kTronem, kBombe1m, kBarre1, kBarre2, kBarre3, kBarre4, kAiguille, kBaretav, kBaretar, kCapsular,
		kCapsulav, kEcrouh, kEcroub, kGrandt, kPetitt, kShapeCount
	};
	static const char *const shapeNames[kShapeCount] = {
		"tronem", "bombe1m", "barre1", "barre2", "barre3", "barre4", "aiguille", "baretav", "baretar",
		"capsular", "capsulav", "ecrouh", "ecroub", "grandt", "petitt"
	};
	// Object ids (E-1253): TOURNEVIS 20, RUYI 22, MARTEAU 25
	enum { kTournevis = 20, kRuyi = 22, kMarteau = 25 };
	static const char *const kSounds[9] = { "", "", "coussin.wav", "tvis.wav", "pese.wav", "clang.wav",
	                                         "clic.wav", "metal.wav", "tuyau.wav" };

	Sprite shape[kShapeCount];
	if (!puzzleBackground("trone")) {
		return -1;
	}
	for (uint i = 0; i < kShapeCount; i++) {
		if (!puzzleSprite(shapeNames[i], shape[i])) {
			return -1;
		}
	}
	PuzzleInput in;
	puzzleStart(in);
	int step = 0;
	int32 result = 0;
	bool done = false;
	while (!shouldAbort() && !done) {
		puzzlePoll(in);
		const Common::Point p = cursorTopLeft(); // E-1254
		auto on = [&](int i) { return pixelHit(shape[i], p); };
		auto with = [&](uint id) { return _heldObject == id; };
		auto go = [&](int sound, const char *background, int next) {
			if (sound) {
				puzzleSound(kSounds[sound], sound);
			}
			if (background) {
				puzzleBackground(background);
			}
			step = next;
		};
		auto gameOver = [&](const char *videoName) {
			video(videoName);
			done = true;
		};

		if (in.space || in.rightPress) {
			const int32 stay = interfaceScreen();
			in.latch = getCurrentMouseButton() == 1;
			in.rightLatch = getCurrentMouseButton() == 2;
			if (!stay) {
				break;
			}
			continue;
		}
		if (in.press) {
			if (step == 0) {
				if (on(kTronem)) {
					go(2, "bombe1", 1);
				}
			} else if (step == 1) {
				if (on(kBombe1m)) {
					go(4, "bombe2", 2);
				}
			} else if (step <= 5) {
				if (on(kBarre1 + step - 2)) {
					go(7, Common::String::format("bombe2%d", step - 1).c_str(), step + 1);
				} else if (on(kAiguille)) {
					gameOver("gamover1");
				}
			} else if (step == 6) {
				if (on(kAiguille)) {
					gameOver("gamover1");
				} else {
					go(0, "bombe3", 7);
				}
			} else if (step == 7 || step == 8) {
				const int nut = step == 7 ? kEcroub : kEcrouh;
				if (on(nut)) {
					go(6, nullptr, step + 1);
				} else if ((on(kCapsular) || on(kCapsulav)) && (with(kRuyi) || with(kMarteau) || with(kTournevis))) {
					gameOver("gamover2");
				}
			} else if (step == 9) {
				if (on(kPetitt) && with(kTournevis)) {
					go(0, "bombe31", 10);
				}
			} else if (step == 10) {
				if (on(kPetitt) && with(kTournevis)) {
					go(0, "bombe32", 11);
				}
			} else if (step == 11) {
				if (on(kGrandt) && with(kTournevis)) {
					go(8, "bombe33", 12);
				}
			} else if (step == 12) {
				if (on(kBaretav) && with(kTournevis)) {
					go(3, "bombe34", 13);
				}
			} else if (step == 13) {
				if (on(kCapsulav)) {
					go(5, "bombe35", 14);
				}
			} else if (step == 14) {
				if (on(kBaretar) && with(kTournevis)) {
					go(3, "bombe36", 15);
				}
			} else if (step == 15) {
				if (on(kCapsular)) {
					go(5, "bombe37", 16);
				}
			} else {
				result = 1;
				done = true;
			}
		}
		if (done) {
			break;
		}
		// Hover label and cursor
		Common::String hover;
		if (step >= 2 && step <= 6 && on(kAiguille)) {
			hover = label("aiguille");
		} else if ((step >= 7 && step <= 15 && on(kCapsular)) || (step >= 7 && step <= 13 && on(kCapsulav))) {
			hover = label("poison");
		}
		int cursor = kPuzzleCursorDefault;
		if (_heldObject != kNoObject) {
			cursor = kCursorHeld;
		} else if ((step <= 5 && (on(kTronem) || on(kBombe1m) || on(kBarre1) || on(kBarre2) || on(kBarre3) ||
		                          on(kBarre4))) ||
		           (step == 7 && on(kEcroub)) || (step == 8 && on(kEcrouh)) || (step == 13 && on(kCapsulav)) ||
		           (step == 15 && on(kCapsular)) || step == 6 || step == 16) {
			cursor = kPuzzleCursorTake;
		} else if (((step == 9 || step == 10) && on(kPetitt)) || (step == 11 && on(kGrandt)) ||
		           (step == 12 && on(kBaretav)) || (step == 14 && on(kBaretar))) {
			cursor = kPuzzleCursorUse;
		}
		setCursorSprite(cursor);
		_screen.blitFrom(_still);
		puzzleFlip(hover);
		if (in.escape) {
			break;
		}
	}
	// A held object goes back to the inventory (E-1253)
	puzzleReturnHeld();
	return result;
}

} // End of namespace China
} // End of namespace CryOmni3D
