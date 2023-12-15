/*
 * LibAnvil
 * Copyright (C) 2012 - 2020 David Jolly
 * ----------------------
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
 */

#ifndef COMPRESSION_H_
#define COMPRESSION_H_

#include <vector>

class compression {
public:

	/*
	 * Zlib segment size
	 */
	static const unsigned int SEG_SIZE = 16384;

	/*
	 * Deflate a char buffer
	 */
	static bool deflate_(std::vector<char> &data);

	/*
	 * Inflate a char buffer
	 */
	static bool inflate_(std::vector<char> &data);
};

#endif // COMPRESSION_H_
