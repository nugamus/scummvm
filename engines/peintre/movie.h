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

#ifndef PEINTRE_MOVIE_H
#define PEINTRE_MOVIE_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

namespace Graphics {
struct Surface;
}

namespace Peintre {

class PeintreEngine;

/** One record of the EXE's movie table (formats README, ".CVY"; E-0204). */
struct MovieEntry {
	Common::String name;
	bool hasCvy;
	Common::Rect rect;   ///< where the top-left w x h of the frame goes
	bool soundOn;
};

class MoviePlayer {
public:
	MoviePlayer(PeintreEngine *vm) : _vm(vm) {}

	/** Reads the movie table out of the game's EXE. */
	bool loadTable();
	const MovieEntry *findEntry(const Common::String &name) const;

	/**
	 * Plays Data/MOVIES/<name>.HNM to its end (or until skipped); a movie in the table is
	 * drawn into its rectangle with its CVY mask, others fill the screen.
	 * Returns false when the movie could not be opened.
	 */
	bool play(const Common::String &name, bool skippable = true);

private:
	bool loadMasks(const Common::String &name, Common::Array<Common::Array<byte> > &masks);
	void paintMask(Graphics::Surface &dst, const Common::Array<byte> &mask, const Common::Point &origin);

	PeintreEngine *_vm;
	Common::Array<MovieEntry> _table;
};

} // End of namespace Peintre

#endif // PEINTRE_MOVIE_H
