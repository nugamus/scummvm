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

#ifndef PEINTRE_DISPLAY_H
#define PEINTRE_DISPLAY_H

#include "common/array.h"
#include "common/rect.h"

#include "graphics/surface.h"

#include "peintre/render3d.h"

namespace Peintre {

/**
 * Where frames go. The software display is the original: a 640x480 RGB565 page. The
 * OpenGL display (enhancement) draws the 3D at the window's resolution, optionally wider
 * than 4:3, and scales the 2D page into the 4:3 middle.
 *
 * Logical coordinates stay the original's: the 2D page is x 0..639, y 0..479. A wide
 * frame adds columns on both sides, from left() to left() + width().
 */
class Display {
public:
	/** Starts the graphics mode: OpenGL when the renderer setting allows it. */
	static Display *create();
	virtual ~Display() {}

	virtual bool hardware() const { return false; }
	int width() const { return _width; }
	int left() const { return -(_width - 640) / 2; }
	/** Follows the window's size; true if the logical frame changed. */
	virtual bool updateSize() { return false; }
	virtual Common::Point toLogical(const Common::Point &window) const { return window; }
	virtual Common::Point toWindow(const Common::Point &logical) const { return logical; }
	/** Window pixels per logical pixel, rounded (for the cursor). */
	virtual int pixelScale() const { return 1; }

	/** Shows the 2D page. */
	virtual void present(const Graphics::Surface &page) = 0;

	// Hardware only: one 3D frame, its 2D images, then end3D() shows it.
	virtual void begin3D(float focal, float nearZ, float farZ) {}
	virtual void drawTriangles(const Common::Array<Tri3D> &tris) {}
	/** A 2D image at logical (x, y); keyed: the TGA key colour is transparent. */
	virtual void drawImage(const Graphics::Surface &image, int x, int y, bool keyed) {}
	virtual void end3D() {}
	/** Drops textures made from 3D textures (they are freed with the scene). */
	virtual void forgetTextures() {}
	/** The last frame shown, RGB565 or RGBA; false when the page is the frame. */
	virtual bool snapshot(Graphics::Surface &out) { return false; }

protected:
	int _width = 640;
};

Display *createOpenGLDisplay(bool widescreen, bool filterTextures);

} // End of namespace Peintre

#endif // PEINTRE_DISPLAY_H
