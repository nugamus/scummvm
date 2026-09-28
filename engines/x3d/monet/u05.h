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

#include "x3d/unit.h"

namespace X3D {

// U05, the Saint-Lazare waiting room, the dog and Mazout
class U05 : public Unit {
public:
	explicit U05(X3DEngine *vm) : Unit(vm), _random("x3d_u05") {}

	void afterLoad() override;
	void start(bool newGame, bool video) override;
	bool handle(const Common::String &action) override;
	void afterFrame() override;
	void syncState(Common::Serializer &s) override; // the clock (not in the original)

private:
	void noCollisionFamily(const Common::String &object);
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

	Common::RandomSource _random;
	uint32 _timer = 0;   // the dog's wait, then the station master's chatter (logic ms)
	bool _leaving = false;
};

} // End of namespace X3D

#endif // X3D_MONET_U05_H
