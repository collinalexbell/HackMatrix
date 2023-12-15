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

#ifndef INT_ARRAY_TAG_H_
#define INT_ARRAY_TAG_H_

#include <string>
#include <vector>
#include "generic_tag.h"

class int_array_tag : public generic_tag {
private:

	/*
	 * Integer array tag value
	 */
	std::vector<int> value;

public:

	/*
	 * Integer array tag constructor
	 */
	int_array_tag(void) : generic_tag(INT_ARRAY) { return; }

	/*
	 * Integer array tag constructor
	 */
	int_array_tag(const int_array_tag &other) : generic_tag(other.name, INT_ARRAY), value(other.value) { return; };

	/*
	 * Integer array tag constructor
	 */
	explicit int_array_tag(const std::string &name) : generic_tag(name, INT_ARRAY) { return; }

	/*
	 * Integer array tag constructor
	 */
	explicit int_array_tag(const std::vector<int> &value) : generic_tag(INT_ARRAY) { this->value = value; }

	/*
	 * Integer array tag constructor
	 */
	int_array_tag(const std::string &name, const std::vector<int> &value) : generic_tag(name, INT_ARRAY) { this->value = value; }

	/*
	 * Integer array tag destructor
	 */
	virtual ~int_array_tag(void) { return; }

	/*
	 * Integer array tag assignment operator
	 */
	int_array_tag &operator=(const int_array_tag &other);

	/*
	 * Integer array tag equals operator
	 */
	bool operator==(const generic_tag &other) override;

	/*
	 * Integer array tag not-equals operator
	 */
	bool operator!=(const generic_tag &other) override { return !(*this == other); }

	/*
	 * Returns a integer array tag integer at a given index
	 */
	int &at(unsigned int index) { return value.at(index); }

	/*
	 * Returns a integer array tag's empty status
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
	std::vector<int> &get_value(void) { return value; }

	/*
	 * Insert a integer into a integer array tag at a given index
	 */
	void insert(int value, unsigned int index) { this->value.insert(this->value.begin() + index, value); }

	/*
	 * Insert a integer onto the tail of a integer array tag
	 */
	void push_back(int value) { this->value.push_back(value); }

	/*
	 * Set a integer array tag's value
	 */
	void set_value(const std::vector<int> &value) { this->value = value; }

	/*
	 * Returns a integer array tag value's size
	 */
	unsigned int size(void) { return value.size(); }

	/*
	 * Return a string representation of a integer array tag
	 */
	std::string to_string(unsigned int tab) override;
};

#endif // INT_ARRAY_TAG_H_
