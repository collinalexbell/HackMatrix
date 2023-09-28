/*
 *  Copyright (C) 2007  Simon Perreault
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef POINT3D_H
#define POINT3D_H

#include "tinyvector.h"

template< typename T >
class Point3D : public TinyVector<T,3>
{
public:
    Point3D() {}

    Point3D( T x, T y, T z )
    {
        (*this)(0) = x;
        (*this)(1) = y;
        (*this)(2) = z;
    }

    Point3D( const TinyVector<T,3>& v )
        : TinyVector<T,3>(v)
    {
    }

    const T& x() const { return this->at(0); }
    const T& y() const { return this->at(1); }
    const T& z() const { return this->at(2); }
    T& x() { return (*this)(0); }
    T& y() { return (*this)(1); }
    T& z() { return (*this)(2); }
};

#endif
