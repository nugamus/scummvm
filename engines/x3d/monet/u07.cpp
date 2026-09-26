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
#include "common/file.h"

#include "x3d/collision.h"
#include "x3d/interaction.h"
#include "x3d/inventory.h"
#include "x3d/scene.h"
#include "x3d/sound.h"
#include "x3d/monet/u07.h"
#include "x3d/x3d.h"

namespace X3D {

using Math::Vector3d;

static const float kHalfPi = (float)M_PI / 2;
static const float kKeep = X3DEngine::kKeep;
static const char *const kPlank = "*U06_30";

void U07::afterLoad() {
	// u07.md, Load-time renames (E-0394)
	Scene *scene = _vm->scene();
	Scene::Model *m;
	uint o;
	for (int i = 0; scene->findObject("Object04", m, o); i++)
		m->file.objects[o].name = i == 0 ? Common::String("*U06_260") : i == 1 ? Common::String("*U06_26") : Common::String::format("*U06_26%d", i);
	scene->renameObject("secretpioc", "*U06_28");
	scene->renameObject("*U06_27", "*U06_29"); // Cave2's wall: the grille keeps *U06_27
	if (scene->findObject("planche", m, o) && m->file.objects[o].parent >= 0)
		m->file.objects[m->file.objects[o].parent].name = kPlank;
}

void U07::effect(const char *name, const Vector3d &position) {
	_vm->sound()->emit(Sound::kEffectsEmitter, Common::Path(_vm->scene()->dir() + "Sound/" + name + ".wav"), position, false);
}

float U07::heightAboveGround(const Vector3d &p) {
	float t;
	return _vm->collision()->cast(p, p - Vector3d(0, 0, 10000), t) ? 10000 * t : 1e6f;
}

void U07::start(bool newGame, bool video) {
	// u07.md, Entry (E-0394)
	Scene *scene = _vm->scene();
	Sound *sound = _vm->sound();
	Player &player = _vm->player();
	Interaction *interaction = _vm->interaction();
	sound->play(Common::Path(scene->dir() + "Sound/s4_16.wav"), Sound::kAmbient, 85, true);
	// ponytail: the original's falls in U07 are silent (camera +0x58 = 0); the engine's land
	// with SAUT
	for (Scene::Model *m : scene->models())
		for (const O3DObject &obj : m->file.objects)
			if (obj.name.hasPrefix("halo"))
				_vm->collision()->setEnabled(obj.name, false);
	const float s = scene->scale;
	if (!interaction->exhausted(7)) {
		sound->setEmitter(Sound::kUnitEmitter1, 4, 50 * s);
		sound->emit(Sound::kUnitEmitter1, Common::Path(scene->dir() + "Sound/s4_22.wav"), Vector3d(1299, -143, 473), true);
	}
	sound->setEmitter(Sound::kUnitEmitter2, 5, 50 * s);
	sound->emit(Sound::kUnitEmitter2, Common::Path(scene->dir() + "Sound/s4_19.wav"), Vector3d(1536, 363, 374), true);
	player.setSphere(s / 4, 0.4f * 1.5f * s);
	player.groundObject = kPlank;
	if (interaction->exhausted(4))
		_vm->collision()->setEnabled("ColGrille", false);
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
	_vm->saveGameState(_vm->getAutosaveSlot(), "Automatic save", true);
}

void U07::syncState(Common::Serializer &s) {
	s.syncAsByte(_plankTipped);
}

bool U07::input(float dt) {
	// u07.md, Input hook (E-0395): the ladder, then the plank and the water
	Player &player = _vm->player();
	const Keys &keys = _vm->keys();
	const float s = _vm->scene()->scale, h = 1.5f * s;
	if (!_vm->interaction()->exhausted(1)) {
		// One 20-unit rung per key press
		if (keys.up && !_upWas) {
			const Vector3d p = player.eye + Vector3d(0, 0, 20);
			float t;
			if (!_vm->collision()->cast(p, p + Vector3d(0, 0, 0.4f * s), t)) {
				const float to[3] = { p.x(), p.y(), p.z() };
				_vm->moveTo(1200, to, kKeep, kKeep);
			}
		} else if (keys.down && !_downWas) {
			const Vector3d p = player.eye - Vector3d(0, 0, 20);
			if (heightAboveGround(p) >= h) {
				const float to[3] = { p.x(), p.y(), p.z() };
				_vm->moveTo(1200, to, kKeep, kKeep);
			}
		}
		_upWas = keys.up;
		_downWas = keys.down;
		return false;
	}
	// Both tests compare the ground object's name, not its identity (E-0619)
	bool handled = false;
	if (!_plankTipped && player.groundObject.equalsIgnoreCase("Planch01")) {
		tipPlank(); // then the water test, in the same call
		handled = true;
	}
	if (player.groundObject.empty() || player.groundObject.hasPrefixIgnoreCase("*eau")) {
		fallInWater();
		handled = true;
	}
	return handled;
}

void U07::switchAndDescent() {
	// DoInterrupteur: the light, the dynamite's gauge, down to the floor
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	Sound *sound = _vm->sound();
	const float s = scene->scale, h = 1.5f * s;
	_vm->suspend(true);
	scene->runNodeTo("*U06_22", -1, false);
	effect("s1_08bis", player.eye);
	while (sound->isGroupPlaying(Sound::kEffects) && !_vm->shouldQuit())
		_vm->runFor(0);
	scene->runNodeTo("*Eteint01", -1, false);
	effect("s4_20", player.eye);
	_vm->moveTo(1000, nullptr, 4.64f, 0.3f);
	while (scene->nodeRunning("*Eteint01") && !_vm->shouldQuit())
		_vm->runFor(0);
	_vm->startGauge(270000);
	_vm->moveTo(1000, nullptr, 1.54286f, 0.530796f);
	do {
		const float q[3] = { player.eye.x(), player.eye.y(), player.eye.z() - 0.6f * s };
		_vm->moveTo(1000, q, kKeep, kKeep);
		_vm->runFor(300);
	} while (heightAboveGround(player.eye) >= 1.8f * h && !_vm->shouldQuit());
	// The snap is a fall done here, in the sequence (u07.md step 6)
	if (player.ground(*_vm->collision())) {
		const Keys none;
		while (player.falling() && !_vm->shouldQuit()) {
			player.tick(1.0f / X3DEngine::kStepsPerSecond, none, *_vm->collision());
			const uint32 t = _vm->logicMs();
			while (_vm->logicMs() == t && !_vm->shouldQuit())
				_vm->runFor(0);
		}
	}
	if (Common::File::exists("SAUT.WAV"))
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
	const float p1[3] = { 1034.62f, 287.2f, 374 }, p2[3] = { 1034.55f, 284.2f, 374.3f };
	_vm->moveTo(800, p1, 7.863f, 1.0108f);
	_vm->moveTo(1000, p2, 4.743f, 0.85f);
	if (scene->addObjectNode(kPlank, "Anim/planche.A3D", 30)) {
		scene->setNodeLoop(kPlank, false);
		effect("planche", _vm->player().eye);
		while (scene->nodeRunning(kPlank) && !_vm->shouldQuit())
			_vm->runFor(0);
	}
	_vm->moveTo(1000, nullptr, kKeep, kHalfPi);
	_vm->suspend(false);
}

void U07::fallInWater() {
	// U07_FallInWater: U02's fall without the dimming
	Scene *scene = _vm->scene();
	Player &player = _vm->player();
	const float s = scene->scale, h = 1.5f * s;
	_vm->suspend(true);
	effect("eau", player.eye);
	const Vector3d d(cosf(player.yaw) * sinf(player.pitch), -sinf(player.yaw) * sinf(player.pitch), -cosf(player.pitch));
	const float p[3] = { player.eye.x() + s * d.x(), player.eye.y() + s * d.y(), player.eye.z() - h + 10 };
	const float drop = player.eye.z() - p[2]; // movement.md's fall is vertical
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
	float fov = 70;
	bool first = true;
	while (_vm->logicMs() - t0 < 2000 && !_vm->shouldQuit()) {
		_vm->moveTo(100, nullptr, kKeep, kKeep, fov);
		fov = fov == 70 ? 90 : 70;
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
	// CouperDynamite: the fuse is cut, the epilogue, the Option menu (E-0399)
	Scene *scene = _vm->scene();
	_vm->stopGauge();
	_vm->interaction()->useUp(""); // only the held item goes (E-0539)
	scene->hideObject("*U06_26");
	_vm->collision()->setEnabled("*U06_26", false);
	_vm->sound()->stopEmitter(Sound::kUnitEmitter1);
	_vm->runFor(3000);
	_vm->playVideo("Epilogue", "epilogue");
	_vm->sound()->stopGroup(Sound::kVoice);
	_vm->afterOptionMenu(_vm->optionMenu());
}

bool U07::handle(const Common::String &action) {
	// u07.md, Click handlers (E-0396)
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
		_vm->interaction()->useUp(""); // only the held item goes, no cursor changes (E-0539)
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
