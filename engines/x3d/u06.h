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

#ifndef X3D_U06_H
#define X3D_U06_H

#include "math/vector3d.h"

#include "x3d/unit.h"

namespace X3D {

class X3DEngine;

// U06, the orangery garden and the clown with the rifle (docs/engine-spec/u06.md)
class U06 : public Unit {
public:
	explicit U06(X3DEngine *vm) : _vm(vm) {}

	void start(bool newGame, bool video) override;
	bool input(float dt) override;
	bool handle(const Common::String &action) override;
	void afterFrame() override;
	void afterAnimate() override; // the clown turns

private:
	void effect(const char *name, const Math::Vector3d &position);
	void startShooting();
	void stopShooting();
	void shotDown();
	void wakeMan();
	void drain();

	X3DEngine *_vm;
	bool _shooting = true; // never written by the original: debug-heap fill, so true (E-0534)
	int _shots = 0;
	float _clownYaw = 3 * (float)M_PI / 2;
	bool _turned = false;
};

} // End of namespace X3D

#endif // X3D_U06_H
