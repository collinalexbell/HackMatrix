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

#ifndef REGION_DIM_H_
#define REGION_DIM_H_

class region_dim {
public:

	/*
	 * Maximum number of surface blocks per chunk
	 */
	static const unsigned int BLOCK_COUNT = 256;

	/*
	 * Block width of a chunk
	 */
	static const unsigned int BLOCK_WIDTH = 16;

	/*
	 * Block height of a chunk
	 */
	static const unsigned int BLOCK_HEIGHT = 256;

	/*
	 * Maximum number of chunks in region
	 */
	static const unsigned int CHUNK_COUNT = 1024;

	/*
	 * Chunk width of a region
	 */
	static const unsigned int CHUNK_WIDTH = 32;

	/*
	 * Region file header offset
	 */
	static const unsigned int HEADER_OFFSET = 8192;

	/*
	 * Region file sector size
	 */
	static const unsigned int SECTOR_SIZE = 4096;
};

#endif // REGION_DIM_H_
