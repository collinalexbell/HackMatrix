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

#include <cstring>

/**
 * \class Octree
 * \brief Generic octree template
 *
 * \author Simon Perreault <nomis80@nomis80.org>
 * \date April 2007
 *
 * This class template represents an octree, often used for manipulating 3-D
 * scattered data efficiently. The type of the contained data is supplied as a
 * template parameter.
 *
 * \param T Type of the contained data. Requirements on type: must be copyable
 * and default-constructible.
 *
 * \param AS Short for "aggregate size." As an optimization, leaves can be
 * aggregated so that the relative size of pointers is diminished. This is 1 by
 * default, but should be set higher when the size of \a T is small. <b>Must be
 * a power of two.</b>
 */

/**
 * \param size Size of octree, in nodes. Should be a power of two. For example,
 * an octree with \a size = 256 will represent a cube divided into 256x256x256
 * nodes. <b>Must be a power of two.</b>
 *
 * \param emptyValue This is the value that will be returned when accessing
 * regions of the 3-D volume where no node has been allocated. In other words,
 * instead of following a null node pointer, this value is returned. Since the
 * octree root is initially a null pointer, the whole volume is initialized to
 * this value.
 */
template< typename T, int AS >
Octree<T,AS>::Octree( int size, const T& emptyValue )
    : root_(0)
    , emptyValue_(emptyValue)
    , size_(size)
{
    // Make sure size is power of two.
    assert( ((size - 1) & size) == 0 );
    assert( ((AS - 1) & AS) == 0 );
}

/**
 * Performs a deep copy of an octree. All branch pointers will be followed
 * recursively and new nodes will be allocated.
 *
 * \param o Octree to be copied.
 */
template< typename T, int AS >
Octree<T,AS>::Octree( const Octree<T,AS>& o )
    : emptyValue_( o.emptyValue_ )
    , size_( o.size_ )
{
    if ( !o.root_ ) {
        root_ = 0;
    } else {
        switch ( o.root_->type() ) {
            case BranchNode:
                root_ = new Branch   ( *reinterpret_cast<Branch   *>(o.root_) );
                break;
            case LeafNode:
                root_ = new Leaf     ( *reinterpret_cast<Leaf     *>(o.root_) );
                break;
            case AggregateNode:
                root_ = new Aggregate( *reinterpret_cast<Aggregate*>(o.root_) );
                break;
        }
    }
}

/**
 * Recursively deletes all nodes by following branch pointers.
 */
template< typename T, int AS >
Octree<T,AS>::~Octree()
{
    deleteNode(&root_);
}

/**
 * Swaps the octree's contents with another's. This is a cheap operation as only
 * the root pointers are swapped, not the whole structure.
 */
template< typename T, int AS >
void Octree<T,AS>::swap( Octree<T,AS>& o )
{
    std::swap( emptyValue_, o.emptyValue_ );  // This can throw.

    // These can't.
    std::swap( root_, o.root_ );
    std::swap( size_, o.size_ );
}

/**
 * Assigns to this octree the contents of octree \a o.
 */
template< typename T, int AS >
Octree<T,AS>& Octree<T,AS>::operator= ( Octree<T,AS> o )
{
    swap(o);
    return *this;
}

/**
 * \return Size of octree, in nodes, as specified in the constructor.
 */
template< typename T, int AS >
int Octree<T,AS>::size() const
{
    return size_;
}

/**
 * \return Value of empty nodes, as specified in the constructor.
 * \see setEmptyValue()
 */
template< typename T, int AS >
const T& Octree<T,AS>::emptyValue() const
{
    return emptyValue_;
}

/**
 * Sets the value of empty nodes to \a emptyValue.
 * \see setEmptyValue()
 */
template< typename T, int AS >
void Octree<T,AS>::setEmptyValue( const T& emptyValue )
{
    emptyValue_ = emptyValue;
}

/**
 * Deletes a node polymorphically. If the node is a branch node, it will delete
 * all its subtree recursively.
 */
template< typename T, int AS >
void Octree<T,AS>::deleteNode( Node** node )
{
    assert(node);
    if (*node) {
        if ( (*node)->type() == BranchNode ) {
            delete reinterpret_cast<Branch*>(*node);
        }
        else if ( (*node)->type() == AggregateNode ) {
            delete reinterpret_cast<Aggregate*>(*node);
        }
        else {
            assert( (*node)->type() == LeafNode );
            delete reinterpret_cast<Leaf*>(*node);
        }
        *node = 0;
    }
}

/**
 * \return Pointer to octree's root node.
 */
template< typename T, int AS >
typename Octree<T,AS>::Node*& Octree<T,AS>::root()
{
    return root_;
}

/**
 * Const version of above.
 */
template< typename T, int AS >
const typename Octree<T,AS>::Node* Octree<T,AS>::root() const
{
    return root_;
}

/**
 * \return Value at index (\a x,\a y,\a z). If no node exists at this index, the
 * value returned by emptyValue() is returned.
 *
 * \remarks Memory access is faster when \a x varies the quickest, followed by
 * \a y and then by \a z. Therefore you should write nested loops in this order
 * for faster access:
 *
 * \code
 * for ( int z = 0; z < ...; ++z ) {
 *     for ( int y = 0; y < ...; ++y ) {
 *         for ( int x = 0; x < ...; ++x ) {
 *             ... = octree.at(x,y,z);
 *         }
 *     }
 * }
 * \endcode
 *
 * However, zSlice() provides an even faster way.
 */
template< typename T, int AS >
const T& Octree<T,AS>::at( int x, int y, int z ) const
{
    assert( x >= 0 && x < size_ );
    assert( y >= 0 && y < size_ );
    assert( z >= 0 && z < size_ );

    Node* n = root_;
    int size = size_;

    while ( size != aggregateSize_ ) {
        if (!n) {
            return emptyValue_;
        }
        else if ( n->type() == BranchNode ) {
            size /= 2;
            n = reinterpret_cast<Branch*>(n)->child(
                    !!(x & size), !!(y & size), !!(z & size) );
        }
        else {
            assert( n->type() == LeafNode );
            return reinterpret_cast<Leaf*>(n)->value();
        }
    }

    if (!n) {
        return emptyValue_;
    }

    --size;
    return reinterpret_cast<Aggregate*>(n)->value(
            x & size, y & size, z & size );
}

/**
 * Synonym of at().
 */
template< typename T, int AS >
const T& Octree<T,AS>::operator() ( int x, int y, int z ) const
{
    return at(x,y,z);
}

/**
 * \return Reference to value at index (\a x,\a y,\a z). If no node exists at
 * this index, a new one is created (along with the necessary ancestry),
 * initialized to the value returned by emptyValue(), and returned.
 *
 * \remarks Be careful when calling this function. If you do not want to
 * inadvertently create new nodes, use the at() function.
 *
 * \see at()
 */
template< typename T, int AS >
T& Octree<T,AS>::operator() ( int x, int y, int z )
{
    assert( x >= 0 && x < size_ );
    assert( y >= 0 && y < size_ );
    assert( z >= 0 && z < size_ );

    Node** n = &root_;
    int size = size_;

    while ( size != aggregateSize_ ) {
        if (!*n) {
            *n = new Branch;
        }
        else if ( (*n)->type() == BranchNode ) {
            size /= 2;
            n = &reinterpret_cast<Branch*>(*n)->child(
                    !!(x & size), !!(y & size), !!(z & size) );
        }
        else {
            return reinterpret_cast<Leaf*>(*n)->value();
        }
    }

    if (!*n) {
        *n = new Aggregate(emptyValue_);
    }

    --size;
    return reinterpret_cast<Aggregate*>(*n)->value(
            x & size, y & size, z & size );
}

/**
 * Sets the value of the node at (\a x, \a y, \a z) to \a value. If \a value is
 * the empty value, the node is erased. Otherwise, the node is created if it did
 * not already exist and its value is set to \a value.
 */
template< typename T, int AS >
void Octree<T,AS>::set( int x, int y, int z, const T& value )
{
    if ( value != emptyValue() ) {
        (*this)(x,y,z) = value;
    }
    else {
        erase(x,y,z);
    }
}

/**
 * Erases the node at index (\a x,\a y,\a z). After the call,
 * <code>at(x,y,z)</code> will return the value returned by emptyValue().
 *
 * This function will free as much memory as possible. For example, when erasing
 * the single child of a branch node, the branch node itself will be erased and
 * replaced by a null pointer in its parent. This will percolate to the top of
 * the tree if necessary.
 */
template< typename T, int AS >
void Octree<T,AS>::erase( int x, int y, int z )
{
    assert( x >= 0 && x < size_ );
    assert( y >= 0 && y < size_ );
    assert( z >= 0 && z < size_ );

    eraseRecursive( &root_, size_, x, y, z );
}

/**
 * Helper function for erase() method.
 */
template< typename T, int AS >
void Octree<T,AS>::eraseRecursive( Node** node, int size, int x, int y, int z )
{
    assert(node);

    if ( !*node ) {
        return;
    }

    if ( size != aggregateSize_ ) {
        if ( (*node)->type() == BranchNode ) {
            size /= 2;
            Branch* b = reinterpret_cast<Branch*>(*node);
            eraseRecursive( &b->child(!!(x & size), !!(y & size), !!(z & size)),
                    size, x, y, z );

            for ( int i = 0; i < 8; ++i ) {
                if ( b->child(i) ) {
                    return;
                }
            }
            deleteNode(node);
        }
        else if ( reinterpret_cast<Leaf*>(*node)->value() == emptyValue_ ) {
            deleteNode(node);
        }
        else {
            Branch* b = new Branch;
            size /= 2;
            int childIndex = ( x & size ? 1 : 0 )
                           | ( y & size ? 2 : 0 )
                           | ( z & size ? 4 : 0 );
            const T& value = reinterpret_cast<Leaf*>(*node)->value();
            try {
                for ( int i = 0; i < 8; ++i ) {
                    if ( i == childIndex ) {
                        continue;
                    }
                    if ( size == aggregateSize_ ) {
                        b->child(i) = new Leaf(value);
                    }
                    else {
                        b->child(i) = new Aggregate(value);
                    }
                }
            }
            catch (...) {
                Node* bb = b;
                deleteNode(&bb);
                throw;
            }

            deleteNode(node);
            *node = b;
            node = &b->child(childIndex);
        }
    }
    else {
        --size;
        Aggregate* a = reinterpret_cast<Aggregate*>(*node);
        a->setValue( x & size, y & size, z & size, emptyValue_ );

        for ( int i = 0; i < AS*AS*AS; ++i ) {
            if ( a->value(i) != emptyValue_ ) {
                return;
            }
        }
        deleteNode(node);
    }
}

/**
 * \return Number of bytes a branch node occupies.
 */
template< typename T, int AS >
unsigned long Octree<T,AS>::branchBytes()
{
    return sizeof(Branch);
}

/**
 * \return Number of bytes an aggregate node occupies.
 */
template< typename T, int AS >
unsigned long Octree<T,AS>::aggregateBytes()
{
    return sizeof(Aggregate);
}

/**
 * \return Number of bytes a leaf node occupies.
 */
template< typename T, int AS >
unsigned long Octree<T,AS>::leafBytes()
{
    return sizeof(Leaf);
}

/**
 * \return Total number of nodes in the octree.
 */
template< typename T, int AS >
int Octree<T,AS>::nodes() const
{
    return nodesRecursive(root_);
}

/**
 * Helper function for nodes() method.
 */
template< typename T, int AS >
int Octree<T,AS>::nodesRecursive( const Node* node )
{
    if ( !node ) {
        return 0;
    }

    int n = 1;
    if ( node->type() == BranchNode ) {
        for ( int i = 0; i < 8; ++i ) {
            n += nodesRecursive(
                    reinterpret_cast<const Branch*>(node)->child(i) );
        }
    }

    return n;
}

/**
 * \return Total number of bytes the octree occupies.
 *
 * \remarks Memory fragmentation may make the actual memory usage significantly
 * higher.
 */
template< typename T, int AS >
unsigned long Octree<T,AS>::bytes() const
{
    return bytesRecursive(root_) + sizeof(*this);
}

/**
 * Helper function for bytes() method.
 */
template< typename T, int AS >
unsigned long Octree<T,AS>::bytesRecursive( const Node* node )
{
    if ( !node ) {
        return 0;
    }

    unsigned long b = 0;
    switch ( node->type() ) {
        case BranchNode:
            b = sizeof(Branch);
            for ( int i = 0; i < 8; ++i ) {
                b += bytesRecursive(
                        reinterpret_cast<const Branch*>(node)->child(i) );
            }
            break;

        case LeafNode:
            b = sizeof(Leaf);
            break;

        case AggregateNode:
            b = sizeof(Aggregate);
            break;
    }

    return b;
}

/**
 * \return Number of nodes of at size \a size. For example, the root (if
 * allocated) is the single node of size 1. At size <i>n</i> there may be a
 * maximum of 2<sup><i>n</i></sup> nodes.
 *
 * For sizes lower than the aggregate size, this function will always return
 * zero.
 */
template< typename T, int AS >
int Octree<T,AS>::nodesAtSize( int size ) const
{
    return nodesAtSizeRecursive( size, size_, root_ );
}

/**
 * Helper function for nodesAtSize() method.
 */
template< typename T, int AS >
int Octree<T,AS>::nodesAtSizeRecursive( int targetSize, int size, Node* node )
{
    if (node) {
        if ( size == targetSize ) {
            return 1;
        }

        if ( node->type() == BranchNode ) {
            int sum = 0;
            for ( int i = 0; i < 2; ++i ) {
                for ( int j = 0; j < 2; ++j ) {
                    for ( int k = 0; k < 2; ++k ) {
                        sum += nodesAtSizeRecursive( targetSize, size/2,
                                reinterpret_cast<Branch*>(node)->child(k,j,i) );
                    }
                }
            }
            return sum;
        }
    }

    return 0;
}

template< typename T, int AS >
Octree<T,AS>::Node::Node( NodeType type )
    : type_(type)
{
}

template< typename T, int AS >
typename Octree<T,AS>::NodeType Octree<T,AS>::Node::type() const
{
    return type_;
}

template< typename T, int AS >
Octree<T,AS>::Branch::Branch()
    : Node(BranchNode)
{
    memset( children, 0, sizeof(children) );
}

template< typename T, int AS >
Octree<T,AS>::Branch::Branch( const Branch& b )
    : Node(BranchNode)
{
    for ( int i = 0; i < 8; ++i ) {
        if ( b.child(i) ) {
            switch ( b.child(i)->type() ) {
                case BranchNode:
                    child(i) = new Branch(
                            *reinterpret_cast<const Branch   *>(b.child(i)) );
                    break;
                case LeafNode:
                    child(i) = new Leaf(
                            *reinterpret_cast<const Leaf     *>(b.child(i)) );
                    break;
                case AggregateNode:
                    child(i) = new Aggregate(
                            *reinterpret_cast<const Aggregate*>(b.child(i)) );
                    break;
            }
        }
        else {
            child(i) = 0;
        }
    }
}

template< typename T, int AS >
Octree<T,AS>::Branch::~Branch()
{
    for ( int i = 0; i < 2; ++i ) {
        for ( int j = 0; j < 2; ++j ) {
            for ( int k = 0; k < 2; ++k ) {
                assert( children[i][j][k] != this );
                deleteNode( &children[i][j][k] );
            }
        }
    }
}

template< typename T, int AS >
const typename Octree<T,AS>::Node* Octree<T,AS>::Branch::child(
        int x, int y, int z ) const
{
    assert( x == 0 || x == 1 );
    assert( y == 0 || y == 1 );
    assert( z == 0 || z == 1 );
    return children[z][y][x];
}

template< typename T, int AS >
typename Octree<T,AS>::Node*& Octree<T,AS>::Branch::child( int x, int y, int z )
{
    assert( x == 0 || x == 1 );
    assert( y == 0 || y == 1 );
    assert( z == 0 || z == 1 );
    return children[z][y][x];
}

template< typename T, int AS >
const typename Octree<T,AS>::Node* Octree<T,AS>::Branch::child( int index )
    const
{
    assert( index >= 0 && index < 8 );
    return *( &children[0][0][0] + index );
}

template< typename T, int AS >
typename Octree<T,AS>::Node*& Octree<T,AS>::Branch::child( int index )
{
    assert( index >= 0 && index < 8 );
    return *( &children[0][0][0] + index );
}

template< typename T, int AS >
Octree<T,AS>::Aggregate::Aggregate( const T& v )
    : Node(AggregateNode)
{
    for ( int i = 0; i < AS; ++i ) {
        for ( int j = 0; j < AS; ++j ) {
            for ( int k = 0; k < AS; ++k ) {
                value_[i][j][k] = v;
            }
        }
    }
}

template< typename T, int AS >
const T& Octree<T,AS>::Aggregate::value( int x, int y, int z ) const
{
    assert( x >= 0 && x < AS );
    assert( y >= 0 && y < AS );
    assert( z >= 0 && z < AS );

    return value_[z][y][x];
}

template< typename T, int AS >
T& Octree<T,AS>::Aggregate::value( int x, int y, int z )
{
    assert( x >= 0 && x < AS );
    assert( y >= 0 && y < AS );
    assert( z >= 0 && z < AS );

    return value_[z][y][x];
}

template< typename T, int AS >
void Octree<T,AS>::Aggregate::setValue( int x, int y, int z, const T& v )
{
    assert( x >= 0 && x < AS );
    assert( y >= 0 && y < AS );
    assert( z >= 0 && z < AS );

    value_[z][y][x] = v;
}

template< typename T, int AS >
const T& Octree<T,AS>::Aggregate::value( int i ) const
{
    assert( i >= 0 && i < AS*AS*AS );

    return *( &value_[0][0][0] + i );
}

template< typename T, int AS >
T& Octree<T,AS>::Aggregate::value( int i )
{
    assert( i >= 0 && i < AS*AS*AS );

    return *( &value_[0][0][0] + i );
}

template< typename T, int AS >
void Octree<T,AS>::Aggregate::setValue( int i, const T& v )
{
    assert( i >= 0 && i < AS*AS*AS );

    *( &value_[0][0][0] + i ) = v;
}

template< typename T, int AS >
Octree<T,AS>::Leaf::Leaf( const T& v )
    : Node(LeafNode)
    , value_(v)
{
}

template< typename T, int AS >
const T& Octree<T,AS>::Leaf::value() const
{
    return value_;
}

template< typename T, int AS >
T& Octree<T,AS>::Leaf::value()
{
    return value_;
}

template< typename T, int AS >
void Octree<T,AS>::Leaf::setValue( const T& v )
{
    value_ = v;
}

/**
 * \return A slice of the octree, perpendicular to the Z axis. The content of
 * all nodes for which the Z index is \a z will be copied into the returned
 * array. If no node exists for a given index, the value returned by
 * emptyValue() will be written instead.
 *
 * \remarks This method ought to be relatively fast as long the the time
 * required to copy values does not dwarf the time for indexing into the octree
 * (this should be the case for built-in C++ types such as int and double).
 * As a result, using this function is an easy way to accelerate the infamous
 * three-level nested loops. For example:
 *
 * \code
 * for ( int z = 0; z < ...; ++z ) {
 *     tmp = octree.zSlice(z);
 *     for ( int y = 0; y < ...; ++y ) {
 *         for ( int x = 0; x < ...; ++x ) {
 *             ... = tmp(y,x);
 *         }
 *     }
 * }
 * \endcode
 */
template< typename T, int AS >
Array2D<T> Octree<T,AS>::zSlice( int z ) const
{
    assert( z >= 0 && z < size_ );

    Array2D<T> slice( size_, size_ );
    zSliceRecursive( slice, root_, size_, 0, 0, 0, z );

    return slice;
}

/**
 * Helper function for zSlice() method.
 */
template< typename T, int AS >
void Octree<T,AS>::zSliceRecursive( Array2D<T> slice, const Node* node,
        int size, int x, int y, int z, int targetZ ) const
{
    if (!node) {
        for ( int i = 0; i < slice.M(); ++i ) {
            for ( int j = 0; j < slice.N(); ++j ) {
                slice(i,j) = emptyValue_;
            }
        }
    }
    else if ( node->type() == BranchNode ) {
        size /= 2;
        for ( int i = 0; i < 2; ++i ) {
            for ( int j = 0; j < 2; ++j ) {
                zSliceRecursive( slice.subarray(    i*size,     j*size,
                                                (i+1)*size, (j+1)*size),
                        reinterpret_cast<const Branch*>(node)->child(
                            j, i, !!(targetZ & size)),
                        size, x, y, z, targetZ );
            }
        }
    }
    else if ( node->type() == AggregateNode ) {
        for ( int i = 0; i < slice.M(); ++i ) {
            for ( int j = 0; j < slice.N(); ++j ) {
                slice(i,j) = reinterpret_cast<const Aggregate*>(node)->value(
                        j, i, targetZ - z & (size-1) );
            }
        }
    }
    else {
        assert( node->type() == LeafNode );
        for ( int i = 0; i < slice.M(); ++i ) {
            for ( int j = 0; j < slice.N(); ++j ) {
                slice(i,j) = reinterpret_cast<const Leaf*>(node)->value();
            }
        }
    }
}

/**
 * Writes the octree in binary form to the output stream \a out. This should be
 * fast, but note that the type \a T will be written as it appears in memory.
 * That is, if it is a complex type containing pointers, the pointer addresses
 * will be written instead of the data pointed at. For complex types, you should
 * roll your own function.
 */
template< typename T, int AS >
void Octree<T,AS>::writeBinary( std::ostream& out ) const
{
    if ( !root_ ) {
        static const char zero = 0;
        out.write( &zero, 1 );
    }
    else {
        static const char one = 1;
        out.write( &one, 1 );

        writeBinaryRecursive( out, root() );
    }

    out.write( reinterpret_cast<const char*>(&emptyValue_), sizeof(T) );
    out.write( reinterpret_cast<const char*>(&size_), sizeof(int) );
}

template< typename T, int AS >
void Octree<T,AS>::writeBinaryRecursive( std::ostream& out, const Node* node )
{
    assert(node);

    if ( !out.good() ) {
        return;
    }

    char type = node->type();
    out.write( &type, 1 );

    switch (type) {
        case BranchNode:
            {
                const Branch* b = reinterpret_cast<const Branch*>(node);

                char children = 0;
                for ( int i = 0; i < 8; ++i ) {
                    children |= ( b->child(i) != 0 ) << i;
                }
                out.write( &children, 1 );

                for ( int i = 0; i < 8; ++i ) {
                    if ( b->child(i) ) {
                        writeBinaryRecursive( out, b->child(i) );
                    }
                }
            }
            break;

        case AggregateNode:
            out.write( reinterpret_cast<const char*>(
                        &reinterpret_cast<const Aggregate*>(node)->value(0,0,0)
                        ),
                    AS*AS*AS*sizeof(T) );
            break;

        case LeafNode:
            out.write( reinterpret_cast<const char*>(
                        &reinterpret_cast<const Leaf*>(node)->value()
                        ),
                    sizeof(T) );
            break;
    }
}

/**
 * Reads the octree from \a in. It must previously have been written using
 * writeBinary().
 */
template< typename T, int AS >
void Octree<T,AS>::readBinary( std::istream& in )
{
    Octree<T,AS> tmp(0);

    char root;
    in.read( &root, 1 );
    if (root) {
        readBinaryRecursive( in, &tmp.root_ );
    }

    in.read( reinterpret_cast<char*>(&tmp.emptyValue_), sizeof(T) );
    in.read( reinterpret_cast<char*>(&tmp.size_), sizeof(int) );

    if ( in.good() ) {
        swap(tmp);
    }
}

template< typename T, int AS >
void Octree<T,AS>::readBinaryRecursive( std::istream& in, Node** node )
{
    assert(node);

    if ( !in.good() ) {
        return;
    }

    char type;
    in.read( &type, 1 );

    switch (type) {
        case BranchNode:
            {
                Branch* b = new Branch;
                *node = b;

                char children;
                in.read( &children, 1 );

                for ( int i = 0; i < 8; ++i ) {
                    if ( children & (1 << i) ) {
                        readBinaryRecursive( in, &b->child(i) );
                    }
                }
            }
            break;

        case AggregateNode:
            {
                Aggregate* a = new Aggregate( T(0) );
                *node = a;
                in.read( reinterpret_cast<char*>(&a->value(0,0,0)),
                        AS*AS*AS*sizeof(T) );
            }
            break;

        case LeafNode:
            {
                Leaf* l = new Leaf( T(0) );
                *node = l;
                in.read( reinterpret_cast<char*>(&l->value()), sizeof(T) );
            }
            break;
    }
}
