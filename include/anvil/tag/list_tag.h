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

#ifndef LIST_TAG_H_
#define LIST_TAG_H_

#include <string>
#include <vector>
#include "generic_tag.h"

class list_tag : public generic_tag {
private:

	/*
	 * List tag element type
	 */
	char ele_type;

	/*
	 * List tag value
	 */
	std::vector<generic_tag *> value;

public:

	/*
	 * List tag constructor
	 */
	list_tag(void) : generic_tag(LIST), ele_type(BYTE) { return; }

	/*
	 * List tag constructor
	 */
	list_tag(const list_tag &other) : generic_tag(other.name, LIST), ele_type(other.ele_type), value(other.value) { return; };

	/*
	 * List tag constructor
	 */
	explicit list_tag(char ele_type) : generic_tag(LIST), ele_type(ele_type) { return; }

	/*
	 * List tag constructor
	 */
	list_tag(const std::string &name, char ele_type) : generic_tag(name, LIST), ele_type(ele_type) { return; }

	/*
	 * List tag destructor
	 */
	virtual ~list_tag(void) { return; }

	/*
	 * List tag assignment operator
	 */
	list_tag &operator=(const list_tag &other);

	/*
	 * List tag equals operator
	 */
	bool operator==(const generic_tag &other) override;

	/*
	 * List tag not-equals operator
	 */
	bool operator!=(const generic_tag &other) override { return !(*this == other); }

	/*
	 * Returns a list tag tag at a given index
	 */
	generic_tag *at(unsigned int index) { return value.at(index); }

	/*
	 * Returns a list tag's empty status
	 */
	bool empty(void) { return value.empty(); }

	/*
	 * Erase a tag in a list tag at a given index
	 */
	void erase(unsigned int index) { value.erase(value.begin() + index); }

	/*
	 * Return a list tag's data
	 */
	std::vector<char> get_data(bool list_ele) override;

	/*
	 * Returns a list tag's element type
	 */
	char get_element_type(void) { return ele_type; }

	/*
	 * Return a list tag's value
	 */
	std::vector<generic_tag *> &get_value(void) { return value; }

	/*
	 * Insert a tag into a list tag at a given index
	 */
	bool insert(generic_tag *value, unsigned int index);

	/*
	 * Insert a tag onto the tail of a list tag
	 */
	bool push_back(generic_tag *value);

	/*
	 * Set a list tag's value
	 */
	void set_value(const std::vector<generic_tag *> &value) { this->value = value; }

	/*
	 * Returns a list tag value's size
	 */
	unsigned int size(void) { return value.size(); }

	/*
	 * Return a string representation of a list tag
	 */
	std::string to_string(unsigned int tab) override;
};

#endif // LIST_TAG_H_
