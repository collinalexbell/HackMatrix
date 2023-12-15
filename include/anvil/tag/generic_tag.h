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

#ifndef GENERIC_TAG_H_
#define GENERIC_TAG_H_

#include <sstream>
#include <string>
#include <vector>

class generic_tag {
public:

	/*
	 * Tag's name
	 */
	std::string name;

	/*
	 * Tag's type
	 */
	unsigned char type;

	/*
	 * Supported tag types
	 */
	enum TYPE {
		END, BYTE, SHORT, INT, LONG, FLOAT, DOUBLE, BYTE_ARRAY, STRING, LIST, COMPOUND, INT_ARRAY, LONG_ARRAY
	};

	/*
	 * Generic tag constructor
	 */
	generic_tag(void) : name(""), type(END) { return; }

	/*
	 * Generic tag constructor
	 */
	generic_tag(const generic_tag &other) : name(other.name), type(other.type) { return; }

	/*
	 * Generic tag constructor
	 */
	explicit generic_tag(unsigned char type) : name(""), type(type) { return; }

	/*
	 * Generic tag constructor
	 */
	generic_tag(const std::string &name, unsigned char type) : name(name), type(type) { return; }

	/*
	 * Generic tag destructor
	 */
	virtual ~generic_tag(void) { return; }

	/*
	 * Generic tag assignment operator
	 */
	generic_tag &operator=(const generic_tag &other);

	/*
	 * Generic tag equals operator
	 */
	virtual bool operator==(const generic_tag &other);

	/*
	 * Generic tag not-equals operator
	 */
	virtual bool operator!=(const generic_tag &other) { return !(*this == other); }

	/*
	 * Append a certain number of tabs to a given stringstream
	 */
	static void append_tabs(unsigned int tab, std::stringstream &ss);

	/*
	 * Return a generic tag's data
	 */
	virtual std::vector<char> get_data(bool list_ele) = 0;

	/*
	 * Return a generic tag's name
	 */
	std::string get_name(void) { return name; }

	/*
	 * Return a generic tag's type
	 */
	unsigned char get_type(void) { return type; }

	/*
	 * Set a generic tag's name
	 */
	void set_name(const std::string &name) { this->name = name; }

	/*
	 * Set a generic tag's type
	 */
	void set_type(unsigned char type) { this->type = type; }

	/*
	 * Return a string representation of a generic tag
	 */
	virtual std::string to_string(unsigned int tab);

	/*
	 * Return a string representation of a tag type
	 */
	static std::string type_to_string(unsigned char type);
};

#endif // GENERIC_TAG_H_
