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
#include "common/debug.h"
#include "common/system.h"

#include "engines/util.h"

#include "graphics/surface.h"
#include "graphics/transform_struct.h"

#include "x3d/detection.h"
#include "x3d/renderer.h"

#if defined(USE_OPENGL_GAME)

#include "graphics/opengl/context.h"
#include "graphics/opengl/system_headers.h"

namespace X3D {

// GL_EXT_texture_filter_anisotropic's names, which the GL headers may lack
enum {
	kGLTextureMaxAnisotropy = 0x84FE,
	kGLMaxTextureMaxAnisotropy = 0x84FF
};

// Hardware rendering with the fixed-function OpenGL pipeline
class OpenGLRenderer : public Renderer {
public:
	OpenGLRenderer(int width, int height) {
		_width = width;
		_height = height;
		initGraphics3d(width, height);
		_viewport = Common::Rect(width, height);
		_windowHeight = height;
		const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
		if (extensions && strstr(extensions, "GL_EXT_texture_filter_anisotropic")) {
			GLfloat most = 1;
			glGetFloatv(kGLMaxTextureMaxAnisotropy, &most);
			_anisotropy = MIN<GLfloat>(most, 8);
		}
	}

	~OpenGLRenderer() override {
		for (const CachedImage &i : _images)
			deleteTexture(i.texture);
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
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, _filterTextures ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if (_filterTextures) {
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
			if (_anisotropy)
				glTexParameterf(GL_TEXTURE_2D, kGLTextureMaxAnisotropy, _anisotropy);
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
		// D3D's default culling: the original never sets a cull mode
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
		// Keyed texels have alpha 0; blended faces carry their vertex alpha. Opaque and
		// additive passes cut where the texture pass does, so light stops at its edge
		if (keyed) {
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, blend == kOpaque || blend == kAdditive ? 0.5f : 0.0f);
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
		const CachedImage &c = cachedImage(image);
		begin2D();
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, c.texture);
		glColor3ub(255, 255, 255);
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2i(x, y);
		glTexCoord2f(c.u, 0);
		glVertex2i(x + image.w, y);
		glTexCoord2f(c.u, c.v);
		glVertex2i(x + image.w, y + image.h);
		glTexCoord2f(0, c.v);
		glVertex2i(x, y + image.h);
		glEnd();
	}

	void fillRect(int x0, int y0, int x1, int y1, byte r, byte g, byte b) override {
		if (x1 <= x0 || y1 <= y0)
			return;
		begin2D();
		glDisable(GL_TEXTURE_2D);
		glColor3ub(r, g, b);
		glBegin(GL_QUADS);
		glVertex2i(x0, y0);
		glVertex2i(x1, y0);
		glVertex2i(x1, y1);
		glVertex2i(x0, y1);
		glEnd();
	}

	void clear() override {
		// Alpha too, as begin3D does: a resized window's new buffer starts at alpha 0,
		// and ScummVM composites the frame by its alpha (else a 2D screen stays black)
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
	}

	void present() override {
		g_system->updateScreen();
		// Images not drawn for a while are dropped (their surfaces may be gone)
		_frame++;
		for (uint i = 0; i < _images.size();) {
			if (_frame - _images[i].lastFrame > kImageKeepFrames) {
				deleteTexture(_images[i].texture);
				_images.remove_at(i);
			} else {
				i++;
			}
		}
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
	// 2D images keep their texture between frames: static bitmaps are uploaded once. An
	// entry is found by the surface's pixels, size, pitch and format, and re-uploaded when
	// a hash of its pixels changes (video frames, edits, lists, or a new surface at a freed
	// one's address)
	struct CachedImage {
		const void *pixels;
		int w, h, pitch;
		Graphics::PixelFormat format;
		uint32 hash;
		GLuint texture;
		float u, v; // the image's extent in its texture
		uint32 lastFrame;
	};
	static const uint32 kImageKeepFrames = 10; // presents an unused texture survives

	static uint32 hashPixels(const Graphics::Surface &image) {
		uint32 hash = 2166136261u; // FNV-1a
		const uint rowBytes = image.w * image.format.bytesPerPixel;
		for (int y = 0; y < image.h; y++) {
			const byte *row = (const byte *)image.getBasePtr(0, y);
			for (uint i = 0; i < rowBytes; i++)
				hash = (hash ^ row[i]) * 16777619u;
		}
		return hash;
	}

	const CachedImage &cachedImage(const Graphics::Surface &image) {
		const uint32 hash = hashPixels(image);
		CachedImage *c = nullptr;
		for (CachedImage &i : _images)
			if (i.pixels == image.getPixels() && i.w == image.w && i.h == image.h && i.pitch == image.pitch && i.format == image.format) {
				c = &i;
				break;
			}
		if (c && c->hash == hash) {
			c->lastFrame = _frame;
			return *c;
		}

		Graphics::Surface *rgba = image.convertTo(Graphics::PixelFormat::createFormatRGBA32());
		if (!c) {
			// Without NPOT support the image goes into the corner of a power-of-two texture
			int w = rgba->w, h = rgba->h;
			if (!OpenGLContext.NPOTSupported) {
				w = h = 1;
				while (w < rgba->w)
					w <<= 1;
				while (h < rgba->h)
					h <<= 1;
			}
			CachedImage n;
			n.pixels = image.getPixels();
			n.w = image.w;
			n.h = image.h;
			n.pitch = image.pitch;
			n.format = image.format;
			n.u = (float)rgba->w / w;
			n.v = (float)rgba->h / h;
			glGenTextures(1, &n.texture);
			glBindTexture(GL_TEXTURE_2D, n.texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			_images.push_back(n);
			c = &_images.back();
		} else {
			glBindTexture(GL_TEXTURE_2D, c->texture);
		}
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rgba->w, rgba->h, GL_RGBA, GL_UNSIGNED_BYTE, rgba->getPixels());
		rgba->free();
		delete rgba;
		c->hash = hash;
		c->lastFrame = _frame;
		_texture = ~0u;
		return *c;
	}

	// Pixel coordinates in an orthographic view over the frame, no depth, no blending
	void begin2D() {
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
		_texture = ~0u;
		_blend = -1;
	}

	Common::Array<CachedImage> _images;
	uint32 _frame = 0;
	Common::Rect _viewport; // in window pixels, top-left origin
	int _windowHeight = 480;
	GLfloat _anisotropy = 0; // the most anisotropic filtering used, 0 without the extension
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
