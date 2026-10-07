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

#ifndef CRYOMNI3D_OMNI3D_H
#define CRYOMNI3D_OMNI3D_H

#include "graphics/surface.h"

namespace CryOmni3D {

class Omni3DManager {
public:
	Omni3DManager() : _vfov(0), _alpha(0), _beta(0), _xSpeed(0), _ySpeed(0), _alphaMin(0), _alphaMax(0),
		_betaMin(0), _betaMax(0), _helperValue(0), _rowShiftX(10), _rowShiftY(15), _dirty(true),
		_dirtyCoords(true), _sourceSurface(nullptr) {}
	virtual ~Omni3DManager();

	/**
	 * Builds the projection tables for a horizontal field of view (radians).
	 * The vertical field of view is derived from it unless vfov is given.
	 */
	void init(double hfov, double vfov = 0.);
	/**
	 * Sets how much the per-pixel step changes from one row of a 16x16 block to the next
	 * (a right shift of the block's row difference): Versailles uses 10 and 15.
	 */
	void setRowStepShifts(uint shiftX, uint shiftY) { _rowShiftX = shiftX; _rowShiftY = shiftY; _dirty = true; }

	void setSourceSurface(const Graphics::Surface *surface) { _sourceSurface = surface; _dirty = true; }

	void clearConstraints();
	void setAlphaConstraints(double alphaMin, double alphaMax) { _alphaMin = alphaMin; _alphaMax = alphaMax; }
	void setBetaMinConstraint(double betaMin) { _betaMin = betaMin; }
	void setBetaMaxConstraint(double betaMax) { _betaMax = betaMax; }

	void setAlpha(double alpha) { _alpha = alpha; _dirtyCoords = true; }
	void setBeta(double beta) { _beta = beta; _dirtyCoords = true; }
	void updateCoords(int xDelta, int yDelta, bool useOldSpeed);

	double getAlpha() const { return _alpha; }
	double getBeta() const { return _beta; }

	Common::Point mapMouseCoords(const Common::Point &mouse);

	bool hasSpeed() { return _xSpeed != 0. || _ySpeed != 0.; }
	bool needsUpdate() { return _dirty || _dirtyCoords; }
	const Graphics::Surface *getSurface();

private:
	void updateImageCoords();
	template<typename T>
	void drawWarp();

	double _vfov;

	double _alpha, _beta;
	double _xSpeed, _ySpeed;

	double _alphaMin, _alphaMax;
	double _betaMin, _betaMax;

	int _imageCoords[2544];
	double _squaresCoords[31][21];
	double _hypothenusesH[31];
	double _anglesH[31];
	double _oppositeV[21];
	double _helperValue;
	uint _rowShiftX, _rowShiftY;

	bool _dirty;
	bool _dirtyCoords;
	const Graphics::Surface *_sourceSurface;
	Graphics::Surface _surface;
};

} // End of namespace CryOmni3D

#endif
