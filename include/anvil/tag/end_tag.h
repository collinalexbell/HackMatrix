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

#ifndef END_TAG_H_
#define END_TAG_H_

#include <string>
#include <vector>
#include "generic_tag.h"

class end_tag : public generic_tag {
public:

	/*
	 * End tag constructor
	 */
	end_tag(void) : generic_tag(END) { return; }

	/*
	 * End tag constructor
	 */
	end_tag(const end_tag &other) : generic_tag(other.name, END) { return; };

	/*
	 * End tag destructor
	 */
	virtual ~end_tag(void) { return; }

	/*
	 * End tag assignment operator
	 */
	end_tag &operator=(const end_tag &other);

	/*
	 * End tag equals operator
	 */
	bool operator==(const generic_tag &other) override;

	/*
	 * End tag not-equals operator
	 */
	bool operator!=(const generic_tag &other) override { return !(*this == other); }

	/*
	 * Return a end tag's data
	 */
	std::vector<char> get_data(bool list_ele) override;

	/*
	 * Return a string representation of a end tag
	 */
	std::string to_string(unsigned int tab) override { return generic_tag::to_string(tab); }
};

#endif // END_TAG_H_
