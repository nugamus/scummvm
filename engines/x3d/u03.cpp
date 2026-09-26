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
#include "x3d/inventory.h"
#include "x3d/sound.h"
#include "x3d/talk.h"
#include "x3d/u03.h"
#include "x3d/x3d.h"

namespace X3D {

using Math::Vector3d;

static const float kHalfPi = (float)M_PI / 2;
static const char *const kClown = "*U03_02";
static const char *const kFlic = "*U03_09";

void U03::afterLoad() {
	// u03.md, Entry 1-3: before the hotspots
	Scene *scene = _vm->scene();
	scene->hideObject("$$$DUMMY.Dummycolis");
	scene->renameObject("Box121", "*U03_22");
	scene->renameObject("*U03_18", "Quille tete"); // the juggler's pin: hovers as the clown
	scene->hideObjectOnly("*U03_03");               // the postcard in the juggler's hand
	scene->hideObject("pedaledrt");
}

Vector3d U03::at(const char *object) const {
	return _vm->scene()->objectPosition(object);
}

float U03::facing(const Vector3d &target, const Vector3d &from) const {
	const Vector3d d = target - from;
	return atan2f(-d.y(), d.x());
}

Vector3d U03::ground(float x, float y, float z) {
	// Cast down, then eye height h = 1.5 s above the ground (U03::SnapToGround)
	const Vector3d from(x, y, z);
	float t;
	if (!_vm->collision()->cast(from, from - Vector3d(0, 0, 10000), t))
		return from;
	return Vector3d(x, y, z - 10000 * t + 1.5f * _vm->scene()->scale);
}

void U03::run(uint32 id) {
	Common::StringArray actions;
	_vm->interaction()->runAction(id, actions);
	for (const Common::String &a : actions)
		_vm->addUnitAction(a);
}

void U03::waitVoice(bool enterSkips) {
	while (_vm->sound()->isGroupPlaying(Sound::kVoice) && !(enterSkips && _vm->enterHeld()) && !_vm->shouldQuit())
		_vm->runFor(0);
}

void U03::start(bool newGame, bool video) {
	// u03.md, Entry (E-0301)
	Scene *scene = _vm->scene();
	Interaction *interaction = _vm->interaction();
	Collision *collision = _vm->collision();
	Sound *sound = _vm->sound();
	collision->setEnabled("pedaledrt", false);
	_vm->player().setSphere(30.5f, 10.5f);
	scene->pauseNode("*U03_08");
	scene->setNodeFps(kFlic, 15);
	scene->setNodeLoop("*path", false);
	scene->setNodePingPong("*path", false);
	scene->setNodeFps("*path", 4);
	// The route's whole model: *path, its white guide plane 0000aaaaaa and their dummy
	// (u03.md Entry 7 hides *path; the plane cannot be visible either, Q-0144)
	Scene::Model *m;
	uint o;
	if (scene->findObject("*path", m, o)) {
		m->hidden = true;
		for (const O3DObject &obj : m->file.objects)
			collision->setEnabled(obj.name, false);
	}
	if (!interaction->exhausted(10))
		collision->setEnabled(kFlic, false, true); // he walks through the player
	else
		collision->setEnabled("ColClown", false, true);

	// The café and the clown's music: extra looping emitters on groups 5 and 6
	sound->setEmitter(Sound::kUnitEmitter1, 5, 1000);
	sound->emit(Sound::kUnitEmitter1, Common::Path(scene->dir() + "Sound/s2_03.wav"), at("*U03_01"), true);
	if (!interaction->exhausted(10)) {
		sound->setEmitter(Sound::kUnitEmitter2, 6, 1000);
		sound->emit(Sound::kUnitEmitter2, Common::Path(scene->dir() + "Sound/s2_04.wav"), at(kClown), true);
	} else {
		_follow = false;
	}
	_vm->talk()->addTalker("U03_01", "$$$DUMMY.*visage");
	_vm->talk()->addTalker("U03_02", "$$$DUMMY.visage");
	_vm->talk()->addTalker("U03_09", "$$$DUMMY.visage");

	if (newGame) {
		run(1); // the policeman's greeting, ignoring its condition
		const Vector3d p = ground(-132.19f, -463.89f, 70);
		const float eye[3] = { p.x(), p.y(), p.z() };
		_vm->setView(eye, -1.28f, kHalfPi);
		_vm->saveGameState(_vm->getAutosaveSlot(), "Automatic save", true);
	}
	if (!_vm->inventory()->has("U01_19P"))
		_vm->inventory()->add("U01_19P"); // the clock card from U01
	sound->play(Common::Path(scene->dir() + "Sound/s2_01.wav"), Sound::kAmbient, 85, true);
}

void U03::afterAnimate() {
	// The policeman walks the recorded route turned by 180 degrees, the walk cycle on top
	// (U03::FlicFollowPath, E-0302)
	Scene *scene = _vm->scene();
	if (!_follow || !scene->nodeRunning(kFlic))
		return;
	O3DObject *flic = scene->object(kFlic), *path = scene->object("*path");
	if (!flic || !path)
		return;
	// Rotation: L := L * Rz(pi) * path (row vectors, 3x3 parts)
	float r[9], out[9];
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++) {
			r[i * 3 + j] = 0;
			for (int k = 0; k < 3; k++) {
				const float rz = k == 2 ? path->matrix[k * 4 + j] : -path->matrix[k * 4 + j];
				r[i * 3 + j] += (i == k ? 1.0f : 0.0f) * rz;
			}
		}
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++) {
			out[i * 3 + j] = 0;
			for (int k = 0; k < 3; k++)
				out[i * 3 + j] += flic->matrix[i * 4 + k] * r[k * 3 + j];
		}
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			flic->matrix[i * 4 + j] = out[i * 3 + j];

	// Position: the policeman's origin (a top-level object) on the path's global
	// position (the last pose's: one logic step behind)
	float origin[3];
	for (int k = 0; k < 3; k++)
		origin[k] = path->world[12 + k];
	for (int k = 0; k < 3; k++) {
		flic->localPosition[k] = origin[k];
		for (int j = 0; j < 3; j++)
			flic->localPosition[k] += flic->pivot[j] * flic->localScale[j] * flic->matrix[j * 4 + k];
	}
}

void U03::clownTrick() {
	// AnimeSpeakClown (E-0303)
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	Interaction *interaction = _vm->interaction();
	Sound *sound = _vm->sound();
	_vm->suspend(true);
	_vm->collision()->setEnabled(kFlic, true, true);
	_vm->collision()->setEnabled("ColClown", false, true);
	_vm->talk()->say("U03_02", "U03_01_04");
	interaction->setCursorKind(kClown, 0);
	const float p1[3] = { -126, -581, player.eye.z() };
	_vm->moveTo(6000, p1, facing(at(kClown), Vector3d(p1[0], p1[1], p1[2])), kHalfPi);
	waitVoice();

	const float eye[3] = { player.eye.x(), player.eye.y(), player.eye.z() };
	const float yaw = player.yaw, pitch = player.pitch;
	const float p2[3] = { -118, -644, 71 };
	_vm->moveTo(3000, p2, 1.62f, 1.6f, 43);

	// The juggler gives way to the magician (U03::SwapClownModel)
	scene->hideObject(kClown);
	scene->renameNode(kClown, "ClownDeleted");
	scene->enableNode("ClownDeleted", false);
	scene->addModel("Anim/U03_02/U03_02.O3D", "Anim/U03_02/magie.A3D", 15);
	scene->setNodeFrame(kClown, 1);
	scene->pauseNode(kClown);
	scene->setNodeLoop(kClown, false);
	_vm->talk()->addTalker("U03_02", "$$$DUMMY.visage"); // the new model's face
	player.eye += Vector3d(3.5f, 0, 5);
	_vm->setView(nullptr, player.yaw, player.pitch);
	sound->stopEmitter(Sound::kUnitEmitter2);

	_vm->talk()->say("U03_02", "U03_01_04B");
	const uint32 t0 = _vm->logicMs();
	_vm->moveTo(3000, eye, yaw, pitch, 90);
	while (sound->isGroupPlaying(Sound::kVoice) && _vm->logicMs() < t0 + 9000 && !_vm->enterHeld() && !_vm->shouldQuit())
		_vm->runFor(0);
	scene->runNodeTo(kClown, -1, false);
	while (sound->isGroupPlaying(Sound::kVoice) && !_vm->shouldQuit()) {
		if (scene->nodeFrame(kClown) > 48 || _vm->enterHeld()) {
			// U03::GiveCartePostale. If the line ends first there is no card (Q-0141).
			scene->hideObjectOnly("*U03_03");
			if (!_vm->inventory()->has("U03_06P"))
				_vm->inventory()->add("U03_06P");
			break;
		}
		_vm->runFor(0);
	}

	// The policeman stops his round and waits by the clock (U03::FlicAttenteHorloge)
	scene->setClip(kFlic, "Anim/U03_09/PARLENBOUCLE.A3D"); // AttenteHorloge, slot 1
	scene->setNodeLoop(kFlic, true);
	scene->setNodeFps(kFlic, 20);
	scene->runNodeTo(kFlic, -1, false);
	_follow = false;
	interaction->setCursorKind(kFlic, 3);
	interaction->setCondition(20, "TRUE");

	while (scene->nodeRunning(kClown) && !_vm->enterHeld() && !_vm->shouldQuit())
		_vm->runFor(0);
	if (_vm->enterHeld())
		sound->stopEmitter(Sound::kVoiceEmitter);
	scene->pauseNode(kClown);
	if (sound->isGroupPlaying(Sound::kVoice))
		_vm->runFor(7000);

	// He walks off. ponytail: the 10-frame blend from magie into marche is a cut
	// (U03::AnimateTransition, rotation rule Q-0143)
	scene->setClip(kClown, "Anim/U03_02/marche.A3D");
	scene->setNodeFps(kClown, 15);
	scene->runNodeTo(kClown, -1, false);
	while (scene->nodeRunning(kClown) && !_vm->shouldQuit()) {
		scene->setNodeFrame(kClown, scene->nodeFrame(kClown) + 1);
		_vm->runFor(0);
	}
	scene->hideObject(kClown);
	_vm->collision()->setEnabled(kClown, false);
	_vm->suspend(false);
}

void U03::salute() {
	// FlicSalut (E-0304)
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	const Vector3d p = ground(-489, -464, player.eye.z());
	scene->setNodeLoop(kFlic, false);
	const float to[3] = { p.x(), p.y(), p.z() };
	_vm->moveTo(2000, to, facing(at(kFlic), p), kHalfPi);
	scene->setClip(kFlic, "Anim/U03_09/salut.A3D", "", 2);
	scene->setNodeLoop(kFlic, false);
	scene->setNodeFps(kFlic, 15);
	scene->runNodeTo(kFlic, -1, false);
	while (scene->nodeFrame(kFlic) < scene->nodeLastFrame(kFlic) - 10 && scene->nodeRunning(kFlic) && !_vm->shouldQuit())
		_vm->runFor(0);
	scene->activateSlot(kFlic, 1); // back to AttenteHorloge
	scene->setNodeLoop(kFlic, true);
	scene->runNodeTo(kFlic, -1, false);
	waitVoice(false);
	_vm->suspend(false);
}

void U03::aimAtTarget() {
	const Vector3d target = at("*Target");
	const Vector3d d = target - _vm->player().eye;
	const float len = d.getMagnitude();
	if (len == 0)
		return;
	_vm->player().yaw = atan2f(-d.y(), d.x());
	_vm->player().pitch = acosf(CLIP(-d.z() / len, -1.0f, 1.0f));
}

void U03::cut() {
	// The eye jumps to *camera, facing *Target (FUN_00419580)
	const Vector3d c = at("*camera");
	const float eye[3] = { c.x(), c.y(), c.z() };
	_vm->setView(eye, _vm->player().yaw, _vm->player().pitch);
	aimAtTarget();
	_vm->setView(nullptr, _vm->player().yaw, _vm->player().pitch);
}

void U03::cutscene() {
	// DoCinematiqueFlic (E-0305)
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	Interaction *interaction = _vm->interaction();
	_vm->suspend(true);
	interaction->setCondition(25, "FALSE");
	interaction->setCursorKind("*U03_08", 0);
	_vm->inventory()->hide();

	// The camera rig and the policeman at the door (U03::LoadCinematique)
	_camera = scene->addModel("cinematiques/Coordcam.o3d", "cinematiques/Coordcam.a3d", 15);
	_cine = scene->addModel("cinematiques/Cine01.o3d", "cinematiques/Cine01.a3d", 15);
	if (!_camera || !_cine) {
		warning("U03: the cutscene's models are missing");
		_vm->suspend(false);
		_vm->gotoScene("U33.X3D");
		return;
	}
	// Both roots are $$$DUMMY.Dummy01, as elsewhere in the scene: name their nodes
	const Common::String camNode = "Coordcam", cineNode = "Cine01";
	scene->nameNodes(_camera, camNode);
	scene->nameNodes(_cine, cineNode);
	for (const Common::String &n : { camNode, cineNode }) {
		scene->setNodeLoop(n, false);
		scene->setNodeFrame(n, 1);
		scene->pauseNode(n);
	}
	_camera->hidden = _cine->hidden = true;
	_vm->runFor(0);

	const Vector3d p = ground(-489, -464, player.eye.z());
	scene->setNodeLoop(kFlic, false);
	const float to[3] = { p.x(), p.y(), p.z() };
	_vm->moveTo(800, to, facing(at(kFlic), p), kHalfPi);
	run(31);

	scene->setClip(kFlic, "Anim/U03_09/REFLECTION.A3D", "", 3); // refelction
	scene->setNodeLoop(kFlic, false);
	scene->setNodeFps(kFlic, 15);
	scene->runNodeTo(kFlic, -1, false);
	while (scene->nodeFrame(kFlic) < scene->nodeLastFrame(kFlic) - 47 && scene->nodeRunning(kFlic) && !_vm->shouldQuit())
		_vm->runFor(0);
	scene->pauseNode(kFlic);
	_vm->moveTo(2000, nullptr, 3.17f, kHalfPi);
	_vm->runFor(4500);

	scene->setClip(kFlic, "Anim/U03_09/GOTOHORLOGE.A3D", "", 4); // vers horloger, paused
	scene->setNodeLoop(kFlic, false);
	scene->setNodeFps(kFlic, 15);
	bool pending = true;
	scene->hideObject(kFlic);

	// The walk to the clockmaker's, the view on *Target (the WalkPath callback)
	const Vector3d start = player.eye, target(-871.48f, -584.464f, at("*camera").z());
	const uint n = 10000 * X3DEngine::kStepsPerSecond / 1000;
	for (uint i = 1; i <= n && !_vm->shouldQuit(); i++) {
		player.eye = start + (target - start) * ((float)i / n);
		if (i == 8) {
			scene->hideObject(kFlic, false);
			scene->runNodeTo(kFlic, -1, false);
		}
		if (pending && scene->nodeFrame(kFlic) > scene->nodeLastFrame(kFlic) - 45) {
			run(33);
			pending = false;
		}
		aimAtTarget();
		const uint32 t = _vm->logicMs();
		while (_vm->logicMs() == t && !_vm->shouldQuit())
			_vm->runFor(0);
	}
	player.eye = target;
	if (pending)
		run(33);
	aimAtTarget();
	while (scene->clipPlaying(kFlic) && !_vm->shouldQuit())
		_vm->runFor(0);

	// The cinematic: cuts on the Coordcam frame, the FOV closing to 50
	_cine->hidden = false;
	scene->hideObject(kFlic);
	scene->runNodeTo(camNode, -1, false);
	scene->runNodeTo(cineNode, -1, false);
	scene->setNodeLoop("*U03_08", false);
	scene->runNodeTo("*U03_08", -1, false);
	const float fovStep = (50 - player.fov) * 0.05f;
	int k = 0;
	while (scene->nodeRunning(cineNode) && !_vm->shouldQuit()) {
		const float f = scene->nodeFrame(camNode);
		if ((f > 1 && k == 0) || (f > 132 && k == 2) || (f > 186 && k == 3) || (f > 278 && k == 5)) {
			cut();
			if (++k == 6)
				break;
		} else if (f > 240 && k == 4) {
			run(35);
			k = 5;
		} else if (f > 20 && k == 1) {
			k = 2;
		}
		_vm->runFor(0);
		if (player.fov > 50)
			player.fov += fovStep;
	}
	scene->pauseNode(camNode);
	scene->pauseNode(cineNode);
	_vm->runFor(7000);
	scene->runNodeTo(camNode, -1, false);
	scene->runNodeTo(cineNode, -1, false);
	while (scene->nodeFrame(camNode) < 420 && scene->nodeRunning(camNode) && !_vm->shouldQuit()) {
		_vm->runFor(0);
		if (scene->nodeFrame(camNode) > 360 && k == 6) {
			cut();
			k = 7;
		}
	}
	waitVoice(false);
	_vm->gotoScene("U33.X3D");
	scene->removeModel(_camera);
	scene->removeModel(_cine);
	_camera = _cine = nullptr;
	_vm->suspend(false);
}

bool U03::handle(const Common::String &action) {
	if (action.equalsIgnoreCase("AnimeSpeakClown"))
		clownTrick();
	else if (action.equalsIgnoreCase("FlicSalut"))
		salute();
	else if (action.equalsIgnoreCase("DoCinematiqueFlic"))
		cutscene();
	else
		return false;
	return true;
}

} // End of namespace X3D
