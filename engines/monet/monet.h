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

#ifndef MONET_MONET_H
#define MONET_MONET_H

#include "common/scummsys.h"
#include "common/str.h"
#include "engines/engine.h"

#include "monet/detection.h"

namespace Graphics {
class Screen;
}

namespace Monet {

class MonetEngine : public Engine {
public:
	MonetEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~MonetEngine() override;

	bool hasFeature(EngineFeature f) const override {
		return f == kSupportsReturnToLauncher;
	}

protected:
	Common::Error run() override;

private:
	void showBitmap(const Common::Path &path);
	void wait(uint32 ms);
	void playVideo(const Common::String &name);

	const ADGameDescription *_gameDescription;
	Graphics::Screen *_screen = nullptr;
};

} // End of namespace Monet

#endif // MONET_MONET_H
