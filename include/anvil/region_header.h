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

#ifndef REGION_HEADER_H_
#define REGION_HEADER_H_

#include <string>
#include <vector>
#include "chunk_info.h"
#include "region_dim.h"

class region_header {

	/*
	 * Total header size
	 */
	static const unsigned int HEADER_LENGTH = 8192;

private:

	/*
	 * Holds information on all chunks
	 */
	chunk_info info[region_dim::CHUNK_COUNT];

public:

	/*
	 * Region header constructor
	 */
	region_header(void);

	/*
	 * Region header constructor
	 */
	region_header(const region_header &other);

	/*
	 * Region header constructor
	 */
	explicit region_header(const chunk_info (&info)[region_dim::CHUNK_COUNT]);

	/*
	 * Region header destructor
	 */
	virtual ~region_header(void) { return; }

	/*
	 * Region header assignment operator
	 */
	region_header &operator=(const region_header &other);

	/*
	 * Region header equals operator
	 */
	bool operator==(const region_header &other);

	/*
	 * Region header not-equals operator
	 */
	bool operator!=(const region_header &other) { return !(*this == other); }

	/*
	 * Return a region header's region count
	 */
	unsigned int get_count(void);

	/*
	 * Return a region header as character vector
	 */
	std::vector<char> get_data(void);

	/*
	 * Return a region header's info
	 */
	const chunk_info (&get_info(void) const)[region_dim::CHUNK_COUNT] { return info; }

	/*
	 * Return a region header's info at a given index
	 */
	chunk_info &get_info_at(unsigned int index);

	/*
	 * Set a region header's info
	 */
	void set_info(const chunk_info (&info)[region_dim::CHUNK_COUNT]);

	/*
	 * Set a region header's info at a given index
	 */
	void set_info_at(unsigned int index, const chunk_info &info);

	/*
	 * Return a string representation of a region header
	 */
	std::string to_string(void);
};

#endif // REGION_HEADER_H_
