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

#ifndef FLOAT_TAG_H_
#define FLOAT_TAG_H_

#include <string>
#include <vector>
#include "generic_tag.h"

class float_tag : public generic_tag {
private:

	/*
	 * Float tag value
	 */
	float value;

public:

	/*
	 * Float tag constructor
	 */
	float_tag(void) : generic_tag(FLOAT) { value = 0; }

	/*
	 * Float tag constructor
	 */
	float_tag(const float_tag &other) : generic_tag(other.name, FLOAT) { value = other.value; };

	/*
	 * Float tag constructor
	 */
	explicit float_tag(const std::string &name) : generic_tag(name, FLOAT) { value = 0; }

	/*
	 * Float tag constructor
	 */
	explicit float_tag(float value) : generic_tag(FLOAT) { this->value = value; }

	/*
	 * Float tag constructor
	 */
	float_tag(const std::string &name, float value) : generic_tag(name, FLOAT) { this->value = value; }

	/*
	 * Float tag destructor
	 */
	virtual ~float_tag(void) { return; }

	/*
	 * Float tag assignment operator
	 */
	float_tag &operator=(const float_tag &other);

	/*
	 * Float tag equals operator
	 */
	bool operator==(const generic_tag &other) override;

	/*
	 * Float tag not-equals operator
	 */
	bool operator!=(const generic_tag &other) override { return !(*this == other); }

	/*
	 * Return a float tag's data
	 */
	std::vector<char> get_data(bool list_ele) override;

	/*
	 * Return a float tag's value
	 */
	float get_value(void) { return value; }

	/*
	 * Set a float tag's value
	 */
	void set_value(float value) { this->value = value; }

	/*
	 * Return a string representation of a float tag
	 */
	std::string to_string(unsigned int tab) override;
};

#endif // FLOAT_TAG_H_
