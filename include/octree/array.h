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

#ifndef ARRAY_H
#define ARRAY_H

#include "shareddata.h"
#include "tinyvector.h"

template< typename T, int N >
class Array
{
public:
    Array();
    Array( const TinyVector<int,N>& sizes );

    const TinyVector<int,N>& sizes() const;

    const T& at( const TinyVector<int,N>& indices ) const;
    const T& operator() ( const TinyVector<int,N>& indices ) const;
    T& operator() ( const TinyVector<int,N>& indices );

    Array<T,N> subarray( const TinyVector<int,N>& begin,
                         const TinyVector<int,N>& end );

private:
    int dataIndex( const TinyVector<int,N>& indices ) const;

private:
    SharedData<T> data_;
    int offset_;
    TinyVector<int,N> strides_;
    TinyVector<int,N> sizes_;
};

template< typename T, int N >
Array<T,N>::Array()
    : data_(0)
    , sizes_(0)
{
}

template< typename T, int N >
Array<T,N>::Array( const TinyVector<int,N>& sizes )
    : data_( new T[prod(sizes)] )
    , offset_(0)
    , strides_( cumprod(sizes)/sizes(0) )
    , sizes_(sizes)
{
}

template< typename T, int N >
const TinyVector<int,N>& Array<T,N>::sizes() const
{
    return sizes_;
}

template< typename T, int N >
const T& Array<T,N>::at( const TinyVector<int,N>& indices ) const
{
    return data_[ dataIndex(indices) ];
}

template< typename T, int N >
const T& Array<T,N>::operator() ( const TinyVector<int,N>& indices ) const
{
    return at(indices);
}

template< typename T, int N >
T& Array<T,N>::operator() ( const TinyVector<int,N>& indices )
{
    return data_[ dataIndex(indices) ];
}

template< typename T, int N >
int Array<T,N>::dataIndex( const TinyVector<int,N>& indices ) const
{
    for ( int i = 0; i < N; ++i ) {
        assert( indices(i) >= 0 && indices(i) < sizes_(i) );
    }

    return offset_ + sum( indices * strides_ );
}

template< typename T, int N >
Array<T,N> Array<T,N>::subarray( const TinyVector<int,N>& begin,
                                 const TinyVector<int,N>& end )
{
    Array<T,N> sub;
    sub.data_ = data_;
    sub.offset_ = dataIndex(begin);
    sub.strides_ = strides_;
    sub.sizes_ = end - begin;
    return sub;
}

#endif
