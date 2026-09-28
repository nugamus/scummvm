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
#include "common/debug.h"
#include "common/hashmap.h"
#include "common/system.h"

#include "engines/util.h"

#include "graphics/pixelformat.h"
#include "graphics/renderer.h"

#include "peintre/detection.h"
#include "peintre/display.h"
#include "peintre/gfx.h"

namespace Peintre {

/** The original: the 640x480 RGB565 page is the frame. */
class SoftwareDisplay : public Display {
public:
	SoftwareDisplay() {
		const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
		initGraphics(640, 480, &format);
	}

	void present(const Graphics::Surface &page) override {
		g_system->copyRectToScreen(page.getPixels(), page.pitch, 0, 0, page.w, page.h);
		g_system->updateScreen();
	}
};

Display *Display::create() {
	const Graphics::RendererType desired = Graphics::Renderer::parseTypeCode(ConfMan.get("renderer"));
	const Graphics::RendererType type = Graphics::Renderer::getBestMatchingAvailableType(desired,
#if defined(USE_OPENGL_GAME) && !defined(USE_GLES2)
		Graphics::kRendererTypeOpenGL |
#endif
		Graphics::kRendererTypeTinyGL);
	// OpenGL at the window's size only with the high_res option (enhancement).
	if (type == Graphics::kRendererTypeOpenGL && ConfMan.getBool("high_res"))
		if (Display *d = createOpenGLDisplay(ConfMan.getBool("widescreen"), ConfMan.getBool("filter_textures")))
			return d;
	// Otherwise the original's software renderer.
	return new SoftwareDisplay();
}

} // End of namespace Peintre

#if defined(USE_OPENGL_GAME) && !defined(USE_GLES2)

#include "graphics/opengl/context.h"
#include "graphics/opengl/system_headers.h"

namespace Peintre {

/** Enhancement: the 3D at the window's resolution through fixed-function OpenGL (render.md "for GL"). */
class OpenGLDisplay : public Display {
public:
	OpenGLDisplay(bool widescreen, bool filterTextures) : _widescreen(widescreen), _filter(filterTextures) {
		initGraphics3d(widescreen ? 1708 : 1280, 960); // twice the original; ScummVM fits it to the desktop
		_smooth2D = ConfMan.getBool("filtering");
		updateSize();
	}

	~OpenGLDisplay() override {
		forgetTextures();
		if (_page)
			glDeleteTextures(1, &_page);
	}

	bool hardware() const override { return true; }

	bool updateSize() override {
		// The frame fills the window in widescreen (never narrower than 4:3), else it is
		// 4:3; either way centred in the window.
		const int ww = MAX<int>(1, g_system->getWidth()), wh = MAX<int>(1, g_system->getHeight());
		const int width = _widescreen ? MAX(640, (int)(480.0f * ww / wh + 0.5f)) : 640;
		int vw = ww, vh = (int)((float)ww * 480 / width + 0.5f);
		if (vh > wh) {
			vh = wh;
			vw = (int)((float)wh * width / 480 + 0.5f);
		}
		const Common::Rect viewport((ww - vw) / 2, (wh - vh) / 2, (ww - vw) / 2 + vw, (wh - vh) / 2 + vh);
		const bool changed = width != _width || viewport != _viewport;
		_width = width;
		_viewport = viewport;
		_windowHeight = wh;
		if (changed)
			debugC(1, kDebugGraphics, "window %dx%d, frame %dx480, viewport %d,%d %dx%d", ww, wh, _width,
				   viewport.left, viewport.top, vw, vh);
		return changed;
	}

	Common::Point toLogical(const Common::Point &p) const override {
		return Common::Point((p.x - _viewport.left) * _width / _viewport.width() + left(),
							 (p.y - _viewport.top) * 480 / _viewport.height());
	}

	Common::Point toWindow(const Common::Point &p) const override {
		return Common::Point(_viewport.left + (p.x - left()) * _viewport.width() / _width,
							 _viewport.top + p.y * _viewport.height() / 480);
	}

	int pixelScale() const override {
		return MAX(1, (_viewport.height() + 240) / 480);
	}

	void present(const Graphics::Surface &page) override {
		updateSize();
		clearWindow();
		if (!_page) {
			glGenTextures(1, &_page);
			glBindTexture(GL_TEXTURE_2D, _page);
			setFilter(_smooth2D, false);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			_pageW = OpenGLContext.NPOTSupported ? 640 : 1024;
			_pageH = OpenGLContext.NPOTSupported ? 480 : 512;
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _pageW, _pageH, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, nullptr);
		}
		glBindTexture(GL_TEXTURE_2D, _page);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, page.pitch / 2);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, page.w, page.h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, page.getPixels());
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		begin2D();
		glDisable(GL_ALPHA_TEST);
		quad(0, 0, 640, 480, 640.0f / _pageW, 480.0f / _pageH);
		g_system->updateScreen();
	}

	void begin3D(float focal, float nearZ, float farZ) override {
		updateSize();
		clearWindow();
		setViewport();
		// Camera space is x right, y down, z forward; GL's eye space has y up and z
		// backward. The frame is width() x 480 around the centre, focal f on both axes.
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		const float kx = nearZ * _width / 2 / focal, ky = nearZ * 240 / focal;
		glFrustum(-kx, kx, -ky, ky, nearZ, farZ);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glScalef(1, -1, -1);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_TRUE);
		// Counter-clockwise as the player sees it is the front (render.md "Back faces").
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);
		glDisable(GL_LIGHTING);
		glDisable(GL_BLEND);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}

	void drawTriangles(const Common::Array<Tri3D> &tris) override {
		// Runs of triangles with the same texture and type, in the order given (the −4
		// blend comes last from Renderer3D).
		uint i = 0;
		while (i < tris.size()) {
			uint j = i + 1;
			while (j < tris.size() && tris[j].type == tris[i].type && tris[j].tex == tris[i].tex)
				j++;
			drawRun(tris, i, j);
			i = j;
		}
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}

	void drawImage(const Graphics::Surface &image, int x, int y, bool keyed) override {
		const ImageKey key = { image.getPixels(), keyed };
		GLuint id;
		if (_images.contains(key)) {
			id = _images[key];
		} else {
			id = makeImage(image, keyed);
			_images[key] = id;
		}
		if (!_in2D) {
			begin2D();
			_in2D = true;
		}
		glBindTexture(GL_TEXTURE_2D, id);
		if (keyed) {
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, 0.5f);
		} else {
			glDisable(GL_ALPHA_TEST);
		}
		int tw = image.w, th = image.h;
		if (!OpenGLContext.NPOTSupported)
			tw = pot(tw), th = pot(th);
		quad(x, y, image.w, image.h, (float)image.w / tw, (float)image.h / th);
	}

	void end3D() override {
		_in2D = false;
		g_system->updateScreen();
	}

	void forgetTextures() override {
		for (auto &t : _textures)
			glDeleteTextures(1, &t._value);
		_textures.clear();
		for (auto &t : _images)
			glDeleteTextures(1, &t._value);
		_images.clear();
	}

	bool snapshot(Graphics::Surface &out) override {
		Graphics::Surface frame;
		frame.create(_viewport.width(), _viewport.height(), Graphics::PixelFormat::createFormatRGBA32());
		glPixelStorei(GL_PACK_ALIGNMENT, 4);
		glReadBuffer(GL_FRONT);
		glReadPixels(_viewport.left, _windowHeight - _viewport.bottom, _viewport.width(), _viewport.height(),
					 GL_RGBA, GL_UNSIGNED_BYTE, frame.getPixels());
		glReadBuffer(GL_BACK);
		out.create(frame.w, frame.h, frame.format);
		for (int y = 0; y < frame.h; y++)
			memcpy(out.getBasePtr(0, y), frame.getBasePtr(0, frame.h - 1 - y), frame.pitch);
		frame.free();
		return true;
	}

private:
	struct ImageKey {
		const void *pixels;
		bool keyed;
	};
	struct ImageKeyHash {
		uint operator()(const ImageKey &k) const { return (uint)(uintptr)k.pixels ^ (k.keyed ? 0x9e3779b9 : 0); }
	};
	struct ImageKeyEqual {
		bool operator()(const ImageKey &a, const ImageKey &b) const { return a.pixels == b.pixels && a.keyed == b.keyed; }
	};

	static int pot(int n) {
		int p = 1;
		while (p < n)
			p <<= 1;
		return p;
	}

	static void rgb565(uint16 c, byte *out) {
		out[0] = (c >> 11) * 255 / 31;
		out[1] = ((c >> 5) & 63) * 255 / 63;
		out[2] = (c & 31) * 255 / 31;
	}

	void setViewport() {
		glViewport(_viewport.left, _windowHeight - _viewport.bottom, _viewport.width(), _viewport.height());
	}

	void clearWindow() {
		// The frame's alpha stays 1: ScummVM composites the 3D frame by its alpha.
		glViewport(0, 0, g_system->getWidth(), g_system->getHeight());
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
	}

	void setFilter(bool smooth, bool mipmaps) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, !smooth ? GL_NEAREST : mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
	}

	void begin2D() {
		// Logical pixels over the whole frame; the 2D page is x 0..639.
		setViewport();
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(left(), left() + _width, 480, 0, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		glEnable(GL_TEXTURE_2D);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}

	void quad(int x, int y, int w, int h, float u, float v) {
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2i(x, y);
		glTexCoord2f(u, 0);
		glVertex2i(x + w, y);
		glTexCoord2f(u, v);
		glVertex2i(x + w, y + h);
		glTexCoord2f(0, v);
		glVertex2i(x, y + h);
		glEnd();
	}

	GLuint makeImage(const Graphics::Surface &image, bool keyed) {
		// RGB565 to RGBA; the TGA key colour is transparent when keyed.
		const int tw = OpenGLContext.NPOTSupported ? image.w : pot(image.w);
		const int th = OpenGLContext.NPOTSupported ? image.h : pot(image.h);
		Common::Array<byte> rgba(tw * th * 4, 0);
		for (int y = 0; y < image.h; y++) {
			const uint16 *src = (const uint16 *)image.getBasePtr(0, y);
			for (int x = 0; x < image.w; x++) {
				byte *d = &rgba[(y * tw + x) * 4];
				rgb565(src[x], d);
				d[3] = keyed && src[x] == kTgaKey ? 0 : 255;
			}
		}
		GLuint id;
		glGenTextures(1, &id);
		glBindTexture(GL_TEXTURE_2D, id);
		setFilter(_smooth2D, false);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
		return id;
	}

	GLuint texture(const Texture3D *tex, bool keyed) {
		// Row 16 of the shade table (render.md "Lighting"); keyed: texel 0 transparent,
		// its colour taken from a neighbour so filtering does not bleed the key's colour.
		const TextureKey key = { tex, keyed };
		if (_textures.contains(key))
			return _textures[key];
		Common::Array<byte> rgba(256 * 256 * 4);
		for (int i = 0; i < 256 * 256; i++) {
			const byte t = i < (int)tex->texels.size() ? tex->texels[i] : 0;
			rgb565(tex->shades[16][t], &rgba[i * 4]);
			rgba[i * 4 + 3] = keyed && t == 0 ? 0 : 255;
		}
		if (keyed) {
			for (int i = 0; i < 256 * 256; i++) {
				if (rgba[i * 4 + 3])
					continue;
				static const int kN[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
				for (const auto &n : kN) {
					const int j = (((i >> 8) + n[1]) & 255) * 256 + (((i & 255) + n[0]) & 255);
					if (rgba[j * 4 + 3]) {
						memcpy(&rgba[i * 4], &rgba[j * 4], 3);
						break;
					}
				}
			}
		}
		GLuint id;
		glGenTextures(1, &id);
		glBindTexture(GL_TEXTURE_2D, id);
		setFilter(_filter, _filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		if (_filter) {
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
			const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
			if (extensions && strstr(extensions, "GL_EXT_texture_filter_anisotropic")) {
				GLfloat most = 1;
				glGetFloatv(0x84FF, &most); // GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
				glTexParameterf(GL_TEXTURE_2D, 0x84FE, MIN<GLfloat>(most, 8)); // GL_TEXTURE_MAX_ANISOTROPY_EXT
			}
		}
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
		_textures[key] = id;
		return id;
	}

	void drawRun(const Common::Array<Tri3D> &tris, uint from, uint to) {
		// Face-group types (render.md "Face groups"): 3 opaque, −6 keyed, −4 half blend
		// without depth writes, 1 flat colour.
		const Tri3D &first = tris[from];
		const bool textured = first.type != 1 && first.tex;
		if (textured) {
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, texture(first.tex, first.type == -6));
		} else {
			glDisable(GL_TEXTURE_2D);
		}
		if (first.type == -6) {
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, 0.5f);
		} else {
			glDisable(GL_ALPHA_TEST);
		}
		if (first.type == -4) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDepthMask(GL_FALSE);
		} else {
			glDisable(GL_BLEND);
			glDepthMask(GL_TRUE);
		}
		const uint n = (to - from) * 3;
		_xyz.resize(n * 3);
		_uv.resize(n * 2);
		_rgba.resize(n * 4);
		for (uint t = from, k = 0; t < to; t++) {
			const Tri3D &tri = tris[t];
			byte c[4] = { 255, 255, 255, (byte)(tri.type == -4 ? 128 : 255) };
			if (!textured)
				rgb565(tri.colour, c);
			for (int i = 0; i < 3; i++, k++) {
				_xyz[k * 3] = tri.x[i];
				_xyz[k * 3 + 1] = tri.y[i];
				_xyz[k * 3 + 2] = tri.z[i];
				_uv[k * 2] = tri.u[i] / 256.0f;
				_uv[k * 2 + 1] = tri.v[i] / 256.0f;
				memcpy(&_rgba[k * 4], c, 4);
			}
		}
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, _xyz.data());
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, _rgba.data());
		if (textured) {
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glTexCoordPointer(2, GL_FLOAT, 0, _uv.data());
		} else {
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		glDrawArrays(GL_TRIANGLES, 0, n);
	}

	struct TextureKey {
		const Texture3D *tex;
		bool keyed;
	};
	struct TextureKeyHash {
		uint operator()(const TextureKey &k) const { return (uint)(uintptr)k.tex ^ (k.keyed ? 0x9e3779b9 : 0); }
	};
	struct TextureKeyEqual {
		bool operator()(const TextureKey &a, const TextureKey &b) const { return a.tex == b.tex && a.keyed == b.keyed; }
	};

	bool _widescreen, _filter, _smooth2D = false;
	bool _in2D = false;
	Common::Rect _viewport;   ///< in window pixels, top-left origin
	int _windowHeight = 480;
	GLuint _page = 0;
	int _pageW = 640, _pageH = 480;
	Common::HashMap<TextureKey, GLuint, TextureKeyHash, TextureKeyEqual> _textures;
	Common::HashMap<ImageKey, GLuint, ImageKeyHash, ImageKeyEqual> _images;
	Common::Array<float> _xyz, _uv;
	Common::Array<byte> _rgba;
};

Display *createOpenGLDisplay(bool widescreen, bool filterTextures) {
	return new OpenGLDisplay(widescreen, filterTextures);
}

} // End of namespace Peintre

#else

namespace Peintre {

Display *createOpenGLDisplay(bool widescreen, bool filterTextures) {
	return nullptr;
}

} // End of namespace Peintre

#endif
