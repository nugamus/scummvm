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

#ifndef RING_MOVIE_H
#define RING_MOVIE_H

#include "common/path.h"

namespace Ring {

class RingEngine;

/**
 * Plays an HBR `.cnm` video (spec/video.md): pictures at (0, 16) on their frame clock,
 * late pictures skipped, the language channel's sound streamed. Returns false when the
 * file cannot be read; Escape and quitting end playback early.
 */
bool playMovie(RingEngine *vm, const Common::Path &path, int soundChannel, float rate = 0.0f);

} // End of namespace Ring

#endif // RING_MOVIE_H
