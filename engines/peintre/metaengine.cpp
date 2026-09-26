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

#include "common/savefile.h"
#include "common/system.h"

#include "engines/advancedDetector.h"

#include "peintre/detection.h"
#include "peintre/peintre.h"

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
		return f == kSupportsListSaves || f == kSupportsDeleteSave;
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
