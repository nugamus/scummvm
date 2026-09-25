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

#include "common/array.h"
#include "common/system.h"

#include "engines/util.h"

#include "graphics/surface.h"
#include "graphics/tinygl/tinygl.h"

#include "x3d/renderer.h"

namespace X3D {

// Software rendering through TinyGL: 2D images are composited into its framebuffer
class TinyGLRenderer : public Renderer {
public:
	TinyGLRenderer(int width, int height) {
		_width = width;
		_height = height;
		const Graphics::PixelFormat format = g_system->getSupportedFormats().front();
		initGraphics(width, height, &format);
		// U01 draws ~5,000 immediate-mode faces a frame, more than the default 5 MB of draw calls
		TinyGL::createContext(width, height, g_system->getScreenFormat(), 256, false, false, 64 * 1024 * 1024);
	}

	~TinyGLRenderer() override {
		for (Image &i : _images)
			i.surface.free();
		TinyGL::destroyContext();
	}

	uint32 createTexture(const Graphics::Surface &rgba) override {
		TGLuint id;
		tglGenTextures(1, &id);
		tglBindTexture(TGL_TEXTURE_2D, id);
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_MIN_FILTER, TGL_LINEAR);
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_MAG_FILTER, TGL_LINEAR);
		tglTexImage2D(TGL_TEXTURE_2D, 0, TGL_RGBA, rgba.w, rgba.h, 0, TGL_RGBA, TGL_UNSIGNED_BYTE, rgba.getPixels());
		return id;
	}

	void deleteTexture(uint32 texture) override {
		TGLuint id = texture;
		tglDeleteTextures(1, &id);
	}

	void begin3D(const float projection[16], const float view[16]) override {
		tglViewport(0, 0, _width, _height);
		tglClearColor(0, 0, 0, 1);
		tglClear(TGL_COLOR_BUFFER_BIT | TGL_DEPTH_BUFFER_BIT);
		tglMatrixMode(TGL_PROJECTION);
		tglLoadMatrixf(projection);
		tglMatrixMode(TGL_MODELVIEW);
		tglLoadMatrixf(view);
		tglEnable(TGL_DEPTH_TEST);
		tglDisable(TGL_CULL_FACE);
		tglDisable(TGL_LIGHTING);
		tglEnable(TGL_ALPHA_TEST);
		tglAlphaFunc(TGL_GREATER, 0.5f);
		_texture = ~0u;
	}

	void setTexture(uint32 texture) override {
		if (texture == _texture)
			return;
		if (texture) {
			tglEnable(TGL_TEXTURE_2D);
			tglBindTexture(TGL_TEXTURE_2D, texture);
		} else {
			tglDisable(TGL_TEXTURE_2D);
		}
		_texture = texture;
	}

	void setClamp(bool clamp) override {
		const int mode = clamp ? TGL_CLAMP_TO_EDGE : TGL_REPEAT;
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_WRAP_S, mode);
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_WRAP_T, mode);
	}

	void setAdditive(bool additive) override {
		if (additive) {
			tglEnable(TGL_BLEND);
			tglBlendFunc(TGL_ONE, TGL_ONE);
			tglDepthMask(TGL_FALSE);
			tglDepthFunc(TGL_LEQUAL);
		} else {
			tglDisable(TGL_BLEND);
			tglDepthMask(TGL_TRUE);
			tglDepthFunc(TGL_LESS);
		}
	}

	void drawFan(const float *xyz, const float *uv, const byte *rgb, uint count) override {
		tglBegin(TGL_TRIANGLE_FAN);
		for (uint i = 0; i < count; i++) {
			if (rgb)
				tglColor3ub(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
			else
				tglColor3ub(255, 255, 255);
			if (uv)
				tglTexCoord2f(uv[i * 2], uv[i * 2 + 1]);
			tglVertex3f(xyz[i * 3], xyz[i * 3 + 1], xyz[i * 3 + 2]);
		}
		tglEnd();
	}

	void drawImage(const Graphics::Surface &image, int x, int y, bool keyed) override {
		Image i;
		i.surface.copyFrom(image);
		i.surface.convertToInPlace(g_system->getScreenFormat());
		i.x = x;
		i.y = y;
		i.keyed = keyed;
		_images.push_back(i);
	}

	void clear() override {
		tglClearColor(0, 0, 0, 1);
		tglClear(TGL_COLOR_BUFFER_BIT | TGL_DEPTH_BUFFER_BIT);
	}

	void present() override {
		TinyGL::presentBuffer();
		Graphics::Surface frame;
		TinyGL::getSurfaceRef(frame);
		for (Image &i : _images) {
			const uint32 white = frame.format.RGBToColor(255, 255, 255);
			for (int y = 0; y < i.surface.h; y++) {
				for (int x = 0; x < i.surface.w; x++) {
					const int fx = i.x + x, fy = i.y + y;
					if (fx < 0 || fy < 0 || fx >= frame.w || fy >= frame.h)
						continue;
					const uint32 c = i.surface.getPixel(x, y);
					if (!i.keyed || c != white)
						frame.setPixel(fx, fy, c);
				}
			}
			i.surface.free();
		}
		_images.clear();
		g_system->copyRectToScreen(frame.getPixels(), frame.pitch, 0, 0, frame.w, frame.h);
		g_system->updateScreen();
	}

	Graphics::Surface *thumbnail(int width, int height) override {
		TinyGL::presentBuffer();
		Graphics::Surface frame;
		TinyGL::getSurfaceRef(frame);
		return frame.scale(width, height, true);
	}

private:
	struct Image {
		Graphics::Surface surface;
		int x, y;
		bool keyed;
	};

	Common::Array<Image> _images;
	uint32 _texture = ~0u;
};

Renderer *createTinyGLRenderer(int width, int height) {
	return new TinyGLRenderer(width, height);
}

} // End of namespace X3D
