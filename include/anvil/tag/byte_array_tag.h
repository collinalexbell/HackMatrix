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

#ifndef BYTE_ARRAY_TAG_H_
#define BYTE_ARRAY_TAG_H_

#include <string>
#include <vector>
#include "generic_tag.h"

class byte_array_tag : public generic_tag {
private:

	/*
	 * Byte array tag value
	 */
	std::vector<char> value;

public:

	/*
	 * Byte array tag constructor
	 */
	byte_array_tag(void) : generic_tag(BYTE_ARRAY) { return; }

	/*
	 * Byte array tag constructor
	 */
	byte_array_tag(const byte_array_tag &other) : generic_tag(other.name, BYTE_ARRAY), value(other.value) { return; };

	/*
	 * Byte array tag constructor
	 */
	explicit byte_array_tag(const std::string &name) : generic_tag(name, BYTE_ARRAY) { return; }

	/*
	 * Byte array tag constructor
	 */
	explicit byte_array_tag(const std::vector<char> &value) : generic_tag(BYTE_ARRAY) { this->value = value; }

	/*
	 * Byte array tag constructor
	 */
	byte_array_tag(const std::string &name, const std::vector<char> &value) : generic_tag(name, BYTE_ARRAY) { this->value = value; }

	/*
	 * Byte array tag destructor
	 */
	virtual ~byte_array_tag(void) { return; }

	/*
	 * Byte array tag assignment operator
	 */
	byte_array_tag &operator=(const byte_array_tag &other);

	/*
	 * Byte array tag equals operator
	 */
	bool operator==(const generic_tag &other) override;

	/*
	 * Byte array tag not-equals operator
	 */
	bool operator!=(const generic_tag &other) override { return !(*this == other); }

	/*
	 * Returns a byte array tag byte at a given index
	 */
	char &at(unsigned int index) { return value.at(index); }

	/*
	 * Returns a byte array tag's empty status
	 */
	bool empty(void) { return value.empty(); }

	/*
	 * Erase a byte in a byte array tag at a given index
	 */
	void erase(unsigned int index) { value.erase(value.begin() + index); }

	/*
	 * Return a byte array tag's data
	 */
	std::vector<char> get_data(bool list_ele) override;

	/*
	 * Return a byte array tag's value
	 */
	std::vector<char> &get_value(void) { return value; }

	/*
	 * Insert a byte into a byte array tag at a given index
	 */
	void insert(char value, unsigned int index) { this->value.insert(this->value.begin() + index, value); }

	/*
	 * Insert a byte onto the tail of a byte array tag
	 */
	void push_back(char value) { this->value.push_back(value); }

	/*
	 * Set a byte array tag's value
	 */
	void set_value(const std::vector<char> &value) { this->value = value; }

	/*
	 * Returns a byte array tag value's size
	 */
	unsigned int size(void) { return value.size(); }

	/*
	 * Return a string representation of a byte array tag
	 */
	std::string to_string(unsigned int tab) override;
};

#endif // BYTE_ARRAY_TAG_H_
