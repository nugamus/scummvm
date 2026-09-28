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
#include "common/textconsole.h"

#include "x3d/collision.h"
#include "x3d/interaction.h"
#include "x3d/inventory.h"
#include "x3d/scene.h"
#include "x3d/sound.h"
#include "x3d/talk.h"
#include "x3d/monet/u02.h"
#include "x3d/x3d.h"

namespace X3D {

using Math::Vector3d;

static const float kKeep = X3DEngine::kKeep;
static const uint32 kGaugeMs = 180000;

// The magpie, the clerk, the seller, the inspector and the bell (u02.md, Cast)
static const char *const kMagpie = "*U02_05";
static const char *const kClerk = "*U02_04";
static const char *const kSeller = "*U02_03";
static const char *const kInspector = "*U02_02";
static const char *const kBell = "$Z$*U02_10";     // the whistle cord's node
static const char *const kBellObject = "*U02_10";  // its object after the name cut (E-0271)


void U02::afterLoad() {
	// u02.md, Load-time fixes (E-0160)
	Scene *scene = _vm->scene();
	_vm->sound()->play(Common::Path(scene->dir() + "Sound/s1_15.wav"), Sound::kAmbient, 85, true);
	scene->renameObject("*U02_01", "*U02_06");
	scene->renameObject("lourde05", "*U02_12");
	scene->renameNode("lourde05", "*U02_12");
	scene->renameObject("*U02_07", "*U02_07b");
	scene->renameObject("*U02_07", "*U02_07a");
	scene->renameObject("*U02_07b", "*U02_07");
	scene->renameObject("*ZonePlanc", "*U02_13");
	scene->renameObject("*colplanch", "*U02_14");
}

void U02::start(bool newGame, bool video) {
	// u02.md, Entry (E-0160)
	Talk *talk = _vm->talk();
	talk->addTalker("U02_02", "$$$DUMMY.*U02Parle");
	talk->addTalker("U02_03", "$$$DUMMY.*U_03Parle");
	talk->addTalker("U02_04", "$$$DUMMY.*visage");
	// The clerk asleep on a restore: his snore as a looping effect, not the snore
	// emitter, so the next effect cuts it (E-0618)
	if (_vm->interaction()->exhausted(8) && !_vm->interaction()->exhausted(12))
		_vm->sound()->emit(Sound::kEffectsEmitter, Common::Path(_vm->scene()->dir() + "Sound/d1_25.wav"), hotspotPosition("U02_04"), true);
	if (!newGame)
		return;
	const float p[3] = { 122.626f, 67.0605f, 52.529f };
	_vm->setView(p, 0.1f, kHalfPi);
	if (!_vm->inventory()->has("U02_01P"))
		_vm->inventory()->add("U02_01P");
	// The autosave, named after Message.txt line 1003 (E-0160)
	_vm->saveGameState(_vm->getAutosaveSlot(), "Automatic save", true);
}

// A hotspot's show: visible and back in collision (E-0272)
void U02::show(const char *object) {
	_vm->scene()->hideObject(object, false);
	_vm->collision()->setEnabled(object, true);
}

void U02::say(const char *character, const char *line) {
	_vm->talk()->say(character, line);
}

void U02::effect(const char *name, const Vector3d &position) {
	_vm->sound()->emit(Sound::kEffectsEmitter, Common::Path(_vm->scene()->dir() + "Sound/" + name + ".wav"), position, false);
}

Vector3d U02::hotspotPosition(const char *hotspot) {
	return _vm->scene()->objectPosition(_vm->interaction()->hotspotObject(hotspot));
}

void U02::resetCalls() {
	_callStart = _vm->logicMs();
}

void U02::waitClip(const char *node) {
	while (_vm->scene()->clipPlaying(node) && !_vm->shouldQuit())
		_vm->runFor(0);
}

void U02::waitGroup(int group, bool walk) {
	while (_vm->sound()->isGroupPlaying(group) && !_vm->shouldQuit())
		_vm->runFor(0, walk);
}

void U02::startSnore() {
	_vm->sound()->emit(Sound::kPhoneEmitter, Common::Path(_vm->scene()->dir() + "Sound/d1_25.wav"), hotspotPosition("U02_04"), true);
}

void U02::walkPath(uint32 ms, const Vector3d &target) {
	// WalkPath (E-0161): straight steps snapped to the ground, no collision
	Player &player = _vm->player();
	const uint n = MAX<uint>(1, ms * X3DEngine::kStepsPerSecond / 1000);
	const Vector3d step = (target - player.eye) * (1.0f / n);
	for (uint i = 0; i < n && !_vm->shouldQuit(); i++) {
		player.eye += step;
		player.ground(*_vm->collision());
		const uint32 t = _vm->logicMs();
		while (_vm->logicMs() == t && !_vm->shouldQuit())
			_vm->runFor(0);
	}
	player.eye = target;
}

// The view follows the node's object while its frame is below the given one
void U02::follow(const char *object, float untilFrame) {
	Scene *scene = _vm->scene();
	while (scene->nodeFrame(object) < untilFrame && scene->nodeRunning(object) && !_vm->shouldQuit())
		_vm->lookAt(0, scene->objectPosition(object));
}

bool U02::handle(const Common::String &action) {
	// u02.md, Click handlers (E-0162..E-0164)
	Scene *scene = _vm->scene();
	Interaction *interaction = _vm->interaction();
	if (action.equalsIgnoreCase("ClickControleur")) {
		const bool first = interaction->runs(1) == 1;
		if (first)
			interaction->setCursorKind(kInspector, 5);
		say("U02_02", first || _random.getRandomNumber(1) == 1 ? "d1_15" : "d1_16");
	} else if (action.equalsIgnoreCase("ClickGuichetier")) {
		const int n = interaction->runs(2);
		if (n == 1)
			interaction->setCursorKind(kClerk, 5);
		const bool first = n == 1 || (n > 2 && _random.getRandomNumber(1) == 1);
		say("U02_04", first ? "d1_17" : "d1_18");
		_callPeriod = 15000;
		resetCalls();
	} else if (action.equalsIgnoreCase("Donner100FrsAGuichetier")) {
		refuseCoin();
	} else if (action.equalsIgnoreCase("ReTake100Frs")) {
		retakeCoin();
	} else if (action.equalsIgnoreCase("ClickVendeuse")) {
		_callPeriod = 20000;
		if (interaction->exhausted(8) && !interaction->exhausted(12) && interaction->runs(20) > 0) {
			say("U02_03", _random.getRandomNumber(1) == 1 ? "d1_29" : "d1_30");
			resetCalls();
		}
	} else if (action.equalsIgnoreCase("AcheterMarron")) {
		buyChestnuts();
	} else if (action.equalsIgnoreCase("PieVoleur")) {
		magpieSteals();
	} else if (action.equalsIgnoreCase("ClicDoor")) {
		if (scene->nodeFrame("*U02_12") < 2)
			scene->runNodeTo("*U02_12", 50, false);
		else
			scene->runNodeTo("*U02_12", 1, true);
	} else if (action.equalsIgnoreCase("TakePlanche")) {
		// Only from the floor the plank lies on: the original compares the ground
		// object's name, and plncher01 is unique in U02 (E-0618)
		if (_vm->player().groundObject.equalsIgnoreCase("plncher01"))
			interaction->take("*U02_07");
	} else if (action.equalsIgnoreCase("PoserPlanche")) {
		show("*U02_07a");
		interaction->useUp("*U02_13");
		interaction->setCursorKind("*U02_13", 0);
		interaction->setCursorKind("*U02_07a", 0);
		_vm->collision()->setEnabled("*U02_14", true);
	} else if (action.equalsIgnoreCase("MarronToPie")) {
		feedMagpie();
	} else if (action.equalsIgnoreCase("ClickRonfle")) {
		const int n = interaction->runs(20);
		if (n == 1)
			interaction->setCursorKind(kSeller, 3);
		if (n == 3)
			interaction->setCursorKind(kClerk, 0);
		effect("d1_25_2", hotspotPosition("U02_04"));
		waitGroup(Sound::kEffects, true);
		startSnore();
	} else if (action.equalsIgnoreCase("Sonner")) {
		ringBell();
	} else if (action.equalsIgnoreCase("AcheterTicket")) {
		buyTicket();
	} else if (action.equalsIgnoreCase("TakeTicket")) {
		takeTicket();
	} else if (action.equalsIgnoreCase("MonterDansTrain")) {
		board();
	} else {
		return false;
	}
	return true;
}

void U02::refuseCoin() {
	// The refused coin (E-0162)
	Scene *scene = _vm->scene();
	Interaction *interaction = _vm->interaction();
	Player &player = _vm->player();
	_vm->suspend(true);
	scene->setClip(kClerk, "Anim/U02_04/RendreArgent.A3D");
	scene->runNodeTo(kClerk, -1, false);
	interaction->useUp("*U02_01");
	interaction->setCursorKind("*U02_01", 4);
	scene->hideObject("*U02_01", false);
	_vm->collision()->setEnabled("*U02_01", true);
	scene->setPickable("*U02_01", true);
	say("U02_04", "d1_19");
	resetCalls();
	interaction->setCursorKind(kClerk, 0);
	const float p[3] = { 2339.33f, -116.49f, 78 };
	_vm->moveTo(800, p, 1.62f, kKeep);
	player.canMove = player.canTurn = false;
	interaction->setCondition(16, "TRUE");
	_vm->suspend(false);
}

void U02::retakeCoin() {
	Scene *scene = _vm->scene();
	Interaction *interaction = _vm->interaction();
	interaction->setCondition(16, "FALSE");
	interaction->take("*U02_01");
	// The RendreArgent clip is still the clerk's active one: it plays back
	scene->runNodeTo(kClerk, -1, true);
	_vm->player().canMove = _vm->player().canTurn = true;
	interaction->setCursorKind(kClerk, 3);
	scene->setPickable("*U02_01", false);
}

void U02::buyChestnuts() {
	// AcheterMarron (E-0163)
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	_callPeriod = 30000;
	_callOff = true;
	player.canMove = player.canTurn = false;
	const float p[3] = { 1859.8f, 73.65f, 78.929f };
	_vm->moveTo(1000, p, 3.1f, kKeep);
	_vm->interaction()->setCursorKind("*U02_06", 4);
	scene->setClip(kSeller, "Anim/U02_03/ACTION01.A3D", kSeller);
	scene->setNodeFps(kSeller, 8);
	scene->runNodeTo(kSeller, -1, false);
	say("U02_03", "d1_22");
	resetCalls();
	waitClip(kSeller);
	_vm->interaction()->setCondition(6, "TRUE");
	_vm->suspend(false);
}

void U02::magpieSteals() {
	// PieVoleur: the seller gives the change, the magpie takes it (E-0163)
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	scene->setNodeFrame(kMagpie, 30);
	scene->hideObject(kMagpie, false);
	scene->enableNode(kMagpie, true);
	scene->setNodeFps(kMagpie, 40);
	scene->runNodeTo(kMagpie, -1, false);

	scene->setClip(kSeller, "Anim/U02_03/ACTION02.A3D", kSeller);
	scene->setNodeFps(kSeller, 50);
	scene->setNodeFrame(kSeller, 1);
	scene->runNodeTo(kSeller, -1, false);

	const float yaw = player.yaw, pitch = player.pitch;
	player.collide = false;
	bool said = false;
	while (scene->nodeFrame(kMagpie) <= 145 && scene->nodeRunning(kMagpie) && !_vm->shouldQuit()) {
		_vm->runFor(0);
		if (!said && scene->nodeFrame(kSeller) > 60) {
			said = true;
			say("U02_03", "d1_23_1");
		}
	}
	effect("S1_17", player.eye);
	say("U02_03", "d1_23_2");
	follow(kMagpie, 240);

	player.collide = true;
	_vm->moveTo(300, nullptr, yaw, pitch);
	resetCalls();
	scene->setNodeFps(kSeller, 80);
	waitClip(kSeller);
	scene->endClip(kSeller);
	player.canMove = player.canTurn = true;

	// The flight away waits, paused, until the player comes near (every frame, step 3)
	scene->setClip(kMagpie, "Anim/U02_05/Action02.A3D");
	scene->setNodeFps(kMagpie, 30);
	scene->setNodeFrame(kMagpie, 11);
	_magpie = 1;
	_callOff = false;
}

void U02::magpieFlies() {
	// The magpie waits for the player, then flies to the barrier (E-0161)
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	_magpie = 2;
	scene->runNodeTo(kMagpie, -1, false);
	_vm->lookAt(400, scene->objectPosition(kMagpie));
	const Vector3d d(cosf(player.yaw) * sinf(player.pitch), -sinf(player.yaw) * sinf(player.pitch), 0);
	walkPath(1000, player.eye + d * 150);
	effect("S1_17", player.eye);
	follow(kMagpie, 240);
	_vm->interaction()->setCursorKind(kMagpie, 5);
	_vm->suspend(false);
}

void U02::feedMagpie() {
	// MarronToPie: the magpie drops the coin, the gauge starts, the clerk dozes (E-0163)
	Scene *scene = _vm->scene();
	Interaction *interaction = _vm->interaction();
	Player &player = _vm->player();
	_callPeriod = 60000;
	_vm->suspend(true);
	interaction->useUp(kMagpie);

	show("*U02_06a");
	scene->runNodeTo("*U02_06a", -1, false);
	_vm->runFor(300);

	scene->setClip(kMagpie, "Anim/U02_05/ACTION03.A3D");
	scene->setNodeRange(kMagpie, 642, 830);
	scene->setNodeFrame(kMagpie, 642);
	scene->setNodeLoop(kMagpie, true);
	scene->setNodeFps(kMagpie, 20);
	scene->runNodeTo(kMagpie, -1, false);
	_vm->lookAt(500, scene->objectPosition(kMagpie));
	_vm->runFor(500);

	show("*U02_09a");
	scene->setNodeLoop("*U02_09a", false);
	scene->runNodeTo("*U02_09a", -1, false);

	// S'envoler at the node's own rate and loop (40 fps, not looping since PieVoleur)
	scene->setClip(kMagpie, "Anim/U02_05/ACTION04.A3D");
	scene->runNodeTo(kMagpie, -1, false);
	while (scene->nodeFrame(kMagpie) < 29 && scene->nodeRunning(kMagpie) && !_vm->shouldQuit())
		_vm->runFor(0);
	scene->hideObject("*U02_06a");
	_vm->collision()->setEnabled("*U02_06a", false);

	const float last = scene->nodeLastFrame(kMagpie) - 30;
	bool flapped = false;
	while (scene->nodeFrame(kMagpie) < last && scene->nodeRunning(kMagpie) && !_vm->shouldQuit()) {
		_vm->lookAt(0, scene->objectPosition(kMagpie));
		if (!flapped && scene->nodeFrame(kMagpie) > 80) {
			flapped = true;
			effect("s1_17", player.eye);
		}
	}

	resetCalls();
	_vm->suspend(false);
	const Vector3d voice = player.eye - Vector3d(0, 0, 5 * scene->scale); // before the wait
	_vm->runFor(2000, true);
	_vm->sound()->emit(Sound::kVoiceEmitter, Common::Path(scene->dir() + "Sound/d1_24.wav"), voice, false);
	_gauge = true;
	_gaugeStart = _vm->logicMs();
	waitGroup(Sound::kEffects, true);

	// The clerk falls asleep, breathing between two poses
	scene->setClip(kClerk, "Anim/U02_04/ACTION01.A3D");
	scene->setNodeRange(kClerk, 85, 86);
	scene->setNodeFrame(kClerk, 85);
	scene->setNodePingPong(kClerk, true);
	scene->setNodeFps(kClerk, 1);
	scene->runNodeTo(kClerk, -1, false);
	startSnore();
	interaction->setCursorKind(kClerk, 2);
	interaction->setCursorKind("*U02_10", 2);
}

void U02::ringBell() {
	// Sonner: the bell wakes the clerk (E-0164)
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	Sound *sound = _vm->sound();
	_vm->suspend(true);
	scene->runNodeTo(kBell, -1, false);
	_vm->interaction()->setCursorKind(kClerk, 5);

	const float eye[3] = { player.eye.x(), player.eye.y(), player.eye.z() };
	const float yaw = player.yaw, pitch = player.pitch, fov = player.fov;
	_vm->lookAt(500, scene->objectPosition(kBellObject));
	sound->stopEmitter(Sound::kPhoneEmitter);
	effect("SIREN", player.eye);
	_vm->moveTo(2000, nullptr, kKeep, kKeep, 60);
	while (scene->nodeRunning(kBell) && !_vm->shouldQuit())
		_vm->runFor(0);

	// He wakes, running on to the end he is heading for
	scene->setNodeRange(kClerk, -1, -1);
	scene->setNodePingPong(kClerk, false);
	scene->setNodeLoop(kClerk, false);
	scene->setNodeFps(kClerk, 80);

	player.fov = fov;
	const float counter[3] = { 2334.25f, -100, 78.9f };
	_vm->setView(counter, 1.54f, kHalfPi);
	_vm->moveTo(1000, nullptr, kKeep, kKeep, 60);
	waitClip(kClerk);

	scene->endClip(kClerk);
	say("U02_04", "D1_31");
	waitGroup(Sound::kVoice);

	_vm->setView(eye, yaw, pitch);
	resetCalls();
	_vm->interaction()->setCursorKind(kSeller, 0);
	_vm->suspend(false);
}

void U02::buyTicket() {
	// AcheterTicket (E-0164)
	Scene *scene = _vm->scene();
	_vm->suspend(true);
	const float p[3] = { 2362.52f, -93.43f, 78.9f };
	_vm->moveTo(1400, p, 1.94f, kHalfPi);
	say("U02_04", "d1_32");
	resetCalls();
	scene->setClip(kClerk, "Anim/U02_04/ACTION02.A3D");
	scene->setNodeFps(kClerk, 30);
	scene->setNodeFrame(kClerk, 15);
	scene->runNodeTo(kClerk, -1, false);
	_vm->interaction()->useUp(kClerk);
	_vm->interaction()->setCursorKind(kClerk, 0);
	while (scene->nodeFrame(kClerk) < 100 && scene->clipPlaying(kClerk) && !_vm->shouldQuit())
		_vm->runFor(0);
	effect("s1_20", _vm->player().eye);
	show("*U02_11");
	_vm->suspend(false);
}

void U02::takeTicket() {
	// TakeTicket: the clerk's hand goes back (RendreArgent backward from its end)
	Scene *scene = _vm->scene();
	scene->setClip(kClerk, "Anim/U02_04/RendreArgent.A3D");
	scene->setNodeFrame(kClerk, scene->nodeLastFrame(kClerk));
	scene->runNodeTo(kClerk, -1, true);
}

void U02::board() {
	// MonterDansTrain: leaving U02 for U03 (E-0164)
	Scene *scene = _vm->scene();
	Sound *sound = _vm->sound();
	_vm->suspend(true);
	_vm->interaction()->setCondition(1, "FALSE");
	_gauge = false;
	_vm->interaction()->useUp(kInspector);

	scene->setClip(kInspector, "Anim/U02_02/ACTION02.A3D");
	scene->setNodeFps(kInspector, 30);
	scene->runNodeTo(kInspector, -1, false);
	say("U02_02", "d1_33");
	resetCalls();
	while (scene->clipPlaying(kInspector) && !_vm->enterHeld() && !_vm->shouldQuit())
		_vm->runFor(0);

	// Into the carriage: two steps up
	const float p1[3] = { 1413.16f, 219.05f, 78.93f }, p2[3] = { 1413.16f, 219.05f, 83.93f };
	const float p3[3] = { 1413.16f, 219.05f, 88.93f }, p4[3] = { 1404, 293, 88.036f };
	_vm->moveTo(2000, p1, 4.55f, kKeep);
	_vm->moveTo(1000, p2, 4.55f, kKeep);
	_vm->runFor(200);
	_vm->moveTo(1000, p3, 4.55f, kKeep);
	_vm->runFor(200);
	_vm->moveTo(2000, p4, -1.62f, kKeep);

	effect("s1_22", _vm->player().eye);
	while ((sound->isGroupPlaying(Sound::kVoice) || sound->isGroupPlaying(Sound::kEffects)) && !_vm->shouldQuit())
		_vm->runFor(0);
	_vm->playVideo("LeHavRou", "S6_1");
	sound->stopGroup(Sound::kVoice);
	_vm->suspend(false);
	_vm->gotoScene("U03.x3d");
}

void U02::afterFrame() {
	// u02.md, Every frame (E-0161)
	Player &player = _vm->player();
	Sound *sound = _vm->sound();
	const uint32 now = _vm->logicMs();
	if (player.groundObject.equalsIgnoreCase("*U02_08")) {
		fall();
		return;
	}

	if (!_callOff && now - _callStart > _callPeriod && !sound->isGroupPlaying(Sound::kVoice)) {
		_callStart = now;
		const uint r = _random.getRandomNumber(2);
		say("U02_03", r == 1 ? "d1_12" : r == 2 ? "d1_13" : "d1_14");
	}

	if (_magpie == 1 && (player.eye - Vector3d(1280, -564, -7.15f)).getMagnitude() < 240)
		magpieFlies();

	if (!_gauge)
		return;
	const uint32 elapsed = now - _gaugeStart;
	// Said once per game session (u02.md, Unit state: process-wide, never reset)
	if (!_vm->u02Warned && elapsed > 160000 && elapsed < 170000) {
		_vm->u02Warned = true;
		say("U02_02", "d1_26");
	}
	if (elapsed >= kGaugeMs) {
		// The train leaves without the player
		_gauge = false;
		_vm->suspend(true);
		effect("s1_22", player.eye);
		say("U02_02", "d1_27");
		while ((sound->isGroupPlaying(Sound::kVoice) || sound->isGroupPlaying(Sound::kEffects)) && !_vm->shouldQuit())
			_vm->runFor(0);
		gameOver();
	}
}

void U02::fall() {
	// Falling into the stream (E-0161)
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	const float s = scene->scale;
	_vm->suspend(true);
	effect("s1_19", player.eye);

	// The light down in about a second: a float step, truncated after each (E-0275)
	const int n = X3DEngine::kStepsPerSecond;
	const float step[3] = { (scene->ambient[0] - 90) / (float)n, (scene->ambient[1] - 90) / (float)n, (scene->ambient[2] - 90) / (float)n };
	for (int i = 0; i < n && !_vm->shouldQuit(); i++) {
		for (int k = 0; k < 3; k++)
			scene->ambient[k] = (byte)CLIP((int)(scene->ambient[k] - step[k]), 0, 255);
		_vm->runFor(10);
	}

	const Vector3d d(cosf(player.yaw) * sinf(player.pitch), -sinf(player.yaw) * sinf(player.pitch), -cosf(player.pitch));
	const float p[3] = { player.eye.x() + s * d.x(), player.eye.y() + s * d.y(), player.eye.z() - (1.5f * s - 30) };
	const float drop = (Vector3d(p[0], p[1], p[2]) - player.eye).getMagnitude();
	player.collide = false;
	const float z0 = player.eye.z();
	const uint32 t0 = _vm->logicMs();
	for (;;) {
		const float t = (_vm->logicMs() - t0) / 1000.0f;
		const float z = 5.9f * s * t * t;
		if (z >= drop || _vm->shouldQuit())
			break;
		player.eye.z() = z0 - z;
		_vm->runFor(0);
	}
	player.eye.z() = z0 - drop;
	if (drop > 1.5f * s)
		_vm->sound()->emit(Sound::kEffectsEmitter, "SAUT.WAV", player.eye, false);

	_vm->moveTo(800, p, kKeep, 0.5f, 40);
	_vm->moveTo(600, nullptr, player.yaw + 3, kKeep);
	_vm->moveTo(600, nullptr, player.yaw + 3, kKeep);
	_vm->fadeToBlack(2000);
	gameOver();
}

void U02::gameOver() {
	_vm->suspend(false);
	_vm->player().collide = true;
	_vm->gameOver();
}

void U02::syncState(Common::Serializer &s) {
	// Times as ms since their start: logic time restarts with the scene (Q-0092)
	const uint32 now = _vm->logicMs();
	uint32 call = now - _callStart, gauge = now - _gaugeStart;
	s.syncAsUint32LE(call);
	s.syncAsUint32LE(_callPeriod);
	s.syncAsByte(_callOff);
	s.syncAsSint32LE(_magpie);
	s.syncAsByte(_gauge);
	s.syncAsUint32LE(gauge);
	if (s.isLoading()) {
		_callStart = now - call;
		_gaugeStart = now - gauge;
	}
}

void U02::draw() {
	if (_gauge)
		drawGauge(_vm->renderer(), MIN(1.0f, (_vm->logicMs() - _gaugeStart) / (float)kGaugeMs));
}

} // End of namespace X3D
