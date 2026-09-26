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
#include "common/endian.h"
#include "common/events.h"
#include "common/system.h"

#include "engines/util.h"

#include "graphics/pixelformat.h"

#include "peintre/bfg.h"
#include "peintre/obj3d.h"
#include "peintre/peintre.h"

namespace Peintre {

PeintreEngine::PeintreEngine(OSystem *syst, const ADGameDescription *gameDesc)
	: Engine(syst), _gameDescription(gameDesc) {
	// The detector's directory globs (Data, Scenes_3D) are already in SearchMan.
}

PeintreEngine::~PeintreEngine() {
}

Common::Error PeintreEngine::run() {
	// The original draws on a 640x480 16-bit surface (spec boot.md).
	const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
	initGraphics(640, 480, &format);

	if (ConfMan.getBool("dev_load_all"))
		loadAllScenes();

	while (!shouldQuit()) {
		Common::Event event;
		while (_eventMan->pollEvent(event)) {
		}
		_system->updateScreen();
		_system->delayMillis(10);
	}
	return Common::kNoError;
}

void PeintreEngine::loadAllScenes() {
	// Dev check: every object of every BFG loads (the RE validators' counts, obj3d.py).
	static const char *const kBundles[] = {
		"auberge", "cafe", "chambreb", "chambrev", "champ", "eglise", "hopiext", "hopiint",
		"jardin", "maisonet", "maisonj", "mangeurs", "musee", "pont", "terrasse"
	};
	uint nodes = 0, polys = 0, textures = 0, anims = 0, boxes = 0, failures = 0;
	for (const char *name : kBundles) {
		Bfg bfg;
		if (!bfg.open(Common::Path(Common::String::format("Scenes_3D/%s.BFG", name)))) {
			warning("dev_load_all: cannot open %s.BFG", name);
			failures++;
			continue;
		}
		for (uint i = 0; i < bfg.entryCount(); i++) {
			Common::Array<byte> data;
			if (!bfg.readEntry(bfg.entryName(i), data) || data.size() < kObjectHeaderSize) {
				failures++;
				continue;
			}
			bool ok = true;
			switch (READ_LE_UINT32(data.data() + 8)) {
			case kObjScene: {
				Scene3D s;
				ok = s.load(data);
				nodes += s.nodes.size();
				for (const Node &n : s.nodes)
					for (const FaceGroup &g : n.faceGroups)
						polys += g.polys.size();
				break;
			}
			case kObjTexture: {
				Texture3D *t = new Texture3D();
				ok = t->load(data);
				delete t;
				textures++;
				break;
			}
			case kObjAnim: {
				Anim3D a;
				ok = a.load(data);
				anims++;
				break;
			}
			case kObjBoxes: {
				Boxes3D b;
				ok = b.load(data);
				boxes++;
				break;
			}
			default:
				ok = false;
			}
			if (!ok) {
				warning("dev_load_all: %s:%s failed", name, bfg.entryName(i).c_str());
				failures++;
			}
		}
	}
	debug("dev_load_all: %u nodes, %u polys, %u textures, %u animations, %u box sets, %u failures",
		  nodes, polys, textures, anims, boxes, failures);
}

} // End of namespace Peintre
