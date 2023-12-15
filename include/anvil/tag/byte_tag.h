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

#ifndef BYTE_TAG_H_
#define BYTE_TAG_H_

#include <string>
#include <vector>
#include "generic_tag.h"

class byte_tag : public generic_tag {
private:

	/*
	 * Byte tag value
	 */
	char value;

public:

	/*
	 * Byte tag constructor
	 */
	byte_tag(void) : generic_tag(BYTE) { value = 0; }

	/*
	 * Byte tag constructor
	 */
	byte_tag(const byte_tag &other) : generic_tag(other.name, BYTE) { value = other.value; };

	/*
	 * Byte tag constructor
	 */
	explicit byte_tag(const std::string &name) : generic_tag(name, BYTE) { value = 0; }

	/*
	 * Byte tag constructor
	 */
	explicit byte_tag(char value) : generic_tag(BYTE) { this->value = value; }

	/*
	 * Byte tag constructor
	 */
	byte_tag(const std::string &name, char value) : generic_tag(name, BYTE) { this->value = value; }

	/*
	 * Byte tag destructor
	 */
	virtual ~byte_tag(void) { return; }

	/*
	 * Byte tag assignment operator
	 */
	byte_tag &operator=(const byte_tag &other);

	/*
	 * Byte tag equals operator
	 */
	bool operator==(const generic_tag &other) override;

	/*
	 * Byte tag not-equals operator
	 */
	bool operator!=(const generic_tag &other) override { return !(*this == other); }

	/*
	 * Return a byte tag's data
	 */
	std::vector<char> get_data(bool list_ele) override;

	/*
	 * Return a byte tag's value
	 */
	char get_value(void) { return value; }

	/*
	 * Set a byte tag's value
	 */
	void set_value(char value) { this->value = value; }

	/*
	 * Return a string representation of a byte tag
	 */
	std::string to_string(unsigned int tab) override;
};

#endif // BYTE_TAG_H_
