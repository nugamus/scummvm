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

#ifndef X3D_MONET_U33_H
#define X3D_MONET_U33_H

#include "math/vector3d.h"

#include "x3d/unit.h"

namespace X3D {

class X3DEngine;

// U33, the square at night: the projectionist, the caravan, the clown and the bike
// (games/monet/docs/u33.md)
class U33 : public Unit {
public:
	explicit U33(X3DEngine *vm) : _vm(vm) {}

	void afterLoad() override;
	void start(bool newGame, bool video) override;
	bool handle(const Common::String &action) override;
	void afterFrame() override;
	bool beforeClick() override; // the curtain trap

private:
	void run(uint32 id);
	Math::Vector3d ground(float x, float y, float z);
	void walk(uint32 ms, const Math::Vector3d &to, float yaw);
	void walkG(uint32 ms, float x, float y, float z, float yaw);
	float facing(const Math::Vector3d &target, const Math::Vector3d &from) const;
	Math::Vector3d at(const char *object) const;
	void effect(const char *name, const Math::Vector3d &position);
	void waitNode(const char *node, float frame); // until the node pauses or reaches frame
	void voiceWait(bool enterStops);

	void walkToScreen();
	void afterFilm();
	void firstFilm();
	void secondFilm();
	void shutter();
	void enterCaravan();
	void leaveCaravan();
	void hide();
	void unhide();
	void caughtBehindCurtain();
	void gameOverClown();
	void talkProjectionist();
	void policeman();
	void ride();

	X3DEngine *_vm;
	uint32 _timer = 0; // the nag's and the clown search's shared clock (E-0422)
};

} // End of namespace X3D

#endif // X3D_MONET_U33_H
