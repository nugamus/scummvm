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
#include "common/formats/ini-file.h"
#include "common/savefile.h"
#include "common/system.h"
#include "common/translation.h"

#include "engines/advancedDetector.h"

#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"
#include "backends/keymapper/standard-actions.h"

#include "graphics/hotspot_renderer.h"

#include "gilbert/detection.h"
#include "gilbert/gilbert.h"

static const ADExtraGuiOptionsMap optionsList[] = {
	{
		GAMEOPTION_ALWAYS_RUN,
		{
			_s("Always run"),
			_s("Gilbert runs without Ctrl held; holding Ctrl makes him walk"),
			"always_run",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_SHORTCUTS,
		{
			_s("Keyboard shortcuts"),
			_s("M opens the map, B the book, Escape goes back, Page Up and Page Down scroll the inventory, 1 to 9 pick a dialogue choice"),
			"keyboard_shortcuts",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_WHEEL,
		{
			_s("Scroll the inventory with the mouse wheel"),
			_s("In the close-ups the mouse wheel scrolls the inventory"),
			"wheel_inventory",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_MARK_CHOICES,
		{
			_s("Mark dialogue choices already picked"),
			_s("Choices you have picked before are drawn darker"),
			"mark_choices",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_NEW_TOPICS,
		{
			_s("Highlight new book topics"),
			_s("Topics you have not opened yet are drawn in blue in the book's lists"),
			"mark_new_topics",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_AUTOSAVE,
		{
			_s("Autosave on every room change"),
			_s("Saves the game in slot 50, named Autosave, whenever Gilbert enters another room"),
			"autosave_rooms",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_FULLSCREEN_FILMS,
		{
			_s("Full-screen films"),
			_s("Play the films filling the screen, smoothly scaled"),
			"fullscreen_films",
			false,
			0,
			0
		}
	},
	AD_EXTRA_GUI_OPTIONS_TERMINATOR
};

class GilbertMetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
	const char *getName() const override {
		return "gilbert";
	}

	const ADExtraGuiOptionsMap *getAdvancedExtraGuiOptions() const override {
		return optionsList;
	}

	void registerDefaultSettings(const Common::String &target) const override {
		AdvancedMetaEngine<ADGameDescription>::registerDefaultSettings(target);
		// ScummVM's hotspot overlay marks objects with squares in this game unless the player
		// chose another marker (the game's options, or the global ones).
		if (!target.empty() && ConfMan.hasGameDomain(target) && !ConfMan.hasKey("hotspot_marker", target) &&
		    !ConfMan.hasKey("hotspot_marker", Common::ConfigManager::kApplicationDomain))
			ConfMan.setInt("hotspot_marker", Graphics::kMarkerSquare, target);
	}

	Common::Error createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const override {
		*engine = new Gilbert::GilbertEngine(syst, desc);
		return Common::kNoError;
	}

	bool hasFeature(MetaEngineFeature f) const override {
		return f == kSupportsListSaves || f == kSupportsLoadingDuringStartup || f == kSupportsDeleteSave;
	}

	Common::KeymapArray initKeymaps(const char *target) const override {
		using namespace Common;
		Keymap *keymap = new Keymap(Keymap::kKeymapTypeGame, "gilbert", _("Game keymappings"));
		Action *act = new Action(kStandardActionLeftClick, _("Click"));
		act->setLeftClickEvent();
		act->addDefaultInputMapping("MOUSE_LEFT");
		act->addDefaultInputMapping("JOY_A");
		keymap->addAction(act);
		act = new Action("SKIP", _("Skip film"));
		act->setCustomEngineActionEvent(Gilbert::kActionSkip);
		act->addDefaultInputMapping("ESCAPE");
		act->addDefaultInputMapping("JOY_B");
		keymap->addAction(act);
		act = new Action("RUN", _("Run (hold)"));
		act->setCustomEngineActionEvent(Gilbert::kActionRun);
		act->addDefaultInputMapping("LCTRL");
		act->addDefaultInputMapping("RCTRL");
		act->addDefaultInputMapping("JOY_X");
		keymap->addAction(act);

		// Keys of the game screens, off in the main menu (where the keys type save names).
		Keymap *play = new Keymap(Keymap::kKeymapTypeGame, "gilbert-play", _("Game screen keymappings"));
		act = new Action(kStandardActionToggleHotspots, _("Show hotspots"));
		act->setCustomEngineActionEvent(Gilbert::kActionHotspots);
		act->addDefaultInputMapping("h");
		play->addAction(act);
		act = new Action("MAP", _("Map (keyboard shortcuts)"));
		act->setCustomEngineActionEvent(Gilbert::kActionMap);
		act->addDefaultInputMapping("m");
		play->addAction(act);
		act = new Action("BOOK", _("Book (keyboard shortcuts)"));
		act->setCustomEngineActionEvent(Gilbert::kActionBook);
		act->addDefaultInputMapping("b");
		play->addAction(act);
		act = new Action("INVUP", _("Scroll the inventory up (keyboard shortcuts)"));
		act->setCustomEngineActionEvent(Gilbert::kActionInventoryUp);
		act->addDefaultInputMapping("PAGEUP");
		play->addAction(act);
		act = new Action("INVDOWN", _("Scroll the inventory down (keyboard shortcuts)"));
		act->setCustomEngineActionEvent(Gilbert::kActionInventoryDown);
		act->addDefaultInputMapping("PAGEDOWN");
		play->addAction(act);
		// Action keeps the ID's pointer.
		static const char *const choiceIds[9] = { "CHOICE1", "CHOICE2", "CHOICE3", "CHOICE4", "CHOICE5", "CHOICE6", "CHOICE7", "CHOICE8", "CHOICE9" };
		for (int k = 0; k < 9; k++) {
			act = new Action(choiceIds[k],
			                 Common::U32String::format(_("Dialogue choice %d (keyboard shortcuts)"), k + 1));
			act->setCustomEngineActionEvent(Gilbert::kActionChoice1 + k);
			act->addDefaultInputMapping(Common::String::format("%d", k + 1));
			play->addAction(act);
		}
		KeymapArray keymaps;
		keymaps.push_back(keymap);
		keymaps.push_back(play);
		return keymaps;
	}

	// ScummVM's slots 0..49 are the game's slots 1..50.
	int getMaximumSaveSlot() const override {
		return 49;
	}

	/**
	 * The original's saves: `<target>.game<n>.dat`, listed in `<target>.ini` (its
	 * gilbert.ini) with their names (boot.md "Save slots").
	 */
	SaveStateList listSaves(const char *target) const override {
		SaveStateList list;
		Common::ScopedPtr<Common::InSaveFile> in(g_system->getSavefileManager()->openForLoading(Common::String(target) + ".ini"));
		if (!in)
			return list;
		Common::INIFile ini;
		ini.allowNonEnglishCharacters();
		if (!ini.loadFromStream(*in))
			return list;
		for (int n = 1; n <= 50; n++) {
			Common::String file, name;
			const Common::String section = Common::String::format("SLOT%d", n);
			if (!ini.getKey("file", section, file) || file.empty())
				continue;
			ini.getKey("name", section, name);
			list.push_back(SaveStateDescriptor(this, n - 1, Common::U32String(name, Common::kWindows1252)));
		}
		return list;
	}

	bool removeSaveState(const char *target, int slot) const override {
		Common::SaveFileManager *sfm = g_system->getSavefileManager();
		Common::INIFile ini;
		ini.allowNonEnglishCharacters();
		{
			Common::ScopedPtr<Common::InSaveFile> in(sfm->openForLoading(Common::String(target) + ".ini"));
			if (!in || !ini.loadFromStream(*in))
				return false;
		}
		const Common::String section = Common::String::format("SLOT%d", slot + 1);
		Common::String file;
		if (ini.getKey("file", section, file) && !file.empty()) {
			sfm->removeSavefile(Common::String(target) + "." + file);
			sfm->removeSavefile(Common::String(target) + "." + file + ".seen");
		}
		ini.setKey("file", section, "");
		ini.setKey("name", section, "");
		Common::ScopedPtr<Common::OutSaveFile> out(sfm->openForSaving(Common::String(target) + ".ini", false));
		if (!out || !ini.saveToStream(*out))
			return false;
		out->finalize();
		return true;
	}
};

#if PLUGIN_ENABLED_DYNAMIC(GILBERT)
REGISTER_PLUGIN_DYNAMIC(GILBERT, PLUGIN_TYPE_ENGINE, GilbertMetaEngine);
#else
REGISTER_PLUGIN_STATIC(GILBERT, PLUGIN_TYPE_ENGINE, GilbertMetaEngine);
#endif
