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

#ifndef CHUNK_INFO_H_
#define CHUNK_INFO_H_

#include <string>

class chunk_info {
private:

	/*
	 * Time last modified, offset, and data length
	 */
	unsigned int modified, offset, length;

	/*
	 * Compression type
	 */
	char type;

public:

	/*
	 * Compression types
	 */
	enum TYPE { GZIP = 1, ZLIB };

	/*
	 * Chunk info constructor
	 */
	chunk_info(void) : modified(0), offset(0), length(0), type(GZIP) { return; }

	/*
	 * Chunk info constructor
	 */
	chunk_info(const chunk_info &other) : modified(other.modified), offset(other.offset), length(other.length), type(other.type) { return; }

	/*
	 * Chunk info constructor
	 */
	chunk_info(unsigned int offset, unsigned int length, char type, unsigned int modified) : modified(modified), offset(offset), length(length), type(type) { return; }

	/*
	 * Chunk info destructor
	 */
	virtual ~chunk_info(void) { return; }

	/*
	 * Chunk info assignment operator
	 */
	chunk_info &operator=(const chunk_info &other);

	/*
	 * Chunk info equals operator
	 */
	bool operator==(const chunk_info &other);

	/*
	 * Chunk info not-equals operator
	 */
	bool operator!=(const chunk_info &other) { return !(*this == other); }

	/*
	 * Returns a chunk's empty status
	 */
	bool empty(void) { return !offset; }

	/*
	 * Return a chunk's data length
	 */
	unsigned int get_length(void) { return length; }

	/*
	 * Return a chunk's time last modified
	 */
	unsigned int get_modified(void) { return modified; }

	/*
	 * Return a chunk's file offset
	 */
	unsigned int get_offset(void) { return offset; }

	/*
	 * Return a chunk's compression type
	 */
	char get_type(void) { return type; }

	/*
	 * Set a chunk's data length
	 */
	void set_length(unsigned int length) { this->length = length; }

	/*
	 * Set a chunk's time last modified
	 */
	void set_modified(unsigned int modified) { this->modified = modified; }

	/*
	 * Set a chunk's file offset
	 */
	void set_offset(unsigned int offset) { this->offset = offset; }

	/*
	 * Set a chunk's compression type
	 */
	void set_type(char type) { this->type = type; }

	/*
	 * Returns a string representation of a chunk
	 */
	std::string to_string(void);
};

#endif // CHUNK_INFO_H_
