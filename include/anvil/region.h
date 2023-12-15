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

#ifndef REGION_H_
#define REGION_H_

#include <string>
#include <vector>
#include "chunk_tag.h"
#include "region_dim.h"
#include "region_header.h"

class region {
private:

	/*
	 * Region header
	 */
	region_header header;

	/*
	 * Region chunk tags
	 */
	chunk_tag tags[region_dim::CHUNK_COUNT];

	/*
	 * Region x, z coord
	 */
	int x, z;

public:

	/*
	 * Region constructor
	 */
	region(void) : x(0), z(0) { return; }

	/*
	 * Region constructor
	 */
	region(const region &other);

	/*
	 * Region constructor
	 */
	region(int x, int z) : x(x), z(z) { return; }

	/*
	 * Region constructor
	 */
	region(int x, int z, const region_header &header, const chunk_tag (&tags)[region_dim::CHUNK_COUNT]);

	/*
	 * Region destructor
	 */
	virtual ~region(void) { return; }

	/*
	 * Region assignment operator
	 */
	region &operator=(const region &other);

	/*
	 * Region equals operator
	 */
	bool operator==(const region &other);

	/*
	 * Region not-equals operator
	 */
	bool operator!=(const region &other) { return !(*this == other); }

	/*
	 * Generate a new region
	 */
	static void generate(int x, int z, region &reg);

	/*
	 * Generate a new chunk in a region
	 */
	static void generate_chunk(unsigned int x, unsigned int z, region &reg);

	/*
	 * Returns a region's header
	 */
	region_header &get_header(void) { return header; }

	/*
	 * Returns a region's tags
	 */
	const chunk_tag (&get_tags(void) const)[region_dim::CHUNK_COUNT] { return tags; }

	/*
	 * Returns a region's tag at a given index
	 */
	chunk_tag &get_tag_at(unsigned int index);

	/*
	 * Returns a region's x coordinate
	 */
	int get_x(void) { return x; }

	/*
	 * Returns a region's z coordinate
	 */
	int get_z(void) { return z; }

	/*
	 * Returns a region's chunk filled status
	 */
	bool is_filled(unsigned int index);

	/*
	 * Sets a region's header
	 */
	void set_header(const region_header &header) { this->header = header; }

	/*
	 * Sets a region's tags
	 */
	void set_tags(const chunk_tag (&tags)[region_dim::CHUNK_COUNT]);

	/*
	 * Sets a region tag at a given index
	 */
	void set_tag_at(unsigned int index, const chunk_tag &tag);

	/*
	 * Sets a region's x coordinate
	 */
	void set_x(int x) { this->x = x; }

	/*
	 * Sets a region's z coordinate
	 */
	void set_z(int z) { this->z = z; }

	/*
	 * Returns a string representation of a region
	 */
	std::string to_string(void);
};

#endif // REGION_H_
