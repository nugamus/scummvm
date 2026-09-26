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
#include "x3d/scene.h"
#include "x3d/sound.h"
#include "x3d/talk.h"
#include "x3d/monet/u33.h"
#include "x3d/x3d.h"

namespace X3D {

using Math::Vector3d;

static const float kHalfPi = (float)M_PI / 2;
static const float kKeep = X3DEngine::kKeep;
static const char *const kClown = "*U03_02";
static const char *const kDoor = "*U03_16";
static const char *const kCurtain = "*U03_18";
static const char *const kShutter = "*U03_15";

void U33::afterLoad() {
	// u33.md, Entry 1: before the hotspots
	Scene *scene = _vm->scene();
	for (int i = 0; i < 3; i++) {
		const Common::String from = Common::String::format("GeoSphere%d", i), to = Common::String::format("*U03_3%d", i);
		scene->renameObject(from, to);
		scene->renameNode(from, to);
	}
	scene->renameObject("U03_18", kCurtain);
	scene->renameNode("U03_18", kCurtain);
	scene->hideObject("$$$DUMMY.Dummycolis");
	scene->pauseNode("*U03_08");
	scene->setNodeFrame("*U03_08", 0);
	scene->renameObject("quille hau", "*U03_33");
	scene->renameObject("Box121", "*U03_22");
	scene->hideParent("*Ecran01");
	scene->renameObject("*u03_24", "*U03_24"); // the net effect of the four renames
	scene->renameObject("*U03_36f", "*U03_36");
	scene->renameObject("*U03_13 bo", "*U03_37");
	scene->renameObject("*U03_36cle", "*U03_36");
	scene->renameObject("pedalegch", "Zpedale");
	scene->renameObject("pedaledrt", "ZZpedale");
}

Vector3d U33::at(const char *object) const {
	return _vm->scene()->objectPosition(object);
}

float U33::facing(const Vector3d &target, const Vector3d &from) const {
	const Vector3d d = target - from;
	return atan2f(-d.y(), d.x());
}

Vector3d U33::ground(float x, float y, float z) {
	const Vector3d from(x, y, z);
	float t;
	if (!_vm->collision()->cast(from, from - Vector3d(0, 0, 10000), t))
		return from;
	return Vector3d(x, y, z - 10000 * t + _vm->player().eyeHeight());
}

void U33::walk(uint32 ms, const Vector3d &to, float yaw) {
	const float p[3] = { to.x(), to.y(), to.z() };
	_vm->moveTo(ms, p, yaw, kHalfPi);
}

void U33::walkG(uint32 ms, float x, float y, float z, float yaw) {
	walk(ms, ground(x, y, z), yaw);
}

void U33::run(uint32 id) {
	Common::StringArray actions;
	_vm->interaction()->runAction(id, actions);
	for (const Common::String &a : actions)
		_vm->addUnitAction(a);
}

void U33::effect(const char *name, const Vector3d &position) {
	_vm->sound()->emit(Sound::kEffectsEmitter, Common::Path(_vm->scene()->dir() + "Sound/" + name + ".wav"), position, false);
}

void U33::waitNode(const char *node, float frame) {
	Scene *scene = _vm->scene();
	while (scene->nodeRunning(node) && (frame < 0 || scene->nodeFrame(node) < frame) && !_vm->shouldQuit())
		_vm->runFor(0);
}

void U33::voiceWait(bool enterStops) {
	while (_vm->sound()->isGroupPlaying(Sound::kVoice) && !_vm->shouldQuit()) {
		if (enterStops && _vm->enterHeld()) {
			_vm->sound()->stopGroup(Sound::kVoice);
			break;
		}
		_vm->runFor(0);
	}
}

void U33::start(bool newGame, bool video) {
	// u33.md, Entry (E-0421)
	Scene *scene = _vm->scene();
	Collision *collision = _vm->collision();
	collision->setEnabled(kClown, false, true);
	collision->setEnabled("*Ernest", false, true);
	scene->hideObject("*Ernest");
	scene->hideObject("*U03_25");
	for (const char *n : { "*U03_30", "*U03_31", "*U03_32" })
		scene->pauseNode(n);
	_vm->talk()->addTalker("U03_01", "$$$DUMMY.*visage");
	_vm->talk()->addTalker("U03_02", "$$$DUMMY.visage");
	_vm->talk()->addTalker("U03_09", "$$$DUMMY.visage");
	if (newGame) {
		_vm->player().setSphere(38.5f, 19);
		const Vector3d p = ground(-132.19f, -463.89f, 70);
		const float eye[3] = { p.x(), p.y(), p.z() };
		_vm->setView(eye, -1.28f, kHalfPi);
		_vm->saveGameState(_vm->getAutosaveSlot(), "Automatic save", true);
	}
	_vm->sound()->play(Common::Path(scene->dir() + "Sound/s2_01.wav"), Sound::kAmbient, 85, true);
}

void U33::afterFrame() {
	// u33.md, Every frame (E-0422). ponytail: after rendering, not before
	Interaction *interaction = _vm->interaction();
	Sound *sound = _vm->sound();
	const bool voice = sound->isGroupPlaying(Sound::kVoice);
	if (interaction->exhausted(92) && !interaction->exhausted(58) && _vm->gaugeExpired()) {
		gameOverClown();
		return;
	}
	if (interaction->exhausted(5) && !interaction->exhausted(6)) {
		if (!_timer)
			_timer = _vm->logicMs();
		if (_vm->logicMs() - _timer > 25000 && !voice) {
			run(6); // the projectionist's nag
			_timer = 0;
		}
	}
	if (interaction->exhausted(8) && !interaction->exhausted(6))
		interaction->exhaust(6);
	if (interaction->exhausted(62) && !interaction->exhausted(64) && !interaction->exhausted(63) && !voice) {
		// The shared clock is not reset here: the nag's old start may still stand (E-0422)
		if (!_timer)
			_timer = _vm->logicMs();
		if (_vm->logicMs() - _timer > 7000)
			run(63);
	}
	if (interaction->exhausted(62) && interaction->exhausted(63) && !interaction->exhausted(64) && !voice)
		interaction->exhaust(64); // the search is over
	if (interaction->exhausted(85) && _vm->gaugeExpired()) {
		_vm->sound()->stopAll();
		_vm->gameOver(); // the bike left behind
	}
}

bool U33::beforeClick() {
	// Any click while the clown searches behind the curtain gives the player away (E-0423)
	Interaction *interaction = _vm->interaction();
	if (!interaction->exhausted(62) || interaction->exhausted(64))
		return false;
	caughtBehindCurtain();
	return true;
}

// Film leader frames shown during the walk to the screen (U33::CountdownCallback)
static const int kCountdown[16] = { 9, 8, 10, 7, 9, 6, 10, 5, 9, 4, 10, 3, 9, 2, 10, 1 };

void U33::walkToScreen() {
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	const float z0 = player.eye.z();
	walkG(6000, 699, 61, z0, 0);
	const Vector3d start = player.eye, to(825.89f, 60.1939f, 62);
	// Yaw to 0 the short way round (WalkPath)
	float yaw0 = fmod(player.yaw, 2 * (float)M_PI);
	if (yaw0 > M_PI)
		yaw0 -= 2 * (float)M_PI;
	else if (yaw0 < -M_PI)
		yaw0 += 2 * (float)M_PI;
	const float pitch0 = player.pitch;
	const uint n = MAX<uint>(1, 6000 * X3DEngine::kStepsPerSecond / 1000);
	const uint k = MAX<uint>(1, n / 15);
	for (uint i = 1; i <= n && !_vm->shouldQuit(); i++) {
		player.eye = start + (to - start) * ((float)i / n);
		player.yaw = yaw0 + (0 - yaw0) * i / n;
		player.pitch = pitch0 + (kHalfPi - pitch0) * i / n;
		if (i % k == 0) {
			const uint j = i / k + 1;
			if (j > 0 && j < 16) {
				scene->hideObject("Box186");
				scene->hideParent("*Ecran01");
				const int t = kCountdown[j];
				scene->hideObject(t == 10 ? Common::String("*Ecran10") : Common::String::format("*Ecran0%d", t), false);
			}
		}
		const uint32 now = _vm->logicMs();
		while (_vm->logicMs() == now && !_vm->shouldQuit())
			_vm->runFor(0);
	}
	scene->hideObject("Box186", false);
	const Vector3d back = ground(699, 61, z0);
	const float eye[3] = { back.x(), back.y(), back.z() };
	_vm->setView(eye, player.yaw, player.pitch);
	_vm->suspend(false);
}

void U33::afterFilm() {
	_vm->sound()->stopEmitter(Sound::kEffectsEmitter);
	const Vector3d p = ground(686, 90, 68);
	const float eye[3] = { p.x(), p.y(), p.z() };
	_vm->setView(eye, 2.16f, kHalfPi);
	_vm->runFor(0);
}

void U33::firstFilm() {
	// DoCinemaA (E-0425): the film with the projectionist's speech over it
	Scene *scene = _vm->scene();
	scene->hideObject("*U03_37", false);
	effect("s2_12", _vm->player().eye + Vector3d(0, 0, 3 * scene->scale));
	walkToScreen();
	scene->hideParent("*Ecran01");
	_vm->playVideo("U33_01", "", 11, true); // M11 over the film; Enter stops the voice
	afterFilm();
}

void U33::secondFilm() {
	// DoCinema (E-0425)
	Scene *scene = _vm->scene();
	_vm->interaction()->setCursorKind("*U03_10", 0);
	effect("s2_12", _vm->player().eye + Vector3d(0, 0, 3 * scene->scale));
	walkToScreen();
	scene->hideParent("*Ecran01");
	_vm->playVideo("U33_02", "", 0, true);
	afterFilm();
}

void U33::shutter() {
	// OuvreVolet: Ernest at the window, then the key (E-0426)
	Scene *scene = _vm->scene();
	_vm->suspend(true);
	_vm->interaction()->setCursorKind("*U03_09", 5);
	scene->setNodeLoop(kShutter, false);
	scene->runNodeTo(kShutter, -1, false);
	scene->hideObject("*fenetrero");
	scene->hideObject("*Ernest", false);
	walkG(4000, -412.59f, 203.73f, 70, 4.56f);
	scene->setClip("*U03_01", "Anim/U03_01/Assis.a3d"); // he sits
	scene->setNodeLoop("*U03_01", true);
	scene->setNodeFps("*U03_01", 15);
	scene->runNodeTo("*U03_01", -1, false);
	voiceWait(true);
	scene->setNodeLoop(kShutter, false);
	scene->setNodeFrame(kShutter, 10);
	scene->runNodeTo(kShutter, -1, true);
	run(98);
	waitNode(kShutter, -1);
	const float p[3] = { -450.2f, 214.38f, 68.82f };
	_vm->moveTo(800, p, 4.02f, kKeep);
	while (_vm->sound()->isGroupPlaying(Sound::kEffects) && !_vm->shouldQuit())
		_vm->runFor(0);
	scene->hideObject("*Ernest");
	scene->hideObject("*fenetrero", false);
	scene->hideObject("*U03_36", false);
	_vm->moveTo(1000, p, 6.3f, kKeep);
	_vm->suspend(false);
}

void U33::enterCaravan() {
	// EnterCaravan (E-0427)
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	player.setSphere(22.5f, 8.5f);
	walkG(2000, -248, 230, 200, 3.44f);
	scene->setNodeLoop(kDoor, false);
	scene->runNodeTo(kDoor, -1, false);
	player.setEyeHeight(58);
	run(97);
	waitNode(kDoor, 30);
	scene->setNodeFrame(kDoor, 60);
	const Vector3d p = ground(-358, 265, 100);
	scene->runNodeTo(kDoor, -1, true); // it closes behind the player
	walk(2000, p, facing(p, player.eye));
	_vm->interaction()->setRuns(2, 0); // inside
	_vm->interaction()->exhaust(42);
	player.runAllowed = player.jumpAllowed = false;
	_vm->suspend(false);
}

void U33::leaveCaravan() {
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	player.setSphere(38.5f, 19);
	player.setEyeHeight(67.5f);
	walkG(2000, -321, 252, 100, 0.32f);
	scene->setNodeLoop(kDoor, false);
	scene->runNodeTo(kDoor, -1, false);
	run(97);
	waitNode(kDoor, 30);
	scene->setNodeFrame(kDoor, 60);
	scene->runNodeTo(kDoor, -1, true);
	walkG(2000, -216, 215, 100, 0.08f);
	_vm->interaction()->setRuns(2, 1); // outside
	player.runAllowed = player.jumpAllowed = true; // movement.md Ctrl run and Shift jump
	_vm->suspend(false);
}

void U33::hide() {
	// CacheDerriereRideau (E-0428)
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	scene->setNodeLoop(kCurtain, false);
	scene->runNodeTo(kCurtain, -1, false);
	walkG(2000, -335, 260, 85, 4.56f);
	waitNode(kCurtain, 30);
	scene->pauseNode(kCurtain);
	walk(2000, ground(-362, 303, 60), player.yaw);
	walk(2000, player.eye, 7.28f);
	scene->runNodeTo(kCurtain, -1, true);
	waitNode(kCurtain, -1);
	_vm->stopGauge();
	run(62); // the clown comes in
	player.canMove = false;
	_vm->suspend(false);
}

void U33::unhide() {
	// SortRideau
	Scene *scene = _vm->scene();
	_vm->suspend(true);
	scene->setNodeLoop(kCurtain, false);
	scene->runNodeTo(kCurtain, -1, false);
	waitNode(kCurtain, 30);
	scene->pauseNode(kCurtain);
	walk(2000, ground(-335, 260, 85), _vm->player().yaw);
	_vm->player().canMove = true;
	_vm->suspend(false);
}

void U33::caughtBehindCurtain() {
	Scene *scene = _vm->scene();
	_vm->suspend(true);
	scene->setNodeLoop(kCurtain, false);
	scene->runNodeTo(kCurtain, -1, false);
	scene->setClip(kClown, "Anim/U03_02/choppe02.a3d"); // Choppe02
	scene->setNodeLoop(kClown, true);
	scene->setNodeFps(kClown, 15);
	scene->runNodeTo(kClown, -1, false);
	scene->hideObject(kClown, false);
	run(61);
	voiceWait(false);
	_vm->suspend(false);
	_vm->gameOver();
}

void U33::gameOverClown() {
	// DoGameOverClown1
	Scene *scene = _vm->scene();
	_vm->suspend(true);
	_vm->stopGauge();
	scene->setNodeLoop(kDoor, false);
	scene->runNodeTo(kDoor, -1, false);
	scene->setNodeLoop(kClown, false);
	scene->runNodeTo(kClown, -1, false);
	scene->hideObject(kClown, false);
	run(61);
	walkG(2000, -362, 263, 100, 6.56f);
	voiceWait(false);
	_vm->runFor(1000);
	_vm->suspend(false);
	_vm->gameOver();
}

void U33::talkProjectionist() {
	// DoParleProjectionniste: four lines, then nothing
	Interaction *interaction = _vm->interaction();
	if (interaction->exhausted(71)) {
		if (!interaction->exhausted(76))
			run(76);
		else if (!interaction->exhausted(77))
			run(77);
	} else if (!interaction->exhausted(78)) {
		run(78);
	} else if (!interaction->exhausted(79)) {
		run(79);
	}
}

void U33::policeman() {
	// AttenteFinFlicParle (E-0429)
	Player &player = _vm->player();
	_vm->suspend(true);
	const Vector3d p = ground(-489, -464, player.eye.z());
	walk(6000, p, facing(at("*U03_09"), p));
	_vm->runFor(27000);
	_vm->startGauge(15000);
	_vm->scene()->hideObject("*U03_35", false);
	_vm->suspend(false);
}

void U33::ride() {
	// TransitionVelo: on the bike to U04 (E-0430)
	Scene *scene = _vm->scene();
	Sound *sound = _vm->sound();
	_vm->suspend(true);
	_vm->stopGauge();
	scene->hideObject("*U03_23");
	scene->hideObject("*U03_24");
	scene->hideObject("*U03_25", false);
	scene->setNodeLoop("*U03_25", false);
	scene->setNodeFps("*U03_25", 15 * 0.5f + 2);
	scene->setNodeFrame("*U03_25", 50);
	scene->runNodeTo("*U03_25", -1, false);
	scene->hideObject("*selle");
	scene->hideObject("*guidon");
	sound->play(Common::Path(scene->dir() + "Sound/s2_24.wav"), 4, 85, true);
	// The camera follows the bike in each step (afterStep), so it is drawn in between
	// steps with the bike
	_roll = 0;
	_rho = 2;
	_riding = true;
	afterStep();
	bool second = false;
	while (scene->nodeFrame("*U03_25") < 260 && scene->nodeRunning("*U03_25") && !_vm->shouldQuit()) {
		if (!second && scene->nodeFrame("*U03_25") > 170) {
			second = true;
			sound->play(Common::Path(scene->dir() + "Sound/S2_23.wav"), 4, 90, true);
		}
		_vm->runFor(0);
	}
	_riding = false;
	scene->pauseNode("*U03_25");
	sound->stopGroup(4);
	_vm->fadeToBlack(2000);
	_vm->playVideo("RouGiv", "S6_1");
	sound->stopGroup(Sound::kVoice);
	_vm->suspend(false);
	_vm->gotoScene("U04.x3d");
}

void U33::afterStep() {
	if (!_riding)
		return;
	Player &player = _vm->player();
	_roll += _rho / X3DEngine::kStepsPerSecond;
	if (fabs(_roll) > 1) {
		_roll = CLIP(_roll, -1.0f, 1.0f);
		_rho = -_rho;
	}
	player.roll = _roll;
	player.eye = at("*U03_25") + Vector3d(0, 0, 33);
	const Vector3d d = at("*guidon") - at("*selle");
	player.yaw = atan2f(-d.y(), d.x());
	player.pitch = kHalfPi;
}

void U33::syncState(Common::Serializer &s) {
	// The original saves no chunk for this unit (save.md), so its clock restarted on every
	// load; kept here as the time since it started (save version 5)
	uint32 since = _timer ? _vm->logicMs() - _timer : 0;
	s.syncAsUint32LE(since, 5);
	if (s.isLoading())
		_timer = since ? _vm->logicMs() - since : 0;
}

bool U33::handle(const Common::String &action) {
	// u33.md, Clicks (E-0423)
	Scene *scene = _vm->scene();
	Interaction *interaction = _vm->interaction();
	if (action.equalsIgnoreCase("FinActionM01")) {
		// M44 names it; the original has no handler (u33.md)
	} else if (action.equalsIgnoreCase("FinTestM06")) {
		interaction->exhaust(6);
	} else if (action.equalsIgnoreCase("MaskBoiteAllu")) {
		scene->hideObject("*U03_14");
	} else if (action.equalsIgnoreCase("DoCinema")) {
		secondFilm();
	} else if (action.equalsIgnoreCase("OuvreVolet")) {
		shutter();
	} else if (action.equalsIgnoreCase("TransitionInterieurRoulotte")) {
		enterCaravan();
		_vm->runFor(3000, true);
		run(51);
		_vm->startGauge(20000); // 20 s to hide
	} else if (action.equalsIgnoreCase("OuvreTiroirFond")) {
		for (const char *n : { "*U03_17", "*U03_30", "*U03_31", "*U03_32" }) {
			scene->setNodeLoop(n, false);
			scene->runNodeTo(n, -1, false);
		}
	} else if (action.equalsIgnoreCase("OuvreMalle")) {
		scene->enableNode("*U03_21", true);
		scene->activateSlot("*U03_21", 0);
		scene->setNodeLoop("*U03_21", false);
		scene->runNodeTo("*U03_21", -1, false);
	} else if (action.equalsIgnoreCase("OuvreDiablotin")) {
		scene->setNodeLoop("*U03_20", false);
		scene->runNodeTo("*U03_20", -1, false);
	} else if (action.equalsIgnoreCase("CacheDerriereRideau")) {
		hide();
	} else if (action.equalsIgnoreCase("DoGameOverClown1")) {
		gameOverClown();
	} else if (action.equalsIgnoreCase("SortRideau")) {
		unhide();
	} else if (action.equalsIgnoreCase("PousseMalle")) {
		scene->setClip("*U03_21", "Anim/coffre.a3d"); // XXChoppe02, slot 1
		scene->setNodeLoop("*U03_21", false);
		scene->runNodeTo("*U03_21", -1, false);
		scene->hideObject("*U03_33", false);
		scene->hideObject("*U03_22", false);
	} else if (action.equalsIgnoreCase("OuvrePorte")) {
		// M02's run count says where the player is: 1 outside, 0 inside (E-0427)
		if (interaction->runs(2) != 0)
			enterCaravan();
		else if (interaction->exhausted(58))
			leaveCaravan();
	} else if (action.equalsIgnoreCase("DoParleProjectionniste")) {
		talkProjectionist();
	} else if (action.equalsIgnoreCase("DoCinemaA")) {
		firstFilm();
	} else if (action.equalsIgnoreCase("AttenteFinFlicParle")) {
		policeman();
	} else if (action.equalsIgnoreCase("TransitionVelo")) {
		ride();
	} else if (action.equalsIgnoreCase("OuvrePorteClown")) {
		if (!interaction->exhausted(36))
			run(3);
	} else {
		return false;
	}
	return true;
}

} // End of namespace X3D
