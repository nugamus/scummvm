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

#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"
#include "common/translation.h"
#include "engines/advancedDetector.h"

#include "grumpa/detection.h"
#include "grumpa/grumpa.h"

class GrumpaMetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
	const char *getName() const override {
		return "grumpa";
	}

	Common::Error createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const override {
		*engine = new Grumpa::GrumpaEngine(syst, desc);
		return Common::kNoError;
	}

	Common::KeymapArray initKeymaps(const char *target) const override {
		using namespace Common;
		Keymap *keymap = new Keymap(Keymap::kKeymapTypeGame, "grumpa", _("Game keymappings"));
		Action *act = new Action("DISMOUNT", _("Leave the mount / let the companion go"));
		act->setCustomEngineActionEvent(Grumpa::kActionDismount);
		act->addDefaultInputMapping("BACKSPACE");
		keymap->addAction(act);
		act = new Action("STANCE", _("Combat stance"));
		act->setCustomEngineActionEvent(Grumpa::kActionStance);
		act->addDefaultInputMapping("LCTRL");
		act->addDefaultInputMapping("RCTRL");
		keymap->addAction(act);
		act = new Action("JUMP", _("Jump"));
		act->setCustomEngineActionEvent(Grumpa::kActionJump);
		act->addDefaultInputMapping("SPACE");
		keymap->addAction(act);
		return Keymap::arrayOf(keymap);
	}
};

#if PLUGIN_ENABLED_DYNAMIC(GRUMPA)
REGISTER_PLUGIN_DYNAMIC(GRUMPA, PLUGIN_TYPE_ENGINE, GrumpaMetaEngine);
#else
REGISTER_PLUGIN_STATIC(GRUMPA, PLUGIN_TYPE_ENGINE, GrumpaMetaEngine);
#endif
