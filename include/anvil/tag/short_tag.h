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

#ifndef SHORT_TAG_H_
#define SHORT_TAG_H_

#include <string>
#include <vector>
#include "generic_tag.h"

class short_tag : public generic_tag {
private:

	/*
	 * Short tag value
	 */
	short value;

public:

	/*
	 * Short tag constructor
	 */
	short_tag(void) : generic_tag(SHORT) { value = 0; }

	/*
	 * Short tag constructor
	 */
	short_tag(const short_tag &other) : generic_tag(other.name, SHORT) { value = other.value; };

	/*
	 * Short tag constructor
	 */
	explicit short_tag(const std::string &name) : generic_tag(name, SHORT) { value = 0; }

	/*
	 * Short tag constructor
	 */
	explicit short_tag(short value) : generic_tag(SHORT) { this->value = value; }

	/*
	 * Short tag constructor
	 */
	short_tag(const std::string &name, short value) : generic_tag(name, SHORT) { this->value = value; }

	/*
	 * Short tag destructor
	 */
	virtual ~short_tag(void) { return; }

	/*
	 * Short tag assignment operator
	 */
	short_tag &operator=(const short_tag &other);

	/*
	 * Short tag equals operator
	 */
	bool operator==(const generic_tag &other) override;

	/*
	 * Short tag not-equals operator
	 */
	bool operator!=(const generic_tag &other) override { return !(*this == other); }

	/*
	 * Return a short tag's data
	 */
	std::vector<char> get_data(bool list_ele) override;

	/*
	 * Return a short tag's value
	 */
	short get_value(void) { return value; }

	/*
	 * Set a short tag's value
	 */
	void set_value(short value) { this->value = value; }

	/*
	 * Return a string representation of a short tag
	 */
	std::string to_string(unsigned int tab) override;
};

#endif // SHORT_TAG_H_
