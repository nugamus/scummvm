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

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/memstream.h"
#include "common/system.h"

#include "audio/audiostream.h"
#include "audio/decoders/raw.h"
#include "audio/decoders/wave.h"
#include "graphics/cursorman.h"
#include "video/hnm_decoder.h"

#include "cryomni3d/china/engine.h"

namespace CryOmni3D {
namespace China {

// DIAL.TXT (E-0200): blocks `#id#`, `<text>`, `GOTO target`; ids and targets lowercased,
// `fin` ends a dialogue; `/` starts a comment running to the end of the line.
void CryOmni3DEngine_China::loadDialogues() {
	Common::File file;
	if (!file.open("LOC/DIAL.TXT")) {
		warning("China: no LOC/DIAL.TXT");
		return;
	}
	Common::String id;
	DialogueBlock block;
	while (!file.eos() && !file.err()) {
		Common::String line = file.readLine();
		const int comment = line.find('/');
		if (comment >= 0 && !line.contains('<')) {
			line = Common::String(line.c_str(), comment);
		}
		line.trim();
		if (line.size() > 2 && line.firstChar() == '#' && line.lastChar() == '#') {
			id = Common::String(line.c_str() + 1, line.size() - 2);
			id.trim();
			id.toLowercase();
			block = DialogueBlock();
		} else if (!id.empty() && line.hasPrefix("<")) {
			const size_t end = line.findLastOf('>');
			block.text = Common::String(line.c_str() + 1, end == Common::String::npos ? line.size() - 1 : end - 1);
			// Original bug: one line spells "Douairière" with code page 850's 0x82 for 'é',
			// which Windows-1252 shows as a low quote (E-0200).
			for (uint i = 0; i < block.text.size(); i++) {
				if ((byte)block.text[i] == 0x82) {
					block.text.setChar((char)0xe9, i);
				}
			}
		} else if (!id.empty() && line.size() > 4 && !scumm_strnicmp(line.c_str(), "goto", 4)) {
			block.next = Common::String(line.c_str() + 4);
			block.next.trim();
			block.next.toLowercase();
			_dialogues[id] = block;
			id.clear();
		}
	}
}

// The subtitle band (E-0951): text wrapped to 630 pixels in font slot 1, a black band of
// n x 15 + 10 rows at the bottom, line i in white at x 5, y = band top + 5 + 15 i. A line
// holding `$` is drawn from after it.
void CryOmni3DEngine_China::drawSubtitle(const Common::String &text) {
	_fontManager.setCurrentFont(1);
	Common::Array<Common::String> lines;
	Common::String current;
	Common::String word;
	for (uint i = 0; i <= text.size(); i++) {
		const char c = i < text.size() ? text[i] : ' ';
		if (c != ' ') {
			word += c;
			continue;
		}
		if (word.empty()) {
			continue;
		}
		const Common::String candidate = current.empty() ? word : current + " " + word;
		if (!current.empty() && _fontManager.getStrWidth(candidate) > 630) {
			lines.push_back(current);
			current = word;
		} else {
			current = candidate;
		}
		word.clear();
	}
	if (!current.empty()) {
		lines.push_back(current);
	}
	if (lines.empty()) {
		return;
	}
	const int band = lines.size() * 15 + 10;
	_screen.fillRect(Common::Rect(0, 480 - band, 640, 480), _format.RGBToColor(0, 0, 0));
	_fontManager.setForeColor(textColor(0xffff));
	for (uint i = 0; i < lines.size(); i++) {
		Common::String line = lines[i];
		const int dollar = line.find('$');
		if (dollar >= 0) {
			line = Common::String(line.c_str() + dollar + 1);
		}
		_fontManager.displayStr(5, 480 - band + 5 + 15 * i, line);
	}
}

// Plays a voice file on the voice channel and keeps its samples for the lip sync.
bool CryOmni3DEngine_China::playVoice(const Common::String &id, Common::Array<int16> &samples) {
	samples.clear();
	Common::File file;
	if (!file.open(Common::Path("LOC/VOICES/" + id + ".WAV"))) {
		warning("China: no voice %s", id.c_str());
		return false;
	}
	int size = 0, rate = 0;
	byte flags = 0;
	// Voices are 16-bit mono (E-0104); the lip sync reads them sample by sample
	if (!Audio::loadWAVFromStream(file, size, rate, flags) || !(flags & Audio::FLAG_16BITS) ||
	    (flags & Audio::FLAG_STEREO)) {
		warning("China: bad voice %s", id.c_str());
		return false;
	}
	byte *data = (byte *)malloc(size);
	if (!data || file.read(data, size) != (uint32)size) {
		free(data);
		return false;
	}
	samples.resize(size / 2);
	for (uint i = 0; i < samples.size(); i++) {
		samples[i] = (int16)READ_LE_UINT16(data + 2 * i);
	}
	_voiceRate = rate;
	_mixer->stopHandle(_voiceHandle);
	_mixer->playStream(Audio::Mixer::kSpeechSoundType, &_voiceHandle,
	                   Audio::makeRawStream(data, size, rate, flags, DisposeAfterUse::YES));
	return true;
}

void CryOmni3DEngine_China::waitSoundChannel() {
	while (_mixer->isSoundHandleActive(_soundHandle) && !shouldAbort()) {
		pollEvents();
		g_system->updateScreen();
		g_system->delayMillis(10);
	}
}

// Voice line (E-0951): the place stays on screen with the block's text in the subtitle band
// whatever the subtitle option; Escape ends the current block.
void CryOmni3DEngine_China::voice(const char *line) {
	waitSoundChannel();
	Common::String id(line);
	id.toLowercase();
	Common::Array<int16> samples;
	while (_dialogues.contains(id) && !shouldAbort()) {
		const DialogueBlock &block = _dialogues[id];
		playVoice(id, samples);
		drawView();
		drawSubtitle(block.text);
		g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, 640, 480);
		while (_mixer->isSoundHandleActive(_voiceHandle) && !shouldAbort()) {
			pollEvents();
			if (checkKeysPressed(1, Common::KEYCODE_ESCAPE)) {
				_mixer->stopHandle(_voiceHandle);
			}
			g_system->updateScreen();
			g_system->delayMillis(10);
		}
		id = block.next;
	}
	if (!_dialogues.contains(id) && id != "fin") {
		warning("China: no dialogue block %s", id.c_str());
	}
}

bool CryOmni3DEngine_China::loadFaces(const char *stem, Face &face) {
	if (!stem) {
		return false;
	}
	for (uint k = 0; k < 4; k++) {
		face.loops[k].clear();
		Video::HNMDecoder decoder(_format, false, nullptr);
		if (!decoder.loadFile(Common::Path(Common::String::format("SYNC/%s%u.HNM", stem, k)))) {
			warning("China: no face %s%u", stem, k);
			return false;
		}
		decoder.start();
		while (!decoder.endOfVideo()) {
			const Graphics::Surface *frame = decoder.decodeNextFrame();
			if (!frame) {
				break;
			}
			face.loops[k].push_back(Graphics::ManagedSurface());
			face.loops[k].back().copyFrom(*frame);
		}
		if (face.loops[k].empty()) {
			return false;
		}
	}
	return true;
}

// Lip-sync dialogue (E-0952): full-screen faces, the speaking one only; its mouth follows
// the voice's amplitude a quarter of a second ahead.
void CryOmni3DEngine_China::dialogue(const char *line, const char *stemA, const char *stemB, bool noSubtitles) {
	waitSoundChannel();
	Face faces[2];
	const bool loaded[2] = { loadFaces(stemA, faces[0]), loadFaces(stemB, faces[1]) };
	uint frameOf[2] = { 0, 0 };
	const bool subtitles = !noSubtitles && ConfMan.getBool("subtitles");
	_mixer->setChannelVolume(_musicHandle, 20 * Audio::Mixer::kMaxChannelVolume / 127);

	Common::String id(line);
	id.toLowercase();
	const Common::String first(id.c_str(), MIN<uint>(3, id.size()));
	Common::Array<int16> samples;
	bool stop = false;
	CursorMan.showMouse(false);
	while (_dialogues.contains(id) && !stop && !shouldAbort()) {
		const DialogueBlock &block = _dialogues[id];
		const uint speaker = id.hasPrefix(first) ? 0 : 1;
		playVoice(id, samples);
		const uint32 duration = _voiceRate ? (uint32)((uint64)samples.size() * 1000 / _voiceRate) : 0;
		int loop = 3, last = -1;
		uint repeats[3] = { 0, 0, 0 };
		uint32 lastShut = 0;
		bool shutShown = false;
		const uint32 start = g_system->getMillis();
		// The faces follow the voice until half a second before its end (E-0952)
		while (_mixer->isSoundHandleActive(_voiceHandle) && _mixer->getSoundElapsedTime(_voiceHandle) + 500 < duration &&
		       !stop && !shouldAbort()) {
			// Pick a loop: 5 samples 0.25 s ahead of the play position
			const uint32 elapsed = _mixer->getSoundElapsedTime(_voiceHandle);
			const uint pos = (uint)((uint64)(elapsed + 250) * _voiceRate / 1000);
			int32 sum = 0;
			for (uint i = 0; i < 5 && pos + i < samples.size(); i++) {
				sum += samples[pos + i];
			}
			const uint32 now = g_system->getMillis() - start;
			if (ABS(sum / 5) < 1024) {
				if (shutShown && now - lastShut < 2000) {
					// Silence, but the shut mouth was shown less than 2 s ago: keep the frame
					pollEvents();
					if (checkKeysPressed(1, Common::KEYCODE_ESCAPE)) {
						_mixer->stopHandle(_voiceHandle);
						stop = true;
					}
					g_system->updateScreen();
					g_system->delayMillis(10);
					continue;
				}
				loop = 3;
				shutShown = true;
				lastShut = now;
			} else {
				// A talking loop picked twice in a row is drawn again once; only the counters of
				// loops 0 and 1 are reset, so loop 2 may repeat later (E-0952)
				loop = _rnd.getRandomNumber(2);
				if (loop == last && repeats[loop] < 1) {
					repeats[loop]++;
					loop = (loop + 1 + _rnd.getRandomNumber(1)) % 3;
				} else if (loop != last) {
					repeats[0] = repeats[1] = 0;
				}
				last = loop;
			}
			// A talking loop plays its 7 frames at 51 ms or more each; the shut loop one frame
			const uint frames = loop == 3 ? 1 : 7;
			for (uint f = 0; f < frames && !stop && !shouldAbort(); f++) {
				const uint32 frameStart = g_system->getMillis();
				pollEvents();
				if (checkKeysPressed(1, Common::KEYCODE_ESCAPE)) {
					_mixer->stopHandle(_voiceHandle);
					stop = true;
					break;
				}
				const Common::Array<Graphics::ManagedSurface> &frames7 = faces[speaker].loops[loop];
				if (loaded[speaker] && !frames7.empty()) {
					_screen.blitFrom(frames7[frameOf[speaker] % frames7.size()]);
				} else {
					_screen.clear(_format.RGBToColor(0, 0, 0));
				}
				frameOf[speaker] = (frameOf[speaker] + 1) % 7;
				if (subtitles) {
					drawSubtitle(block.text);
				}
				g_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, 640, 480);
				g_system->updateScreen();
				const uint32 spent = g_system->getMillis() - frameStart;
				if (spent < 51) {
					g_system->delayMillis(51 - spent);
				}
			}
		}
		id = block.next;
	}
	while (_mixer->isSoundHandleActive(_voiceHandle) && !stop && !shouldAbort()) {
		pollEvents();
		g_system->delayMillis(10);
	}
	_mixer->stopHandle(_voiceHandle);
	_mixer->setChannelVolume(_musicHandle, Audio::Mixer::kMaxChannelVolume);
	CursorMan.showMouse(true);
	clearKeys();
}

// Place sounds (E-0950): one channel; queue drops the call while it plays, never loops.
void CryOmni3DEngine_China::playSound(const Common::String &name) {
	Common::File *file = new Common::File();
	if (!file->open(Common::Path("LOC/VOICES/" + name + ".WAV")) && !file->open(Common::Path("SOUND/" + name + ".WAV"))) {
		delete file;
		return;
	}
	Audio::RewindableAudioStream *stream = Audio::makeWAVStream(file, DisposeAfterUse::YES);
	if (stream) {
		_mixer->playStream(Audio::Mixer::kSFXSoundType, &_soundHandle, stream);
	}
}

void CryOmni3DEngine_China::soundQueue(const char *name) {
	if (_mixer->isSoundHandleActive(_soundHandle)) {
		return;
	}
	playSound(name);
}

void CryOmni3DEngine_China::soundPlayWait(const char *name) {
	waitSoundChannel();
	playSound(name);
	waitSoundChannel();
}

void CryOmni3DEngine_China::soundStop() {
	_mixer->stopHandle(_soundHandle);
}

} // End of namespace China
} // End of namespace CryOmni3D
