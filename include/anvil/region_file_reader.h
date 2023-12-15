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

#ifndef REGION_FILE_READER_H_
#define REGION_FILE_READER_H_

#include <fstream>
#include <stdexcept>
#include <string>
#include "byte_stream.h"
#include "region_file.h"

class region_file_reader : public region_file {
private:

	/*
	 * Region file
	 */
	std::ifstream file;

	/*
	 * Read a chunk tag from data
	 */
	void parse_chunk_tag(const std::vector<char> &data, chunk_tag &tag);

	/*
	 * Read a tag from data
	 */
	generic_tag *parse_tag(byte_stream &stream, bool is_list, char list_type);

	/*
	 * Reads an array tag value from stream
	 */
	template <class T>
	std::vector<T> read_array_value(byte_stream &stream) {
		int ele_len;
		std::vector<T> value;

		// check stream status
		if(!stream.good())
			throw std::runtime_error("Unexpected end of stream");

		// retrieve value
		ele_len = read_value<int>(stream);
		for(int i = 0; i < ele_len; ++i)
			value.push_back(read_value<T>(stream));
		return value;
	}

	/*
	 * Reads chunk data from a file
	 */
	void read_chunks(void);

	/*
	 * Reads header data from a file
	 */
	void read_header(void);

	/*
	 * Reads a string tag value from stream
	 */
	std::string read_string_value(byte_stream &stream);

	/*
	 * Reads a numeric tag value from stream
	 */
	template <class T>
	T read_value(byte_stream &stream) {
		T value;

		// check stream status
		if(!stream.good())
			throw std::runtime_error("Unexpected end of stream");

		// retrieve value
		stream >> value;
		return value;
	}

public:

	/*
	 * Region file reader constructor
	 */
	region_file_reader(void) { return; }

	/*
	 * Region file reader constructor
	 */
	explicit region_file_reader(const std::string &path) : region_file(path) { return; }

	/*
	 * Region file reader constructor
	 */
	region_file_reader(const region_file_reader &other) : region_file(other.path, other.reg) { return; }

	/*
	 * Region file reader destructor
	 */
	virtual ~region_file_reader(void) { file.close(); }

	/*
	 * Region file reader assignment operator
	 */
	region_file_reader &operator=(const region_file_reader &other);

	/*
	 * Region file reader equals operator
	 */
	bool operator==(const region_file_reader &other);

	/*
	 * Region file reader not-equals operator
	 */
	bool operator!=(const region_file_reader &other) { return !(*this == other); }

	/*
	 * Returns a region biome value at a given x, z & b coord
	 */
	char get_biome_at(unsigned int x, unsigned int z, unsigned int b_x, unsigned int b_z);

	/*
	 * Returns a region's biomes at a given x, z coord
	 */
	std::vector<char> get_biomes_at(unsigned int x, unsigned int z);

	/*
	 * Returns a region block value at given x, z & b coord
	 */
	int get_block_at(unsigned int x, unsigned int z, unsigned int b_x, unsigned int b_y, unsigned int b_z);

	/*
	 * Returns a region's blocks at a given x, z coord
	 */
	std::vector<int> get_blocks_at(unsigned int x, unsigned int z);

	/*
	 * Returns a region's chunk tag at a given x, z coord
	 */
	chunk_tag &get_chunk_tag_at(unsigned int x, unsigned int z);

	/*
	 * Returns a region height value at a given x, z & b coord
	 */
	int get_height_at(unsigned int x, unsigned int z, unsigned int b_x, unsigned int b_z);

	/*
	 * Returns a region's height map at a given x, z coord
	 */
	std::vector<int> get_heightmap_at(unsigned int x, unsigned int z);

	/*
	 * Returns a region file reader's file
	 */
	std::ifstream &get_file(void) { return file; }

	/*
	 * Return a region's x coordinate
	 */
	int get_x_coord(void) { return get_region().get_x(); }

	/*
	 * Return a region's z coordinate
	 */
	int get_z_coord(void) { return get_region().get_z(); }

	/*
	 * Return a region's filled status
	 */
	bool is_filled(unsigned int x, unsigned int z);

	/*
	 * Reads a file into region_file
	 */
	void read(void);

	/*
	 * Returns a string representation of a region file reader
	 */
	std::string to_string(void) override { return region_file::to_string(); }
};

#endif // REGION_FILE_READER_H_
