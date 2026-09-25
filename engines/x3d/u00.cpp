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

#include "common/system.h"

#include "x3d/collision.h"
#include "x3d/interaction.h"
#include "x3d/scene.h"
#include "x3d/sound.h"
#include "x3d/talk.h"
#include "x3d/u00.h"
#include "x3d/x3d.h"

namespace X3D {

using Math::Vector3d;

static const float kStart[3] = { 81.1334f, 265.81f, 15.0f };
static const float kStartYaw = 4.67f, kStartPitch = 1.47f;

void U00::afterLoad() {
	// u00.md, Start 1-2: the glasses Monet will wear, the glasses to take, U04 extras
	Scene *scene = _vm->scene();
	scene->renameObject("*Lunettes0", "*U04_81");
	scene->hideObject("*U04_81");
	scene->renameObject("*Lunettes0", "*U04_80");
	for (const char *name : { "*U04_63", "*U04_31", "*U04_44", "*U04_05", "*U04_53" })
		scene->hideObject(name);
}

void U00::start(bool newGame, bool video) {
	Player &player = _vm->player();
	_vm->sound()->play(Common::Path("U04/Sound/s3_01.wav"), Sound::kAmbient, 85, true);
	player.setSphere(3.0f, 1.0f);
	_vm->talk()->addTalker("U04_03", "$$$DUMMY.*visage", "Anim/U04_03_Lunettes/");
	_vm->setView(kStart, kStartYaw, kStartPitch);
	if (_practice)
		return;

	// Monet's first line, then the players screen over the paused scene (ui.md)
	_vm->runFor(0);
	say("sb01", false);
	for (;;) {
		const Common::String c = _vm->runMenu("OptionUser");
		if (_vm->shouldQuit() || c == "escape") {
			_vm->quitGame();
			return;
		}
		if ((c == "SelectUser" || c == "enter") && !_vm->menuText().empty())
			break;
	}
	// ponytail: every player is new (profiles are not implemented), so the tutorial runs
}

void U00::say(const char *line, bool cut) {
	// Blocking, no skip key (u00.md, Monet's lines)
	Player &player = _vm->player();
	const Vector3d eye = player.eye;
	const float yaw = player.yaw, pitch = player.pitch, fov = player.fov;
	if (cut) {
		_vm->setView(kStart, kStartYaw, kStartPitch);
		player.fov = 90;
	}
	_vm->scene()->startAnimation("*U04_03");
	_vm->talk()->say("U04_03", line);
	do
		_vm->runFor(0);
	while (_vm->sound()->isGroupPlaying(Sound::kVoice) && !_vm->shouldQuit());
	if (cut) {
		const float p[3] = { eye.x(), eye.y(), eye.z() };
		_vm->setView(p, yaw, pitch);
		player.fov = fov;
	}
}

void U00::remark(const char *line) {
	Sound *sound = _vm->sound();
	sound->stopGroup(Sound::kVoice);
	sound->play(Common::Path(Common::String("U00/Sound/") + line + ".wav"), Sound::kVoice, 100, false);
}

void U00::startGauge(int state) {
	_state = state;
	_gaugeStart = g_system->getMillis();
}

bool U00::fired() const {
	return _gaugeStart && g_system->getMillis() - _gaugeStart >= 10000;
}

bool U00::onStone(const Common::String &ground) {
	Scene::Model *m;
	uint o;
	if (ground.empty() || !_vm->scene()->findObject(ground, m, o))
		return false;
	for (int a = o; a >= 0; a = m->file.objects[a].parent)
		if (m->file.objects[a].name.equalsIgnoreCase("*U04_32"))
			return true;
	return false;
}

void U00::afterFrame() {
	// u00.md, Every frame
	if (!_started) {
		_started = true;
		say(_practice ? "sb03_bis" : "sb03", false);
		startGauge(1);
		_vm->setView(kStart, kStartYaw, kStartPitch);
		_vm->player().fov = 90;
		return;
	}
	if (!fired())
		return;
	const bool holding = !_vm->interaction()->heldItem().empty();
	switch (_state) {
	case 1:
		say("sb04", true);
		startGauge(1);
		break;
	case 2:
		if (_onStone) {
			remark("sb06");
			startGauge(2);
		}
		break;
	case 3:
		if (_onStone) {
			remark("sb05");
			startGauge(3);
		}
		break;
	case 4:
		if (_onStone) {
			remark("sb07");
			_glassesTaken = true;
			startGauge(5);
		}
		break;
	case 5:
		if (_onStone) {
			remark("sb08");
			startGauge(5);
		}
		break;
	case 7:
		if (holding) {
			remark("sb09");
			startGauge(7);
		} else if (!_nearMonet) {
			_nearMonet = true;
			stopGauge();
			if (_onStone)
				startGauge(3);
		}
		break;
	case 8:
		if (!_spaceSeen) {
			say("sb11", false);
			startGauge(8);
		}
		break;
	default:
		break;
	}
}

bool U00::input(float dt) {
	// u00.md, Input hook
	Player &player = _vm->player();
	const Keys &keys = _vm->keys();
	if (_state == 1 && (keys.up || keys.down)) {
		stopGauge();
		_moved = true;
	}
	if (_state == 2 && (keys.left || keys.right || keys.pageUp || keys.pageDown)) {
		stopGauge();
		_turned = true;
	}
	if (_onStone && _turned && !_glassesTaken && _state != 4)
		startGauge(4);
	if (keys.shift && _onStone) {
		// Jump off: a scripted move, not the jump of movement.md
		_onStone = false;
		const float p[3] = { 149.87f, 60.56f, -9.0f };
		_vm->moveTo(2000, p, 0.427f, (float)M_PI / 2);
		player.canMove = true;
		player.collide = true;
		player.groundObject.clear();
		if (_state == 2)
			stopGauge();
		return true;
	}
	if (keys.space) {
		_spaceSeen = true;
		if (_state == 8)
			stopGauge();
	}

	if (player.tick(dt, keys, *_vm->collision()))
		_vm->sound()->emit(Sound::kEffectsEmitter, "SAUT.WAV", player.eye, false);
	player.probeGround(*_vm->collision());

	if (!_onStone && onStone(player.groundObject)) {
		// Step onto the stone: parked on it, looking at *U04_43
		_onStone = true;
		player.eye = _vm->scene()->objectPosition("*U04_32") + Vector3d(0, 0, 10);
		player.canMove = false;
		player.collide = false;
		_vm->lookAt(0, _vm->scene()->objectPosition("*U04_43"));
		if (!_turned)
			startGauge(2);
	}
	if (_glassesTaken && !_spaceSeen && _state != 8 &&
	    (player.eye - _vm->scene()->objectPosition("*U04_03")).getMagnitude() < 2 * _vm->scene()->scale) {
		_vm->lookAt(1000, _vm->scene()->objectPosition("*U04_03"));
		_nearMonet = true;
		say("sb10", false);
		startGauge(8);
	}
	return true;
}

void U00::waitClip() {
	while (_vm->scene()->clipPlaying("*U04_03") && !_vm->shouldQuit())
		_vm->runFor(0);
}

bool U00::handle(const Common::String &action) {
	// u00.md, The glasses (E-0202)
	Scene *scene = _vm->scene();
	if (action.equalsIgnoreCase("TakeLunettes")) {
		stopGauge();
		startGauge(7);
		_glassesTaken = true;
		return true;
	}
	if (!action.equalsIgnoreCase("DonnerLunettes"))
		return false;

	_vm->interaction()->holdItem("");
	_vm->suspend(true);
	_spaceSeen = true;
	stopGauge();
	scene->playClip("*U04_03", "Anim/U04_03_Lunettes/prend.A3D");
	_vm->moveTo(1000, kStart, X3DEngine::kKeep, X3DEngine::kKeep, 90);
	waitClip();
	scene->hideObject("*U04_81", false);
	scene->playClip("*U04_03", "Anim/U04_03_Lunettes/MET.A3D");
	say("sb14", false);
	while (scene->clipPlaying("*U04_03") && scene->nodeFrame("*U04_03") < 50 && !_vm->shouldQuit())
		_vm->runFor(0);
	scene->hideObject("*U04_81", false);
	waitClip();
	_vm->runFor(2000);
	_vm->suspend(false);
	_vm->afterOptionMenu(_vm->optionMenu());
	return true;
}

} // End of namespace X3D
