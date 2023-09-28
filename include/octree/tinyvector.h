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

#ifndef TINYVECTOR_H
#define TINYVECTOR_H

#include "numtraits.h"

template< typename T, int N >
class TinyVector
{
public:
    TinyVector();
    TinyVector( const T& value );

    template< typename T2 >
    TinyVector( const TinyVector<T2,N>& v );

    template< typename T2 >
    TinyVector<T,N>& operator= ( const TinyVector<T2,N>& v );

    const T& at( int i ) const { return data_[i]; }
    const T& operator() ( int i ) const { return at(i); }
    T& operator() ( int i ) { return data_[i]; }

    const T* data() const { return data_; }
    T* data() { return data_; }

    void operator+= ( const TinyVector<T,N>& v )
    {
        for ( int i = 0; i < N; ++i ) {
            data_[i] += v.data_[i];
        }
    }

    void operator-= ( const TinyVector<T,N>& v )
    {
        for ( int i = 0; i < N; ++i ) {
            data_[i] -= v.data_[i];
        }
    }

    void operator*= ( const TinyVector<T,N>& v )
    {
        for ( int i = 0; i < N; ++i ) {
            data_[i] *= v.data_[i];
        }
    }

    void operator*= ( const T& x )
    {
        for ( int i = 0; i < N; ++i ) {
            data_[i] *= x;
        }
    }

    void operator/= ( const T& x )
    {
        for ( int i = 0; i < N; ++i ) {
            data_[i] /= x;
        }
    }

private:
    T data_[N];
};

template< typename T, int N >
TinyVector<T,N>::TinyVector()
{
}

template< typename T, int N >
TinyVector<T,N>::TinyVector( const T& value )
{
    for ( int i = 0; i < N; ++i ) {
        data_[i] = value;
    }
}

template< typename T, int N >
template< typename T2 >
TinyVector<T,N>::TinyVector( const TinyVector<T2,N>& v )
{
    *this = v;
}

template< typename T, int N >
template< typename T2 >
TinyVector<T,N>& TinyVector<T,N>::operator= ( const TinyVector<T2,N>& v )
{
    for ( int i = 0; i < N; ++i ) {
        data_[i] = v(i);
    }
    return *this;
}

template< typename T1, typename T2, int N >
TinyVector< typename BinaryNumericTraits<T1,T2>::OpResult, N > operator+ (
        const TinyVector<T1,N>& v1, const TinyVector<T2,N>& v2 )
{
    TinyVector< typename BinaryNumericTraits<T1,T2>::OpResult, N > v3(v1);
    v3 += v2;
    return v3;
}

template< typename T1, typename T2, int N >
TinyVector< typename BinaryNumericTraits<T1,T2>::OpResult, N > operator- (
        const TinyVector<T1,N>& v1, const TinyVector<T2,N>& v2 )
{
    TinyVector< typename BinaryNumericTraits<T1,T2>::OpResult, N > v3(v1);
    v3 -= v2;
    return v3;
}

template< typename T1, typename T2, int N >
TinyVector< typename BinaryNumericTraits<T1,T2>::OpResult, N > operator* (
        const TinyVector<T1,N>& v1, const TinyVector<T2,N>& v2 )
{
    TinyVector< typename BinaryNumericTraits<T1,T2>::OpResult, N > v3(v1);
    v3 *= v2;
    return v3;
}

template< typename T1, typename T2, int N >
TinyVector< typename BinaryNumericTraits<T1,T2>::OpResult, N > operator* (
        const TinyVector<T1,N>& v1, const T2& x )
{
    TinyVector< typename BinaryNumericTraits<T1,T2>::OpResult, N > v2(v1);
    v2 *= x;
    return v2;
}

template< typename T, int N >
TinyVector<T,N> operator* ( const T& x, TinyVector<T,N> v1 )
{
    for ( int i = 0; i < N; ++i ) {
        v1(i) = x * v1(i);
    }
    return v1;
}

template< typename T1, typename T2, int N >
TinyVector< typename BinaryNumericTraits<T1,T2>::OpResult, N > operator* (
        const T2& x, const TinyVector<T1,N>& v1 )
{
    return x *
        TinyVector< typename BinaryNumericTraits<T1,T2>::OpResult, N >(v1);
}

template< typename T1, typename T2, int N >
TinyVector< typename BinaryNumericTraits<T1,T2>::OpResult, N > operator/ (
        const TinyVector<T1,N>& v1, const TinyVector<T2,N>& v2 )
{
    TinyVector< typename BinaryNumericTraits<T1,T2>::OpResult, N > v3(v1);
    v3 /= v2;
    return v3;
}

template< typename T1, typename T2, int N >
TinyVector< typename BinaryNumericTraits<T1,T2>::OpResult, N > operator/ (
        const TinyVector<T1,N>& v1, const T2& x )
{
    TinyVector< typename BinaryNumericTraits<T1,T2>::OpResult, N > v2(v1);
    v2 /= x;
    return v2;
}

template< typename T, int N >
T sum( const TinyVector<T,N>& v )
{
    T sum(0);
    for ( int i = 0; i < N; ++i ) {
        sum += v(i);
    }
    return sum;
}

template< typename T, int N >
T prod( const TinyVector<T,N>& v )
{
    T prod(1);
    for ( int i = 0; i < N; ++i ) {
        prod *= v(i);
    }
    return prod;
}

template< typename T, int N >
TinyVector<T,N> cumprod( const TinyVector<T,N>& v )
{
    TinyVector<T,N> p;
    T prev(1);
    for ( int i = 0; i < N; ++i ) {
        p(i) = prev = prev * v(i);
    }
    return p;
}

template< typename T, int N >
T norm( const TinyVector<T,N>& v )
{
    return sqrt( sum(v*v) );
}


#endif
