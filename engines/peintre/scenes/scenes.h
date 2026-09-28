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

#ifndef PEINTRE_SCENES_SCENES_H
#define PEINTRE_SCENES_SCENES_H

#include "peintre/world.h"

namespace Peintre {

// Helpers shared by the scenes' code (games/mission-sunlight/docs/<scene>.md).

/** A row of a scene's object table ("Scene data"). */
struct ObjectDef {
	const char *name;
	byte cursorType;
	bool hidden;
};

/** A row of a scene's animation table. */
struct AnimDef {
	const char *anim;
	const char *node;
	bool playing;   ///< the playing word in the EXE's data
};

/** Fills and resolves the object table. */
void setObjects(World &w, const ObjectDef *defs, uint count);
/** Fills and loads the animation records; every frame starts at 1 (E-0318). */
void setAnims(World &w, const AnimDef *defs, uint count);

/**
 * interaction.md "Hover and click" steps 2 and 3 for the object index `idx` (-1 = none):
 * returns `idx` when it is clicked with the arrow, else applies its hover and returns -1.
 */
int clickedObject(World &w, int idx);

/**
 * Advances a playing record by `step` and poses it. Returns true on the frame it reaches
 * `end` (the frame is then held at `end`) without posing it: the scenes' step functions
 * apply their end rule and return, so the node keeps the previous frame (0x42b5fe, 0x41a2ab).
 */
bool stepAnim(World &w, uint rec, int32 step, int32 end);
inline bool stepAnim(World &w, uint rec, int32 step) {
	return stepAnim(w, rec, step, w.anims[rec].length);
}
/**
 * The step of a track the original advances by `elapsed / 2` per handled tick: one frame
 * per two ticks. The original's integer halving gives 0 (or its floor of 1) at its full
 * 15 frames per second, so these tracks froze or ran at double speed on a fast machine
 * and ran as designed only where frames took two ticks (a bug fix, E-0378).
 */
int32 halfStep(World &w, uint rec);
/** A looping record: past its end it restarts at frame 1, posed the same tick. */
bool loopAnim(World &w, uint rec, int32 step);
/** Sets a record's frame and poses it there. */
void poseAt(World &w, uint rec, int32 frame);

/** Picking up an item (interaction.md): hide, cursor = item, open the bar, flag := 1. */
void takeItem(World &w, int objIdx, int item, uint32 flag);
/** Plays a static sound unless it already plays (a playing DirectSound buffer goes on). */
void startSound(World &w, const Common::String &name, bool loop = false);
/** Adds (du, dv) to every UV of a node (0x4399d0); `wrap` masks the moved components, 0 = none. */
void scrollUVs(World &w, int node, int32 du, int32 dv, uint32 wrap = 0);

inline int node(World &w, int objIdx) { return w.objects[objIdx].node; }

SceneScript *createMusee();
SceneScript *createAuberge();
SceneScript *createHopiext();
SceneScript *createMaisonet();
SceneScript *createMangeurs();
SceneScript *createCafe();
SceneScript *createChambreb();
SceneScript *createChambrev();
SceneScript *createMaisonj();
SceneScript *createHopiint();
SceneScript *createPont();
SceneScript *createTerrasse();
SceneScript *createJardin();
SceneScript *createChamp();
SceneScript *createEglise();

} // End of namespace Peintre

#endif // PEINTRE_SCENES_SCENES_H
