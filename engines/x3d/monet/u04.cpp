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
#include "x3d/x3d.h"
#include "x3d/monet/u00.h"
#include "x3d/monet/u04.h"

namespace X3D {

using Math::Vector3d;

static const float kKeep = X3DEngine::kKeep;
static const char *const kMonet = "*U04_03";
static const char *const kDoor = "*U04_02";
static const char *const kBoat = "*U04_32";
static const char *const kMazout = "MazoutRT"; // the runtime Mazout's node
static const char *const kErnest = "ErnestRT";

// Gauge times
static const uint32 kDoorMs = 12000;      // the studio door stays open
static const uint32 kPaintMs = 80000;     // Monet paints
static const uint32 kPaintAgainMs = 30000; // Monet paints again after a look at a painting
static const uint32 kPlusViteMs = 6000;   // until Monet calls for help
static const uint32 kSinkMs = 20000;      // the boat sinks without the cork

// Action ids
enum {
	kActionOpenDoor = 2,     // M02 MonetOpenDoor
	kActionEnterStudio = 3,  // M03 EntrerDansAtelier
	kActionUsePot = 5,       // M05 UsePot: the bees leave
	kActionUseTube = 7,      // M07 UseTube
	kActionPostcard = 17,    // M17 UseCartePostale: Mazout
	kActionKidnapping = 18,  // M18 the kidnapping
	kActionUngag = 20,       // M20 EnleverBaillon
	kActionBreakWindow = 24, // M24 BriserVitre
	kActionFishKey = 29,     // M29 fish the key from the boat
	kActionUseCork = 30,     // M30 UseBouchon
	kActionTakeGlove = 32,   // M32 take the glove
	kActionErnest = 40       // M40 Ernest locks the studio
};

void U04::afterLoad() {
	// Before the hotspots
	fixU04Names(_vm->scene());
}

// Monet's TETE: the first object of that name below his root
Vector3d U04::head() const {
	Scene::Model *m;
	uint p;
	if (!_vm->scene()->findObject(kMonet, m, p))
		return Vector3d();
	const Common::Array<O3DObject> &objects = m->file.objects;
	for (uint o = p + 1; o < objects.size(); o++)
		if (objects[o].name.equalsIgnoreCase("TETE"))
			return Vector3d(objects[o].world[12], objects[o].world[13], objects[o].world[14]);
	return at(kMonet);
}

// A hotspot's hide: invisible and out of collision
void U04::hide(const char *object) {
	_vm->scene()->hideObject(object);
	_vm->collision()->setEnabled(object, false);
}

void U04::say(const char *line) {
	_vm->talk()->say("U04_03", line);
}

void U04::monetClip(const char *file, bool loop, int slot) {
	Scene *scene = _vm->scene();
	scene->setClip(kMonet, Common::String("Anim/U04_03/") + file, "", slot);
	scene->setNodeLoop(kMonet, loop);
	scene->runNodeTo(kMonet, -1, false);
}

void U04::paintingActions(bool on) {
	// U04_DisablePaintingActions: M08..M15 and their paintings' cursors
	static const char *const hotspots[] = { "*U04_08", "*U04_09", "*U04_10", "*U04_11", "*U04_12", "*U04_13", "*U04_14", "*U04_16" };
	for (int i = 0; i < 8; i++) {
		_vm->interaction()->setCondition(8 + i, on ? "TRUE" : "FALSE");
		_vm->interaction()->setCursorKind(hotspots[i], on ? 2 : 0);
	}
}

void U04::studioEmitter() {
	Sound *sound = _vm->sound();
	sound->setEmitter(Sound::kUnitEmitter2, 5, 50 * _vm->scene()->scale);
	sound->emit(Sound::kUnitEmitter2, soundPath("s3_07"), Vector3d(165.82f, 332.92f, 15), true);
}

void U04::beeEmitter() {
	Sound *sound = _vm->sound();
	sound->setEmitter(Sound::kUnitEmitter1, 4, 50 * _vm->scene()->scale);
	sound->emit(Sound::kUnitEmitter1, soundPath("s3_05"), at("*U04_06"), true);
}

void U04::faceMap(bool gagged) {
	_vm->scene()->setObjectMap("TETE", gagged ? "teteBA.TGA" : "tete.TGA");
}

void U04::start(bool newGame, bool video) {
	Scene *scene = _vm->scene();
	Interaction *interaction = _vm->interaction();
	Player &player = _vm->player();
	_vm->sound()->play(Common::Path(scene->dir() + "Sound/s3_01.wav"), Sound::kAmbient, 85, true);
	disableU04Boxes(_vm->collision());
	player.setSphere(5, 0);
	scene->setPickWhenHidden("*U04_43", !interaction->exhausted(kActionUseCork));
	scene->setPickWhenHidden("*U04_52", !interaction->exhausted(38));
	if (interaction->exhausted(kActionEnterStudio) && !interaction->exhausted(kActionPostcard))
		studioEmitter();
	if (interaction->exhausted(kActionEnterStudio) && !interaction->exhausted(kActionUsePot))
		beeEmitter();
	if (_onBoat) {
		player.eye.z() = at(kBoat).z() + 10;
		scene->scale = 20;
	}
	_vm->talk()->addTalker("U04_03", "$$$DUMMY.*visage", "Anim/U04_03/");
	if (interaction->exhausted(kActionKidnapping) && !interaction->exhausted(kActionUngag))
		faceMap(true);
	if (_painted)
		scene->hideObject("pain_Sot01");
	if (interaction->exhausted(kActionKidnapping))
		scene->hideObject("kokliko01");
	if (!newGame)
		return;
	for (const char *item : { "U01_04P", "U03_06P" })
		if (!_vm->inventory()->has(item))
			_vm->inventory()->add(item);
	const float p[3] = { -33.566f, 352.43f, 15 };
	_vm->setView(p, 1.36f, kHalfPi);
	_vm->autosave();
}

void U04::syncState(Common::Serializer &s) {
	s.syncAsByte(_onBoat);
	byte faceSwapped = 0; // the original's flag, read by nothing
	s.syncAsByte(faceSwapped);
	s.syncAsByte(_painted);
	// The timer parked under the boat's (not saved by the original), as the engine's gauge
	uint32 elapsed = _vm->logicMs() - _savedGauge.start;
	s.syncAsUint32LE(_savedGauge.ms, 6);
	s.syncAsUint32LE(elapsed, 6);
	s.syncAsByte(_savedGauge.visible, 6);
	s.syncString(_savedGauge.label, 6);
	if (s.isLoading())
		_savedGauge.start = _vm->logicMs() - elapsed;
}

void U04::afterFrame() {
	Interaction *interaction = _vm->interaction();
	Player &player = _vm->player();
	if (_vm->gaugeOver()) {
		const Common::String label = _vm->gaugeLabel();
		if (label == "Door") {
			_vm->stopGauge();
			if (!interaction->exhausted(kActionEnterStudio)) {
				// The door closes on the player
				Scene *scene = _vm->scene();
				scene->setNodeFps(kMonet, 10);
				scene->runNodeTo(kMonet, -1, true);
				scene->runNodeTo(kDoor, -1, true);
				waitNode(kDoor, -1, true);
				player.canMove = true;
				interaction->setCondition(kActionOpenDoor, "TRUE");
			}
		} else if (label == "Paint") {
			// Waits until the player is in the studio
			if (!interaction->exhausted(kActionPostcard) && player.eye.x() > 180 && player.eye.y() > 275)
				finishPainting();
			else if (interaction->exhausted(kActionPostcard))
				_vm->stopGauge();
		} else if (label == "ecroule") {
			_vm->stopGauge();
			if (_onBoat) {
				boatSinks();
				return;
			}
		} else if (label == "PlusVite") {
			_vm->stopGauge();
			voiceAt("d3_20", player.eye - Vector3d(10 * _vm->scene()->scale, 0, 0));
		} else {
			_vm->stopGauge();
		}
	}
	if (interaction->exhausted(kActionPostcard) && !interaction->exhausted(kActionKidnapping) && player.eye.y() < 40)
		kidnapping();
	if (interaction->exhausted(kActionKidnapping) && !interaction->exhausted(kActionUngag) && !interaction->exhausted(kActionErnest) &&
	    (player.eye - Vector3d(213.47f, 296.4f, 15)).getMagnitude() < 2 * _vm->scene()->scale)
		ernest();
	// A name test in the original, unlike the window's
	if (!_onBoat && player.groundObject.equalsIgnoreCase(kBoat))
		stepOntoBoat();
}

bool U04::input(float dt) {
	Player &player = _vm->player();
	const Keys &keys = _vm->keys();
	const float s = _vm->scene()->scale;
	const float yaw = fmodf(fmodf(player.yaw, 2 * (float)M_PI) + 2 * (float)M_PI, 2 * (float)M_PI);
	if (!_onBoat) {
		if (_vm->interaction()->exhausted(kActionBreakWindow) && keys.shift &&
		    (player.eye - Vector3d(191, 240.8f, 15)).getMagnitude() < 0.5f * s && fabsf(yaw - 1.54f) < 1.5f) {
			climbOut();
			return true;
		}
		if (_vm->interaction()->exhausted(kActionTakeGlove) && _vm->interaction()->exhausted(39) && keys.up &&
		    player.eye.x() < -22 && player.eye.y() > 307 && fabsf(yaw - 4.9f) < 1.5f) {
			leave();
			return true;
		}
	} else {
		if (keys.up || keys.down) {
			row(keys.up);
			return true;
		}
		if (keys.shift) {
			Scene *scene = _vm->scene();
			const bool clip = scene->activeSlot(kBoat) != 0;
			const float frame = scene->nodeFrame(kBoat);
			if (frame < 60 || frame > scene->nodeLastFrame(kBoat) - 60) {
				jumpOffBoat();
				scene->setNodeFrame(kBoat, 1); // after the jump
				return true;
			}
			if (!clip) {
				jumpOffBoat();
				return true;
			}
			// Mid-pond with a clip: on to the window test and generic input
		}
	}
	// The Box70 object itself, a pointer test
	if (player.standsOn(*_vm->scene(), "Box70"))
		climbOut();
	return false;
}

void U04::openDoor() {
	// MonetOpenDoor
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	Interaction *interaction = _vm->interaction();
	_vm->suspend(true);
	player.canMove = false;
	const float p[3] = { 144.32f, 273.78f, 15 };
	_vm->moveTo(2500, p, 6.56f, kKeep);
	if (interaction->runs(kActionOpenDoor) == 1)
		scene->setClip(kMonet, "Anim/U04_03/ouvre01.A3D"); // OpenDoor
	else
		scene->activateSlot(kMonet, 1); // still active in the original; saves made before
		                                // the closed door stopped handing back need it
	scene->setNodeFps(kMonet, 15);
	scene->setNodeLoop(kMonet, false);
	scene->setNodeFrame(kMonet, 1);
	scene->runNodeTo(kMonet, -1, false);
	waitGroup(Sound::kEffects);
	scene->setNodeRange(kDoor, -1, 25);
	scene->runNodeTo(kDoor, -1, false);
	effect("s3_13", player.eye);
	say(interaction->runs(kActionOpenDoor) == 1 ? "d3_01" : "d3_02");
	_vm->sound()->setEmitterPosition(Sound::kVoiceEmitter, player.eye);
	waitVoice(false);
	_vm->startGauge(kDoorMs, false, "Door");
	interaction->setCondition(kActionOpenDoor, "FALSE");
	_vm->suspend(false);
}

void U04::enterStudio() {
	// EntrerDansAtelier
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	Interaction *interaction = _vm->interaction();
	Sound *sound = _vm->sound();
	_vm->suspend(true);
	interaction->setCursorKind(kDoor, 0);
	_vm->stopGauge();
	paintingActions(false);
	monetClip("ouvre02.A3D", false);
	say("d3_03");
	const float p1[3] = { 144.32f, 273.78f, 15 };
	_vm->moveTo(1000, p1, 6.56f, kKeep);
	while (scene->clipPlaying(kMonet) && !_vm->shouldQuit()) {
		sound->setEmitterPosition(Sound::kVoiceEmitter, head());
		_vm->runFor(0);
	}
	_vm->runFor(1000);
	monetClip("change.A3D", false);
	const float p2[3] = { 164.78f, 263.87f, 15 };
	_vm->moveTo(1500, p2, 6.0f, kKeep);
	scene->setClip(kDoor, "Anim/U04_02/ouvre02.A3D");
	scene->setNodeFrame(kDoor, scene->nodeLastFrame(kDoor)); // wide open
	_vm->moveTo(1000, nullptr, 5.2f, kKeep);
	studioEmitter();
	const float p3[3] = { 187.17f, 316.78f, 15 };
	const float p4[3] = { 188.4f, 325.57f, 15 };
	_vm->moveTo(9000, p3, 4.24f, kKeep);
	_vm->moveTo(3000, p4, 4.0f, kKeep);
	_vm->suspend(false);
	player.canMove = true;
	if (sound->isGroupPlaying(Sound::kVoice))
		sound->setEmitterPosition(Sound::kVoiceEmitter, at(kMonet));
	if (!interaction->exhausted(kActionUsePot))
		beeEmitter();
	interaction->setCondition(kActionUseTube, "FALSE");
	while (sound->isGroupPlaying(Sound::kVoice) && !interaction->exhausted(kActionUseTube) && !_vm->shouldQuit())
		_vm->runFor(0, true); // the player walks while Monet talks
	paintingActions(true);
	interaction->setCondition(kActionUseTube, "TRUE");
}

void U04::useTube() {
	// UseTube: Monet paints for 80 s
	Scene *scene = _vm->scene();
	_vm->startGauge(kPaintMs, false, "Paint");
	monetClip("Recharge.A3D", false, 2);
	waitClip(kMonet);
	scene->activateSlot(kMonet, 1);
	scene->runNodeTo(kMonet, -1, false);
	paintingActions(false);
	waitGroup(Sound::kVoice, true);
	paintingActions(true);
}

void U04::finishPainting() {
	// U04_MonetFinishesPainting
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	paintingActions(false);
	const bool far = player.eye.y() > 308;
	_vm->sound()->stopEmitter(Sound::kUnitEmitter2);
	_vm->lookAt(1000, head());
	if (!far) {
		monetClip("aller.A3D", false);
		scene->setNodeFps(kMonet, 15);
		waitClip(kMonet);
		say("d3_14");
		monetClip("parle.A3D", true, 2);
		const float p[3] = { 180, 306, 15 };
		_vm->moveTo(2000, p, kKeep, kHalfPi);
		_vm->lookAt(800, head());
	} else {
		say("d3_14");
		monetClip("JAIFINI.A3D", false);
	}
	waitVoice(true);
	if (!far) {
		scene->activateSlot(kMonet, 1);
		scene->setNodeFps(kMonet, 8);
		scene->runNodeTo(kMonet, -1, true);
	}
	const float p2[3] = { 183, 351, 15 };
	_vm->moveTo(3000, p2, 2.0f, kKeep);
	say("d3_17");
	waitVoice(true);
	monetClip(far ? "deplace02.A3D" : "deplacement.A3D", false);
	waitClip(kMonet, true);
	scene->hideObject("pain_Sot01");
	_painted = true;
	_vm->suspend(false);
	_vm->interaction()->setCursorKind(kMonet, 5);
	hide("*U04_50");
	monetClip("range.A3D", true);
	_vm->interaction()->setCondition(kActionPostcard, "TRUE");
	paintingActions(true);
	_vm->stopGauge();
}

void U04::painting() {
	// ScrollTableau: the clicked painting full screen (the action's id picks it)
	static const char *const paintings[] = { "U13_01", "U13_99", "U13_04", "U13_06", "U16_02", "U16_01", "U14_01", "U13_11", "U13_14" };
	const uint32 id = _vm->interaction()->lastRun();
	if (id >= 8 && id <= 16)
		_vm->showPainting(paintings[id - 8]);
	if (_vm->interaction()->exhausted(kActionUseTube) && !_painted)
		_vm->startGauge(kPaintAgainMs, false, "Paint"); // a look restarts Monet's timer
}

void U04::mazout() {
	// UseCartePostale: Mazout comes and runs off
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	Sound *sound = _vm->sound();
	_vm->suspend(true);
	paintingActions(false);
	const float p1[3] = { 201.19f, 300.77f, 15 };
	_vm->moveTo(4000, p1, 6.24f, kHalfPi);
	monetClip("replace.A3D", false);
	waitClip(kMonet);
	monetClip("parle02.A3D", true);
	_vm->runFor(0);
	_vm->lookAt(2000, head());
	waitVoice(true);
	sound->stopGroup(Sound::kVoice);

	Scene::Model *mazout = scene->addModel("Anim/U04_04/U04_04.o3d", "Anim/U04_04/invite.a3d", 15);
	if (mazout)
		scene->nameNodes(mazout, kMazout);
	_vm->talk()->addTalker("U04_04", "$$$DUMMY.*visage", "Anim/U04_04/");
	scene->runNodeTo(kMazout, -1, false);

	monetClip("ZIVAT.A3D", false);
	while (scene->nodeFrame(kMonet) < 50 && scene->clipPlaying(kMonet) && !_vm->shouldQuit()) {
		sound->setEmitterPosition(Sound::kEffectsEmitter, at(kMonet));
		_vm->lookAt(0, head());
	}
	scene->pauseNode(kMonet);
	auto beside = [&]() {
		Vector3d p = at(kMonet) + Vector3d(scene->scale, -scene->scale / 2, 0);
		float t;
		if (_vm->collision()->cast(p + Vector3d(0, 0, 100), p - Vector3d(0, 0, 10000), t))
			p.z() = p.z() + 100 - 10100 * t + player.eyeHeight();
		return p;
	};
	moveTo(800, beside(), 3.14f, kHalfPi);
	scene->runNodeTo(kMonet, -1, false);
	while (scene->nodeFrame(kMonet) < 300 && scene->clipPlaying(kMonet) && !_vm->shouldQuit()) {
		player.eye = beside();
		sound->setEmitterPosition(Sound::kEffectsEmitter, at(kMonet));
		_vm->runFor(0);
	}
	const float p3[3] = { 78.51f, 272.95f, 15 };
	_vm->moveTo(2000, p3, 3.4f, 1.5f);
	say("d3_18b");
	sound->stopGroup(Sound::kEffects);
	waitClip(kMonet);
	monetClip("Presente.A3D", true);
	waitVoice(false);
	_vm->runFor(200);
	scene->setNodeFps(kMonet, 10);

	const Vector3d q = player.eye - Vector3d(10, 0, 0);
	effect("ArbreCraque", q);
	_vm->runFor(1500);
	_vm->talk()->say("U04_04", "d3_19");
	const uint32 t0 = _vm->logicMs();
	while (sound->isGroupPlaying(Sound::kVoice) && _vm->logicMs() < t0 + 1000 && !_vm->shouldQuit())
		_vm->runFor(0);
	scene->setClip(kMazout, "Anim/U04_04/Course.A3D");
	scene->setNodeLoop(kMazout, false);
	scene->runNodeTo(kMazout, -1, false);
	bool steps = false;
	while (scene->clipPlaying(kMazout) && !_vm->shouldQuit()) {
		const float f = scene->nodeFrame(kMazout);
		if (!steps && f > 50) {
			steps = true;
			effect("s3_08_pas", q);
		}
		if (f <= 100 || !mazout)
			_vm->runFor(0);
		else
			_vm->lookAt(0, Vector3d(mazout->file.objects[0].world[12], mazout->file.objects[0].world[13], mazout->file.objects[0].world[14]));
	}
	const float p4[3] = { 75.9f, 261.43f, 15 };
	_vm->moveTo(1000, p4, 2.071f, kKeep);
	_vm->suspend(false);
	hide("*U04_04");
	scene->hideObject("*U04_44", false);
	_vm->collision()->setEnabled("*U04_44", true);
	if (mazout)
		scene->removeModel(mazout);
	monetClip("parle04.A3D", true);
	scene->setNodeFps(kMonet, 5);
}

void U04::kidnapping() {
	// U04_AuSecours
	Scene *scene = _vm->scene();
	_vm->interaction()->exhaust(kActionKidnapping);
	voiceAt("d3_21", _vm->player().eye - Vector3d(8 * scene->scale, 0, 0));
	monetClip("evanoui.A3D", true);
	hide("*U04_08");
	faceMap(true);
	scene->setClip(kDoor, "Anim/U04_02/SENVAT.A3D"); // paused at 1
	scene->setNodeFrame(kDoor, 1);
	_vm->startGauge(kPlusViteMs, false, "PlusVite");
	scene->hideObject("kokliko01");
}

void U04::ernest() {
	// U04_ErnestLocksStudio
	Scene *scene = _vm->scene();
	Interaction *interaction = _vm->interaction();
	_vm->suspend(true);
	interaction->exhaust(kActionErnest);
	_vm->lookAt(800, at(kMonet));
	voiceAt("d3_23", _vm->player().eye);
	const uint32 t0 = _vm->logicMs();
	Scene::Model *ernest = scene->addModel("Anim/Ernest/ERNST2.O3D", "Anim/Ernest/respect.A3D", 15);
	if (!ernest)
		warning("U04: Ernest's model is missing");
	else
		scene->nameNodes(ernest, kErnest);
	_vm->lookAt(1000, at(kDoor));
	while (_vm->sound()->isGroupPlaying(Sound::kVoice) && _vm->logicMs() - t0 < 28000 && !_vm->enterHeld() && !_vm->shouldQuit())
		_vm->runFor(0);
	if (ernest) {
		scene->setClip(kErnest, "Anim/Ernest/SENVAT.A3D");
		scene->setNodeLoop(kErnest, false);
		scene->runNodeTo(kErnest, -1, false);
	}
	scene->runNodeTo(kDoor, -1, false);
	waitNode(kDoor, 70);
	effect("s3_09a", _vm->player().eye);
	waitNode(kDoor);
	_vm->runFor(2000);
	if (ernest)
		scene->removeModel(ernest);
	scene->hideObject("*U04_36", false);
	_vm->collision()->setEnabled("*U04_36", true);
	interaction->setCursorKind(kDoor, 5);
	interaction->setCursorKind(kMonet, 2);
	interaction->setCondition(kActionUngag, "TRUE");
	_vm->sound()->stopGroup(Sound::kEffects);
	_vm->suspend(false);
	_vm->runFor(2000);
	effect("s3_09b", _vm->player().eye);
}

void U04::ungag() {
	// EnleverBaillon: Say(*U04_03) matches no talker, so d3_24 plays at the eye
	Interaction *interaction = _vm->interaction();
	faceMap(false);
	monetClip("reveil.A3D", false);
	voiceAt("d3_24", _vm->player().eye);
	interaction->setCursorKind(kMonet, 3);
	waitClip(kMonet);
	monetClip("parle03.A3D", true);
	interaction->setCursorKind("*U04_22", 2);
	interaction->setCursorKind("*U04_23", 4);
	waitGroup(Sound::kVoice, true);
	_vm->scene()->setNodeFps(kMonet, 6);
	interaction->setCursorKind(kMonet, 3);
}

void U04::climbOut() {
	// U04_ClimbOutOfWindow
	_vm->suspend(true);
	const float p1[3] = { 194, 246, 21 };
	const float p2[3] = { 186, 220.26f, 21 };
	_vm->moveTo(1000, p1, 1.707f, 1.57f);
	_vm->moveTo(1000, p2, kKeep, kKeep);
	_vm->player().ground(*_vm->collision());
	effect("s3_12", _vm->player().eye);
	_vm->suspend(false);
}

void U04::climbIn() {
	_vm->suspend(true);
	const float p1[3] = { 187.89f, 234, 24 };
	const float p2[3] = { 189.43f, 244, 24 };
	_vm->moveTo(1000, p1, 4.9f, 1.57f);
	_vm->moveTo(1000, p2, kKeep, kKeep);
	_vm->player().ground(*_vm->collision());
	effect("s3_12", _vm->player().eye);
	_vm->suspend(false);
}

void U04::stepOntoBoat() {
	// U04_StepOntoBoat
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	Interaction *interaction = _vm->interaction();
	_onBoat = true;
	scene->scale = 20;
	player.eye = at(kBoat) + Vector3d(0, 0, 10);
	player.canMove = false;
	player.collide = false;
	_vm->lookAt(0, at("*U04_43"));
	if (interaction->runs(kActionUseCork) == 0) {
		// No cork: the boat sinks in 20 s; a running gauge waits underneath
		_savedGauge = _vm->gaugeRunning() ? _vm->gauge() : X3DEngine::Gauge();
		_vm->startGauge(kSinkMs, true, "ecroule");
		effect("s3_15", player.eye);
	}
	interaction->setCursorKind("*U04_36", 5);
	interaction->setCondition(kActionFishKey, "TRUE"); // the key can be fished only from the boat
}

void U04::jumpOffBoat() {
	Player &player = _vm->player();
	Interaction *interaction = _vm->interaction();
	_vm->scene()->scale = 10;
	_onBoat = false;
	const float p[3] = { 149.87f, 60.56f, -9.0f };
	_vm->moveTo(2000, p, 0.427f, kHalfPi);
	player.canMove = true;
	player.collide = true;
	player.groundObject.clear();
	if (_vm->gaugeLabel() == "ecroule") {
		_vm->stopGauge();
		_vm->sound()->stopGroup(Sound::kEffects);
		if (_savedGauge.ms)
			_vm->setGauge(_savedGauge);
	}
	interaction->setCursorKind("*U04_36", 0);
	interaction->setCondition(kActionFishKey, "FALSE");
}

void U04::row(bool forward) {
	// U04_RowBoat: both oars follow the trajectory, one turns in a circle
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	Interaction *interaction = _vm->interaction();
	_vm->sound()->setEmitterPosition(Sound::kEffectsEmitter, player.eye);
	const bool right = interaction->exhausted(33) || interaction->exhausted(34);
	const bool left = interaction->exhausted(35) || interaction->exhausted(36);
	if (!right && !left)
		return;
	_vm->rideView();
	// The clip is reloaded only if slot 1 holds another (it is saved with the node)
	const Common::String path = right && left ? "Anim/BARKE_TRAJECTOIRE1.A3D" : "Anim/BARKE_TOURNEROND.A3D";
	if (!scene->clipPath(kBoat).equalsIgnoreCase(path)) {
		scene->setClip(kBoat, path);
		scene->setNodeLoop(kBoat, true);
		scene->setNodeFps(kBoat, 30);
		_vm->runFor(0);
	}
	const bool backward = right && left ? !forward : right ? !forward : forward;
	effect("s3_14", player.eye);
	scene->runNodeTo(kBoat, -1, backward);
	scene->pauseNode(kBoat);
	scene->stepNode(kBoat, 1.0f / X3DEngine::kStepsPerSecond);
	player.eye = at(kBoat) + Vector3d(0, 0, 10);
	const Vector3d d = at("*U04_43") - player.eye;
	const float len = d.getMagnitude();
	if (len == 0)
		return;
	float yaw = atan2f(-d.y(), d.x());
	const float pitch = acosf(CLIP(-d.z() / len, -1.0f, 1.0f));
	while (yaw < 0)
		yaw += 6.283f;
	// The original compares the raw angles, so crossing 0/2pi counts as a full turn and
	// rows stall for a 1-s turn there (twice going forward); the difference the short way
	// round removes that stall (a fix)
	float dYaw = player.yaw - yaw;
	dYaw -= 2 * (float)M_PI * floorf(dYaw / (2 * (float)M_PI) + 0.5f);
	if (fabsf(truncf(dYaw)) + fabsf(truncf(player.pitch - pitch)) >= 1) {
		_vm->moveTo(1000, nullptr, yaw, pitch);
	} else {
		player.yaw = yaw;
		player.pitch = pitch;
	}
}

void U04::boatSinks() {
	// U04_BoatSinks: game over
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	scene->setClip(kBoat, "Anim/BARKE_PLACEMENT_COULE1.A3D");
	scene->setNodeLoop(kBoat, false);
	scene->setNodeFps(kBoat, 30);
	scene->runNodeTo(kBoat, -1, false);
	effect("s3_15", player.eye);
	while (scene->clipPlaying(kBoat) && !_vm->shouldQuit()) {
		player.eye = at(kBoat) + Vector3d(0, 0, 10);
		_vm->runFor(0);
	}
	_vm->fadeToBlack(2000);
	_vm->suspend(false);
	_vm->gameOver();
}

void U04::useKey() {
	// UseCle: the door opens, Monet sees the player out
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	Interaction *interaction = _vm->interaction();
	_vm->suspend(true);
	interaction->setCursorKind(kMonet, 0);
	interaction->setCondition(43, "FALSE");
	const bool west = player.eye.x() <= 156;
	const float a[3] = { 135.25f, 280.78f, 15 };
	const float b[3] = { 169.9f, 269.81f, 15 };
	_vm->moveTo(1000, west ? a : b, west ? 0.5f : 3.64f, kKeep);
	effect("s3_18", player.eye);
	_vm->runFor(400);
	scene->activateSlot(kDoor, 1);
	scene->setNodeFps(kDoor, 15);
	scene->setNodeLoop(kDoor, false);
	scene->runNodeTo(kDoor, -1, true);
	_vm->runFor(1000);
	say("d3_31B");
	monetClip("RACOMPAGNE.A3D", false);
	while (scene->nodeFrame(kMonet) < scene->nodeLastFrame(kMonet) / 2 && scene->clipPlaying(kMonet) && !_vm->shouldQuit())
		_vm->runFor(0);
	_vm->lookAt(1000, head());
	while (scene->nodeFrame(kMonet) < scene->nodeLastFrame(kMonet) - 30 && scene->clipPlaying(kMonet) && !_vm->shouldQuit()) {
		_vm->sound()->setEmitterPosition(Sound::kVoiceEmitter, head());
		_vm->lookAt(0, head());
	}
	const float c[3] = { 80.5f, 259.2f, 15 };
	_vm->moveTo(5000, c, 5.042f, kKeep);
	if (!interaction->exhausted(kActionTakeGlove)) {
		// The glove is still there: Monet points it out and waits
		waitVoice(true);
		say("d3_31_3");
		monetClip("TROUVLEGANT.A3D", false);
		while (scene->clipPlaying(kMonet) && !_vm->enterHeld() && !_vm->shouldQuit())
			_vm->runFor(0);
		_vm->lookAt(1000, at("*U04_44"));
		player.canMove = player.canTurn = false;
		_vm->suspend(false);
		while (!interaction->exhausted(kActionTakeGlove) && !_vm->shouldQuit() && !_vm->sceneChanging())
			_vm->frameWithInput(); // Escape disallowed
		player.canMove = player.canTurn = true;
		say("d3_35");
	}
	monetClip("ASSOIT.A3D", false);
	waitClip(kMonet, true);
	monetClip("parle04.A3D", true);
	scene->setNodeFps(kMonet, 5);
	say("d3_35");
	_vm->suspend(false);
	_vm->runFor(1000);
}

void U04::leave() {
	// The exit to U05
	_vm->suspend(true);
	const float p[3] = { -28.948f, 358.7f, 15 };
	_vm->moveTo(2000, p, 4.6f, kHalfPi, 90);
	_vm->fadeToBlack(2000);
	_vm->playVideo("GivParis", "S6_1");
	_vm->sound()->stopGroup(Sound::kVoice);
	_vm->suspend(false);
	_vm->gotoScene("U05.x3d");
}

void U04::hintsAfterGag() {
	Interaction *interaction = _vm->interaction();
	if (interaction->exhausted(kActionFishKey) || interaction->exhausted(kActionBreakWindow))
		return;
	const int n = interaction->runs(41);
	say(n == 1 || (n > 2 && _random.getRandomNumber(1) == 1) ? "d3_25" : "d3_26");
}

void U04::hintsAfterHammer() {
	Interaction *interaction = _vm->interaction();
	const bool fished = interaction->exhausted(kActionFishKey);
	const int n = interaction->runs(42);
	const char *line;
	if (n == 1) {
		line = fished ? "d3_29" : "d3_28";
	} else if (n == 2) {
		line = "d3_29";
	} else if (n == 3) {
		line = fished ? "d3_29" : "d3_30";
	} else {
		const uint r = _random.getRandomNumber(2);
		line = fished || r == 2 ? "d3_29" : r == 1 ? "d3_28" : "d3_30";
	}
	say(line);
}

bool U04::handle(const Common::String &action) {
	Scene *scene = _vm->scene();
	Interaction *interaction = _vm->interaction();
	if (action.equalsIgnoreCase("MonetOpenDoor")) {
		openDoor();
	} else if (action.equalsIgnoreCase("EntrerDansAtelier")) {
		enterStudio();
	} else if (action.equalsIgnoreCase("UsePot")) {
		scene->enableNode("*U04_06", false);
		hide("*U04_06");
		scene->hideObject("*U04_53", false);
		_vm->sound()->stopEmitter(Sound::kUnitEmitter1);
		effect("s3_06", at("*U04_06"));
		interaction->setCursorKind("*U04_07", 4);
	} else if (action.equalsIgnoreCase("UseTube")) {
		useTube();
	} else if (action.equalsIgnoreCase("ScrollTableau")) {
		painting();
	} else if (action.equalsIgnoreCase("UseCartePostale")) {
		mazout();
	} else if (action.equalsIgnoreCase("EnleverBaillon")) {
		ungag();
	} else if (action.equalsIgnoreCase("BriserVitre")) {
		effect("Vitre", _vm->player().eye);
		scene->hideObject("*U04_41", false);
		waitGroup(Sound::kEffects);
		climbOut();
	} else if (action.equalsIgnoreCase("UseEchelle")) {
		scene->setPickWhenHidden("*U04_52", false);
		interaction->setCursorKind("*U04_63", 2);
	} else if (action.equalsIgnoreCase("MonterSurEchelle")) {
		if (_vm->player().eye.y() < 240)
			climbIn();
		else
			climbOut();
	} else if (action.equalsIgnoreCase("UseBouchon")) {
		// The cork: the boat no longer sinks
		scene->setPickWhenHidden("*U04_43", false);
		_vm->stopGauge();
		if (_savedGauge.ms)
			_vm->setGauge(_savedGauge);
		_savedGauge = X3DEngine::Gauge();
		_vm->sound()->stopGroup(Sound::kEffects);
	} else if (action.equalsIgnoreCase("UseCle")) {
		useKey();
	} else if (action.equalsIgnoreCase("MonetTalkAfterBaillon")) {
		hintsAfterGag();
	} else if (action.equalsIgnoreCase("MonetTalkAfterMarteau")) {
		hintsAfterHammer();
	} else if (action.equalsIgnoreCase("RameToGauche")) {
		interaction->setCursorKind("*U04_40", 0);
	} else if (action.equalsIgnoreCase("RameToDroite")) {
		interaction->setCursorKind("*U04_39", 0);
	} else {
		return false;
	}
	return true;
}

} // End of namespace X3D
