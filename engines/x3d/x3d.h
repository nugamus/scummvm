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

#ifndef X3D_X3D_H
#define X3D_X3D_H

#include "common/scummsys.h"
#include "common/str.h"
#include "engines/engine.h"

#include "x3d/detection.h"

namespace Graphics {
class Screen;
}

namespace X3D {

class X3DEngine : public Engine {
public:
	X3DEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~X3DEngine() override;

	bool hasFeature(EngineFeature f) const override {
		return f == kSupportsReturnToLauncher;
	}

protected:
	Common::Error run() override;

private:
	void showBitmap(const Common::Path &path);
	void wait(uint32 ms);
	void playVideo(const Common::String &name);
	void playSound(const Common::String &name);

	// Logic steps per second. The original ran one step per rendered frame; its frame rate
	// is unknown (Q-0022), so this rate is provisional and sets the turn speed.
	static const uint kStepsPerSecond = 60;

	const ADGameDescription *_gameDescription;
	Graphics::Screen *_screen = nullptr;
};

} // End of namespace X3D

#endif // X3D_X3D_H
