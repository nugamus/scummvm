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
#include "backends/keymapper/standard-actions.h"

#include "common/translation.h"

#include "engines/advancedDetector.h"

#include "graphics/scaler.h"
#include "graphics/thumbnail.h"

#include "ring/detection.h"
#include "ring/ring.h"

class RingMetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
	const char *getName() const override {
		return "ring";
	}

	Common::Error createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const override {
		*engine = new Ring::RingEngine(syst, desc);
		return Common::kNoError;
	}

	Common::KeymapArray initKeymaps(const char *target) const override {
		using namespace Common;
		Keymap *keymap = new Keymap(Keymap::kKeymapTypeGame, "ring", _("Game keymappings"));
		Action *act = new Action(kStandardActionLeftClick, _("Click"));
		act->setLeftClickEvent();
		act->addDefaultInputMapping("MOUSE_LEFT");
		act->addDefaultInputMapping("JOY_A");
		keymap->addAction(act);
		act = new Action(kStandardActionRightClick, _("Inventory"));
		act->setRightClickEvent();
		act->addDefaultInputMapping("MOUSE_RIGHT");
		act->addDefaultInputMapping("JOY_Y");
		keymap->addAction(act);
		act = new Action("SKIP", _("Skip / cancel"));
		act->setCustomEngineActionEvent(Ring::kActionSkip);
		act->addDefaultInputMapping("ESCAPE");
		act->addDefaultInputMapping("JOY_B");
		keymap->addAction(act);
		act = new Action("MENU", _("Game menu"));
		act->setCustomEngineActionEvent(Ring::kActionMenu);
		act->addDefaultInputMapping("F12");
		act->addDefaultInputMapping("JOY_X");
		keymap->addAction(act);
		return Keymap::arrayOf(keymap);
	}

	void getSavegameThumbnail(Graphics::Surface &thumb) override {
		// The screen F12 left (the save screen covers the game).
		if (Ring::g_engine && !Ring::g_engine->snapScreen().empty()) {
			Graphics::Surface *small = Ring::g_engine->snapScreen().rawSurface().scale(kThumbnailWidth, kThumbnailHeight2, true);
			thumb.copyFrom(*small);
			small->free();
			delete small;
		} else {
			MetaEngine::getSavegameThumbnail(thumb);
		}
	}
};

#if PLUGIN_ENABLED_DYNAMIC(RING)
REGISTER_PLUGIN_DYNAMIC(RING, PLUGIN_TYPE_ENGINE, RingMetaEngine);
#else
REGISTER_PLUGIN_STATIC(RING, PLUGIN_TYPE_ENGINE, RingMetaEngine);
#endif
