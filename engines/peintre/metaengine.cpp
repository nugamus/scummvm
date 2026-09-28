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
#include "common/savefile.h"
#include "common/translation.h"
#include "common/system.h"

#include "engines/advancedDetector.h"
#include "engines/dialogs.h"

#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"
#include "backends/keymapper/standard-actions.h"

#include "gui/ThemeEval.h"
#include "gui/widget.h"

#include "peintre/detection.h"
#include "peintre/peintre.h"

static const ADExtraGuiOptionsMap optionsList[] = {
	{
		GAMEOPTION_HIGH_FPS,
		{
			_s("High frame rate"),
			_s("Draw the 3D scenes at the display's rate, moving smoothly between the game's steps (the original draws 15 frames per second)"),
			"high_fps",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_HIGH_RES,
		{
			_s("High resolution"),
			_s("Draw the 3D at the window's resolution with OpenGL (the original draws 640x480, scaled up to the window)"),
			"high_res",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_WIDESCREEN,
		{
			_s("Widescreen"),
			_s("Show more of the 3D scenes to the sides on a wide display (with high resolution)"),
			"widescreen",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_FILTER_TEXTURES,
		{
			_s("Improved texture filtering"),
			_s("Smooth and sharpen near, distant and slanted textures (mipmaps and anisotropic filtering; with high resolution)"),
			"filter_textures",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_MODERN_CONTROLS,
		{
			_s("Modern controls"),
			_s("Look around with the mouse, click at the centre of the screen, WASD to walk and strafe, right click for the inventory"),
			"modern_controls",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_INVERT_Y,
		{
			_s("Invert mouse look"),
			_s("With modern controls, moving the mouse up looks down"),
			"invert_y",
			false,
			0,
			0
		}
	},
	AD_EXTRA_GUI_OPTIONS_TERMINATOR
};

// Options with a range of values, as sliders under the checkboxes.
static const struct {
	const char *guioFlag, *configOption, *label, *tooltip, *unit;
	int defaultValue, minValue, maxValue;
} sliderList[] = {
	{ GAMEOPTION_MOUSE_SENSITIVITY, "mouse_sensitivity", _s("Mouse look speed:"), _s("How fast the mouse turns the view with modern controls"), "%", 100, 25, 300 },
	{ GAMEOPTION_TURN_SPEED, "turn_speed", _s("Turn speed:"), _s("How fast the arrow keys turn the view and look up or down (100% is the original)"), "%", 100, 50, 200 },
	{ GAMEOPTION_FOV, "fov", _s("Field of view:"), _s("Horizontal field of view of the 3D scenes, in degrees (67 is the original)"), "", 67, 60, 100 },
};

class PeintreOptionsWidget : public GUI::ExtraGuiOptionsWidget {
public:
	PeintreOptionsWidget(GuiObject *boss, const Common::String &name, const Common::String &domain, const ExtraGuiOptions &options) :
		ExtraGuiOptionsWidget(boss, name, domain, options), _checkboxes(options.size()) {
		const Common::String guiOptions = ConfMan.get("guioptions", domain);
		for (uint i = 0; i < ARRAYSIZE(sliderList); i++) {
			_sliders[i] = nullptr;
			_values[i] = nullptr;
			if (!checkGameGUIOption(sliderList[i].guioFlag, guiOptions))
				continue;
			const Common::String id = _dialogLayout + "." + sliderList[i].configOption;
			new GUI::StaticTextWidget(widgetsBoss(), id + "_desc", _(sliderList[i].label), _(sliderList[i].tooltip));
			_sliders[i] = new GUI::SliderWidget(widgetsBoss(), id, _(sliderList[i].tooltip), kSliderCmd + i);
			_sliders[i]->setMinValue(sliderList[i].minValue);
			_sliders[i]->setMaxValue(sliderList[i].maxValue);
			_values[i] = new GUI::StaticTextWidget(widgetsBoss(), id + "_value", Common::U32String());
		}
	}

	void load() override {
		ExtraGuiOptionsWidget::load();
		for (uint i = 0; i < ARRAYSIZE(sliderList); i++) {
			if (!_sliders[i])
				continue;
			const int v = ConfMan.hasKey(sliderList[i].configOption, _domain) ? ConfMan.getInt(sliderList[i].configOption, _domain) : sliderList[i].defaultValue;
			_sliders[i]->setValue(CLIP(v, sliderList[i].minValue, sliderList[i].maxValue));
			showValue(i);
		}
	}

	bool save() override {
		ExtraGuiOptionsWidget::save();
		for (uint i = 0; i < ARRAYSIZE(sliderList); i++)
			if (_sliders[i])
				ConfMan.setInt(sliderList[i].configOption, _sliders[i]->getValue(), _domain);
		return true;
	}

	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override {
		if (cmd >= kSliderCmd && cmd < kSliderCmd + ARRAYSIZE(sliderList)) {
			showValue(cmd - kSliderCmd);
			return;
		}
		ExtraGuiOptionsWidget::handleCommand(sender, cmd, data);
	}

protected:
	// The base class's checkboxes, then a label, slider and value per row.
	void defineLayout(GUI::ThemeEval &layouts, const Common::String &layoutName, const Common::String &overlayedLayout) const override {
		layouts.addDialog(layoutName, overlayedLayout);
		layouts.addLayout(GUI::ThemeLayout::kLayoutVertical).addPadding(0, 0, 0, 0);
		for (uint i = 0; i < _checkboxes; i++)
			layouts.addWidget(Common::String::format("customOption%dCheckbox", i + 1), "Checkbox");
		for (uint i = 0; i < ARRAYSIZE(sliderList); i++) {
			if (!_sliders[i])
				continue;
			const Common::String id = sliderList[i].configOption;
			layouts.addLayout(GUI::ThemeLayout::kLayoutHorizontal).addPadding(0, 0, 0, 0);
			layouts.addWidget(id + "_desc", "OptionsLabel");
			layouts.addWidget(id, "Slider");
			layouts.addWidget(id + "_value", "ShortOptionsLabel").closeLayout();
		}
		layouts.closeLayout().closeDialog();
	}

private:
	enum { kSliderCmd = 0x5054534C }; // PTSL

	void showValue(uint i) {
		_values[i]->setLabel(Common::String::format("%d%s", _sliders[i]->getValue(), sliderList[i].unit));
		_values[i]->markAsDirty();
	}

	uint _checkboxes;
	GUI::SliderWidget *_sliders[ARRAYSIZE(sliderList)];
	GUI::StaticTextWidget *_values[ARRAYSIZE(sliderList)];
};

class PeintreMetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
	const char *getName() const override {
		return "peintre";
	}

	Common::Error createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const override {
		*engine = new Peintre::PeintreEngine(syst, desc);
		return Common::kNoError;
	}

	bool hasFeature(MetaEngineFeature f) const override {
		return f == kSupportsListSaves || f == kSupportsDeleteSave || f == kSupportsLoadingDuringStartup;
	}

	const ADExtraGuiOptionsMap *getAdvancedExtraGuiOptions() const override {
		return optionsList;
	}

	GUI::OptionsContainerWidget *buildEngineOptionsWidget(GUI::GuiObject *boss, const Common::String &name, const Common::String &target) const override {
		return new PeintreOptionsWidget(boss, name, target, getExtraGuiOptions(target));
	}

	int getMaximumSaveSlot() const override {
		return 434; // player 4, slot 34 (save.md)
	}

	/**
	 * The original's game saves `<target>.game<pp><ss>` (save.md): slot pp * 100 + ss,
	 * named after the player and the object that made the save.
	 */
	SaveStateList listSaves(const char *target) const override {
		Common::SaveFileManager *sfm = g_system->getSavefileManager();
		// Player names from `<target>.users` (u32 count, 40-byte records, name first).
		Common::String names[5];
		Common::ScopedPtr<Common::InSaveFile> users(sfm->openForLoading(Common::String(target) + ".users"));
		if (users) {
			const uint32 count = users->readUint32LE();
			for (uint32 i = 0; i < count && i < 5; i++) {
				char name[33];
				users->read(name, 32);
				name[32] = 0;
				names[i] = name;
				users->skip(8);
			}
		}
		SaveStateList list;
		const Common::StringArray files = sfm->listSavefiles(Common::String(target) + ".game????");
		for (const Common::String &f : files) {
			const Common::String digits = f.substr(f.size() - 4);
			const int player = atoi(digits.substr(0, 2).c_str());
			const int slot = atoi(digits.substr(2).c_str());
			if (slot < 1 || player > 4) // slot 0 (zone 0) is never offered (save.md)
				continue;
			const Common::String who = names[player].empty() ? Common::String::format("Player %d", player + 1) : names[player];
			list.push_back(SaveStateDescriptor(this, player * 100 + slot, Common::String::format("%s - object %d", who.c_str(), slot)));
		}
		Common::sort(list.begin(), list.end(), SaveStateDescriptorSlotComparator());
		return list;
	}

	/**
	 * The original's keys (movement.md, ui.md) as remappable actions, and the same on modern
	 * keys (the modern_controls option; the engine enables one of the two). Each action
	 * carries the key it stands for, and the engine treats it as that key (pollEvents).
	 */
	Common::KeymapArray initKeymaps(const char *target) const override {
		using namespace Common;
		Keymap *keymap = new Keymap(Keymap::kKeymapTypeGame, "peintre", _("Game keymappings"));
		Keymap *modern = new Keymap(Keymap::kKeymapTypeGame, "peintre-modern", _("Modern keymappings"));
		static const struct {
			const char *id, *label, *key, *modernKey, *joy;
			int code;
		} actions[] = {
			{ "FORWARD", _s("Walk forward"), "UP", "w", "JOY_UP", KEYCODE_UP },
			{ "BACKWARD", _s("Walk backward"), "DOWN", "s", "JOY_DOWN", KEYCODE_DOWN },
			{ "LEFT", _s("Turn left"), "LEFT", "LEFT", "JOY_LEFT", KEYCODE_LEFT },
			{ "RIGHT", _s("Turn right"), "RIGHT", "RIGHT", "JOY_RIGHT", KEYCODE_RIGHT },
			{ "STRAFELEFT", _s("Strafe left"), nullptr, "a", nullptr, Peintre::kKeyStrafeLeft },
			{ "STRAFERIGHT", _s("Strafe right"), nullptr, "d", nullptr, Peintre::kKeyStrafeRight },
			{ "LOOKUP", _s("Look up"), "PAGEUP", "PAGEUP", "JOY_LEFT_SHOULDER", KEYCODE_PAGEUP },
			{ "LOOKDOWN", _s("Look down"), "PAGEDOWN", "PAGEDOWN", "JOY_RIGHT_SHOULDER", KEYCODE_PAGEDOWN },
			{ "INVENTORY", _s("Inventory"), "SPACE", "MOUSE_RIGHT", "JOY_X", KEYCODE_SPACE },
			{ "RETURN", _s("Back to the museum"), "BACKSPACE", "BACKSPACE", "JOY_B", KEYCODE_BACKSPACE },
			{ "MENU", _s("Options"), "ESCAPE", "ESCAPE", "JOY_START", KEYCODE_ESCAPE },
		};
		KeymapArray keymaps;
		keymaps.push_back(keymap);
		keymaps.push_back(modern);
		for (Keymap *k : keymaps) {
			for (const auto &a : actions) {
				const char *key = k == modern ? a.modernKey : a.key;
				if (!key)
					continue;
				Action *act = new Action(a.id, _(a.label));
				act->setCustomEngineActionEvent(a.code);
				act->addDefaultInputMapping(key);
				if (a.joy)
					act->addDefaultInputMapping(a.joy);
				if (k == modern && a.code == KEYCODE_SPACE)
					act->addDefaultInputMapping("SPACE"); // Space opens the inventory too
				k->addAction(act);
			}
			Action *click = new Action(kStandardActionLeftClick, _("Click"));
			click->setLeftClickEvent();
			click->addDefaultInputMapping("MOUSE_LEFT");
			click->addDefaultInputMapping("JOY_A");
			k->addAction(click);
			Action *hotspots = new Action(kStandardActionToggleHotspots, _("Show hotspots"));
			hotspots->setCustomEngineActionEvent(Peintre::kKeyHotspots);
			hotspots->addDefaultInputMapping("h");
			k->addAction(hotspots);
		}
		return keymaps;
	}

	bool removeSaveState(const char *target, int slot) const override {
		return g_system->getSavefileManager()->removeSavefile(
			Common::String::format("%s.game%02d%02d", target, slot / 100, slot % 100));
	}
};

#if PLUGIN_ENABLED_DYNAMIC(PEINTRE)
REGISTER_PLUGIN_DYNAMIC(PEINTRE, PLUGIN_TYPE_ENGINE, PeintreMetaEngine);
#else
REGISTER_PLUGIN_STATIC(PEINTRE, PLUGIN_TYPE_ENGINE, PeintreMetaEngine);
#endif
