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

#ifndef CHUNK_TAG_H_
#define CHUNK_TAG_H_

#include <string>
#include <vector>
#include "tag/compound_tag.h"
#include "tag/generic_tag.h"

class chunk_tag {
private:

	/*
	 * Chunk root tag
	 */
	compound_tag root;

	/*
	 * Returns a chunk tag sub-tag at a given name helper
	 */
	void get_tag_by_name_helper(const std::string &name, generic_tag *tag, std::vector<generic_tag *> &tags);

public:

	/*
	 * Chunk tag constructor
	 */
	chunk_tag(void) { return; }

	/*
	 * Chunk tag constructor
	 */
	chunk_tag(const chunk_tag &other) : root(other.root) { return; }

	/*
	 * Chunk tag constructor
	 */
	explicit chunk_tag(const compound_tag &root) : root(root) { return; }

	/*
	 * Chunk tag destructor
	 */
	virtual ~chunk_tag(void) { clean_root(); }

	/*
	 * Chunk tag assignment operator
	 */
	chunk_tag &operator=(const chunk_tag &other);

	/*
	 * Chunk tag equals operator
	 */
	bool operator==(const chunk_tag &other);

	/*
	 * Chunk tag not-equals operator
	 */
	bool operator!=(const chunk_tag &other) { return !(*this == other); }

	/*
	 * Clean chunk tag root tag (recursively)
	 */
	void clean_root(void);

	/*
	 * Clean chunk tag (recursively)
	 */
	static void clean_tag(generic_tag *tag);

	/*
	 * Copy chunk tag
	 */
	void copy(chunk_tag &other);

	/*
	 * Copy chunk tag (recursively)
	 */
	static generic_tag *copy_tag(generic_tag *src);

	/*
	 * Copy chunk tag helper
	 */
	template <class T>
	static T *copy_tag_helper(generic_tag *src) {
		T *src_tag = static_cast<T *>(src), *dest_tag = NULL;

		// assign attributes
		dest_tag = new T(src_tag->get_name(), src_tag->get_value());
		return dest_tag;
	}

	/*
	 * Return a chunk tag's root tag data
	 */
	std::vector<char> get_data(void) { return root.get_data(false); }

	/*
	 * Return a chunk tag's root tag
	 */
	compound_tag &get_root_tag(void) { return root; }

	/*
	 * Returns a chunk tag sub-tag at a given name
	 */
	std::vector<generic_tag *> get_sub_tag_by_name(const std::string &name);

	/*
	 * Sets a chunk tag's root tag
	 */
	void set_root_tag(const compound_tag &root) { this->root = root; }

	/*
	 * Returns a string representation of a chunk tag
	 */
	std::string to_string(void) { return root.to_string(0); }
};

#endif // CHUNK_TAG_H_
