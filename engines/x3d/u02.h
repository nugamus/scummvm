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

#ifndef X3D_U02_H
#define X3D_U02_H

#include "common/random.h"

#include "math/vector3d.h"

#include "x3d/unit.h"

namespace X3D {

class X3DEngine;

// U02, the level crossing and the ticket office (docs/engine-spec/u02.md)
class U02 : public Unit {
public:
	explicit U02(X3DEngine *vm) : _vm(vm), _random("x3d_u02") {}

	void afterLoad() override;
	void start(bool newGame, bool video) override;
	bool handle(const Common::String &action) override;
	void afterFrame() override;
	void draw() override; // the train gauge
	void syncState(Common::Serializer &s) override; // TIMEVENDEUSE and the gauge

private:
	void say(const char *character, const char *line);
	void show(const char *object);
	void effect(const char *name, const Math::Vector3d &position);
	Math::Vector3d hotspotPosition(const char *hotspot);
	void resetCalls();
	void waitClip(const char *node);
	void waitGroup(int group, bool walk = false);
	void startSnore();
	void walkPath(uint32 ms, const Math::Vector3d &target);
	void follow(const char *object, float untilFrame);

	void refuseCoin();
	void retakeCoin();
	void buyChestnuts();
	void magpieSteals();
	void magpieFlies();
	void feedMagpie();
	void ringBell();
	void buyTicket();
	void takeTicket();
	void board();
	void fall();
	void gameOver();

	X3DEngine *_vm;
	Common::RandomSource _random;
	uint32 _callStart = 0, _callPeriod = 10000; // the seller's calls, in logic ms
	bool _callOff = false;
	int _magpie = 0; // 0 idle, 1 perched and waiting, 2 flown
	bool _gauge = false;
	uint32 _gaugeStart = 0;
};

} // End of namespace X3D

#endif // X3D_U02_H
