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

#ifndef X3D_MONET_GALLERY3D_H
#define X3D_MONET_GALLERY3D_H

#include "x3d/unit.h"

namespace X3D {

// The painting's 3D scene from the gallery: unit class 50
class Gallery3D : public Unit {
public:
	Gallery3D(X3DEngine *vm, const Common::String &painting) : Unit(vm), _painting(painting) {}

	// The U0nD.X3D scene of a painting, or "" when it has none
	static Common::String sceneFor(const Common::String &painting);

	void afterLoad() override;
	void start(bool newGame, bool video) override;
	bool beforeClick() override { return true; } // the mouse does nothing
	bool gameStarted() const override { return false; }
	bool escape() override;

private:
	int unit() const; // 1..6, 33

	Common::String _painting;
};

} // End of namespace X3D

#endif // X3D_MONET_GALLERY3D_H
