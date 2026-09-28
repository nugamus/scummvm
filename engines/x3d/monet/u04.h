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

#ifndef X3D_MONET_U04_H
#define X3D_MONET_U04_H

#include "common/random.h"

#include "math/vector3d.h"

#include "x3d/unit.h"
#include "x3d/x3d.h"

namespace X3D {

// U04, Monet's garden at Giverny (games/monet/docs/u04.md)
class U04 : public Unit {
public:
	explicit U04(X3DEngine *vm) : _vm(vm), _random("x3d_u04") {}

	void afterLoad() override;
	void start(bool newGame, bool video) override;
	bool input(float dt) override;
	bool handle(const Common::String &action) override;
	void afterFrame() override;
	void syncState(Common::Serializer &s) override; // PARAMS

private:
	void say(const char *line); // Monet
	void hide(const char *object);
	void voiceAt(const char *name, const Math::Vector3d &position);
	void effect(const char *name, const Math::Vector3d &position);
	Math::Vector3d at(const char *object) const;
	Math::Vector3d head() const; // Monet's TETE
	void monetClip(const char *file, bool loop, int slot = 1);
	void waitVoice(bool enterStops = false);
	void waitMonet(bool walk = false);
	void paintingActions(bool on);
	void studioEmitter();
	void beeEmitter();
	void faceMap(bool gagged);

	void openDoor();
	void enterStudio();
	void useTube();
	void finishPainting();
	void painting();
	void mazout();
	void kidnapping();
	void ernest();
	void ungag();
	void climbOut();
	void climbIn();
	void stepOntoBoat();
	void jumpOffBoat();
	void row(bool forward);
	void boatSinks();
	void useKey();
	void leave();
	void hintsAfterGag();
	void hintsAfterHammer();

	X3DEngine *_vm;
	Common::RandomSource _random;
	bool _onBoat = false, _painted = false;
	X3DEngine::Gauge _savedGauge;
};

} // End of namespace X3D

#endif // X3D_MONET_U04_H
