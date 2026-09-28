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

#include "x3d/collision.h"
#include "x3d/interaction.h"
#include "x3d/scene.h"
#include "x3d/sound.h"
#include "x3d/talk.h"
#include "x3d/x3d.h"
#include "x3d/monet/u06.h"

namespace X3D {

using Math::Vector3d;

static const float kKeep = X3DEngine::kKeep;
static const char *const kClown = "*U03_02";
static const char *const kCan = "*U06_17";
static const float kRange = 330; // the clown shoots within this distance

void U06::start(bool newGame, bool video) {
	Scene *scene = _vm->scene();
	_vm->sound()->play(Common::Path(scene->dir() + "Sound/s4_11.wav"), Sound::kAmbient, 85, true);
	_vm->talk()->addTalker("U06_18", "$$$DUMMY.*visage");
	scene->pauseNode("lourde");
	_shots = 0;
	scene->setNodeLoop(kClown, false);
	_clownYaw = 3 * (float)M_PI / 2;
	_vm->player().sphereOffset = 5; // the offset only: setSphere would set the radius too
	if (!newGame)
		return;
	const float p[3] = { -673.3f, 475, 25.4f };
	_vm->setView(p, 2.16f, kHalfPi);
	_vm->autosave();
}

void U06::startShooting() {
	_vm->scene()->runNodeTo(kClown, -1, false);
	_shooting = true;
}

void U06::stopShooting() {
	if (!_shooting)
		return;
	_vm->scene()->runNodeTo(kClown, 49, false);
	_shooting = false;
	_shots = 0;
}

void U06::afterFrame() {
	// Three shots in range are fatal
	const Vector3d &eye = _vm->player().eye;
	const bool safe = eye.y() > 280 || (eye.x() > -800 && eye.y() < 230);
	if (safe) {
		stopShooting();
	} else {
		const float d = (eye - _vm->scene()->objectPosition(kClown)).getMagnitude();
		if (!_shooting && d <= kRange)
			startShooting();
		else if (_shooting && d > kRange)
			stopShooting();
	}
	Scene *scene = _vm->scene();
	if (_shooting && !scene->nodeRunning(kClown)) {
		scene->setNodeFrame(kClown, 1);
		scene->runNodeTo(kClown, -1, false);
		effect("Tir", eye);
		if (++_shots > 2)
			shotDown();
	}
}

bool U06::input(float dt) {
	// The generic input first, then, while the player walks, the clown turns toward the
	// new eye (Hotspot_TurnToYaw)
	const Keys &keys = _vm->keys();
	if (_vm->player().tick(dt, keys, *_vm->collision()))
		_vm->sound()->emit(Sound::kEffectsEmitter, "SAUT.WAV", _vm->player().eye, false);
	if (keys.up || keys.down) {
		const Vector3d d = _vm->player().eye - _vm->scene()->objectPosition(kClown);
		_clownYaw = atan2f(-d.y(), d.x());
		_turned = true;
	}
	return true;
}

void U06::afterAnimate() {
	// Each tick after the pose: the clown's local matrix times Rz(3pi/2 - yaw); the
	// hotspot's load-time matrix is the identity
	if (!_turned)
		return;
	O3DObject *clown = _vm->scene()->object(kClown);
	if (!clown)
		return;
	const float a = 3 * (float)M_PI / 2 - _clownYaw;
	const float c = cosf(a);
	const float s = sinf(a);
	const float rz[9] = { c, s, 0, -s, c, 0, 0, 0, 1 };
	float out[9];
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++) {
			out[i * 3 + j] = 0;
			for (int k = 0; k < 3; k++)
				out[i * 3 + j] += clown->matrix[i * 4 + k] * rz[k * 3 + j];
		}
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			clown->matrix[i * 4 + j] = out[i * 3 + j];
}

void U06::shotDown() {
	// U06_ShotDown: game over
	Player &player = _vm->player();
	_vm->suspend(true);
	_vm->runFor(800);
	_vm->moveTo(800, nullptr, kKeep, 2.0f);
	_vm->moveTo(1200, nullptr, player.yaw + 0.5f, 2.76f);
	_vm->moveTo(1400, nullptr, player.yaw + (float)M_PI, kKeep);
	_shots = 0;
	_vm->fadeToBlack(600);
	_vm->runFor(1000);
	_vm->suspend(false);
	_vm->gameOver();
}

void U06::wakeMan() {
	// UseArrosoir: the watering can wakes the sleeping man
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	scene->hideObject(kCan, false);
	_vm->collision()->setEnabled(kCan, true);
	scene->setNodeFrame(kCan, 1);
	scene->runNodeTo(kCan, -1, false);
	scene->setClip("*U06_18", "Anim/U06_18/reveil.A3D", "", 1, false); // not active yet
	const float half = (scene->nodeLastFrame(kCan) - 1) * 0.5f;
	bool splashed = false;
	while (scene->nodeRunning(kCan) && !_vm->enterHeld() && !_vm->shouldQuit()) {
		const Vector3d g = at(kCan);
		setView(Vector3d(g.x() - 3.09f, g.y() - 8.542f, player.eye.z()), player.yaw, player.pitch);
		_vm->lookAt(0, Vector3d(-1058.87f, 406.222f, 15.2054f));
		const float f = scene->nodeFrame(kCan);
		if (!splashed && f > half - 15) {
			splashed = true;
			effect("s4_13", player.eye);
		}
		if (f > half && scene->activeSlot("*U06_18") != 1) {
			scene->activateSlot("*U06_18", 1);
			scene->setNodeLoop("*U06_18", false);
			scene->runNodeTo("*U06_18", -1, false);
		}
	}
	while (scene->clipPlaying("*U06_18") && !_vm->enterHeld() && !_vm->shouldQuit())
		_vm->runFor(0);
	_vm->talk()->say("U06_18", "d4_12");
	const float p[3] = { -1049.73f, 395.246f, 25.41f };
	_vm->moveTo(4000, p, -2.8f, 0.77f);
	while (_vm->talk()->talking() && !_vm->shouldQuit())
		_vm->runFor(0);
	_vm->moveTo(1000, p, -5.2f, 1.33f);
	scene->hideObject("*U06_18");
	_vm->collision()->setEnabled("*U06_18", false);
	_vm->suspend(false);
}

void U06::drain() {
	// UseBiche: down the drain to U07
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	scene->setNodeLoop("*U06_21", false);
	scene->runNodeTo("*U06_21", -1, false);
	effect("s4_15", player.eye);
	waitNode("*U06_21");
	const float p1[3] = { -685.55f, -1224.05f, 25 };
	const float p2[3] = { -685.295f, -1224.03f, -7.15f };
	_vm->moveTo(1000, p1, kKeep, kKeep);
	_vm->moveTo(1000, p2, 4.28f, kKeep);
	_vm->runFor(1000);
	_vm->moveTo(800, nullptr, 4.11f, 2.777f);
	scene->runNodeTo("*U06_21", -1, true); // the cover closes over the player
	effect("s4_15", player.eye);
	waitNode("*U06_21");
	_vm->suspend(false);
	_vm->gotoScene("U07.x3d");
}

bool U06::handle(const Common::String &action) {
	if (action.equalsIgnoreCase("OpenDoor")) {
		_vm->scene()->runNodeTo("*U06_16", -1, false);
		effect("s1_04", _vm->player().eye);
		_vm->interaction()->setCursorKind("*U06_16", 0);
	} else if (action.equalsIgnoreCase("UseArrosoir")) {
		wakeMan();
	} else if (action.equalsIgnoreCase("UseBiche")) {
		drain();
	} else {
		return false;
	}
	return true;
}

} // End of namespace X3D
