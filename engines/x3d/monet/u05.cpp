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
#include "x3d/x3d.h"
#include "x3d/monet/u05.h"

namespace X3D {

using Math::Vector3d;

static const float kKeep = X3DEngine::kKeep;
static const uint32 kGaugeMs = 15000;   // Mazout's gauge
static const uint32 kSitMs = 4000;      // the dog turns before it sits
static const uint32 kChatterMs = 20000; // between the station master's lines
static const float kNearChef = 220;     // the station master's talking distance

// Action ids
enum {
	kActionDogSits = 4,       // M04, exhausted by code
	kActionDogIdles = 5,      // M05, exhausted by code
	kActionNotice = 6,        // M06: the station master notices the dog
	kActionGrate = 11,        // M11 MaskGrille
	kActionAmbush = 12,       // M12, exhausted by code: Mazout's ambush
	kActionMazoutSpeaks = 13, // M13: Mazout's line, the gauge
	kActionLamp = 14,         // M14, exhausted by LampeTombe
	kActionCall = 23,         // M23: the station master calls the player
	kActionParcel = 24        // M24 AfficheCariolle: the parcel given
};

static const char *const kDog = "*U05_05";
static const char *const kMazout = "*U04_04";
static const char *const kChef = "*U05_02";

void U05::afterLoad() {
	// Before the hotspots, which name *Fil
	Scene *scene = _vm->scene();
	scene->renameObject("fil", "*Fil");
	scene->renameNode("fil", "*Fil");
}

bool U05::near(const char *object, float distance) const {
	return (_vm->player().eye - at(object)).getMagnitude() <= distance;
}

// The object, its children and its siblings leave collision (Object_SetNoCollisionTree)
void U05::noCollisionFamily(const Common::String &object) {
	Collision *collision = _vm->collision();
	collision->setEnabled(object, false, true);
	for (const Common::String &s : _vm->scene()->siblings(object))
		collision->setEnabled(s, false, true);
}

void U05::start(bool newGame, bool video) {
	Scene *scene = _vm->scene();
	Interaction *interaction = _vm->interaction();
	Collision *collision = _vm->collision();
	noCollisionFamily("*Fil");
	for (const char *name : { "battant02", "battant", "col103" })
		collision->setEnabled(name, false);
	_vm->player().setSphere(32, 0);
	noCollisionFamily(interaction->hotspotObject("U05_05"));
	noCollisionFamily(interaction->hotspotObject("U04_04"));
	Scene::Model *m;
	uint o;
	if (scene->findObject("Box17", m, o))
		m->unpickable[o] = true;
	_vm->talk()->addTalker("U05_02", "$$$DUMMY.*visage");
	_vm->talk()->addTalker("U04_04", "$$$DUMMY.*visage");
	if (interaction->exhausted(9))
		collision->setEnabled("ColPorte2", false);
	if (interaction->exhausted(kActionGrate))
		collision->setEnabled("ColPorte1", false);

	if (newGame) {
		scene->setClip(kDog, "Anim/U05_05/Attente.A3D"); // ChienTourne, paused at 245
		scene->setNodeLoop(kDog, true);
		scene->setNodeFps(kDog, 15);
		scene->setNodeFrame(kDog, 245);
		scene->setClip(kMazout, "Anim/U04_04/Attente.A3D"); // MazoutAttente
		scene->setNodeLoop(kMazout, true);
		scene->setNodeFps(kMazout, 15);
		scene->runNodeTo(kMazout, -1, false);
		const float p[3] = { -47.6f, 77.27f, 45.89f };
		_vm->setView(p, -6.31f, kHalfPi);
		_vm->autosave();
	}
	_vm->sound()->play(Common::Path(scene->dir() + "Sound/s4_01a.wav"), Sound::kAmbient, 85, true);
}

bool U05::barkAndSit() {
	Scene *scene = _vm->scene();
	scene->setNodeLoop(kDog, false);
	if (scene->nodeRunning(kDog))
		return false; // ChienTourne finishes its cycle first
	run(82);
	scene->setClip(kDog, "Anim/U05_05/ACTION01.A3D"); // ChienAttente
	scene->setNodeFps(kDog, 15);
	scene->runNodeTo(kDog, -1, false);
	return true;
}

bool U05::waitLoop() {
	Scene *scene = _vm->scene();
	scene->setNodeLoop(kDog, false);
	if (scene->nodeRunning(kDog))
		return false;
	scene->setClip(kDog, "Anim/U05_05/ATTENTE02.A3D"); // Attente2
	scene->setNodeLoop(kDog, true);
	scene->setNodeFps(kDog, 15);
	scene->runNodeTo(kDog, -1, false);
	return true;
}

bool U05::chefNotices() {
	if (_vm->sound()->isGroupPlaying(Sound::kVoice) || !near(kChef, kNearChef))
		return false;
	run(kActionNotice);
	_vm->interaction()->setCondition(7, "TRUE");
	_vm->interaction()->setCondition(9, "TRUE");
	return true;
}

bool U05::mazoutConfronts() {
	Player &player = _vm->player();
	Scene *scene = _vm->scene();
	if (player.eye.x() >= 1425)
		return false;
	_vm->suspend(true);
	const Vector3d p(1306.35f, -945.6f, player.eye.z());
	moveTo(4000, p, facing(at(kMazout), p), kHalfPi);
	scene->setClip(kMazout, "Anim/U04_04/Action01.A3D"); // MazoutA01
	scene->setNodeFps(kMazout, 15);
	scene->runNodeTo(kMazout, -1, false);
	run(kActionMazoutSpeaks);
	waitVoice(true);
	_vm->sound()->stopEmitter(Sound::kVoiceEmitter);
	_vm->startGauge(kGaugeMs); // saved with the game (JAUGE)
	_timer = 0;
	_vm->suspend(false);
	return true;
}

void U05::gaugeExpired() {
	// Mazout catches the player: only he stays visible
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->stopGauge();
	_vm->suspend(true);
	scene->setClip(kMazout, "Anim/U04_04/Action02.A3D"); // MazoutA03
	scene->setNodeFps(kMazout, 15);
	scene->runNodeTo(kMazout, -1, false);
	const Common::String mazout = _vm->interaction()->hotspotObject("U04_04");
	scene->hideAll();
	scene->hideObject(mazout, false);
	const float p[3] = { 1310, -986, player.eye.z() };
	_vm->setView(p, player.yaw, player.pitch);
	_vm->lookAt(0, at(kMazout) + Vector3d(0, 0, 30));
	effect("s4_04", player.eye);
	waitClip(kMazout);
	_vm->fadeToBlack(2000);
	_vm->suspend(false);
	_vm->gameOver();
}

void U05::leave() {
	Player &player = _vm->player();
	_vm->suspend(true);
	const Vector3d p(1096, 1173, player.eye.z());
	const Vector3d cart(1266, 1302, at("*U05_13").z());
	moveTo(7000, p, facing(cart, p), kHalfPi);
	_leaving = true;
	_vm->fadeToBlack(2000);
	_vm->suspend(false);
	_vm->gotoScene("U06.x3d");
}

void U05::afterFrame() {
	// The original runs these checks just before rendering, the engine just after: one
	// frame later.
	Interaction *interaction = _vm->interaction();
	Player &player = _vm->player();
	Sound *sound = _vm->sound();
	const uint32 now = _vm->logicMs();

	if (interaction->exhausted(1) && !interaction->exhausted(kActionNotice)) {
		if (!_timer)
			_timer = now;
		if (!interaction->exhausted(kActionDogSits)) {
			if (now - _timer > kSitMs && barkAndSit())
				interaction->exhaust(kActionDogSits);
		} else if (!interaction->exhausted(kActionDogIdles)) {
			if (waitLoop())
				interaction->exhaust(kActionDogIdles);
		} else if (chefNotices()) {
			run(kActionNotice); // a second time in the same frame: Say restarts the line
			if (!_vm->inventory()->has("U04_44P"))
				_vm->inventory()->add("U04_44P");
		}
	}
	if (interaction->exhausted(kActionGrate) && !interaction->exhausted(kActionAmbush) && mazoutConfronts())
		interaction->exhaust(kActionAmbush);
	if (interaction->exhausted(kActionMazoutSpeaks) && !interaction->exhausted(kActionLamp) && _vm->gaugeExpired()) {
		gaugeExpired();
		return;
	}
	if (interaction->exhausted(19) && !interaction->exhausted(kActionCall) && near(kChef, kNearChef))
		run(kActionCall);
	if (interaction->exhausted(kActionParcel) && !interaction->exhausted(25)) {
		if (!_timer)
			_timer = now;
		if (!sound->isGroupPlaying(Sound::kVoice) && now - _timer >= kChatterMs && near(kChef, kNearChef)) {
			_timer = now;
			run(_random.getRandomNumber(1) == 1 ? 26 : 27);
		}
	}
	if (interaction->exhausted(kActionParcel) && !_leaving && player.eye.y() > 928)
		leave();
	// The barks (M10) never start: nothing runs or exhausts M10
}

void U05::testSpeakChef() {
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	const Vector3d p(1493, -532, player.eye.z());
	moveTo(10000, p, facing(at(kChef), p), 1.45f);
	waitVoice(false);
	_vm->suspend(false);
	scene->runNodeTo(kDog, -1, false); // ChienTourne turns
	scene->hideObject(kDog, false);
}

void U05::dogToDoor() {
	// ChienVersPorte
	Scene *scene = _vm->scene();
	Interaction *interaction = _vm->interaction();
	_vm->suspend(true);
	interaction->setCondition(7, "FALSE");
	interaction->setCursorKind(kChef, 0);
	interaction->setCursorKind(kDog, 0);
	scene->setClip(kDog, "Anim/U05_05/ACTION03.A3D"); // Action3
	scene->setNodeFps(kDog, 15);
	scene->runNodeTo(kDog, -1, false);
	voiceAt("U05_06A", _vm->player().eye);
	bool opened = false;
	while (scene->clipPlaying(kDog) && !_vm->shouldQuit()) {
		_vm->lookAt(0, at(kDog));
		const float frame = scene->nodeFrame(kDog);
		if (!opened && frame > 97 && frame < 99 && !scene->nodeRunning("*U05_01")) {
			scene->setNodeLoop("*U05_01", false);
			scene->setNodeFps("*U05_01", 15);
			scene->runNodeTo("*U05_01", -1, false);
			opened = true;
		} else if (opened && frame > 102 && !scene->nodeRunning("*U05_01")) {
			scene->setNodeFps("*U05_01", 15);
			scene->runNodeTo("*U05_01", -1, true);
			voiceAt("U05_06B", _vm->player().eye);
			break;
		}
	}
	scene->pauseNode("*U05_06");
	scene->setNodeFrame("*U05_06", 20);
	scene->setNodeFps("*U05_12", 15);
	scene->setNodeLoop("*U05_12", false);
	scene->runNodeTo("*U05_12", -1, true);
	_vm->suspend(false);
	_vm->collision()->setEnabled("ColPorte2", false);
}

void U05::maskGrille() {
	// MaskGrille: under the grate and up again
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	scene->hideObject("*U05_07");
	const float z = player.eye.z();
	const float down[3] = { 1489.92f, -1189.56f, 0.65f };
	const float up[3] = { 1489.92f, -1134.35f, z };
	_vm->moveTo(4000, down, kKeep, kKeep);
	_vm->moveTo(1500, up, -1.55f, kKeep);
	scene->hideObject(kChef);
	scene->hideObject(kDog);
	_vm->suspend(false);
	_vm->collision()->setEnabled("ColPorte1", false);
}

void U05::lampFalls() {
	// LampeTombe
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	_vm->stopGauge();
	scene->setClip(kMazout, "Anim/U04_04/Action03.A3D");
	scene->setNodeFps(kMazout, 15);
	scene->runNodeTo(kMazout, -1, false);
	scene->hideObjectOnly("*U05_08");
	scene->setNodeLoop("*Fil", false);
	scene->setNodeFps("*Fil", 15);
	scene->runNodeTo("*Fil", -1, false);
	effect("s4_0809", player.eye);
	const Vector3d p(1306.35f, -945.6f, player.eye.z());
	moveTo(1000, p, facing(at(kMazout), p), kHalfPi);
	waitVoice(false);
	scene->hideObject("*U05_09", false);
	_vm->collision()->setEnabled("*U05_09", true);
	_vm->interaction()->exhaust(kActionLamp);
	_vm->suspend(false);
}

void U05::syncState(Common::Serializer &s) {
	// The original saves no chunk for this unit, so its clock restarted on every
	// load; kept here as the time since it started (save version 5)
	uint32 since = _timer ? _vm->logicMs() - _timer : 0;
	s.syncAsUint32LE(since, 5);
	if (s.isLoading())
		_timer = since ? _vm->logicMs() - since : 0;
}

bool U05::handle(const Common::String &action) {
	Scene *scene = _vm->scene();
	Interaction *interaction = _vm->interaction();
	if (action.equalsIgnoreCase("TestSpeakChef")) {
		testSpeakChef();
	} else if (action.equalsIgnoreCase("ChienVersPorte")) {
		dogToDoor();
	} else if (action.equalsIgnoreCase("MaskGrille")) {
		maskGrille();
	} else if (action.equalsIgnoreCase("LampeTombe")) {
		lampFalls();
	} else if (action.equalsIgnoreCase("OuvrePorteConsigne")) {
		scene->setNodeLoop("*U05_10", false);
		scene->setNodeFps("*U05_10", 15);
		scene->runNodeTo("*U05_10", -1, false);
		interaction->setCursorKind("*U05_10", 0);
	} else if (action.equalsIgnoreCase("OuvrePorteSalle")) {
		for (const char *node : { "*U05_01", "*U05_12" }) {
			scene->setNodeLoop(node, false);
			scene->setNodeFps(node, 15);
			scene->runNodeTo(node, -1, false);
		}
		interaction->setCondition(83, "FALSE");
		run(84);
	} else if (action.equalsIgnoreCase("AfficheChefEtChiot")) {
		scene->hideObject(kChef, false);
		scene->hideObject(kDog, false);
	} else if (action.equalsIgnoreCase("AfficheCariolle")) {
		scene->hideObject("*U05_13", false);
	} else if (action.equalsIgnoreCase("FermePorteConsigne")) {
		scene->setNodeLoop("*U05_10", false);
		scene->setNodeFps("*U05_10", 15);
		scene->runNodeTo("*U05_10", -1, true);
	} else if (action.equalsIgnoreCase("DoTableauA") || action.equalsIgnoreCase("DoTableauB")) {
		_vm->showPainting(action.hasSuffix("A") ? "U14_02" : "U14_05");
	} else {
		return false;
	}
	return true;
}

} // End of namespace X3D
