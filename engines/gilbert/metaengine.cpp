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

#include "common/formats/ini-file.h"
#include "common/savefile.h"
#include "common/system.h"

#include "engines/advancedDetector.h"

#include "gilbert/detection.h"
#include "gilbert/gilbert.h"

class GilbertMetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
	const char *getName() const override {
		return "gilbert";
	}

	Common::Error createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const override {
		*engine = new Gilbert::GilbertEngine(syst, desc);
		return Common::kNoError;
	}

	bool hasFeature(MetaEngineFeature f) const override {
		return f == kSupportsListSaves || f == kSupportsLoadingDuringStartup || f == kSupportsDeleteSave;
	}

	int getMaximumSaveSlot() const override {
		return 50;
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
			list.push_back(SaveStateDescriptor(this, n, Common::U32String(name, Common::kWindows1252)));
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
		const Common::String section = Common::String::format("SLOT%d", slot);
		Common::String file;
		if (ini.getKey("file", section, file) && !file.empty())
			sfm->removeSavefile(Common::String(target) + "." + file);
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
