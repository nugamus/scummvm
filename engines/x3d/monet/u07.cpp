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

#include "x3d/collision.h"
#include "x3d/interaction.h"
#include "x3d/inventory.h"
#include "x3d/scene.h"
#include "x3d/sound.h"
#include "x3d/x3d.h"
#include "x3d/monet/u07.h"

namespace X3D {

using Math::Vector3d;

static const float kKeep = X3DEngine::kKeep;
static const char *const kPlank = "*U06_30";
static const uint32 kDynamiteMs = 270000; // until the dynamite explodes

void U07::afterLoad() {
	Scene *scene = _vm->scene();
	Scene::Model *m;
	uint o;
	for (int i = 0; scene->findObject("Object04", m, o); i++) {
		Common::String &name = m->file.objects[o].name;
		if (i == 0)
			name = "*U06_260";
		else if (i == 1)
			name = "*U06_26";
		else
			name = Common::String::format("*U06_26%d", i);
	}
	scene->renameObject("secretpioc", "*U06_28");
	scene->renameObject("*U06_27", "*U06_29"); // Cave2's wall: the grille keeps *U06_27
	if (scene->findObject("planche", m, o) && m->file.objects[o].parent >= 0)
		m->file.objects[m->file.objects[o].parent].name = kPlank;
}

float U07::heightAboveGround(const Vector3d &p) {
	float t;
	return _vm->collision()->cast(p, p - Vector3d(0, 0, 10000), t) ? 10000 * t : 1e6f;
}

void U07::start(bool newGame, bool video) {
	Scene *scene = _vm->scene();
	Sound *sound = _vm->sound();
	Player &player = _vm->player();
	Interaction *interaction = _vm->interaction();
	sound->play(Common::Path(scene->dir() + "Sound/s4_16.wav"), Sound::kAmbient, 85, true);
	// The original's falls in U07 are silent (its landing sound is off); the engine's
	// land with SAUT
	for (Scene::Model *m : scene->models())
		for (const O3DObject &obj : m->file.objects)
			if (obj.name.hasPrefix("halo"))
				_vm->collision()->setEnabled(obj.name, false);
	const float s = scene->scale;
	if (!interaction->exhausted(7)) {
		sound->setEmitter(Sound::kUnitEmitter1, 4, 50 * s);
		sound->emit(Sound::kUnitEmitter1, soundPath("s4_22"), Vector3d(1299, -143, 473), true);
	}
	sound->setEmitter(Sound::kUnitEmitter2, 5, 50 * s);
	sound->emit(Sound::kUnitEmitter2, soundPath("s4_19"), Vector3d(1536, 363, 374), true);
	player.setSphere(s / 4, 0.4f * 1.5f * s);
	player.groundObject = kPlank;
	if (interaction->exhausted(4))
		_vm->collision()->setEnabled("ColGrille", false);
	// The original restores the plank untipped; here it comes back at its last frame
	if (!newGame && _plankTipped && scene->addObjectNode(kPlank, "Anim/planche.A3D", 30)) {
		scene->pauseNode(kPlank);
		scene->setNodeFrame(kPlank, scene->nodeLastFrame(kPlank));
	}
	if (!newGame)
		return;
	const float p[3] = { 1454.67f, 1912.72f, 570.605f };
	_vm->setView(p, 1.04f, 2.77f); // looking down the shaft
	player.canMove = false;
	player.collide = false;
	if (_vm->inventory()->has("U06_19P")) {
		scene->hideObject("*U06_19");
		_vm->collision()->setEnabled("*U06_19", false);
	}
	_vm->autosave();
}

void U07::syncState(Common::Serializer &s) {
	s.syncAsByte(_plankTipped);
}

bool U07::input(float dt) {
	// The ladder, then the plank and the water
	Player &player = _vm->player();
	const Keys &keys = _vm->keys();
	const float s = _vm->scene()->scale;
	const float h = 1.5f * s;
	// The generic input first: the tests below see this step's ground
	if (player.tick(dt, keys, *_vm->collision()))
		_vm->sound()->emit(Sound::kEffectsEmitter, "SAUT.WAV", player.eye, false);
	if (!_vm->interaction()->exhausted(1)) {
		// One 20-unit rung per key press
		if (keys.up && !_upWas) {
			const Vector3d p = player.eye + Vector3d(0, 0, 20);
			float t;
			if (!_vm->collision()->cast(p, p + Vector3d(0, 0, 0.4f * s), t))
				moveTo(1200, p, kKeep, kKeep);
		} else if (keys.down && !_downWas) {
			const Vector3d p = player.eye - Vector3d(0, 0, 20);
			if (heightAboveGround(p) >= h)
				moveTo(1200, p, kKeep, kKeep);
		}
		_upWas = keys.up;
		_downWas = keys.down;
		return true;
	}
	// Both tests compare the ground object's name, not its identity
	if (!_plankTipped && player.groundObject.equalsIgnoreCase("Planch01"))
		tipPlank(); // then the water test, in the same call
	if (player.groundObject.empty() || player.groundObject.hasPrefixIgnoreCase("*eau"))
		fallInWater();
	return true;
}

void U07::switchAndDescent() {
	// DoInterrupteur: the light, the dynamite's gauge, down to the floor
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	Sound *sound = _vm->sound();
	const float s = scene->scale;
	const float h = 1.5f * s;
	_vm->suspend(true);
	scene->runNodeTo("*U06_22", -1, false);
	effect("s1_08bis", player.eye);
	waitGroup(Sound::kEffects);
	scene->runNodeTo("*Eteint01", -1, false);
	effect("s4_20", player.eye);
	_vm->moveTo(1000, nullptr, 4.64f, 0.3f);
	waitNode("*Eteint01");
	_vm->startGauge(kDynamiteMs);
	_vm->moveTo(1000, nullptr, 1.54286f, 0.530796f);
	do {
		moveTo(1000, player.eye - Vector3d(0, 0, 0.6f * s), kKeep, kKeep);
		_vm->runFor(300);
	} while (heightAboveGround(player.eye) >= 1.8f * h && !_vm->shouldQuit());
	// The snap is a fall done here, in the sequence
	if (player.ground(*_vm->collision())) {
		const Keys none;
		while (player.falling() && !_vm->shouldQuit()) {
			player.tick(1.0f / X3DEngine::kStepsPerSecond, none, *_vm->collision());
			waitStep();
		}
	}
	sound->emit(Sound::kEffectsEmitter, "SAUT.WAV", player.eye, false);
	_vm->moveTo(800, nullptr, kKeep, kHalfPi);
	const float p[3] = { 1453.19f, 1912.76f, 362.294f };
	_vm->moveTo(1500, p, 2.663f, kKeep);
	player.collide = true;
	player.canMove = true;
	_vm->suspend(false);
}

void U07::tipPlank() {
	// U07_TipPlank
	Scene *scene = _vm->scene();
	_vm->suspend(true);
	_plankTipped = true;
	const float p1[3] = { 1034.62f, 287.2f, 374 };
	const float p2[3] = { 1034.55f, 284.2f, 374.3f };
	_vm->moveTo(800, p1, 7.863f, 1.0108f);
	_vm->moveTo(1000, p2, 4.743f, 0.85f);
	if (scene->addObjectNode(kPlank, "Anim/planche.A3D", 30)) {
		scene->setNodeLoop(kPlank, false);
		effect("planche", _vm->player().eye);
		waitNode(kPlank);
	}
	_vm->moveTo(1000, nullptr, kKeep, kHalfPi);
	_vm->suspend(false);
}

void U07::fallInWater() {
	// U07_FallInWater: U02's fall without the dimming
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	const float s = scene->scale;
	const float h = 1.5f * s;
	_vm->suspend(true);
	effect("eau", player.eye);
	const Vector3d d(cosf(player.yaw) * sinf(player.pitch), -sinf(player.yaw) * sinf(player.pitch), -cosf(player.pitch));
	const float p[3] = { player.eye.x() + s * d.x(), player.eye.y() + s * d.y(), player.eye.z() - h + 10 };
	const float drop = player.eye.z() - p[2]; // the fall is vertical
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
	_vm->moveTo(800, p, kKeep, 0.5f, 40);
	_vm->moveTo(600, nullptr, player.yaw + 3, kKeep);
	_vm->moveTo(600, nullptr, player.yaw + 3, kKeep);
	_vm->fadeToBlack(2000);
	_vm->suspend(false);
	player.collide = true;
	_vm->gameOver();
}

void U07::afterFrame() {
	if (_vm->gaugeExpired())
		explosion();
}

void U07::explosion() {
	// U07_DynamiteExplodes: the colours flicker, then game over
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	_vm->suspend(true);
	_vm->sound()->stopEmitter(Sound::kUnitEmitter1);
	const float p[3] = { 1390.44f, -120.8f, 487.27f };
	_vm->setView(p, 9.16f, kHalfPi);
	effect("Explosion", player.eye);
	static const byte colours[5][3] = { { 0, 0, 0 }, { 255, 0, 0 }, { 128, 0, 255 }, { 128, 255, 0 }, { 255, 255, 255 } };
	const uint32 t0 = _vm->logicMs();
	uint32 last = 0;
	bool wide = false; // the FOV flickers between 70 and 90
	bool first = true;
	while (_vm->logicMs() - t0 < 2000 && !_vm->shouldQuit()) {
		_vm->moveTo(100, nullptr, kKeep, kKeep, wide ? 90 : 70);
		wide = !wide;
		if (first || _vm->logicMs() - last > 100) {
			first = false;
			last = _vm->logicMs();
			const uint r = _random.getRandomNumber(4);
			for (int k = 0; k < 3; k++)
				scene->ambient[k] = colours[r][k];
		}
	}
	_vm->fadeToBlack(2000);
	_vm->suspend(false);
	_vm->gameOver();
}

void U07::end() {
	// CouperDynamite: the fuse is cut, the epilogue, the Option menu
	Scene *scene = _vm->scene();
	_vm->stopGauge();
	_vm->interaction()->useUp(""); // only the held item goes
	scene->hideObject("*U06_26");
	_vm->collision()->setEnabled("*U06_26", false);
	_vm->sound()->stopEmitter(Sound::kUnitEmitter1);
	_vm->runFor(3000);
	_vm->playVideo("Epilogue", "epilogue");
	_vm->sound()->stopGroup(Sound::kVoice);
	_vm->afterOptionMenu(_vm->optionMenu());
}

bool U07::handle(const Common::String &action) {
	Scene *scene = _vm->scene();
	Collision *collision = _vm->collision();
	if (action.equalsIgnoreCase("DoInterrupteur")) {
		switchAndDescent();
	} else if (action.equalsIgnoreCase("Open1Secret")) {
		effect("s4_21", _vm->player().eye);
		scene->hideObject("*U06_28");
		collision->setEnabled("*U06_28", false);
		collision->setEnabled("mursecret", false);
	} else if (action.equalsIgnoreCase("Open2Secret")) {
		effect("s4_21", _vm->player().eye);
		scene->hideObject("*U06_29");
		collision->setEnabled("*U06_29", false);
		_vm->interaction()->useUp(""); // only the held item goes, no cursor changes
		collision->setEnabled("mursecreth", false);
	} else if (action.equalsIgnoreCase("CouperDynamite")) {
		end();
	} else if (action.equalsIgnoreCase("OpenGrille")) {
		collision->setEnabled("ColGrille", false);
	} else {
		return false;
	}
	return true;
}

} // End of namespace X3D
