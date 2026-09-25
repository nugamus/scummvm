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

#ifndef X3D_RENDERER_H
#define X3D_RENDERER_H

#include "common/scummsys.h"

namespace Common {
class Path;
class SeekableReadStream;
}

namespace Graphics {
struct Surface;
}

namespace X3D {

// What the engine draws with: textured triangle fans for the 3D view, images for 2D
class Renderer {
public:
	// OpenGL when available, else TinyGL; also initialises the graphics mode
	static Renderer *create(int width, int height);
	virtual ~Renderer() {}

	int width() const { return _width; }
	int height() const { return _height; }

	// Textures from RGBA32 surfaces; texels with alpha 0 are cut out (colour keys)
	virtual uint32 createTexture(const Graphics::Surface &rgba) = 0;
	virtual void deleteTexture(uint32 texture) = 0;

	// Clears the frame and sets up the 3D view; column-major GL matrices
	virtual void begin3D(const float projection[16], const float view[16]) = 0;
	virtual void setTexture(uint32 texture) = 0; // 0: untextured
	virtual void setClamp(bool clamp) = 0; // texture coordinates clamped or wrapped
	// How the following fans reach the frame (scene.md, Drawing order and blending):
	// opaque (depth written), alpha-blended by the vertex alpha or added (depth tested,
	// not written). keyed: texels with texture alpha 0 (the colour key) are cut out.
	enum Blend { kOpaque, kAlpha, kAdditive };
	virtual void setBlend(Blend blend, bool keyed) = 0;
	// count vertices: xyz, uv (may be null) and rgb (0..255, may be null: white) each;
	// alpha for every vertex
	virtual void drawFan(const float *xyz, const float *uv, const byte *rgb, uint count, byte alpha = 255) = 0;

	// A 2D image over the frame at (x, y), drawn at present(); white (255, 255, 255) is
	// transparent when keyed
	virtual void drawImage(const Graphics::Surface &image, int x, int y, bool keyed) = 0;
	// A filled rectangle over the frame, right/bottom exclusive, drawn at present()
	void fillRect(int x0, int y0, int x1, int y1, byte r, byte g, byte b);
	virtual void clear() = 0; // black frame without 3D, for 2D-only screens
	virtual void present() = 0;
	// A thumbnail-sized copy of the frame drawn so far (before present), for saves
	virtual Graphics::Surface *thumbnail(int width, int height) = 0;

protected:
	int _width = 0, _height = 0;
};

// A BMP as RGBA32 (caller frees), or nullptr. Some of the game's BMPs lack their last
// row's padding (E-0024); the data is padded before decoding.
Graphics::Surface *loadBitmap(Common::SeekableReadStream &s);
Graphics::Surface *loadBitmap(const Common::Path &path);

Renderer *createTinyGLRenderer(int width, int height);
Renderer *createOpenGLRenderer(int width, int height);

} // End of namespace X3D

#endif // X3D_RENDERER_H
