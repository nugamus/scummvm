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

#include "common/debug.h"
#include "common/system.h"

#include "engines/util.h"

#include "graphics/surface.h"
#include "graphics/transform_struct.h"

#include "x3d/detection.h"
#include "x3d/renderer.h"

#if defined(USE_OPENGL_GAME) && !defined(USE_GLES2)

#include "graphics/opengl/context.h"
#include "graphics/opengl/system_headers.h"

namespace X3D {

// Hardware rendering with the fixed-function OpenGL pipeline
class OpenGLRenderer : public Renderer {
public:
	OpenGLRenderer(int width, int height) {
		_width = width;
		_height = height;
		initGraphics3d(width, height);
		_viewport = Common::Rect(width, height);
		_windowHeight = height;
	}

	bool updateSize(bool widescreen) override {
		// The logical frame fills the window in widescreen (never narrower than 4:3);
		// otherwise it is 4:3. The viewport is that aspect, centred in the window
		const int ww = MAX<int>(1, g_system->getWidth()), wh = MAX<int>(1, g_system->getHeight());
		const int width = widescreen ? MAX(640, (int)(480.0f * ww / wh + 0.5f)) : 640;
		int vw = ww, vh = (int)((float)ww * 480 / width + 0.5f);
		if (vh > wh) {
			vh = wh;
			vw = (int)((float)wh * width / 480 + 0.5f);
		}
		const Common::Rect viewport((ww - vw) / 2, (wh - vh) / 2, (ww - vw) / 2 + vw, (wh - vh) / 2 + vh);
		const bool changed = width != _width || viewport != _viewport;
		_width = width;
		_height = 480;
		_viewport = viewport;
		_windowHeight = wh;
		debugC(1, kDebugGraphics, "window %dx%d, frame %dx%d, viewport %d,%d %dx%d", ww, wh, _width, _height, viewport.left, viewport.top, vw, vh);
		return changed;
	}

	Common::Point toLogical(const Common::Point &p) const override {
		if (_viewport.isEmpty())
			return p;
		return Common::Point((p.x - _viewport.left) * _width / _viewport.width(),
		                     (p.y - _viewport.top) * _height / _viewport.height());
	}

	Common::Point toWindow(const Common::Point &p) const override {
		if (_viewport.isEmpty())
			return p;
		return Common::Point(_viewport.left + p.x * _viewport.width() / _width,
		                     _viewport.top + p.y * _viewport.height() / _height);
	}

	int pixelScale() const override {
		return MAX(1, (_viewport.height() + 240) / 480);
	}

	void setViewport() {
		glViewport(_viewport.left, _windowHeight - _viewport.bottom, _viewport.width(), _viewport.height());
	}

	uint32 createTexture(const Graphics::Surface &rgba) override {
		GLuint id;
		glGenTextures(1, &id);
		glBindTexture(GL_TEXTURE_2D, id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterTextures ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if (filterTextures) {
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
			const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
			if (extensions && strstr(extensions, "GL_EXT_texture_filter_anisotropic")) {
				GLfloat most = 1;
				glGetFloatv(0x84FF, &most); // GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
				glTexParameterf(GL_TEXTURE_2D, 0x84FE, MIN<GLfloat>(most, 8)); // GL_TEXTURE_MAX_ANISOTROPY_EXT
			}
		}
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba.w, rgba.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.getPixels());
		return id;
	}

	void deleteTexture(uint32 texture) override {
		GLuint id = texture;
		glDeleteTextures(1, &id);
	}

	void begin3D(const float projection[16], const float view[16]) override {
		setViewport();
		glClearColor(0, 0, 0, 1);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// The frame's alpha stays 1: texel and vertex alpha only select and blend
		// (ScummVM composites the 3D frame by its alpha)
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(projection);
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(view);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_TRUE);
		// D3D's default culling: xd3d never sets D3DRENDERSTATE_CULLMODE (E-0205)
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);
		glDisable(GL_LIGHTING);
		glDisable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.5f);
		_texture = ~0u;
		_blend = -1;
	}

	void setTexture(uint32 texture) override {
		if (texture == _texture)
			return;
		if (texture) {
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, texture);
			_clamp = -1; // wrapping is per texture
		} else {
			glDisable(GL_TEXTURE_2D);
		}
		_texture = texture;
	}

	void setClamp(bool clamp) override {
		if (clamp == _clamp)
			return;
		_clamp = clamp;
		const int mode = clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mode);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mode);
	}

	void setBlend(Blend blend, bool keyed) override {
		const int state = blend * 2 + keyed;
		if (state == _blend)
			return;
		_blend = state;
		if (blend == kOpaque) {
			glDisable(GL_BLEND);
			glDepthMask(GL_TRUE);
		} else {
			glEnable(GL_BLEND);
			if (blend != kAdditive)
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			else
				glBlendFunc(GL_ONE, GL_ONE);
			glDepthMask(GL_FALSE);
		}
		if (blend == kTint) {
			glEnable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(-1, -1);
		} else {
			glDisable(GL_POLYGON_OFFSET_FILL);
		}
		// Keyed texels have alpha 0; blended faces carry their vertex alpha
		if (keyed) {
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, blend == kOpaque ? 0.5f : 0.0f);
		} else {
			glDisable(GL_ALPHA_TEST);
		}
	}

	void drawFan(const float *xyz, const float *uv, const byte *rgb, uint count, byte alpha) override {
		glBegin(GL_TRIANGLE_FAN);
		for (uint i = 0; i < count; i++) {
			if (rgb)
				glColor4ub(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2], alpha);
			else
				glColor4ub(255, 255, 255, alpha);
			if (uv)
				glTexCoord2f(uv[i * 2], uv[i * 2 + 1]);
			glVertex3f(xyz[i * 3], xyz[i * 3 + 1], xyz[i * 3 + 2]);
		}
		glEnd();
	}

	void drawLines(const float *xyz, uint count, byte r, byte g, byte b, float width) override {
		glLineWidth(width * pixelScale());
		glColor4ub(r, g, b, 255);
		glBegin(GL_LINES);
		for (uint i = 0; i < count; i++)
			glVertex3f(xyz[i * 3], xyz[i * 3 + 1], xyz[i * 3 + 2]);
		glEnd();
		glLineWidth(1);
	}

	void drawImage(const Graphics::Surface &image, int x, int y, bool keyed) override {
		// Upload, draw at pixel coordinates in an orthographic view, discard
		Graphics::Surface *rgba = image.convertTo(Graphics::PixelFormat::createFormatRGBA32());
		if (keyed) {
			const uint32 white = rgba->format.ARGBToColor(255, 255, 255, 255);
			const uint32 clear = rgba->format.ARGBToColor(0, 255, 255, 255);
			for (int j = 0; j < rgba->h; j++)
				for (int i = 0; i < rgba->w; i++)
					if (rgba->getPixel(i, j) == white)
						rgba->setPixel(i, j, clear);
		}
		// Without NPOT support the image goes into the corner of a power-of-two texture
		int w = rgba->w, h = rgba->h;
		if (!OpenGLContext.NPOTSupported) {
			w = h = 1;
			while (w < rgba->w)
				w <<= 1;
			while (h < rgba->h)
				h <<= 1;
		}
		GLuint texture;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rgba->w, rgba->h, GL_RGBA, GL_UNSIGNED_BYTE, rgba->getPixels());
		const float u = (float)rgba->w / w, v = (float)rgba->h / h;
		rgba->free();
		delete rgba;

		setViewport();
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, _width, _height, 0, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE); // the flipped 2D view winds the quad backward
		glDisable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.5f);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, texture);
		glColor3ub(255, 255, 255);
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2i(x, y);
		glTexCoord2f(u, 0);
		glVertex2i(x + image.w, y);
		glTexCoord2f(u, v);
		glVertex2i(x + image.w, y + image.h);
		glTexCoord2f(0, v);
		glVertex2i(x, y + image.h);
		glEnd();
		deleteTexture(texture);
		_texture = ~0u;
		_blend = -1;
	}

	void clear() override {
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void present() override {
		g_system->updateScreen();
	}

	Graphics::Surface *thumbnail(int width, int height) override {
		Graphics::Surface frame;
		frame.create(_viewport.width(), _viewport.height(), Graphics::PixelFormat::createFormatRGBA32());
		glPixelStorei(GL_PACK_ALIGNMENT, 4);
		glReadPixels(_viewport.left, _windowHeight - _viewport.bottom, _viewport.width(), _viewport.height(), GL_RGBA, GL_UNSIGNED_BYTE, frame.getPixels());
		Graphics::Surface *small = frame.scale(width, height, true, Graphics::FLIP_V);
		frame.free();
		return small;
	}

private:
	Common::Rect _viewport; // in window pixels, top-left origin
	int _windowHeight = 480;
	uint32 _texture = ~0u;
	int _blend = -1, _clamp = -1; // the last setBlend (blend * 2 + keyed) and setClamp; -1: unknown
};

Renderer *createOpenGLRenderer(int width, int height) {
	return new OpenGLRenderer(width, height);
}

} // End of namespace X3D

#else

namespace X3D {

Renderer *createOpenGLRenderer(int width, int height) {
	return nullptr;
}

} // End of namespace X3D

#endif
