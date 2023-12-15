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

#ifndef DOUBLE_TAG_H_
#define DOUBLE_TAG_H_

#include <string>
#include <vector>
#include "generic_tag.h"

class double_tag : public generic_tag {
private:

	/*
	 * Double tag value
	 */
	double value;

public:

	/*
	 * Double tag constructor
	 */
	double_tag(void) : generic_tag(DOUBLE) { value = 0; }

	/*
	 * Double tag constructor
	 */
	double_tag(const double_tag &other) : generic_tag(other.name, DOUBLE) { value = other.value; };

	/*
	 * Double tag constructor
	 */
	explicit double_tag(const std::string &name) : generic_tag(name, DOUBLE) { value = 0; }

	/*
	 * Double tag constructor
	 */
	explicit double_tag(double value) : generic_tag(DOUBLE) { this->value = value; }

	/*
	 * Double tag constructor
	 */
	double_tag(const std::string &name, double value) : generic_tag(name, DOUBLE) { this->value = value; }

	/*
	 * Double tag destructor
	 */
	virtual ~double_tag(void) { return; }

	/*
	 * Double tag assignment operator
	 */
	double_tag &operator=(const double_tag &other);

	/*
	 * Double tag equals operator
	 */
	bool operator==(const generic_tag &other) override;

	/*
	 * Double tag not-equals operator
	 */
	bool operator!=(const generic_tag &other) override { return !(*this == other); }

	/*
	 * Return a double tag's data
	 */
	std::vector<char> get_data(bool list_ele) override;

	/*
	 * Return a double tag's value
	 */
	double get_value(void) { return value; }

	/*
	 * Set a double tag's value
	 */
	void set_value(double value) { this->value = value; }

	/*
	 * Return a string representation of a double tag
	 */
	std::string to_string(unsigned int tab) override;
};

#endif // DOUBLE_TAG_H_
