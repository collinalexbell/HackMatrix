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

#ifndef COMPOUND_TAG_H_
#define COMPOUND_TAG_H_

#include <string>
#include <vector>
#include "generic_tag.h"

class compound_tag : public generic_tag {
private:

	/*
	 * Compound tag value
	 */
	std::vector<generic_tag *> value;

public:

	/*
	 * Compound tag constructor
	 */
	compound_tag(void) : generic_tag(COMPOUND) { return; }

	/*
	 * Compound tag constructor
	 */
	compound_tag(const compound_tag &other) : generic_tag(other.name, COMPOUND), value(other.value) { return; };

	/*
	 * Compound tag constructor
	 */
	explicit compound_tag(const std::string &name) : generic_tag(name, COMPOUND) { return; }

	/*
	 * Compound tag destructor
	 */
	virtual ~compound_tag(void) { return; }

	/*
	 * Compound tag assignment operator
	 */
	compound_tag &operator=(const compound_tag &other);

	/*
	 * Compound tag equals operator
	 */
	bool operator==(const generic_tag &other) override;

	/*
	 * Compound tag not-equals operator
	 */
	bool operator!=(const generic_tag &other) override { return !(*this == other); }

	/*
	 * Returns a compound tag tag at a given index
	 */
	generic_tag *at(unsigned int index) { return value.at(index); }

	/*
	 * Returns a compound tag's empty status
	 */
	bool empty(void) { return value.empty(); }

	/*
	 * Erase a tag in a compound tag at a given index
	 */
	void erase(unsigned int index) { value.erase(value.begin() + index); }

	/*
	 * Return a compound tag's data
	 */
	std::vector<char> get_data(bool list_ele) override;

	/*
	 * Return a compound tag's value
	 */
	std::vector<generic_tag *> &get_value(void) { return value; }

	/*
	 * Insert a tag into a compound tag at a given index
	 */
	void insert(generic_tag *value, unsigned int index) { this->value.insert(this->value.begin() + index, value); }

	/*
	 * Insert a tag onto the tail of a compound tag
	 */
	void push_back(generic_tag *value) { this->value.push_back(value); }

	/*
	 * Set a compound tag's value
	 */
	void set_value(const std::vector<generic_tag *> &value) { this->value = value; }

	/*
	 * Returns a compound tag value's size
	 */
	unsigned int size(void) { return value.size(); }

	/*
	 * Return a string representation of a compound tag
	 */
	std::string to_string(unsigned int tab) override;
};

#endif // COMPOUND_TAG_H_
