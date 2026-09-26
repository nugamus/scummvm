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

#include "common/serializer.h"

#include "x3d/collision.h"
#include "x3d/player.h"

namespace X3D {

using Math::Vector3d;

// docs/engine-spec/movement.md, Camera state
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
}

Vector3d Player::slide(const Vector3d &from, const Vector3d &velocity, const Collision &collision) const {
	const Vector3d offset(0, 0, sphereOffset);
	return collision.resolveSphere(from - offset + velocity, _radius) + offset;
}

bool Player::ground(const Collision &collision) {
	float t;
	if (!collision.cast(eye, eye - Vector3d(0, 0, 10000), t, &groundObject))
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

	// Walk along the view direction's x and y, not renormalised
	const Vector3d d(cosf(yaw) * sinf(pitch), -sinf(yaw) * sinf(pitch), -cosf(pitch));
	const bool running = runAllowed && keys.ctrl;
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
		if (direction) {
			// Head bob: roll 1 degree per second, turning back at +-0.4
			roll += _bob * dt;
			if (fabs(roll) >= 0.4f)
				_bob = -_bob;

			float step = _speed * _scale * dt * (running ? 2 : 1);
			const Vector3d centre = eye - Vector3d(0, 0, sphereOffset);
			const Vector3d ahead(10000 * d.x(), 10000 * d.y(), d.z());
			float t;
			if (collision.cast(centre, centre + ahead, t) && t * ahead.getMagnitude() < 2 * _radius)
				step /= 2;
			velocity.set(direction * d.x() * step, direction * d.y() * step, 0);
		}
		// Shift (press) jumps: the walk at x0.5 when running, x0.25 otherwise
		if (jumpAllowed && keys.shift && !_shiftWas) {
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
			yaw += 0.06f;
		if (keys.left)
			yaw -= 0.06f;
		if (keys.pageUp && !keys.ctrl && pitch < 2.7f)
			pitch += 0.06f;
		if (keys.pageDown && !keys.ctrl && pitch > 0.6f)
			pitch -= 0.06f;
	}

	if (velocity.getMagnitude() > 0) {
		if (!collide) {
			eye += velocity;
			return false;
		}
		eye = slide(eye, velocity, collision);
		if (ground(collision))
			_fallVelocity = velocity;
	}
	return false;
}

void Player::probeGround(const Collision &collision) {
	float t;
	groundObject.clear();
	collision.cast(eye, eye - Vector3d(0, 0, 10000), t, &groundObject);
}

} // End of namespace X3D
