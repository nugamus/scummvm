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

// Monet's own game layer: its units, the players, the Option pages, the save and load
// screens and the gallery (games/monet/docs/, engines/x3d/docs/spec/ui.md and save.md)

#include "common/config-manager.h"
#include "common/ptr.h"
#include "common/savefile.h"
#include "common/events.h"
#include "common/file.h"
#include "common/system.h"

#include "audio/mixer.h"

#include "engines/metaengine.h"

#include "graphics/surface.h"

#include "x3d/frame.h"
#include "x3d/monet/gallery3d.h"
#include "x3d/interaction.h"
#include "x3d/inventory.h"
#include "x3d/renderer.h"
#include "x3d/sound.h"
#include "x3d/monet/u00.h"
#include "x3d/monet/u01.h"
#include "x3d/monet/u02.h"
#include "x3d/monet/u03.h"
#include "x3d/monet/u04.h"
#include "x3d/monet/u05.h"
#include "x3d/monet/u06.h"
#include "x3d/monet/u07.h"
#include "x3d/monet/u33.h"
#include "x3d/x3d.h"

namespace X3D {

void X3DEngine::saveMenu() {
	// OptionSave (save.md): slots 1..98, the first free one selected; OK saves and stays
	Frame frame;
	if (!frame.load("OptionSave"))
		return;
	// The original appends typing to "Save without name"; here typing replaces it
	frame.setText("Save without name");
	Common::StringArray names;
	names.resize(99);
	for (const SaveStateDescriptor &d : getMetaEngine()->listSaves(_targetName.c_str()))
		if (d.getSaveSlot() >= 1 && d.getSaveSlot() <= 98)
			names[d.getSaveSlot()] = d.getDescription();
	MenuList list;
	for (int s = 1; s <= 98; s++) {
		list.rows.push_back(Common::String::format("%d - %s", s + 1, names[s].empty() ? "Empty" : names[s].c_str()));
		if (list.selected < 0 && names[s].empty())
			list.selected = s - 1;
	}
	if (list.selected < 0)
		list.selected = 0;
	for (;;) {
		const Common::String c = runFrame(frame, &list);
		if (shouldQuit())
			return;
		if (c == "OptionSelectSave" || c == "enter") {
			// The frame stays open with the saved row selected and the edit's text kept;
			// Back, Main menu and Quit switch to their ...N bitmaps (E-0622)
			const Common::String name = _menuText.empty() ? "Save without name" : _menuText;
			const int s = list.selected + 1;
			if (saveGameState(s, name).getCode() == Common::kNoError)
				list.rows[s - 1] = Common::String::format("%d - %s", s + 1, name.c_str());
			for (int id : {2, 3, 5}) {
				const int v = frame.indexOf(id);
				Common::String bmp = v >= 0 ? frame.bitmapName(v) : "";
				if (bmp.size() >= 5) {
					bmp.setChar('N', bmp.size() - 5);
					frame.setBitmap(id, bmp);
				}
			}
			continue;
		}
		if (c == "OptionSaveSommaire" || c == "escape") {
			afterOptionMenu(optionMenu());
			return;
		}
		if (c == "SaveQuit") {
			if (runMenu("OptionQuitter") == "QuitterOK")
				quitGame();
			continue;
		}
		return; // OptionSave3D: back to the game
	}
}

bool X3DEngine::loadMenu() {
	// OptionLoad (save.md): the used slots only, nothing selected; OK loads
	MenuList list;
	Common::Array<int> slots;
	for (const SaveStateDescriptor &d : getMetaEngine()->listSaves(_targetName.c_str())) {
		slots.push_back(d.getSaveSlot());
		list.rows.push_back(Common::String::format("%d - %s", d.getSaveSlot() + 1, d.getDescription().c_str()));
	}
	for (;;) {
		const Common::String c = runMenu("OptionLoad", &list);
		if (shouldQuit())
			return false;
		if ((c == "OptionSelectGame" || c == "enter") && list.selected >= 0)
			return loadGameState(slots[list.selected]).getCode() == Common::kNoError;
		if (c == "OptionScreen" || c == "escape")
			return false;
	}
}

// The players and the unit of each one's last save, one "unit name" line per player, in
// the savefile <target>.players; "*" before the unit marks the current player (ui.md)
void X3DEngine::readPlayers(Common::StringArray &names, Common::Array<int> &units, Common::String *current) const {
	Common::ScopedPtr<Common::InSaveFile> in(_saveFileMan->openForLoading(_targetName + ".players"));
	while (in && !in->eos() && !in->err()) {
		const Common::String line = in->readLine();
		const char *space = strchr(line.c_str(), ' ');
		if (!space || !space[1])
			continue;
		const bool isCurrent = line.hasPrefix("*");
		units.push_back(atoi(line.c_str() + (isCurrent ? 1 : 0)));
		names.push_back(space + 1);
		if (isCurrent && current)
			*current = names.back();
	}
}

void X3DEngine::writePlayers(const Common::StringArray &names, const Common::Array<int> &units) {
	Common::ScopedPtr<Common::OutSaveFile> out(_saveFileMan->openForSaving(_targetName + ".players", false));
	if (!out)
		return;
	for (uint i = 0; i < names.size(); i++)
		out->writeString(Common::String::format("%s%d %s\n", names[i] == _playerName ? "*" : "", units[i], names[i].c_str()));
	out->finalize();
}

Common::StringArray X3DEngine::players(Common::String *current) const {
	Common::StringArray names;
	Common::Array<int> units;
	readPlayers(names, units, current);
	return names;
}

bool X3DEngine::selectPlayer(const Common::String &name) {
	_playerName = name;
	// Players share ScummVM's save slots; the original keeps saves per player
	Common::StringArray names;
	Common::Array<int> units;
	readPlayers(names, units);
	// The selected player becomes the current one, which the players screen offers next time
	const bool isNew = Common::find(names.begin(), names.end(), name) == names.end();
	if (isNew) {
		names.push_back(name);
		units.push_back(0);
	}
	writePlayers(names, units);
	return isNew;
}

Common::Error X3DEngine::saveGameState(int slot, const Common::String &desc, bool isAutosave) {
	const Common::Error result = Engine::saveGameState(slot, desc, isAutosave);
	// The player's unit number follows every save: it unlocks the gallery (ui.md)
	if (result.getCode() == Common::kNoError && !_playerName.empty()) {
		Common::StringArray names;
		Common::Array<int> units;
		readPlayers(names, units);
		for (uint i = 0; i < names.size(); i++)
			if (names[i] == _playerName)
				units[i] = atoi(_sceneName.c_str() + 1);
		writePlayers(names, units);
	}
	return result;
}

void X3DEngine::gameOver() {
	_inGameOver = true; // cleared when the next scene starts
	_sound->stopAll();
	storeHeldItem(); // the caught path stores it (E-0210)
	if (!loadMenu())
		afterOptionMenu(optionMenu());
}

Unit *X3DEngine::createUnit(const Common::String &sceneName) {
	// The gallery's 3D view of a painting (ui.md): unit class 50 on the painting's scene
	const bool view3d = !_gallery3D.empty() && sceneName.equalsIgnoreCase(Gallery3D::sceneFor(_gallery3D));
	const Common::String painting = _gallery3D;
	_gallery3D.clear();
	if (view3d)
		return new Gallery3D(this, painting);
	if (sceneName.size() < 3 || !sceneName.hasPrefixIgnoreCase("U") || !Common::isDigit(sceneName[1]))
		return nullptr;
	const int n = atoi(sceneName.c_str() + 1);
	switch (n) {
	case 0:
		return new U00(this, _practice);
	case 1:
		return new U01(this);
	case 2:
		return new U02(this);
	case 3:
		return new U03(this);
	case 4:
		return new U04(this);
	case 5:
		return new U05(this);
	case 6:
		return new U06(this);
	case 7:
		return new U07(this);
	case 33:
		return new U33(this);
	default:
		return nullptr;
	}
}

void X3DEngine::showPainting(const Common::String &name) {
	// TableauJeu (ui.md, Other frames): full screen until a click, Escape blocked; then
	// the voice stops
	Frame frame;
	if (!frame.load("TableauJeu"))
		return;
	frame.setBitmap(1, name);
	for (;;) {
		const Common::String c = runFrame(frame);
		if (shouldQuit() || (c != "escape" && c != "key" && c != "enter"))
			break;
	}
	_sound->stopGroup(Sound::kVoice);
}

void X3DEngine::credits() {
	// Credits (ui.md): a click or 6 s turns the page; page 1 shows twice (the frame's
	// first name has no extension), a key leaves
	Frame frame;
	if (!frame.load("Credits"))
		return;
	for (int page : { 1, 2, 3, 4, 5 }) {
		frame.setBitmap(1, Common::String::format("Credit%02d", page));
		for (int shown = 0; shown < (page == 1 ? 2 : 1); shown++) {
			const Common::String c = runFrame(frame, nullptr, 6000);
			if (shouldQuit() || (c != "timeout" && c != "MoveCredit"))
				return;
		}
	}
}

void X3DEngine::settings() {
	// OptionReglages (ui.md Settings): music is group 1, voice groups 2 and 3. The sliders
	// are ScummVM's music and speech/effects volumes, so they agree with the launcher's
	Frame frame;
	if (!frame.load("OptionReglages"))
		return;
	const int max = frame.sliderMax(4);
	frame.setSliderValue(4, ConfMan.getInt("music_volume") * max / Audio::Mixer::kMaxMixerVolume);
	frame.setSliderValue(5, ConfMan.getInt("speech_volume") * max / Audio::Mixer::kMaxMixerVolume);
	for (;;) {
		const Common::String c = runFrame(frame);
		if (shouldQuit() || c == "ReglageAnnuler" || c == "escape")
			return;
		if (c == "ReglageOK" || c == "enter") {
			const int voice = frame.sliderValue(5) * Audio::Mixer::kMaxMixerVolume / MAX(1, max);
			ConfMan.setInt("music_volume", frame.sliderValue(4) * Audio::Mixer::kMaxMixerVolume / MAX(1, max));
			ConfMan.setInt("speech_volume", voice);
			ConfMan.setInt("sfx_volume", voice);
			syncSoundSettings();
			return;
		}
	}
}

// The gallery's paintings in unlock order and the saved unit that unlocks up to each
// (ui.md Gallery)
static const char *const kPaintings[] = {
	"U11_01", "U11_02", "U11_03", "U12_03", "U12_04", "U13_14", "U13_05", "U13_13", "U13_03",
	"U13_01", "U13_12", "U13_11", "U13_06", "U13_04", "U14_01", "U13_15", "U14_02", "U14_05",
	"U14_03", "U14_07"
};

static uint unlockedPaintings(int unit) {
	switch (unit) {
	case 1: return 1;
	case 2: return 3;
	case 3: return 4;
	case 33: return 5;
	case 4: return 14;
	case 5: case 6: case 7: return 20;
	default: return 0;
	}
}

// The Galerie frame's thumbnail view per painting
static int thumbnailView(const Common::String &painting) {
	static const struct { const char *painting; int id; } views[] = {
		{ "U11_01", 5 }, { "U11_02", 22 }, { "U11_03", 6 }, { "U12_03", 7 }, { "U12_04", 40 },
		{ "U13_01", 8 }, { "U13_03", 9 }, { "U13_04", 10 }, { "U13_05", 11 }, { "U13_06", 12 },
		{ "U13_11", 13 }, { "U13_12", 14 }, { "U13_13", 15 }, { "U13_14", 30 }, { "U14_01", 16 },
		{ "U13_15", 17 }, { "U14_02", 18 }, { "U14_03", 19 }, { "U14_05", 20 }, { "U14_07", 21 }
	};
	for (const auto &v : views)
		if (painting == v.painting)
			return v.id;
	return -1;
}

int X3DEngine::playerUnit() const {
	Common::StringArray names;
	Common::Array<int> units;
	readPlayers(names, units);
	for (uint i = 0; i < names.size(); i++)
		if (names[i] == _playerName)
			return units[i];
	return 0;
}

void X3DEngine::gallery() {
	const uint count = unlockedPaintings(playerUnit());
	for (;;) {
		Frame galerie;
		if (!galerie.load("Galerie"))
			return;
		for (uint i = 0; i < ARRAYSIZE(kPaintings); i++)
			galerie.setVisible(thumbnailView(kPaintings[i]), i < count);
		const Common::String c = runFrame(galerie);
		if (shouldQuit() || c != "GoToTableau")
			return;
		const Common::String painting = galerie.bitmapName(_menuView).substr(0, 6);
		uint index = 0;
		while (index < count && painting != kPaintings[index])
			index++;
		if (index < count && !paintingScreens(index, count))
			return;
	}
}

// Tableau, Taille and Loupe of the unlocked painting index; false to leave the gallery
bool X3DEngine::paintingScreens(uint index, uint count) {
	Common::String screen = "Tableau";
	for (;;) {
		const Common::String p = kPaintings[index];
		Frame frame;
		if (!frame.load(screen))
			return false;
		frame.setBitmap(1, p + (screen == "Tableau" ? "TAB" : "_Size"));
		if (screen == "Tableau" && (p == "U14_02" || p == "U14_05"))
			frame.setVisible(30, false);
		const Common::String c = runFrame(frame);
		if (shouldQuit() || c == "escape")
			return false;
		if (c == "GoBack")
			return true;
		if (c == "GoPrev")
			index = (index + count - 1) % count;
		else if (c == "GoNext")
			index = (index + 1) % count;
		else if (c == "GoTaille")
			screen = "Taille";
		else if (c == "GoEcranTableau")
			screen = "Tableau";
		else if (c == "GoLoupe")
			magnifier(p);
		else if (c == "GotoScene3D" && !Gallery3D::sceneFor(p).empty()) {
			_gallery3D = p; // the Option menu hands over to the scene
			return false;
		}
	}
}

void X3DEngine::returnToPainting(const Common::String &painting) {
	// The painting's Tableau, then the gallery and the Option menu as the player leaves
	const uint count = unlockedPaintings(playerUnit());
	uint index = 0;
	while (index < ARRAYSIZE(kPaintings) && painting != kPaintings[index])
		index++;
	if (index >= count || paintingScreens(index, count))
		gallery();
	afterOptionMenu(_gallery3D.empty() ? optionMenu() : Common::String("Gallery3D"));
}

void X3DEngine::magnifier(const Common::String &painting) {
	// Loupe (ui.md): the parts listed in Media.txt stitched together, panned from the
	// edges, left on a click
	Common::File media;
	int cols = 0, rows = 0;
	if (media.open("2dbit/Media.txt")) {
		while (!media.eos()) {
			const Common::String line = media.readLine();
			if (line.hasPrefixIgnoreCase(painting + "Loupe;")) {
				const char *c = strchr(line.c_str() + painting.size() + 6, ';');
				if (c)
					sscanf(c + 1, "%d,%d", &cols, &rows);
				break;
			}
		}
	}
	if (cols <= 0 || rows <= 0)
		return;
	Common::Array<Graphics::Surface *> parts;
	int width = 0, height = 0;
	for (int i = 0; i < cols * rows; i++) {
		parts.push_back(loadBitmap(Common::Path(Common::String::format("2dbit/%sLoupe%d.BMP", painting.c_str(), i + 1))));
		if (!parts.back()) {
			warning("Missing magnifier part %d of %s", i + 1, painting.c_str());
			continue;
		}
		// Parts sit on a 640x480 grid; the canvas covers each one, even with a part missing
		width = MAX<int>(width, (i % cols) * 640 + parts.back()->w);
		height = MAX<int>(height, (i / cols) * 480 + parts.back()->h);
	}
	Graphics::Surface image;
	image.create(MAX(width, 640), MAX(height, 480), Graphics::PixelFormat::createFormatRGBA32());
	for (int i = 0; i < cols * rows; i++) {
		if (!parts[i])
			continue;
		Graphics::Surface *rgba = parts[i]->convertTo(image.format);
		image.copyRectToSurface(*rgba, (i % cols) * 640, (i / cols) * 480, Common::Rect(rgba->w, rgba->h));
		rgba->free();
		delete rgba;
		parts[i]->free();
		delete parts[i];
	}

	const int x2d = (_renderer->width() - 640) / 2;
	int ox = 0, oy = 0;
	uint32 lastPan = 0;
	bool done = false;
	while (!done && !shouldQuit()) {
		Common::Event e;
		while (_system->getEventManager()->pollEvent(e)) {
			actionToKey(e);
			if (e.type == Common::EVENT_MOUSEMOVE)
				_mouse = e.mouse;
			if (e.type == Common::EVENT_LBUTTONDOWN || (e.type == Common::EVENT_KEYDOWN && e.kbd.keycode == Common::KEYCODE_ESCAPE))
				done = true;
		}
		const int mx = _mouse.x - x2d, my = _mouse.y;
		const int left = mx < 30, right = mx >= 610, top = my < 30, bottom = my >= 450;
		static const int kinds[3][3] = { { 12, 13, 9 }, { 10, 0, 7 }, { 11, 6, 8 } }; // [v][h]
		if (_interaction)
			_interaction->showCursor(kinds[top ? 0 : bottom ? 2 : 1][left ? 0 : right ? 2 : 1]);
		const uint32 now = _system->getMillis();
		if (now - lastPan >= 80) {
			lastPan = now;
			if (left)
				ox -= (30 - mx) * 50 / 30;
			if (right)
				ox += (mx - 609) * 50 / 30;
			if (top)
				oy -= (30 - my) * 50 / 30;
			if (bottom)
				oy += (my - 449) * 50 / 30;
			ox = CLIP(ox, 0, image.w - 640);
			oy = CLIP(oy, 0, image.h - 480);
		}
		_renderer->clear();
		const Graphics::Surface view = image.getSubArea(Common::Rect(ox, oy, ox + 640, oy + 480));
		_renderer->drawImage(view, x2d, 0, false);
		_renderer->present();
		_system->delayMillis(10);
	}
	image.free();
}

Common::String X3DEngine::optionMenu() {
	for (;;) {
		// Load and Gallery are greyed when empty and still react (ui.md, Q-0063)
		Frame frame;
		if (!frame.load("Option"))
			return "";
		if (getMetaEngine()->listSaves(_targetName.c_str()).empty())
			frame.setBitmap(3, "SomB2");
		if (!unlockedPaintings(playerUnit()))
			frame.setBitmap(6, "SomE2");
		const Common::String c = runFrame(frame);
		if (shouldQuit())
			return "";
		if (c == "OptionNouvelleP" || c == "OptionEntrenement")
			return c;
		if (c == "OptionLoad" && loadMenu())
			return c; // the load has chosen the next scene
		if (c == "OptionQuitter") {
			if (runMenu("OptionQuitter") == "QuitterOK") {
				quitGame();
				return "";
			}
		} else if (c == "OptionCredits") {
			credits();
		} else if (c == "OptionReglage") {
			settings();
		} else if (c == "OptionGalerie") {
			gallery();
			if (!_gallery3D.empty())
				return "Gallery3D";
		} else if (!c.empty() && c != "escape" && c != "enter" && c != "key") {
			warning("Menu command %s is not implemented", c.c_str());
		}
	}
}

void X3DEngine::afterOptionMenu(const Common::String &command) {
	if (command == "OptionNouvelleP") {
		// A new game starts at the scene named in App.bin #GAME# (E-0037)
		Common::String scene = "U01.X3D";
		if (Common::SeekableReadStream *game = openBinChunk("App.bin", "#GAME#")) {
			scene = game->readString(0, 30);
			delete game;
		}
		// A new game drops a held item and empties the bar (E-0212)
		_practice = false;
		if (_interaction)
			_interaction->holdItem("");
		_inventory->clear();
		gotoScene(scene);
	} else if (command == "Gallery3D") {
		gotoScene(Gallery3D::sceneFor(_gallery3D));
	} else if (command == "OptionEntrenement") {
		// Practice keeps the bar (E-0212); a held item is stored by the scene switch
		_practice = true;
		gotoScene("U00.X3D");
	}
}

} // End of namespace X3D
