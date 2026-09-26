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

#include "common/config-manager.h"
#include "common/serializer.h"

#include "x3d/collision.h"
#include "x3d/player.h"

namespace X3D {

using Math::Vector3d;

// engines/x3d/docs/spec/movement.md, Camera state
void Player::syncState(Common::Serializer &s) {
	for (int k = 0; k < 3; k++)
		s.syncAsFloatLE(eye.getData()[k]);
	s.syncAsFloatLE(yaw);
	s.syncAsFloatLE(pitch);
	s.syncAsFloatLE(fov);
	s.syncAsFloatLE(_radius);
	s.syncAsFloatLE(sphereOffset);
	s.syncAsByte(canMove);
	s.syncAsByte(canTurn);
	s.syncAsByte(collide);
	s.syncAsFloatLE(_eyeHeight);
}

void Player::init(float scale) {
	_scale = scale;
	_speed = 2;
	_eyeHeight = 1.5f * scale;
	_radius = 0.5f * scale;
	sphereOffset = _eyeHeight - 2 * _radius;
	_runToggle = ConfMan.getBool("run_toggle");
	_crouchToggle = ConfMan.getBool("crouch_toggle");
	_turn = 0.06f * ConfMan.getInt("turn_speed") / 100; // the turn_speed option, percent
}

Vector3d Player::slide(const Vector3d &from, const Vector3d &velocity, const Collision &collision) const {
	const Vector3d offset(0, 0, sphereOffset);
	return collision.resolveSphere(from - offset + velocity, _radius) + offset;
}

bool Player::ground(const Collision &collision) {
	float t;
	groundObject.clear(); // nothing below: no ground object (U07's water check)
	groundModel = nullptr;
	if (!collision.cast(eye, eye - Vector3d(0, 0, 10000), t, &groundObject, &groundModel, &groundIndex))
		return false;
	const float g = eye.z() - 10000 * t;
	if (eye.z() - g <= 1.3f * _eyeHeight) {
		eye.z() = g + _eyeHeight;
		return false;
	}
	_falling = true;
	_fallTime = 0;
	_fallZ = eye.z();
	_fallDrop = eye.z() - (g + _eyeHeight);
	return true;
}

bool Player::tick(float dt, const Keys &keys, const Collision &collision) {
	// The run_toggle option (not in the original): each Ctrl press flips running
	if (_runToggle && keys.ctrl && !_ctrlWas)
		_runOn = !_runOn;
	_ctrlWas = keys.ctrl;

	// A fall blocks input until it lands
	if (_falling) {
		_fallTime += dt;
		const float drop = 5.9f * _scale * _fallTime * _fallTime;
		if (drop < _fallDrop) {
			eye.z() = _fallZ - drop;
			return false;
		}
		eye.z() = _fallZ - _fallDrop;
		_falling = false;
		eye = slide(eye, _fallVelocity, collision);
		return _fallDrop > 1.5f * _scale;
	}

	if (_jumping) {
		_jumpTime += dt;
		const float z = _jumpZ + _scale * _jumpTime - _scale * _jumpTime * _jumpTime / 2;
		Vector3d next = collide ? slide(eye, _jumpVelocity, collision) : eye + _jumpVelocity;
		next.z() = z;
		float t;
		const bool landed = collision.cast(next, next - Vector3d(0, 0, 10000), t) && 10000 * t < _eyeHeight && _jumpTime > 0.1f;
		const bool blocked = collision.cast(next, next + Vector3d(0, 0, 0.4f * _scale), t);
		eye = next;
		if (landed || blocked || _jumpTime >= 2) {
			_jumping = false;
			ground(collision);
		}
		return false;
	}

	// Numpad 0 (press) crouches; the key-down ends the step (movement.md, Keys and Crouch)
	const float crouchHeight = 0.5f * _scale + 3;
	const bool crouchPress = keys.crouch && !_crouchWas;
	_crouchWas = keys.crouch;
	// The crouch_toggle option (not in the original): a second press stands up
	if (_crouchToggle && crouchPress && _crouch != kStanding)
		_crouchLatched = false;
	if (crouchPress && _crouch == kStanding) {
		_crouchLatched = true;
		_crouch = kLowering;
		_crouchTime = 0;
		_standHeight = _eyeHeight;
		_standOffset = sphereOffset;
		sphereOffset = crouchHeight - _radius - 1;
		return false;
	}
	if (_crouch == kLowering || _crouch == kRising) {
		// Down over 1000 ms, up over 2000 ms; nothing else meanwhile
		_crouchTime += dt;
		const bool lowering = _crouch == kLowering;
		const float f = MIN(1.0f, _crouchTime / (lowering ? 1.0f : 2.0f));
		const float h = lowering ? _standHeight + (crouchHeight - _standHeight) * f : crouchHeight + (_standHeight - crouchHeight) * f;
		eye.z() += h - _eyeHeight;
		_eyeHeight = h;
		if (f >= 1) {
			_crouch = lowering ? kCrouched : kStanding;
			if (!lowering)
				sphereOffset = _standOffset;
		}
		return false;
	}
	if (_crouch == kCrouched && !(_crouchToggle ? _crouchLatched : keys.crouch)) {
		// Up when the lowest hit above the eye is at least 0.4 s away (headroom)
		float t;
		if (!collision.cast(eye, eye + Vector3d(0, 0, 0.4f * _scale), t)) {
			_crouch = kRising;
			_crouchTime = 0;
			return false;
		}
	}
	const bool crouched = _crouch == kCrouched;

	// Walk along the view direction's x and y, not renormalised
	const Vector3d d(cosf(yaw) * sinf(pitch), -sinf(yaw) * sinf(pitch), -cosf(pitch));
	const bool running = !crouched && runAllowed && (_runToggle ? _runOn : keys.ctrl);
	Vector3d velocity;
	if (canMove) {
		int direction = 0;
		if (keys.up) {
			_speed = 2;
			direction = 1;
		}
		if (keys.down) {
			if (_speed == 2)
				_speed = 1;
			direction = -1;
		}
		// Modern controls: sideways at the walking speed, through the same slide
		const int strafe = (keys.strafeRight ? 1 : 0) - (keys.strafeLeft ? 1 : 0);
		if (direction || strafe) {
			// Head bob: roll 1 degree per second, turning back at +-0.4
			roll += _bob * dt;
			if (fabs(roll) >= 0.4f)
				_bob = -_bob;

			float step = _speed * _scale * dt * (running ? 2 : 1) * (crouched ? 0.25f : 1);
			const Vector3d centre = eye - Vector3d(0, 0, sphereOffset);
			const Vector3d ahead(10000 * d.x(), 10000 * d.y(), d.z());
			float t;
			if (collision.cast(centre, centre + ahead, t) && t * ahead.getMagnitude() < 2 * _radius)
				step /= 2;
			velocity.set(direction * d.x() * step - strafe * sinf(yaw) * step, direction * d.y() * step - strafe * cosf(yaw) * step, 0);
		}
		// Shift (press) jumps: the walk at x0.5 when running, x0.25 otherwise
		if (jumpAllowed && !crouched && keys.shift && !_shiftWas) {
			_jumping = true;
			_jumpTime = 0;
			_jumpZ = eye.z();
			_jumpVelocity = velocity * 0.25f; // the walk step: x0.5 of a run, x0.25 of a walk
		}
	}
	_shiftWas = keys.shift;

	// Per logic step, not per second: the original's rate per frame (Q-0022)
	if (canTurn) {
		if (keys.right)
			yaw += _turn;
		if (keys.left)
			yaw -= _turn;
		if (keys.pageUp && !keys.ctrl && !crouched && pitch < 2.7f)
			pitch += _turn;
		if (keys.pageDown && !keys.ctrl && !crouched && pitch > 0.6f)
			pitch -= _turn;
	}

	if (velocity.getMagnitude() > 0) {
		if (!collide) {
			eye += velocity;
			return false;
		}
		Vector3d next = slide(eye, velocity, collision);
		// Held at an opening narrower than the sphere: slide once more with the original's
		// longest step (its 8 fps floor, at most r/2 so no flat face is crossed), and take it
		// when that gets at least a step further (movement.md, Narrow openings)
		const Vector3d dir = velocity * (1.0f / velocity.getMagnitude());
		const float progress = Vector3d::dotProduct(next - eye, dir);
		if (progress < 0.1f * velocity.getMagnitude()) {
			const float longStep = MIN(velocity.getMagnitude() / (8 * dt), _radius / 2);
			const Vector3d far = slide(eye, dir * longStep, collision);
			if (Vector3d::dotProduct(far - eye, dir) >= progress + velocity.getMagnitude())
				next = far;
		}
		eye = next;
		if (ground(collision))
			_fallVelocity = velocity;
	}
	return false;
}

void Player::probeGround(const Collision &collision) {
	float t;
	groundObject.clear();
	groundModel = nullptr;
	collision.cast(eye, eye - Vector3d(0, 0, 10000), t, &groundObject, &groundModel, &groundIndex);
}

bool Player::standsOn(Scene &scene, const Common::String &name) const {
	// The object the name finds, not any object of that name (u01.md, E-0083)
	Scene::Model *m;
	uint o;
	return groundModel && scene.findObject(name, m, o) && m == groundModel && o == groundIndex;
}

} // End of namespace X3D
