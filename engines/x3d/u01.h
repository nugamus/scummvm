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

#include "x3d/unit.h"

#include "math/vector3d.h"

namespace X3D {

class X3DEngine;

// U01, the station: its unit code (docs/engine-spec/u01.md)
class U01 : public Unit {
public:
	explicit U01(X3DEngine *vm);

	void afterLoad() override;
	// The scripted entry, after the prologue when video is set; newGame false: state only
	void start(bool newGame, bool video) override;
	bool input(float dt) override; // the train
	bool handle(const Common::String &action) override;
	void afterClick(const Common::String &hotspot) override; // the climb's repeat
	void afterFrame() override;    // the escape timer
	void syncState(Common::Serializer &s) override; // TRAIN_CHANGED and the gauge

private:
	void closeDoor();
	void openDoor();
	void call();
	void climb();
	void throwSwitch();
	void ride(float dt);
	bool onTrain(const Common::String &ground);
	void caught();
	void leave();
	void waitVoice();
	Math::Vector3d objectUnder(const Common::String &parent, const Common::String &name);
	Math::Vector3d at(const char *object);

	X3DEngine *_vm;
	Common::RandomSource _random;
	bool _switchThrown = false, _train2Loaded = false, _onTrain = false, _firstMaire = true;
};

} // End of namespace X3D

#endif // X3D_U01_H
