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
#include "x3d/scene.h"
#include "x3d/sound.h"
#include "x3d/x3d.h"
#include "x3d/monet/gallery3d.h"
#include "x3d/monet/u00.h"

namespace X3D {

// The gallery's 3D view: the scene and the viewpoint of each painting
static const struct {
	const char *painting;
	const char *scene;
	float x, y, z;
	float yaw;
	float pitch;
} kViews[] = {
	{ "U11_01", "U01D.X3D", 308.53f, -508.20f, 29.55f, 1.56f, kHalfPi },
	{ "U11_02", "U02D.X3D", 652.36f, 153.88f, 78.93f, -6.24f, kHalfPi },
	{ "U11_03", "U02D.X3D", 408.00f, -831.52f, 52.53f, -4.44f, kHalfPi },
	{ "U12_03", "U03D.X3D", 449.35f, -232.52f, 68.83f, -8.04f, 2.11f },
	{ "U12_04", "U33D.X3D", 320.98f, -92.88f, 68.83f, -1.68f, 2.05f },
	{ "U13_01", "U04D.X3D", -26.42f, 37.10f, -3.60f, 2.98f, kHalfPi },
	{ "U13_03", "U04D.X3D", 53.62f, 132.69f, -4.60f, 7.66f, kHalfPi },
	{ "U13_04", "U04D.X3D", 196.00f, 284.00f, 15.00f, -0.07f, kHalfPi },
	{ "U13_05", "U04D.X3D", 44.25f, 150.98f, -3.63f, 4.807f, kHalfPi },
	{ "U13_06", "U04D.X3D", 165.60f, 291.76f, 15.00f, 3.307f, kHalfPi },
	{ "U13_11", "U04D.X3D", 176.40f, 311.57f, 15.00f, 3.37f, kHalfPi },
	{ "U13_12", "U04D.X3D", 137.41f, 123.18f, -4.61f, 2.647f, 1.21f },
	{ "U13_13", "U04D.X3D", 110.47f, -48.46f, -4.61f, -6.29f, kHalfPi },
	{ "U13_14", "U04D.X3D", 108.50f, 286.05f, 15.00f, -2.93f, kHalfPi },
	{ "U13_15", "U05D.X3D", 226.37f, -1075.79f, 47.58f, -3.37f, kHalfPi },
	{ "U14_01", "U05D.X3D", 680.45f, -135.73f, 45.83f, -3.19f, kHalfPi },
	{ "U14_03", "U06D.X3D", -1033.06f, 146.35f, 25.41f, -3.66f, kHalfPi },
	{ "U14_07", "U06D.X3D", -1104.90f, -1042.33f, 25.41f, -2.586f, kHalfPi },
};

Common::String Gallery3D::sceneFor(const Common::String &painting) {
	for (const auto &v : kViews)
		if (painting == v.painting)
			return v.scene;
	return "";
}

int Gallery3D::unit() const {
	const Common::String scene = sceneFor(_painting);
	return scene.empty() ? 0 : atoi(scene.c_str() + 1);
}

void Gallery3D::afterLoad() {
	// Step 1: the unit's ambient; step 2: its fix-ups, before the hotspots
	Scene *scene = _vm->scene();
	static const char *const ambients[] = { "", "U01", "s1_15", "s2_01", "s3_01", "s4_01a", "s4_11" };
	const int u = unit();
	const char *ambient = u == 33 ? "s2_01" : (u >= 1 && u <= 6 ? ambients[u] : "");
	if (*ambient)
		_vm->sound()->play(Common::Path(scene->dir() + "Sound/" + ambient + ".wav"), Sound::kAmbient, 85, true);
	if (u == 2) {
		scene->renameObject("*U02_01", "*U02_06");
		scene->renameObject("lourde05", "*U02_12");
		scene->renameObject("*U02_07", "*U02_07b");
		scene->renameObject("*U02_07", "*U02_07a");
		scene->renameObject("*ZonePlanc", "*U02_13");
		scene->renameObject("*colplanch", "*U02_14");
		scene->hideObject("*U02_05", false);
		scene->enableNode("*U02_05", true);
	} else if (u == 3 || u == 33) {
		for (int i = 1; i <= 9; i++)
			scene->hideObject(Common::String::format("*Ecran0%d", i));
		scene->hideObject("Box186");
	} else if (u == 4) {
		fixU04Names(scene);
	}
}

void Gallery3D::start(bool newGame, bool video) {
	Scene *scene = _vm->scene();
	Collision *collision = _vm->collision();
	Player &player = _vm->player();
	const int u = unit();
	auto remove = [&](const char *name) {
		scene->hideObject(name);
		collision->setEnabled(name, false);
	};
	if (u == 3 || u == 33) {
		for (const char *name : { "table03", "tr\xe9pied", "*U03_11", "*U03_12", "*U03_13", "objectif0", "objectif",
		                          "Box206", "Box207", "Box187", "Cylinder28", "Cylinder31", "Cylinder32", "Cylinder33",
		                          "Cylinder34", "Cylinder36", "Cylinder37", "Cylinder38", "Cylinder39", "Sphere03",
		                          "Sphere06", "Tube16", "Tube17", "Tube18" })
			remove(name);
	} else if (u == 4) {
		disableU04Boxes(collision);
		for (const char *name : { "*U04_05", "*U04_31", "*U04_32" })
			remove(name);
		player.setSphere(5, 0);
	} else if (u == 5) {
		player.setSphere(19, 29);
	}
	if (_painting == "U11_03") {
		scene->hideObject("*U02_05", false);
		scene->setNodeLoop("*U02_05", true);
		scene->runNodeTo("*U02_05", -1, false);
	}
	for (const auto &v : kViews)
		if (_painting == v.painting)
			setView(Math::Vector3d(v.x, v.y, v.z), v.yaw, v.pitch);
	if (u == 3 || u == 33)
		remove("*Ecran10");
	_vm->interaction()->actionsEnabled = false; // no hover cursor, no clicks
}

bool Gallery3D::escape() {
	// Back to the painting's Tableau, through the gallery
	_vm->sound()->stopGroup(Sound::kAmbient);
	_vm->returnToPainting(_painting);
	return true;
}

} // End of namespace X3D
