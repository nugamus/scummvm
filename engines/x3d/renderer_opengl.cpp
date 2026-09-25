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

#include "common/system.h"

#include "engines/util.h"

#include "graphics/surface.h"
#include "graphics/transform_struct.h"

#include "x3d/renderer.h"

#if defined(USE_OPENGL_GAME) && !defined(USE_GLES2)

#include "graphics/opengl/system_headers.h"

namespace X3D {

// Hardware rendering with the fixed-function OpenGL pipeline
class OpenGLRenderer : public Renderer {
public:
	OpenGLRenderer(int width, int height) {
		_width = width;
		_height = height;
		initGraphics3d(width, height);
	}

	uint32 createTexture(const Graphics::Surface &rgba) override {
		GLuint id;
		glGenTextures(1, &id);
		glBindTexture(GL_TEXTURE_2D, id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba.w, rgba.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.getPixels());
		return id;
	}

	void deleteTexture(uint32 texture) override {
		GLuint id = texture;
		glDeleteTextures(1, &id);
	}

	void begin3D(const float projection[16], const float view[16]) override {
		glViewport(0, 0, _width, _height);
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(projection);
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(view);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glDisable(GL_CULL_FACE);
		glDisable(GL_LIGHTING);
		glDisable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.5f);
		_texture = ~0u;
	}

	void setTexture(uint32 texture) override {
		if (texture == _texture)
			return;
		if (texture) {
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, texture);
		} else {
			glDisable(GL_TEXTURE_2D);
		}
		_texture = texture;
	}

	void setClamp(bool clamp) override {
		const int mode = clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mode);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mode);
	}

	void setAdditive(bool additive) override {
		if (additive) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			glDepthMask(GL_FALSE);
			glDepthFunc(GL_LEQUAL);
		} else {
			glDisable(GL_BLEND);
			glDepthMask(GL_TRUE);
			glDepthFunc(GL_LESS);
		}
	}

	void drawFan(const float *xyz, const float *uv, const byte *rgb, uint count) override {
		glBegin(GL_TRIANGLE_FAN);
		for (uint i = 0; i < count; i++) {
			if (rgb)
				glColor3ub(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
			else
				glColor3ub(255, 255, 255);
			if (uv)
				glTexCoord2f(uv[i * 2], uv[i * 2 + 1]);
			glVertex3f(xyz[i * 3], xyz[i * 3 + 1], xyz[i * 3 + 2]);
		}
		glEnd();
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
		const uint32 texture = createTexture(*rgba);
		rgba->free();
		delete rgba;

		glViewport(0, 0, _width, _height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, _width, _height, 0, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.5f);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, texture);
		glColor3ub(255, 255, 255);
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2i(x, y);
		glTexCoord2f(1, 0);
		glVertex2i(x + image.w, y);
		glTexCoord2f(1, 1);
		glVertex2i(x + image.w, y + image.h);
		glTexCoord2f(0, 1);
		glVertex2i(x, y + image.h);
		glEnd();
		deleteTexture(texture);
		_texture = ~0u;
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
		frame.create(_width, _height, Graphics::PixelFormat::createFormatRGBA32());
		glReadPixels(0, 0, _width, _height, GL_RGBA, GL_UNSIGNED_BYTE, frame.getPixels());
		Graphics::Surface *small = frame.scale(width, height, true, Graphics::FLIP_V);
		frame.free();
		return small;
	}

private:
	uint32 _texture = ~0u;
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
