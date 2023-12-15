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

#ifndef LONG_TAG_H_
#define LONG_TAG_H_

#include <string>
#include <vector>
#include "generic_tag.h"

class long_tag : public generic_tag {
private:

	/*
	 * Long tag value
	 */
	long long value;

public:

	/*
	 * Long tag constructor
	 */
	long_tag(void) : generic_tag(LONG) { value = 0; }

	/*
	 * Long tag constructor
	 */
	long_tag(const long_tag &other) : generic_tag(other.name, LONG) { value = other.value; };

	/*
	 * Long tag constructor
	 */
	explicit long_tag(const std::string &name) : generic_tag(name, LONG) { value = 0; }

	/*
	 * Long tag constructor
	 */
	explicit long_tag(long long value) : generic_tag(LONG) { this->value = value; }

	/*
	 * Long tag constructor
	 */
	long_tag(const std::string &name, long long value) : generic_tag(name, LONG) { this->value = value; }

	/*
	 * Long tag destructor
	 */
	virtual ~long_tag(void) { return; }

	/*
	 * Long tag assignment operator
	 */
	long_tag &operator=(const long_tag &other);

	/*
	 * Long tag equals operator
	 */
	bool operator==(const generic_tag &other) override;

	/*
	 * Long tag not-equals operator
	 */
	bool operator!=(const generic_tag &other) override { return !(*this == other); }

	/*
	 * Return a long tag's data
	 */
	std::vector<char> get_data(bool list_ele) override;

	/*
	 * Return a long tag's value
	 */
	long long get_value(void) { return value; }

	/*
	 * Set a long tag's value
	 */
	void set_value(long long value) { this->value = value; }

	/*
	 * Return a string representation of a long tag
	 */
	std::string to_string(unsigned int tab) override;
};

#endif // LONG_TAG_H_
