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

#include "common/config-manager.h"

#include "graphics/renderer.h"
#include "graphics/surface.h"

#include "x3d/renderer.h"

namespace X3D {

Renderer *Renderer::create(int width, int height) {
	const Graphics::RendererType desired = Graphics::Renderer::parseTypeCode(ConfMan.get("renderer"));
	const Graphics::RendererType type = Graphics::Renderer::getBestMatchingAvailableType(desired,
#if defined(USE_OPENGL_GAME) && !defined(USE_GLES2)
		Graphics::kRendererTypeOpenGL |
#endif
		Graphics::kRendererTypeTinyGL);

	if (type == Graphics::kRendererTypeOpenGL)
		if (Renderer *r = createOpenGLRenderer(width, height))
			return r;
	return createTinyGLRenderer(width, height);
}

void Renderer::fillRect(int x0, int y0, int x1, int y1, byte r, byte g, byte b) {
	if (x1 <= x0 || y1 <= y0)
		return;
	Graphics::Surface s;
	s.create(x1 - x0, y1 - y0, Graphics::PixelFormat::createFormatRGBA32());
	s.fillRect(Common::Rect(s.w, s.h), s.format.ARGBToColor(255, r, g, b));
	drawImage(s, x0, y0, false);
	s.free();
}

} // End of namespace X3D
