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

#ifndef GILBERT_ROOM_H
#define GILBERT_ROOM_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

#include "gilbert/collection.h"

namespace Gilbert {

class GilbertEngine;

/** The walkmap mode, mode 1 (rooms.md). */
class Room {
public:
	explicit Room(GilbertEngine *vm) : _vm(vm) {}

	/** ResetState's part (rooms.md "State"): after a new game or a load. */
	void reset();

	// Call-backs (rooms.md "Call-backs used in rooms").
	void load(uint32 id, int x, int y, int facing);
	void refreshObjects();
	Common::Point gilbertPosition() const;
	int mapWidth() const { return _mapW; }
	int mapHeight() const { return _mapH; }
	int mapCell(int x, int y) const;
	void walk(int direction);
	void newTopic();
	/** Call-back 10, kind 0: the room music, started when the name changes. */
	void roomMusic(const Common::String &name);
	void restartMusic();

	/** One tick of mode 1 (rooms.md "The main loop in mode 1"). */
	void tick(uint32 elapsed);

	bool shown() const { return _shown; }

private:
	struct Object {
		int picture;
		int x, y;
	};

	void draw();
	void drawLayer(Picture *pic, int ox, int oy, const Common::Rect &r);
	void drawObjectsAndGilbert();
	void drawPanel();
	void handleMouse();
	void action(int item);
	void moveGilbert(int m);
	void stepAreaCheck();
	void fade(bool in);

	GilbertEngine *_vm;
	PictureCollection _objects, _picture, _mask;
	uint32 _current = (uint32)-1;
	Common::Array<uint32> _cells;
	int _mapW = 0, _mapH = 0;
	int _w = 0, _h = 0;
	int _ox = 0, _oy = 0;
	bool _shown = false;
	bool _firstFade = true;
	Common::Array<Object> _list;

	// Gilbert (rooms.md "Gilbert").
	double _x = 0, _y = 0;
	int _frame = 0;
	int _facing = 0;
	double _counter = 0;
	bool _walking = false;
	int _lastStep = -1;
	bool _up = false, _down = false, _left = false, _right = false;

	// Panel.
	int _cursor = 0;
	int _hover = -1, _pressed = -1;
	int _radarAlpha = 0, _radarStep = 1;
	Common::Rect _radar;
	bool _newTopic = false;
	int _blink = 50, _blinkStep = 8;
	Common::String _music;
};

} // End of namespace Gilbert

#endif // GILBERT_ROOM_H
