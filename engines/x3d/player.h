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

#ifndef X3D_PLAYER_H
#define X3D_PLAYER_H

#include "common/str.h"

#include "math/vector3d.h"

#include "x3d/scene.h"

namespace Common {
class Serializer;
}

namespace X3D {

class Collision;

// Keys read by the keyboard camera, held state (docs/engine-spec/movement.md, Keys)
struct Keys {
	bool up = false, down = false, left = false, right = false;
	bool pageUp = false, pageDown = false, ctrl = false, shift = false, space = false, crouch = false;
};

// The first-person camera: movement, turning, collision and falls
class Player {
public:
	void init(float scale); // camera defaults from the scene scale s
	void setSphere(float radius, float offset) { _radius = radius; sphereOffset = offset; }
	void syncState(Common::Serializer &s); // save.md CAMERA

	// One logic step of dt seconds. Returns true when a fall ends with the landing sound.
	bool tick(float dt, const Keys &keys, const Collision &collision);

	Math::Vector3d eye;
	float yaw = 0, pitch = M_PI / 2, fov = 90, roll = 0;
	float sphereOffset = 0; // eye minus collision sphere centre, along Z
	bool canMove = true, canTurn = true;
	bool runAllowed = true, jumpAllowed = true; // Ctrl runs, Shift jumps (never unset: on; U33's caravan turns them off)
	void setEyeHeight(float h) { _eyeHeight = h; }
	float eyeHeight() const { return _eyeHeight; }
	bool collide = true;           // collision and ground snapping (U01's train turns it off)
	Common::String groundObject;   // name of the object the last ground probe hit
	const Scene::Model *groundModel = nullptr; // and the object itself
	uint groundIndex = 0;
	// The ground object is the object the scene finds by this name
	bool standsOn(Scene &scene, const Common::String &name) const;

	// The downward probe alone: updates groundObject, not the position
	void probeGround(const Collision &collision);
	bool ground(const Collision &collision); // snaps eye to the ground or starts a fall
	bool falling() const { return _falling; }
	bool jumping() const { return _jumping; }

private:
	Math::Vector3d slide(const Math::Vector3d &eye, const Math::Vector3d &velocity, const Collision &collision) const;

	float _scale = 1, _speed = 2, _eyeHeight = 1.5f, _radius = 0.5f;
	float _bob = 1; // head bob direction, kept across scenes

	// A jump in progress (movement.md, Jump): z = z0 + s t - s t^2 / 2, the walk sampled
	// once at the start
	bool _jumping = false, _shiftWas = false;
	bool _runToggle = false, _runOn = false, _ctrlWas = false; // the run_toggle option

	// Crouching (movement.md, Crouch): lowering, down, rising; the standing eye height and
	// sphere offset come back when it ends
	enum { kStanding, kLowering, kCrouched, kRising } _crouch = kStanding;
	bool _crouchWas = false;
	float _crouchTime = 0, _standHeight = 0, _standOffset = 0;
	float _jumpTime = 0, _jumpZ = 0;
	Math::Vector3d _jumpVelocity;

	// A fall in progress: drops _fallDrop from _fallZ, then slides once with _fallVelocity
	bool _falling = false;
	float _fallTime = 0, _fallZ = 0, _fallDrop = 0;
	Math::Vector3d _fallVelocity;
};

} // End of namespace X3D

#endif // X3D_PLAYER_H
