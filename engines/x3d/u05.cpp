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

#include "common/debug.h"

#include "x3d/collision.h"
#include "x3d/interaction.h"
#include "x3d/inventory.h"
#include "x3d/scene.h"
#include "x3d/sound.h"
#include "x3d/talk.h"
#include "x3d/u05.h"
#include "x3d/x3d.h"

namespace X3D {

using Math::Vector3d;

static const float kHalfPi = (float)M_PI / 2;
static const float kKeep = X3DEngine::kKeep;
static const uint32 kGaugeMs = 15000;

static const char *const kDog = "*U05_05";
static const char *const kMazout = "*U04_04";
static const char *const kChef = "*U05_02";

void U05::afterLoad() {
	// u05.md, Start 1: before the hotspots, which name *Fil
	Scene *scene = _vm->scene();
	scene->renameObject("fil", "*Fil");
	scene->renameNode("fil", "*Fil");
	scene->renameNode("Object02", "*U05_10"); // no such node in the corpus (E-0361)
}

Vector3d U05::at(const char *object) const {
	return _vm->scene()->objectPosition(object);
}

bool U05::near(const char *object, float distance) const {
	return (_vm->player().eye - at(object)).getMagnitude() <= distance;
}

// The yaw that looks from `from` toward `target` (X3d_Convert_To_Polar, E-0040)
float U05::facing(const Vector3d &target, const Vector3d &from) const {
	const Vector3d d = target - from;
	return atan2f(-d.y(), d.x());
}

void U05::run(uint32 id) {
	// Its steps, then its count, without testing the condition (FUN_0041e4b0)
	Common::StringArray actions;
	_vm->interaction()->runAction(id, actions);
	for (const Common::String &a : actions)
		_vm->addUnitAction(a);
}

void U05::effect(const char *name, const Vector3d &position) {
	_vm->sound()->emit(Sound::kEffectsEmitter, Common::Path(_vm->scene()->dir() + "Sound/" + name + (Common::String(name).contains(".") ? "" : ".wav")), position, false);
}

void U05::voice(const char *name) {
	_vm->sound()->emit(Sound::kVoiceEmitter, Common::Path(_vm->scene()->dir() + "Sound/" + name + ".wav"), _vm->player().eye, false);
}

// The object, its children and its siblings leave collision (Object_SetNoCollisionTree)
void U05::noCollisionFamily(const Common::String &object) {
	Collision *collision = _vm->collision();
	collision->setEnabled(object, false, true);
	for (const Common::String &s : _vm->scene()->siblings(object))
		collision->setEnabled(s, false, true);
}

void U05::start(bool newGame, bool video) {
	// u05.md, Start (E-0361)
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
	if (interaction->exhausted(11))
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
		_vm->saveGameState(_vm->getAutosaveSlot(), "Automatic save", true);
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
	if (_vm->sound()->isGroupPlaying(Sound::kVoice) || !near(kChef, 220))
		return false;
	run(6);
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
	const float p[3] = { 1306.35f, -945.6f, player.eye.z() };
	_vm->moveTo(4000, p, facing(at(kMazout), Vector3d(p[0], p[1], p[2])), kHalfPi);
	scene->setClip(kMazout, "Anim/U04_04/Action01.A3D"); // MazoutA01
	scene->setNodeFps(kMazout, 15);
	scene->runNodeTo(kMazout, -1, false);
	run(13);
	while (_vm->sound()->isGroupPlaying(Sound::kVoice) && !_vm->enterHeld() && !_vm->shouldQuit())
		_vm->runFor(0);
	_vm->sound()->stopEmitter(Sound::kVoiceEmitter);
	_gauge = true;
	_gaugeStart = _vm->logicMs();
	debug(1, "U05: Mazout's gauge starts at %u", _gaugeStart);
	_timer = 0;
	_vm->suspend(false);
	return true;
}

void U05::gaugeExpired() {
	// Mazout catches the player: only he stays visible (E-0362)
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	debug(1, "U05: Mazout's gauge expires at %u", _vm->logicMs());
	_gauge = false;
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
	effect("s4_04.wav", player.eye);
	while (scene->clipPlaying(kMazout) && !_vm->shouldQuit())
		_vm->runFor(0);
	_vm->fadeToBlack(2000);
	_vm->suspend(false);
	_vm->gameOver();
}

void U05::leave() {
	Player &player = _vm->player();
	_vm->suspend(true);
	const float p[3] = { 1096, 1173, player.eye.z() };
	const Vector3d cart(1266, 1302, at("*U05_13").z());
	_vm->moveTo(7000, p, facing(cart, Vector3d(p[0], p[1], p[2])), kHalfPi);
	_leaving = true;
	_vm->fadeToBlack(2000);
	_vm->suspend(false);
	_vm->gotoScene("U06.x3d");
}

void U05::afterFrame() {
	// u05.md, Every frame (E-0362). ponytail: the original runs these checks just before
	// rendering, the engine just after: one frame later.
	Interaction *interaction = _vm->interaction();
	Player &player = _vm->player();
	Sound *sound = _vm->sound();
	const uint32 now = _vm->logicMs();

	if (interaction->exhausted(1) && !interaction->exhausted(6)) {
		if (!_timer)
			_timer = now;
		if (!interaction->exhausted(4)) {
			if (now > _timer + 4000 && barkAndSit())
				interaction->exhaust(4);
		} else if (!interaction->exhausted(5)) {
			if (waitLoop())
				interaction->exhaust(5);
		} else if (chefNotices()) {
			run(6); // a second time in the same frame: Say restarts the line (E-0362)
			if (!_vm->inventory()->has("U04_44P"))
				_vm->inventory()->add("U04_44P");
		}
	}
	if (interaction->exhausted(11) && !interaction->exhausted(12) && mazoutConfronts())
		interaction->exhaust(12);
	if (interaction->exhausted(13) && !interaction->exhausted(14) && _gauge && _vm->logicMs() - _gaugeStart >= kGaugeMs) {
		gaugeExpired();
		return;
	}
	if (interaction->exhausted(19) && !interaction->exhausted(23) && near(kChef, 220))
		run(23);
	if (interaction->exhausted(24) && !interaction->exhausted(25)) {
		if (!_timer)
			_timer = now;
		if (!sound->isGroupPlaying(Sound::kVoice) && now >= _timer + 20000 && near(kChef, 220)) {
			_timer = now;
			run(_random.getRandomNumber(1) == 1 ? 26 : 27);
		}
	}
	if (interaction->exhausted(24) && !_leaving && player.eye.y() > 928)
		leave();
	// The barks (M10) never start: nothing runs or exhausts M10 (E-0365)
}

void U05::testSpeakChef() {
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	const float p[3] = { 1493, -532, player.eye.z() };
	_vm->moveTo(10000, p, facing(at(kChef), Vector3d(p[0], p[1], p[2])), 1.45f);
	while (_vm->sound()->isGroupPlaying(Sound::kVoice) && !_vm->shouldQuit())
		_vm->runFor(0);
	_vm->suspend(false);
	scene->runNodeTo(kDog, -1, false); // ChienTourne turns
	scene->hideObject(kDog, false);
}

void U05::dogToDoor() {
	// ChienVersPorte (E-0363)
	Scene *scene = _vm->scene();
	Interaction *interaction = _vm->interaction();
	_vm->suspend(true);
	interaction->setCondition(7, "FALSE");
	interaction->setCursorKind(kChef, 0);
	interaction->setCursorKind(kDog, 0);
	scene->setClip(kDog, "Anim/U05_05/ACTION03.A3D"); // Action3
	scene->setNodeFps(kDog, 15);
	scene->runNodeTo(kDog, -1, false);
	voice("U05_06A");
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
			voice("U05_06B");
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
	// MaskGrille: under the grate and up again (E-0364)
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	scene->hideObject("*U05_07");
	const float z = player.eye.z();
	const float down[3] = { 1489.92f, -1189.56f, 0.65f }, up[3] = { 1489.92f, -1134.35f, z };
	_vm->moveTo(4000, down, kKeep, kKeep);
	_vm->moveTo(1500, up, -1.55f, kKeep);
	scene->hideObject(kChef);
	scene->hideObject(kDog);
	_vm->suspend(false);
	_vm->collision()->setEnabled("ColPorte1", false);
}

void U05::lampFalls() {
	// LampeTombe (E-0364)
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	_gauge = false;
	scene->setClip(kMazout, "Anim/U04_04/Action03.A3D");
	scene->setNodeFps(kMazout, 15);
	scene->runNodeTo(kMazout, -1, false);
	scene->hideObjectOnly("*U05_08");
	scene->setNodeLoop("*Fil", false);
	scene->setNodeFps("*Fil", 15);
	scene->runNodeTo("*Fil", -1, false);
	effect("s4_0809.wav", player.eye);
	const float p[3] = { 1306.35f, -945.6f, player.eye.z() };
	_vm->moveTo(1000, p, facing(at(kMazout), Vector3d(p[0], p[1], p[2])), kHalfPi);
	while (_vm->sound()->isGroupPlaying(Sound::kVoice) && !_vm->shouldQuit())
		_vm->runFor(0);
	scene->hideObject("*U05_09", false);
	_vm->collision()->setEnabled("*U05_09", true);
	_vm->interaction()->exhaust(14);
	_vm->suspend(false);
}

bool U05::handle(const Common::String &action) {
	// u05.md, Click handlers (E-0363, E-0364)
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
		_vm->showPainting(action.hasSuffix("A") ? "U14_02" : "U14_05"); // ui.md, Other frames
	} else {
		return false;
	}
	return true;
}

void U05::draw() {
	if (_gauge)
		drawGauge(_vm->renderer(), MIN(1.0f, (_vm->logicMs() - _gaugeStart) / (float)kGaugeMs));
}

} // End of namespace X3D
