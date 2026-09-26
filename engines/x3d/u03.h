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

#ifndef X3D_U03_H
#define X3D_U03_H

#include "math/vector3d.h"

#include "x3d/scene.h"
#include "x3d/unit.h"

namespace X3D {

class X3DEngine;

// U03, the street, the clown and the policeman (docs/engine-spec/u03.md)
class U03 : public Unit {
public:
	explicit U03(X3DEngine *vm) : _vm(vm) {}

	void afterLoad() override;
	void start(bool newGame, bool video) override;
	bool handle(const Common::String &action) override;
	void afterAnimate() override; // the policeman's round

private:
	void run(uint32 id);
	Math::Vector3d ground(float x, float y, float z); // G(x, y, z)
	float facing(const Math::Vector3d &target, const Math::Vector3d &from) const;
	Math::Vector3d at(const char *object) const;
	void waitVoice(bool enterSkips = true);
	void transition(int n);

	void clownTrick();
	void salute();
	void cutscene();
	void aimAtTarget();
	void cut();

	X3DEngine *_vm;
	bool _follow = true;
	Scene::Model *_camera = nullptr, *_cine = nullptr; // the cutscene's models
};

} // End of namespace X3D

#endif // X3D_U03_H
