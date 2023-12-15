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

#ifndef REGION_FILE_H_
#define REGION_FILE_H_

#include <boost/regex.hpp>
#include <string>
#include "region.h"

class region_file {
public:

	/*
	 * Region file pattern
	 */
	static const boost::regex PATTERN;

	/*
	 * Region file path
	 */
	std::string path;

	/*
	 * Region file region
	 */
	region reg;

	/*
	 * Region file constructor
	 */
	region_file(void) { return; }

	/*
	 * Region file constructor
	 */
	region_file(const region_file &other) : path(other.path), reg(other.reg) { return; }

	/*
	 * Region file constructor
	 */
	explicit region_file(const std::string &path) : path(path) { return; }

	/*
	 * Region file constructor
	 */
	region_file(const std::string &path, const region &reg) : path(path), reg(reg) { return; }

	/*
	 * Region file destructor
	 */
	virtual ~region_file(void) { return; }

	/*
	 * Region file assignment operator
	 */
	region_file &operator=(const region_file &other);

	/*
	 * Region file equals operator
	 */
	bool operator==(const region_file &other);

	/*
	 * Region file not-equals operator
	 */
	bool operator!=(const region_file &other) { return !(*this == other); }

	/*
	 * Convert between endian types
	 */
	template<class T>
	static void convert_endian(T &data) {

		// convert value to character array
		char *endian = reinterpret_cast<char *>(&data);

		// reverse character array
		std::reverse(endian, endian + sizeof(T));
	}

	/*
	 * Convert between endian types
	 */
	static void convert_endian(std::vector<char> &data);

	/*
	 * Generate a new region file
	 */
	void generate(int x, int z) { region::generate(x, z, reg); }

	/*
	 * Generate a ne chunk in a region
	 */
	void generate_chunk(unsigned int x, unsigned int z) { region::generate_chunk(x, z, reg); };

	/*
	 * Returns a region file's path
	 */
	std::string &get_path(void) { return path; }

	/*
	 * Returns a region file's region
	 */
	region &get_region(void) { return reg; }

	/*
	 * Returns true if a specified path is a region file
	 */
	static bool is_region_file(const std::string &path, int &x, int &z);

	/*
	 * Sets a region file's path
	 */
	void set_path(const std::string &path) { this->path = path; }

	/*
	 * Sets a region file's region
	 */
	void set_region(const region &reg) { this->reg = reg; }

	/*
	 * Returns a string representation of a region file
	 */
	virtual std::string to_string(void);
};

#endif // REGION_FILE_H_
