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

#ifndef OCTREE_H
#define OCTREE_H

#include "array2d.h"
#include "point3d.h"

#include <algorithm>
#include <cassert>
#include <istream>
#include <ostream>

template< typename T, int AS = 1 >
class Octree
{
public:
    Octree( int size, const T& emptyValue = T(0) );
    Octree( const Octree<T,AS>& o );
    ~Octree();

    // Accessors
    int size() const;
    const T& emptyValue() const;

    static unsigned long branchBytes();
    static unsigned long aggregateBytes();
    static unsigned long leafBytes();
    unsigned long bytes() const;

    int nodes() const;
    int nodesAtSize( int size ) const;

    // Mutators
    void setEmptyValue( const T& emptyValue );

    void swap( Octree<T,AS>& o );
    Octree<T,AS>& operator= ( Octree<T,AS> o );

    // Indexing operators
    T& operator() ( int x, int y, int z );
    const T& operator() ( int x, int y, int z ) const;
    const T& at( int x, int y, int z ) const;

    void set( int x, int y, int z, const T& value );
    void erase( int x, int y, int z );

    Array2D<T> zSlice( int z ) const;

    // I/O functions
    void writeBinary( std::ostream& out ) const;
    void readBinary( std::istream& in );

protected:

    // Octree node types
    class Node;
    class Branch;
    class Aggregate;
    class Leaf;
    enum NodeType { BranchNode, AggregateNode, LeafNode };

    Node*& root();
    const Node* root() const;

    static void deleteNode( Node** node );

private:
    // Recursive helper functions
    void eraseRecursive( Node** node, int size, int x, int y, int z );
    static unsigned long bytesRecursive( const Node* node );
    static int nodesRecursive( const Node* node );
    static int nodesAtSizeRecursive( int targetSize, int size, Node* node );
    void zSliceRecursive( Array2D<T> slice, const Node* node, int size,
            int x, int y, int z, int targetZ ) const;
    static void writeBinaryRecursive( std::ostream& out, const Node* node );
    static void readBinaryRecursive( std::istream& in, Node** node );

protected:
    // Node classes

    class Node
    {
    public:
        NodeType type() const;

    protected:
        Node( NodeType type );
        ~Node() {};

    private:
        NodeType type_ : 2;
    };

    class Branch : public Node
    {
    public:
        Branch();
        Branch( const Branch& b );
        ~Branch();

        const Node* child( int x, int y, int z ) const;
        Node*& child( int x, int y, int z );
        const Node* child( int index ) const;
        Node*& child( int index );

        friend void Octree<T,AS>::deleteNode( Node** node );

    private:
        Branch& operator= ( Branch b );

    private:
        Node* children[2][2][2];
    };

    class Aggregate : public Node
    {
    public:
        Aggregate( const T& v );

        const T& value( int x, int y, int z ) const;
        T& value( int x, int y, int z );
        void setValue( int x, int y, int z, const T& v );

        const T& value( int i ) const;
        T& value( int i );
        void setValue( int i, const T& v );

        friend void Octree<T,AS>::deleteNode( Node** node );

    private:
        ~Aggregate() {};

    private:
        T value_[AS][AS][AS];
    };

    class Leaf : public Node
    {
    public:
        Leaf( const T& v );

        const T& value() const;
        T& value();
        void setValue( const T& v );

        friend void Octree<T,AS>::deleteNode( Node** node );

    private:
        ~Leaf() {};

    private:
        T value_;
    };

    static const int aggregateSize_ = AS;

private:
    Node* root_;
    T emptyValue_;
    int size_;
};

#include "octree.tcc"

#endif
