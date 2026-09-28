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

#ifndef X3D_UNIT_H
#define X3D_UNIT_H

#include "common/str.h"

#include "math/vector3d.h"

#include "x3d/interaction.h"
#include "x3d/renderer.h"
#include "x3d/scene.h"
#include "x3d/sound.h"
#include "x3d/x3d.h"

namespace Common {
class Serializer;
}

namespace X3D {

constexpr float kHalfPi = (float)M_PI / 2; // the level pitch

// A unit's own code on top of the generic scene (U##.cpp in the original): U00, U01, ...
class Unit {
public:
	explicit Unit(X3DEngine *vm) : _vm(vm) {}
	virtual ~Unit() {}

	virtual void afterLoad() {}                      // renames and hides, before the hotspots
	virtual void start(bool newGame, bool video) = 0; // the start hook
	virtual bool input(float dt) { return false; }    // the input hook; false: normal camera keys
	virtual bool handle(const Common::String &action) { return false; } // a queued click action
	virtual void afterFrame() {}                     // per-frame checks after rendering
	virtual void afterAnimate() {}                   // each logic step, before the pose
	virtual void afterStep() {}                      // each logic step, after the pose
	virtual void afterClick(const Common::String &hotspot) {} // after a click's queued actions
	virtual bool beforeClick() { return false; }     // true: the click is taken, no pick
	virtual bool escape() { return false; }          // true: the unit handled Escape
	virtual void draw() {}                           // 2D drawn over the frame
	virtual bool gameStarted() const { return true; } // false: Escape opens the Option menu
	virtual void syncState(Common::Serializer &s) {}  // the unit's save chunk

protected:
	// Shared script helpers of the units

	Math::Vector3d at(const char *object) const { return _vm->scene()->objectPosition(object); }

	// The yaw that looks from `from` toward `target` (X3d_Convert_To_Polar)
	static float facing(const Math::Vector3d &target, const Math::Vector3d &from) {
		const Math::Vector3d d = target - from;
		return atan2f(-d.y(), d.x());
	}

	// Runs an action's steps, then counts it, without testing its condition; the unit
	// actions it names are queued
	void run(uint32 id) {
		Common::StringArray actions;
		_vm->interaction()->runAction(id, actions);
		for (const Common::String &a : actions)
			_vm->addUnitAction(a);
	}

	// The unit's Sound/<name>.wav on the effects or the voice emitter
	void effect(const char *name, const Math::Vector3d &position) {
		_vm->sound()->emit(Sound::kEffectsEmitter, soundPath(name), position, false);
	}
	void voiceAt(const char *name, const Math::Vector3d &position) {
		_vm->sound()->emit(Sound::kVoiceEmitter, soundPath(name), position, false);
	}
	Common::Path soundPath(const char *name) const {
		return Common::Path(_vm->scene()->dir() + "Sound/" + name + ".wav");
	}

	// Camera cuts and moves to a point (see X3DEngine::setView and moveTo)
	void setView(const Math::Vector3d &p, float yaw, float pitch) {
		const float eye[3] = { p.x(), p.y(), p.z() };
		_vm->setView(eye, yaw, pitch);
	}
	void moveTo(uint32 ms, const Math::Vector3d &p, float yaw, float pitch, float fov = X3DEngine::kKeep) {
		const float eye[3] = { p.x(), p.y(), p.z() };
		_vm->moveTo(ms, eye, yaw, pitch, fov);
	}

	// Blocking waits: frames run until the condition ends. walk: the camera keys stay on
	void waitStep() { // until the next logic step
		const uint32 t = _vm->logicMs();
		while (_vm->logicMs() == t && !_vm->shouldQuit())
			_vm->runFor(0);
	}
	void waitGroup(int group, bool walk = false) {
		while (_vm->sound()->isGroupPlaying(group) && !_vm->shouldQuit())
			_vm->runFor(0, walk);
	}
	void waitVoice(bool enterSkips) {
		while (_vm->sound()->isGroupPlaying(Sound::kVoice) && !(enterSkips && _vm->enterHeld()) && !_vm->shouldQuit())
			_vm->runFor(0);
	}
	void waitClip(const char *node, bool walk = false) {
		while (_vm->scene()->clipPlaying(node) && !_vm->shouldQuit())
			_vm->runFor(0, walk);
	}
	// Until the node stops, or reaches frame when frame >= 0
	void waitNode(const char *node, float frame = -1, bool walk = false) {
		Scene *scene = _vm->scene();
		while (scene->nodeRunning(node) && (frame < 0 || scene->nodeFrame(node) < frame) && !_vm->shouldQuit())
			_vm->runFor(0, walk);
	}

	X3DEngine *_vm;
};

// The timer gauge: a grey frame and a red bar that shrinks as the elapsed fraction p
// grows, in 640x480 frame pixels
inline void drawGauge(Renderer *r, float p) {
	const int x = (r->width() - 640) / 2;
	r->fillRect(x + 9, 9, x + 111, 21, 0x80, 0x80, 0x80);
	r->fillRect(x + 10, 10, x + 110 - (int)(100 * p), 20, 0xff, 0, 0);
}

} // End of namespace X3D

#endif // X3D_UNIT_H
