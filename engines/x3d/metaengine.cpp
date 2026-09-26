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
#include "common/translation.h"

#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"
#include "backends/keymapper/standard-actions.h"

#include "engines/dialogs.h"

#include "graphics/surface.h"

#include "gui/ThemeEval.h"
#include "gui/widget.h"

#include "x3d/metaengine.h"
#include "x3d/detection.h"
#include "x3d/x3d.h"

static const ADExtraGuiOptionsMap optionsList[] = {
	{
		GAMEOPTION_WIDESCREEN,
		{
			_s("Widescreen"),
			_s("Show more of the scene to the sides on a wide display"),
			"widescreen",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_MAX_DETAIL,
		{
			_s("Full detail at any distance"),
			_s("Always draw the most detailed version of each object, never the simpler distant ones"),
			"max_detail",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_FILTER_TEXTURES,
		{
			_s("Improved texture filtering"),
			_s("Smooth and sharpen distant and slanted textures (mipmaps and anisotropic filtering; OpenGL only)"),
			"filter_textures",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_RUN_TOGGLE,
		{
			_s("Run toggle"),
			_s("The run key switches running on and off instead of running while held"),
			"run_toggle",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_CROUCH_TOGGLE,
		{
			_s("Crouch toggle"),
			_s("The crouch key switches crouching on and off instead of crouching while held"),
			"crouch_toggle",
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

// Options with a range of values, as sliders under the checkboxes
static const struct {
	const char *guioFlag, *configOption, *label, *tooltip, *unit;
	int defaultValue, minValue, maxValue;
} sliderList[] = {
	{ GAMEOPTION_MOUSE_SENSITIVITY, "mouse_sensitivity", _s("Mouse look speed:"), _s("How fast the mouse turns the view with modern controls"), "%", 100, 50, 200 },
	{ GAMEOPTION_TURN_SPEED, "turn_speed", _s("Turn speed:"), _s("How fast the arrow keys turn the view and look up or down (100% is the original)"), "%", 100, 50, 150 },
	{ GAMEOPTION_FOV, "fov", _s("Field of view:"), _s("Horizontal field of view while walking around, in degrees (90 is the original; the scripted views keep theirs)"), "", 90, 90, 110 },
};

class X3DOptionsWidget : public GUI::ExtraGuiOptionsWidget {
public:
	X3DOptionsWidget(GuiObject *boss, const Common::String &name, const Common::String &domain, const ExtraGuiOptions &options) :
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
	// The base class's checkboxes, then a label, slider and value per row
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
	enum { kSliderCmd = 'X3SL' };

	void showValue(uint i) {
		_values[i]->setLabel(Common::String::format("%d%s", _sliders[i]->getValue(), sliderList[i].unit));
		_values[i]->markAsDirty();
	}

	uint _checkboxes;
	GUI::SliderWidget *_sliders[ARRAYSIZE(sliderList)];
	GUI::StaticTextWidget *_values[ARRAYSIZE(sliderList)];
};

GUI::OptionsContainerWidget *X3DMetaEngine::buildEngineOptionsWidget(GUI::GuiObject *boss, const Common::String &name, const Common::String &target) const {
	return new X3DOptionsWidget(boss, name, target, getExtraGuiOptions(target));
}

const ADExtraGuiOptionsMap *X3DMetaEngine::getAdvancedExtraGuiOptions() const {
	return optionsList;
}

const char *X3DMetaEngine::getName() const {
	return "x3d";
}

void X3DMetaEngine::getSavegameThumbnail(Graphics::Surface &thumb) {
	// The 3D screen cannot be read back by ScummVM: the engine redraws its last view
	if (X3D::X3DEngine *engine = static_cast<X3D::X3DEngine *>(g_engine)) {
		if (Graphics::Surface *small = engine->thumbnail(160, 120)) {
			thumb.copyFrom(*small);
			small->free();
			delete small;
		}
	}
}

Common::KeymapArray X3DMetaEngine::initKeymaps(const char *target) const {
	// The original's keys (movement.md, ui.md) as remappable actions, and the same actions
	// on modern keys (the modern_controls option; the engine enables one of the two)
	using namespace Common;
	Keymap *keymap = new Keymap(Keymap::kKeymapTypeGame, "x3d-default", _("Default keymappings"));
	Keymap *modern = new Keymap(Keymap::kKeymapTypeGame, "x3d-modern", _("Modern keymappings"));
	static const struct {
		const char *id, *label, *key, *modernKey, *joy;
		X3D::Action action;
	} actions[] = {
		{ "FORWARD", _s("Walk forward"), "UP", "w", "JOY_UP", X3D::kActionForward },
		{ "BACKWARD", _s("Walk backward"), "DOWN", "s", "JOY_DOWN", X3D::kActionBackward },
		{ "LEFT", _s("Turn left"), "LEFT", "LEFT", "JOY_LEFT", X3D::kActionTurnLeft },
		{ "RIGHT", _s("Turn right"), "RIGHT", "RIGHT", "JOY_RIGHT", X3D::kActionTurnRight },
		{ "STRAFELEFT", _s("Strafe left"), nullptr, "a", nullptr, X3D::kActionStrafeLeft },
		{ "STRAFERIGHT", _s("Strafe right"), nullptr, "d", nullptr, X3D::kActionStrafeRight },
		{ "LOOKUP", _s("Look up"), "PAGEUP", "PAGEUP", "JOY_LEFT_SHOULDER", X3D::kActionLookUp },
		{ "LOOKDOWN", _s("Look down"), "PAGEDOWN", "PAGEDOWN", "JOY_RIGHT_SHOULDER", X3D::kActionLookDown },
		{ "RUN", _s("Run"), "LCTRL", "LSHIFT", "JOY_B", X3D::kActionRun },
		{ "JUMP", _s("Jump"), "LSHIFT", "SPACE", "JOY_Y", X3D::kActionJump },
		{ "INVENTORY", _s("Inventory"), "SPACE", "MOUSE_RIGHT", "JOY_X", X3D::kActionInventory },
		{ "MENU", _s("Menu"), "ESCAPE", "ESCAPE", "JOY_START", X3D::kActionMenu },
		{ "SKIP", _s("Skip"), "RETURN", "RETURN", "JOY_BACK", X3D::kActionSkip },
		{ "CROUCH", _s("Crouch"), "KP0", "LCTRL", "JOY_LEFT_STICK", X3D::kActionCrouch },
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
			act->setCustomEngineActionEvent(a.action);
			act->addDefaultInputMapping(key);
			if (a.joy)
				act->addDefaultInputMapping(a.joy);
			k->addAction(act);
		}
		Action *click = new Action(kStandardActionLeftClick, _("Click"));
		click->setLeftClickEvent();
		click->addDefaultInputMapping("MOUSE_LEFT");
		click->addDefaultInputMapping("JOY_A");
		k->addAction(click);
		Action *hotspots = new Action(kStandardActionToggleHotspots, _("Show hotspots"));
		hotspots->setCustomEngineActionEvent(X3D::kActionToggleHotspots);
		hotspots->addDefaultInputMapping("h");
		k->addAction(hotspots);
	}
	return keymaps;
}

Common::Error X3DMetaEngine::createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const {
	*engine = new X3D::X3DEngine(syst, desc);
	return Common::kNoError;
}

#if PLUGIN_ENABLED_DYNAMIC(X3D)
REGISTER_PLUGIN_DYNAMIC(X3D, PLUGIN_TYPE_ENGINE, X3DMetaEngine);
#else
REGISTER_PLUGIN_STATIC(X3D, PLUGIN_TYPE_ENGINE, X3DMetaEngine);
#endif
