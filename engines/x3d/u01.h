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

#ifndef X3D_U01_H
#define X3D_U01_H

#include "common/random.h"
#include "common/str.h"

#include "math/vector3d.h"

namespace X3D {

class X3DEngine;

// U01, the station: its unit code (docs/engine-spec/u01.md)
class U01 {
public:
	explicit U01(X3DEngine *vm);

	void afterLoad();          // renames and hides, before the hotspots are applied
	// The scripted entry, after the prologue when video is set; newGame false: state only
	void start(bool newGame, bool video = true);
	bool input(float dt);      // the input hook (the train); false: normal camera keys
	bool handle(const Common::String &action); // a queued click action
	void afterFrame();         // the escape timer
	void draw();               // the escape timer's gauge

private:
	void closeDoor();
	void openDoor();
	void call();
	void climb();
	void throwSwitch();
	void ride(float dt);
	void caught();
	void leave();
	void fadeToBlack(uint32 ms);
	void waitVoice();
	Math::Vector3d objectUnder(const Common::String &parent, const Common::String &name);
	Math::Vector3d at(const char *object);

	X3DEngine *_vm;
	Common::RandomSource _random;
	bool _switchThrown = false, _train2Loaded = false, _onTrain = false, _firstMaire = true;
	bool _gauge = false;
	uint32 _gaugeStart = 0;
};

} // End of namespace X3D

#endif // X3D_U01_H
