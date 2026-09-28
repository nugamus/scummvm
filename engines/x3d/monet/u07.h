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

#ifndef X3D_MONET_U07_H
#define X3D_MONET_U07_H

#include "common/random.h"

#include "math/vector3d.h"

#include "x3d/unit.h"

namespace X3D {

// U07, the cellars under the orangery and the end of the game
class U07 : public Unit {
public:
	explicit U07(X3DEngine *vm) : Unit(vm), _random("x3d_u07") {}

	void afterLoad() override;
	void start(bool newGame, bool video) override;
	bool input(float dt) override;
	bool handle(const Common::String &action) override;
	void afterFrame() override;
	void syncState(Common::Serializer &s) override; // PLANCHE

private:
	float heightAboveGround(const Math::Vector3d &p);
	void switchAndDescent();
	void tipPlank();
	void fallInWater();
	void explosion();
	void end();

	Common::RandomSource _random;
	bool _plankTipped = false;
	bool _upWas = false; // one rung per key press
	bool _downWas = false;
};

} // End of namespace X3D

#endif // X3D_MONET_U07_H
