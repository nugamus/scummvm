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

#include "common/translation.h"

#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"
#include "backends/keymapper/standard-actions.h"

#include "graphics/surface.h"

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
	AD_EXTRA_GUI_OPTIONS_TERMINATOR
};

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
	// The original's keys (movement.md, ui.md) as remappable actions
	using namespace Common;
	Keymap *keymap = new Keymap(Keymap::kKeymapTypeGame, "x3d-default", _("Default keymappings"));
	static const struct {
		const char *id, *label, *key, *joy;
		X3D::Action action;
	} actions[] = {
		{ "FORWARD", _s("Walk forward"), "UP", "JOY_UP", X3D::kActionForward },
		{ "BACKWARD", _s("Walk backward"), "DOWN", "JOY_DOWN", X3D::kActionBackward },
		{ "LEFT", _s("Turn left"), "LEFT", "JOY_LEFT", X3D::kActionTurnLeft },
		{ "RIGHT", _s("Turn right"), "RIGHT", "JOY_RIGHT", X3D::kActionTurnRight },
		{ "LOOKUP", _s("Look up"), "PAGEUP", "JOY_LEFT_SHOULDER", X3D::kActionLookUp },
		{ "LOOKDOWN", _s("Look down"), "PAGEDOWN", "JOY_RIGHT_SHOULDER", X3D::kActionLookDown },
		{ "RUN", _s("Run"), "LCTRL", "JOY_B", X3D::kActionRun },
		{ "JUMP", _s("Jump"), "LSHIFT", "JOY_Y", X3D::kActionJump },
		{ "INVENTORY", _s("Inventory"), "SPACE", "JOY_X", X3D::kActionInventory },
		{ "MENU", _s("Menu"), "ESCAPE", "JOY_START", X3D::kActionMenu },
		{ "SKIP", _s("Skip"), "RETURN", "JOY_BACK", X3D::kActionSkip },
	};
	for (const auto &a : actions) {
		Action *act = new Action(a.id, _(a.label));
		act->setCustomEngineActionEvent(a.action);
		act->addDefaultInputMapping(a.key);
		act->addDefaultInputMapping(a.joy);
		keymap->addAction(act);
	}
	Action *click = new Action(kStandardActionLeftClick, _("Click"));
	click->setLeftClickEvent();
	click->addDefaultInputMapping("MOUSE_LEFT");
	click->addDefaultInputMapping("JOY_A");
	keymap->addAction(click);
	return Keymap::arrayOf(keymap);
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
