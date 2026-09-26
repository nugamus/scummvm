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

#ifndef X3D_MONET_U05_H
#define X3D_MONET_U05_H

#include "common/random.h"

#include "math/vector3d.h"

#include "x3d/unit.h"

namespace X3D {

class X3DEngine;

// U05, the Saint-Lazare waiting room, the dog and Mazout (games/monet/docs/u05.md)
class U05 : public Unit {
public:
	explicit U05(X3DEngine *vm) : _vm(vm), _random("x3d_u05") {}

	void afterLoad() override;
	void start(bool newGame, bool video) override;
	bool handle(const Common::String &action) override;
	void afterFrame() override;

private:
	void run(uint32 id);
	void effect(const char *name, const Math::Vector3d &position);
	void voice(const char *name);
	void noCollisionFamily(const Common::String &object);
	float facing(const Math::Vector3d &target, const Math::Vector3d &from) const;
	Math::Vector3d at(const char *object) const;
	bool near(const char *object, float distance) const;

	bool barkAndSit();
	bool waitLoop();
	bool chefNotices();
	bool mazoutConfronts();
	void gaugeExpired();
	void leave();

	void testSpeakChef();
	void dogToDoor();
	void maskGrille();
	void lampFalls();

	X3DEngine *_vm;
	Common::RandomSource _random;
	uint32 _timer = 0;   // the dog's wait, then the station master's chatter (logic ms)
	bool _leaving = false;
};

} // End of namespace X3D

#endif // X3D_MONET_U05_H
