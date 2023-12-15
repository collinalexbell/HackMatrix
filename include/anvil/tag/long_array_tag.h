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

//
// Created by Qiufeng54321 on 2019-08-17.
//

#ifndef LONG_ARRAY_TAG_H_
#define LONG_ARRAY_TAG_H_

#include <string>
#include <vector>
#include "generic_tag.h"

class long_array_tag : public generic_tag {
private:

	/*
	 * Long array tag value
	 */
	std::vector<long> value;

public:

	/*
	 * Long array tag constructor
	 */
	long_array_tag(void) : generic_tag(LONG_ARRAY) { return; }

	/*
	 * Long array tag constructor
	 */
	long_array_tag(const long_array_tag &other) : generic_tag(other.name, LONG_ARRAY), value(other.value) { return; };

	/*
	 * Integer array tag constructor
	 */
	explicit long_array_tag(const std::string &name) : generic_tag(name, LONG_ARRAY) { return; }

	/*
	 * Long array tag constructor
	 */
	explicit long_array_tag(const std::vector<long> &value) : generic_tag(LONG_ARRAY) { this->value = value; }

	/*
	 * Long array tag constructor
	 */
	long_array_tag(const std::string &name, const std::vector<long> &value) : generic_tag(name, LONG_ARRAY) { this->value = value; }

	/*
	 * Long array tag destructor
	 */
	virtual ~long_array_tag(void) { return; }

	/*
	 * Long array tag assignment operator
	 */
	long_array_tag &operator=(const long_array_tag &other);

	/*
	 * Integer array tag equals operator
	 */
	bool operator==(const generic_tag &other) override;

	/*
	 * Integer array tag not-equals operator
	 */
	bool operator!=(const generic_tag &other) override { return !(*this == other); }

	/*
	 * Returns a long array tag integer at a given index
	 */
	long &at(unsigned int index) { return value.at(index); }

	/*
	 * Returns a long array tag's empty status
	 */
	bool empty(void) { return value.empty(); }

	/*
	 * Erase a integer in a integer array tag at a given index
	 */
	void erase(unsigned int index) { value.erase(value.begin() + index); }

	/*
	 * Return a integer array tag's data
	 */
	std::vector<char> get_data(bool list_ele) override;

	/*
	 * Return a integer array tag's value
	 */
	std::vector<long> &get_value(void) { return value; }

	/*
	 * Insert a integer into a integer array tag at a given index
	 */
	void insert(long value, unsigned int index) { this->value.insert(this->value.begin() + index, value); }

	/*
	 * Insert a integer onto the tail of a integer array tag
	 */
	void push_back(long value) { this->value.push_back(value); }

	/*
	 * Set a integer array tag's value
	 */
	void set_value(const std::vector<long> &value) { this->value = value; }

	/*
	 * Returns a long array tag value's size
	 */
	unsigned int size(void) { return value.size(); }

	/*
	 * Return a string representation of a long array tag
	 */
	std::string to_string(unsigned int tab) override;
};

#endif // LONG_ARRAY_TAG_H_
