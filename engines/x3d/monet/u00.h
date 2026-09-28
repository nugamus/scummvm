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

#ifndef X3D_MONET_U00_H
#define X3D_MONET_U00_H

#include "math/vector3d.h"

#include "x3d/unit.h"

namespace X3D {

class Collision;
class Scene;

// U04's name fix-ups and collision exclusions, shared by U00 and U04
void fixU04Names(Scene *scene);
void disableU04Boxes(Collision *collision);

// U00, the garden with Monet: players screen and tutorial
class U00 : public Unit {
public:
	U00(X3DEngine *vm, bool practice) : Unit(vm), _practice(practice) {}

	void afterLoad() override;
	void start(bool newGame, bool video) override;
	bool input(float dt) override;
	bool handle(const Common::String &action) override;
	void afterFrame() override;
	bool gameStarted() const override { return false; }

private:
	void say(const char *line, bool cut);
	void remark(const char *line);
	void startGauge(int state);
	void stopGauge() { _state = 0; _gaugeStart = 0; }
	bool fired() const;
	bool onStone(const Common::String &ground);

	bool _practice;
	bool _started = false;
	bool _onStone = false;
	bool _moved = false;
	bool _turned = false;
	bool _glassesTaken = false;
	bool _nearMonet = false;
	bool _spaceSeen = false;
	int _state = 0;         // the hidden gauge's state
	uint32 _gaugeStart = 0; // 0: stopped
	Math::Vector3d _monet;  // hotspot positions at creation
	Math::Vector3d _boat;
};

} // End of namespace X3D

#endif // X3D_MONET_U00_H
