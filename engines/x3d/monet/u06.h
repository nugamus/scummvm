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

#ifndef X3D_MONET_U06_H
#define X3D_MONET_U06_H

#include "x3d/unit.h"

namespace X3D {

// U06, the orangery garden and the clown with the rifle
class U06 : public Unit {
public:
	explicit U06(X3DEngine *vm) : Unit(vm) {}

	void start(bool newGame, bool video) override;
	bool input(float dt) override;
	bool handle(const Common::String &action) override;
	void afterFrame() override;
	void afterAnimate() override; // the clown turns

private:
	void startShooting();
	void stopShooting();
	void shotDown();
	void wakeMan();
	void drain();

	bool _shooting = true; // never set by the original: its debug heap fills it, so true
	int _shots = 0;
	float _clownYaw = 3 * (float)M_PI / 2;
	bool _turned = false;
};

} // End of namespace X3D

#endif // X3D_MONET_U06_H
