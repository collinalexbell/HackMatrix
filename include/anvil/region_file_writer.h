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

#ifndef REGION_FILE_WRITER_H_
#define REGION_FILE_WRITER_H_

#include <fstream>
#include <string>
#include "region_file.h"

class region_file_writer : public region_file {
private:

	/*
	 * Region file
	 */
	std::ofstream file;

public:

	/*
	 * Region file writer constructor
	 */
	region_file_writer(void) { return; }

	/*
	 * Region file writer constructor
	 */
	region_file_writer(const region_file_writer &other) : region_file(other.path, other.reg) { return; }

	/*
	 * Region file writer constructor
	 */
	explicit region_file_writer(const std::string &path) : region_file(path) { return; }

	/*
	 * Region file writer constructor
	 */
	region_file_writer(const std::string &path, const region &reg) : region_file(path, reg) { return; }

	/*
	 * Region file writer destructor
	 */
	virtual ~region_file_writer(void) { file.close(); }

	/*
	 * Region file writer assignment operator
	 */
	region_file_writer &operator=(const region_file_writer &other);

	/*
	 * Region file writer equals operator
	 */
	bool operator==(const region_file_writer &other);

	/*
	 * Region file writer not-equals operator
	 */
	bool operator!=(const region_file_writer &other) { return !(*this == other); }

	/*
	 * Returns a region file writer's file
	 */
	std::ofstream &get_file(void) { return file; }

	/*
	 * Returns a string representation of a region file writer
	 */
	std::string to_string(void) override { return region_file::to_string(); }

	/*
	 * Write a region file to file
	 */
	void write(void);
};

#endif // REGION_FILE_WRITER_H_
