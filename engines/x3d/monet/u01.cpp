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
#include "common/system.h"
#include "common/textconsole.h"

#include "x3d/collision.h"
#include "x3d/interaction.h"
#include "x3d/inventory.h"
#include "x3d/renderer.h"
#include "x3d/scene.h"
#include "x3d/sound.h"
#include "x3d/talk.h"
#include "x3d/monet/u01.h"
#include "x3d/x3d.h"

namespace X3D {

using Math::Vector3d;

U01::U01(X3DEngine *vm) : _vm(vm), _random("x3d_u01") {
}

Vector3d U01::at(const char *object) {
	return _vm->scene()->objectPosition(object);
}

// The first object with the name, depth first under the named parent (the TETE look-ats)
Vector3d U01::objectUnder(const Common::String &parent, const Common::String &name) {
	Scene::Model *m;
	uint p;
	if (!_vm->scene()->findObject(parent, m, p))
		return Vector3d();
	const Common::Array<O3DObject> &objects = m->file.objects;
	for (uint o = p + 1; o < objects.size(); o++) {
		if (!objects[o].name.equalsIgnoreCase(name))
			continue;
		for (int a = objects[o].parent; a >= 0; a = objects[a].parent)
			if (a == (int)p)
				return Vector3d(objects[o].world[12], objects[o].world[13], objects[o].world[14]);
	}
	return Vector3d();
}

void U01::afterLoad() {
	// u01.md, Load-time fixes (E-0080)
	Scene *scene = _vm->scene();
	scene->renameObject("Box31", "*U01_21");
	scene->renameNode("Object07", "*U01_21");
	Scene::Model *m;
	uint o;
	for (int n = 1; scene->findObject("Box20", m, o); n++)
		m->file.objects[o].name = Common::String::format("Box20%d", n);
	scene->hideObject("Box203");
	scene->setPickable("Box203", false);
	scene->hideObject("Cylinder07");
	scene->renameObject("tige", "*U01_12");
	scene->renameObject("*U01_08P", "*U01_08");
}

void U01::waitVoice() {
	while (_vm->sound()->isGroupPlaying(Sound::kVoice) && !_vm->enterHeld() && !_vm->shouldQuit())
		_vm->runFor(0);
}

void U01::start(bool newGame, bool video) {
	// u01.md, Entry (E-0081)
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	Interaction *interaction = _vm->interaction();
	if (newGame && video)
		_vm->playVideo("Prologue");
	// Step 2 comes before the base start restores CAMERA, so a load keeps the saved
	// offset (20 on the roof); here the restore has already run
	if (!_vm->restoring())
		player.sphereOffset = 37.0f;
	_vm->talk()->addTalker("U01_01", "$$$DUMMY.*01SParle");
	_vm->talk()->addTalker("U01_02", "$$$DUMMY.*02SParle");

	_switchThrown = scene->nodeFrame("*U01_21") >= 2;
	// A restore on the train stands on it at once (E-0081)
	Scene::Model *m;
	uint o;
	if (_onTrain && scene->findObject("*U01_20", m, o)) {
		player.groundObject = "*U01_20";
		player.groundModel = m;
		player.groundIndex = o;
	}
	_vm->collision()->setEnabled("*Ernest", false, true);
	_vm->collision()->setEnabled("Box203", false);
	scene->setPickable("Tapiroug", false);
	_vm->sound()->play(Common::Path(scene->dir() + "Sound/U01.WAV"), Sound::kAmbient, 85, true);
	if (!newGame)
		return;

	// Development shortcut: dev_handover goes straight to the end of the entry
	if (ConfMan.getBool("dev_handover")) {
		const float handover[3] = { -466.36f, -452.495f, 30.48f };
		_vm->setView(handover, 4.7f, kHalfPi);
		if (!_vm->inventory()->has("U02_01P"))
			_vm->inventory()->add("U02_01P");
		scene->playClip("*U01_02", "Anim/U01_02/Action03.A3D");
		player.canMove = player.canTurn = false;
		interaction->setCursorKind("*U01_01", 3);
		return;
	}

	_vm->suspend(true);
	interaction->setCursorKind("*U01_02", 0);
	interaction->setCursorKind("*U01_01", 0);
	if (!_vm->inventory()->has("U02_01P"))
		_vm->inventory()->add("U02_01P"); // a banknote (ui.md)
	const float first[3] = { -258.44f, -508.20f, 29.546f };
	_vm->setView(first, 1.31f, 1.5707960f);
	_vm->runFor(1500);
	interaction->actionsEnabled = false;

	_vm->talk()->say("U01_01", "d1_01");
	_vm->lookAt(1000, objectUnder("*U01_01", "TETE"));
	const float p1[3] = { -405.67f, -494.31f, 29.5f }, p2[3] = { -468.4f, -481.3f, 29.5f };
	_vm->moveTo(2500, p1, 2.9f, X3DEngine::kKeep);
	_vm->moveTo(800, p2, 1.76f, X3DEngine::kKeep);
	waitVoice();
	_vm->runFor(1000);

	Common::StringArray ignored;
	interaction->runAction(1, ignored); // M01: the Marseillaise, the mayor's d1_02
	scene->startAnimation("*U01_03");
	_vm->lookAt(1000, objectUnder("*U01_02", "TETE"));
	_vm->runFor(400);
	const float fov = player.fov;
	const float p3[3] = { -466.36f, -452.495f, 30.48f };
	_vm->moveTo(1400, p3, 4.7f, X3DEngine::kKeep);
	_vm->moveTo(1800, p3, 4.7f, 1.4908f, 45);
	while (_vm->talk()->talking() && !_vm->enterHeld() && !_vm->shouldQuit())
		_vm->runFor(0);

	_vm->sound()->stopGroup(Sound::kEffects);
	_vm->sound()->stopGroup(Sound::kVoice);
	scene->pauseNode("*U01_03");
	_vm->moveTo(600, nullptr, X3DEngine::kKeep, kHalfPi, fov);
	scene->playClip("*U01_02", "Anim/U01_02/Action03.A3D"); // GiveCard
	player.canMove = player.canTurn = false;
	interaction->setCursorKind("*U01_01", 3);
	interaction->actionsEnabled = true;
	_vm->suspend(false);
}

void U01::openDoor() {
	Scene *scene = _vm->scene();
	scene->setNodeFps("*U01_07", 2.5f);
	scene->runNodeTo("*U01_07", 12, false);
	_vm->sound()->emit(Sound::kEffectsEmitter, Common::Path(scene->dir() + "Sound/OpenDoor.WAV"), at("*U01_07"), false);
}

void U01::closeDoor() {
	Scene *scene = _vm->scene();
	_vm->sound()->emit(Sound::kEffectsEmitter, Common::Path(scene->dir() + "Sound/CloseDoor.wav"), at("*U01_07"), false);
	scene->setNodeFps("*U01_07", 4);
	scene->runNodeTo("*U01_07", 0, true);
}

bool U01::handle(const Common::String &action) {
	// u01.md, Click handlers (E-0084..E-0086)
	Scene *scene = _vm->scene();
	Interaction *interaction = _vm->interaction();
	Sound *sound = _vm->sound();
	const Common::String soundDir = scene->dir() + "Sound/";

	if (action.equalsIgnoreCase("TakeCard")) {
		scene->rewindClip("*U01_02");
		while (scene->clipPlaying("*U01_02") && !_vm->shouldQuit())
			_vm->runFor(0);
		scene->backToBase("*U01_02");
		_vm->player().canMove = _vm->player().canTurn = true;
		interaction->setCursorKind("*U01_02", 3);
	} else if (action.equalsIgnoreCase("ClickMaire")) {
		const bool first = _firstMaire || _random.getRandomNumber(1) == 1;
		_firstMaire = false;
		_vm->talk()->say("U01_02", first ? "d1_04" : "d1_05");
	} else if (action.equalsIgnoreCase("OpenDoor")) {
		openDoor();
	} else if (action.equalsIgnoreCase("CloseDoor")) {
		closeDoor();
	} else if (action.equalsIgnoreCase("TakeCarteHorloge")) {
		interaction->setCursorKind("*U01_11", 2);
	} else if (action.equalsIgnoreCase("OpenBoitier")) {
		const bool open = scene->nodeFrame("*U01_13") >= 10;
		scene->runNodeTo("*U01_13", open ? 0 : 10, open);
	} else if (action.equalsIgnoreCase("BaisserManette")) {
		scene->runNodeTo("*U01_14", 10, false);
	} else if (action.equalsIgnoreCase("ClicTel")) {
		const int n = interaction->runs(10);
		if (n % 2 == 0) {
			// Hang up
			scene->runNodeTo("*U01_11", -1, true);
			while (scene->nodeRunning("*U01_11") && !_vm->shouldQuit())
				_vm->runFor(0, true); // the input hook runs during the waits (E-0085)
			sound->stopEmitter(Sound::kPhoneEmitter);
			sound->emit(Sound::kEffectsEmitter, Common::Path(soundDir + "TelGrisi.WAV"), at("*U01_11"), false);
		} else if (interaction->exhausted(16)) {
			interaction->setCursorKind("*U01_11", 0);
			interaction->setCondition(10, "FALSE");
			call();
			scene->runNodeTo("*U01_11", -1, false);
		} else {
			sound->emit(Sound::kEffectsEmitter, Common::Path(soundDir + "TelGrisi.WAV"), at("*U01_11"), false);
			while (sound->isGroupPlaying(Sound::kEffects) && !_vm->shouldQuit())
				_vm->runFor(0, true);
			sound->emit(Sound::kPhoneEmitter, Common::Path(soundDir + "TelGrisi2.wav"), at("*U01_11"), true);
			scene->runNodeTo("*U01_11", -1, false);
		}
		if (n == 1) {
			interaction->setCondition(4, "FALSE");
			interaction->setCondition(5, "FALSE");
			interaction->setCondition(6, "FALSE");
			interaction->setCursorKind("*U01_01", 0);
		}
	} else if (action.equalsIgnoreCase("OpenTiroir1") || action.equalsIgnoreCase("OpenTiroir2")) {
		const Common::String node = action.hasSuffix("1") ? "*U01_15" : "*U01_16";
		scene->setNodeRange(node, 0, 10);
		const float frame = scene->nodeFrame(node);
		if (frame >= 10) {
			sound->stopGroup(Sound::kEffects);
			sound->emit(Sound::kEffectsEmitter, Common::Path(soundDir + "s1_05.WAV"), _vm->player().eye, false);
			scene->runNodeTo(node, 0, true);
		} else {
			if (frame <= 1)
				sound->stopGroup(Sound::kEffects);
			sound->emit(Sound::kEffectsEmitter, Common::Path(soundDir + "s1_05.WAV"), _vm->player().eye, false);
			scene->runNodeTo(node, 10, false);
		}
	} else if (action.equalsIgnoreCase("MonterSurToit")) {
		climb();
	} else if (action.equalsIgnoreCase("DoInterrupteur")) {
		throwSwitch();
	} else {
		return false; // Light255 and EcouterConversation have no handler
	}
	return true;
}

void U01::afterClick(const Common::String &hotspot) {
	// A click on the booth *U01_09 from below once the ladder is up climbs again (E-0250)
	if (_vm->interaction()->exhausted(20) && hotspot.equalsIgnoreCase("*U01_09") && _vm->player().eye.z() < 100)
		climb();
}

void U01::call() {
	// The phone call (u01.md, ClicTel)
	Interaction *interaction = _vm->interaction();
	_vm->suspend(true);
	_vm->scene()->runNodeTo("*U01_11", -1, false);
	if (!interaction->exhausted(24)) {
		Common::StringArray ignored;
		interaction->runAction(24, ignored); // D1_10 at the phone
		interaction->setCondition(19, "TRUE");
		const float p1[3] = { 433.788f, -91.1468f, 24.7812f }, p2[3] = { 421.145f, -69.187f, 24.78f };
		_vm->moveTo(1500, p1, 2.84318f, 1.0708f);
		waitVoice();
		_vm->moveTo(1000, p2, 0.6431f, X3DEngine::kKeep);
		// CloseDoor at the eye first (E-0085)
		_vm->sound()->emit(Sound::kEffectsEmitter, Common::Path(_vm->scene()->dir() + "Sound/CloseDoor.wav"), _vm->player().eye, false);
		closeDoor();
		_vm->startGauge(20000); // the escape timer, visible
		interaction->setCursorKind("*U01_08", 4);
	}
	_vm->suspend(false);
}

void U01::climb() {
	// MonterSurToit (u01.md, E-0086)
	Player &player = _vm->player();
	_vm->suspend(true);
	_vm->stopGauge();
	_vm->scene()->hideObject("*U01_10", false);
	const float p1[3] = { 439.833f, -50.66f, 28.2052f };
	_vm->moveTo(1000, p1, X3DEngine::kKeep, kHalfPi);
	_vm->moveTo(800, nullptr, -0.45f, X3DEngine::kKeep);
	_vm->runFor(500);
	const Vector3d top(439.833f, -50.66f, 100.637f);
	const float d = (top - player.eye).getMagnitude();
	for (int i = 0; i < 5; i++) {
		const float p[3] = { 439.833f, -50.66f, player.eye.z() + d / 7 };
		_vm->moveTo(500, p, X3DEngine::kKeep, X3DEngine::kKeep);
		_vm->runFor(200);
	}
	const float p2[3] = { 469.932f, -40.2943f, 110.637f }, p3[3] = { 485.775f, -44.9f, 139 };
	_vm->moveTo(1500, p2, 7.2731f, X3DEngine::kKeep);
	_vm->moveTo(1200, p3, X3DEngine::kKeep, X3DEngine::kKeep);
	player.sphereOffset = 20;
	_vm->suspend(false);
}

void U01::throwSwitch() {
	// DoInterrupteur (u01.md, E-0086): two cuts to the points and back
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	scene->setNodeLoop("*U01_21", false);
	_vm->sound()->emit(Sound::kEffectsEmitter, Common::Path(scene->dir() + "Sound/s1_13.WAV"), player.eye, false);
	const float eye[3] = { player.eye.x(), player.eye.y(), player.eye.z() };
	const float yaw = player.yaw, pitch = player.pitch;
	const float a[3] = { 667, 723.8f, 94 }, b[3] = { 753.22f, 662.18f, 93.55f };
	if (_switchThrown) {
		scene->runNodeTo("*U01_21", 0, true);
		_switchThrown = false;
		_vm->setView(a, 6.02f, 0.79f);
		_vm->runFor(1200);
		_vm->setView(b, 6.26f, kHalfPi);
		_vm->runFor(1500);
	} else {
		_switchThrown = true;
		scene->runNodeTo("*U01_21", 100, false);
		_vm->runFor(600);
		_vm->setView(b, 6.26f, kHalfPi);
		_vm->runFor(1500);
		_vm->setView(a, 6.02f, 0.79f);
		_vm->runFor(1200);
	}
	_vm->setView(eye, yaw, pitch);
	_vm->runFor(0);
}

bool U01::input(float dt) {
	// The input hook: riding the train (u01.md, E-0083)
	Player &player = _vm->player();
	Collision &collision = *_vm->collision();
	Scene &scene = *_vm->scene();
	const Keys &keys = _vm->keys();
	// Collision is only written in train mode (E-0615)
	_onTrain = player.standsOn(scene, "*U01_20");
	if (_onTrain) {
		player.collide = false;
		if (keys.up)
			ride(dt);
		else
			player.tick(dt, keys, collision);
		player.probeGround(collision);
		if (!player.standsOn(scene, "*U01_20"))
			player.collide = true;
	}
	// The generic camera input runs in every case: on the train the keys act twice
	if (player.tick(dt, keys, collision))
		_vm->sound()->emit(Sound::kEffectsEmitter, "SAUT.WAV", player.eye, false);
	// The ground is read again after the generic input (E-0615)
	if (!keys.up && player.standsOn(scene, "*U01_20"))
		_vm->sound()->stopGroup(Sound::kEffects);
	return true;
}

void U01::ride(float dt) {
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	scene->stepNode("*U01_20", dt);

	// The camera rides behind the train, looking along it
	Vector3d p = at("*U01_20"), q = at("*U01_23");
	if (!_vm->sound()->isGroupPlaying(Sound::kEffects))
		_vm->sound()->emit(Sound::kEffectsEmitter, Common::Path(scene->dir() + "Sound/s1_12.WAV"), player.eye, true);
	else
		_vm->sound()->setEmitterPosition(Sound::kEffectsEmitter, player.eye);
	p.z() = q.z() + 18;
	q.z() += 10;
	Vector3d d = q - p;
	const float yaw = atan2f(-d.y(), d.x());
	d.normalize();
	player.eye.set(p.x() - 40 * d.x(), p.y() - 40 * d.y(), p.z() - 20 * d.z());
	player.yaw = yaw;
	player.pitch = kHalfPi;
	_vm->rideView();

	// The siding: with the points thrown the train leaves on U01_20A.A3D (E-0083, E-0087)
	const float frame = scene->nodeFrame("*U01_20");
	if (_train2Loaded) {
		if (frame >= scene->nodeLastFrame("*U01_20"))
			leave();
	} else if (_switchThrown && fabsf(floorf(frame) - 90) <= 1) {
		scene->loadClip("*U01_20", "Anim/U01_20A.A3D", 10, 15);
		_train2Loaded = true;
	}
}

void U01::leave() {
	// Leaving U01 for U02 (E-0087)
	Sound *sound = _vm->sound();
	_vm->suspend(true);
	_vm->fadeToBlack(2000);
	sound->emit(Sound::kEffectsEmitter, Common::Path(_vm->scene()->dir() + "Sound/s1_12.WAV"), _vm->player().eye, true);
	sound->stopGroup(Sound::kAmbient);
	for (float d = 10; d < 60 * _vm->scene()->scale && !_vm->shouldQuit(); d += 10) {
		sound->setEmitterPosition(Sound::kEffectsEmitter, _vm->player().eye + Vector3d(d, 0, 0));
		_vm->runFor(20);
	}
	_vm->suspend(false);
	_vm->gotoScene("U02.x3d");
}

void U01::afterFrame() {
	if (_vm->gaugeExpired())
		caught();
}

void U01::caught() {
	// The escape timer ran out (u01.md, E-0082)
	Scene *scene = _vm->scene();
	Sound *sound = _vm->sound();
	const Common::String soundDir = scene->dir() + "Sound/";
	_vm->suspend(true);
	sound->emit(Sound::kEffectsEmitter, Common::Path(soundDir + "S1_10.WAV"), _vm->player().eye, false);
	while (sound->isGroupPlaying(Sound::kEffects) && !_vm->shouldQuit())
		_vm->runFor(0);
	scene->hideObject("*Ernest", false);
	_vm->collision()->setEnabled("*Ernest", true, true);
	_vm->lookAt(100, at("*U01_07"));
	scene->setNodeFrame("*U01_07", 2);
	openDoor();
	sound->emit(Sound::kVoiceEmitter, Common::Path(soundDir + "s1_11.WAV"), _vm->player().eye, false);
	while (scene->nodeRunning("*U01_07") && !_vm->shouldQuit())
		_vm->runFor(0);
	const float p[3] = { 486.059f, -107.99f, 24 };
	_vm->moveTo(1000, p, 0.0831f, 1.95f);
	_vm->moveTo(2000, nullptr, 0.1631f, 2.19f, 35);
	_vm->fadeToBlack(2000);
	sound->stopAll();
	_vm->suspend(false);
	_vm->gameOver();
}

void U01::syncState(Common::Serializer &s) {
	s.syncAsByte(_train2Loaded);
	s.syncAsByte(_onTrain);
	s.syncAsByte(_firstMaire);
	// Versions up to 3 kept the escape timer here; it is the engine's gauge since
	byte oldGauge = 0;
	uint32 oldElapsed = 0;
	s.syncAsByte(oldGauge, 0, 3);
	s.syncAsUint32LE(oldElapsed, 0, 3);
	if (s.isLoading() && oldGauge) {
		_vm->startGauge(20000);
		X3DEngine::Gauge g = _vm->gauge();
		g.start = _vm->logicMs() - MIN<uint32>(oldElapsed, 20000);
		_vm->setGauge(g);
	}
}

} // End of namespace X3D
