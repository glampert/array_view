
// ================================================================================================
// -*- C++ -*-
// File: array_view.hpp
// Author: Guilherme R. Lampert
// Created on: 12/06/16
//
// About:
//  Implementation of array_view<T> and strided_array_view<T>,
//  inspired by span<T> from GSL (AKA the C++ Core Guidelines).
//
// License:
//  This software is in the public domain. Where that dedication is not recognized,
//  you are granted a perpetual, irrevocable license to copy, distribute, and modify
//  this file as you see fit. Source code is provided "as is", without warranty of any
//  kind, express or implied. No attribution is required, but a mention about the author
//  is appreciated.
// ================================================================================================

#ifndef ARRAY_VIEW_HPP
#define ARRAY_VIEW_HPP

// Defining this before including the file prevents pulling the Standard headers.
// Useful to be able to place this file inside a user-defined namespace or to simply
// avoid redundant inclusions. User is responsible for providing all the necessary
// Standard headers before #including this one.
#ifndef ARRAY_VIEW_NO_STD_INCLUDES
    #include <cstddef>
    #include <cstdint>
    #include <utility>
    #include <iterator>
    #include <algorithm>
    #include <type_traits>
#endif // ARRAY_VIEW_NO_STD_INCLUDES

// ========================================================
// Configuration switches:
// ========================================================

//
// Define this switch to nonzero to enable additional asserts
// and iterator validation in array_view and strided_array_view.
// Bounds checking on operator[] is only enabled if this is
// defined to a true.
//
#ifndef ARRAY_VIEW_DEBUG_CHECKS
    #if defined(DEBUG) || defined(_DEBUG)
        #define ARRAY_VIEW_DEBUG_CHECKS 1
    #endif // DEBUG || _DEBUG
#endif // ARRAY_VIEW_DEBUG_CHECKS

//
// Define this switch to nonzero to use C++ exceptions for error
// reporting when ARRAY_VIEW_DEBUG_CHECKS is enabled. If not defined
// or defined to zero, a message is logged to std::cerr and std::abort
// is called on error.
//
//#define ARRAY_VIEW_USE_CXX_EXCEPTIONS 1

// ========================================================
// array_view helpers:
// ========================================================

// You're free to redefine this macro and supply your own error handling strategy.
#ifndef ARRAY_VIEW_ERROR
    #if ARRAY_VIEW_USE_CXX_EXCEPTIONS
        #ifndef ARRAY_VIEW_NO_STD_INCLUDES
            #include <string>
            #include <stdexcept>
        #endif // ARRAY_VIEW_NO_STD_INCLUDES
        #define ARRAY_VIEW_ERROR(message) \
            throw std::runtime_error{ std::string(__FILE__) + "(" + std::to_string(__LINE__) + "): " + std::string(message) }
    #else // !ARRAY_VIEW_USE_CXX_EXCEPTIONS
        #ifndef ARRAY_VIEW_NO_STD_INCLUDES
            #include <cstdlib>
            #include <iostream>
        #endif // ARRAY_VIEW_NO_STD_INCLUDES
        #define ARRAY_VIEW_ERROR(message) \
            (std::cerr << __FILE__ << "(" << __LINE__ << "): " << message << std::endl, std::abort())
    #endif // ARRAY_VIEW_USE_CXX_EXCEPTIONS
#endif // ARRAY_VIEW_ERROR

// Size in items of statically declared C-style arrays.
template<typename ArrayType, std::size_t ArraySize>
constexpr std::size_t array_size(const ArrayType (&)[ArraySize]) noexcept
{
    return ArraySize;
}

// ========================================================
// template class array_iterator_base and friends:
// ========================================================

namespace array_view_detail
{
    struct mutable_iterator_tag { };
    struct const_iterator_tag   { };
} // namespace array_view_detail {}

template
<
    typename T,
    typename ParentType,
    typename AccessQualifierTag
>
class array_iterator_base
    : public std::iterator<std::random_access_iterator_tag, T>
{
public:

    //
    // Nested types:
    //

    using parent_type          = ParentType;
    using access_tag_type      = AccessQualifierTag;
    using parent_iterator_type = std::iterator<std::random_access_iterator_tag, T>;

    using size_type            = std::size_t;
    using difference_type      = std::ptrdiff_t;
    using value_type           = typename parent_iterator_type::value_type;
    using pointer              = typename parent_iterator_type::pointer;
    using reference            = typename parent_iterator_type::reference;

    //
    // Constructors / assignment:
    //

    array_iterator_base() noexcept
        : m_parent_array{ nullptr }
        , m_current_index{ 0 }
    { }

    array_iterator_base(parent_type * parent, const difference_type index) noexcept
        : m_parent_array{ parent }
        , m_current_index{ index }
    { }

    //
    // Pointer-emulation operator overloads:
    //

    difference_type operator - (const array_iterator_base & other) const
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        check_same_parent(other);
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        return m_current_index - other.m_current_index;
    }

    array_iterator_base operator + (const difference_type displacement) const
    {
        array_iterator_base temp{ *this };
        return temp.increment(displacement);
    }
    array_iterator_base operator - (const difference_type displacement) const
    {
        array_iterator_base temp{ *this };
        return temp.decrement(displacement);
    }

    array_iterator_base & operator += (const difference_type displacement)
    {
        return increment(displacement);
    }
    array_iterator_base & operator -= (const difference_type displacement)
    {
        return decrement(displacement);
    }

    array_iterator_base & operator++() // pre-increment
    {
        return increment(1);
    }
    array_iterator_base operator++(int) // post-increment
    {
        array_iterator_base temp{ *this };
        increment(1);
        return temp;
    }

    array_iterator_base & operator--() // pre-decrement
    {
        return decrement(1);
    }
    array_iterator_base operator--(int) // post-decrement
    {
        array_iterator_base temp{ *this };
        decrement(1);
        return temp;
    }

    reference operator*() const
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        if (!is_dereferenceable())
        {
            ARRAY_VIEW_ERROR("array_iterator_base::operator*: iterator not dereferenceable!");
        }
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        return (*m_parent_array)[m_current_index];
    }
    reference operator->() const
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        if (!is_dereferenceable())
        {
            ARRAY_VIEW_ERROR("array_iterator_base::operator->: iterator not dereferenceable!");
        }
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        return (*m_parent_array)[m_current_index];
    }
    reference operator[](const size_type index) const
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        if (!is_dereferenceable())
        {
            ARRAY_VIEW_ERROR("array_iterator_base::operator[]: iterator not dereferenceable!");
        }
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        const size_type array_index = m_current_index + index;

        #if ARRAY_VIEW_DEBUG_CHECKS
        if (array_index >= m_parent_array->size())
        {
            ARRAY_VIEW_ERROR("array_iterator_base::operator[]: array index is out-of-bounds!");
        }
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        return (*m_parent_array)[array_index];
    }

    bool operator == (std::nullptr_t) const noexcept
    {
        return m_parent_array == nullptr;
    }
    bool operator != (std::nullptr_t) const noexcept
    {
        return !(*this == nullptr);
    }

    bool operator == (const array_iterator_base & other) const
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        check_same_parent(other);
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        return m_current_index == other.m_current_index;
    }
    bool operator != (const array_iterator_base & other) const
    {
        return !(*this == other);
    }

    bool operator < (const array_iterator_base & other) const
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        check_same_parent(other);
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        return m_current_index < other.m_current_index;
    }
    bool operator > (const array_iterator_base & other) const
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        check_same_parent(other);
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        return m_current_index > other.m_current_index;
    }

    bool operator <= (const array_iterator_base & other) const
    {
        return !(*this > other);
    }
    bool operator >= (const array_iterator_base & other) const
    {
        return !(*this < other);
    }

    //
    // One way conversion from mutable_iterator to const_iterator:
    //

    operator array_iterator_base<const value_type, const parent_type, array_view_detail::const_iterator_tag>() const noexcept
    {
        return array_iterator_base<const value_type, const parent_type, array_view_detail::const_iterator_tag>{ m_parent_array, m_current_index };
    }

    //
    // Non-throwing swap() overload for array_iterator_base:
    //

    friend void swap(array_iterator_base & lhs, array_iterator_base & rhs) noexcept
    {
        using std::swap;
        swap(lhs.m_parent_array,  rhs.m_parent_array);
        swap(lhs.m_current_index, rhs.m_current_index);
    }

private:

    bool is_dereferenceable() const noexcept
    {
        return m_parent_array != nullptr && m_current_index >= 0 &&
               static_cast<size_type>(m_current_index) < m_parent_array->size();
    }

    #if ARRAY_VIEW_DEBUG_CHECKS
    void check_same_parent(const array_iterator_base & other) const
    {
        if (m_parent_array != other.m_parent_array)
        {
            ARRAY_VIEW_ERROR("Array iterators belong to different objects!");
        }
    }
    #endif // ARRAY_VIEW_DEBUG_CHECKS

    array_iterator_base & increment(const difference_type displacement)
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        if (m_parent_array == nullptr)
        {
            ARRAY_VIEW_ERROR("Incrementing an invalid array iterator!");
        }
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        m_current_index += displacement;
        return *this;
    }

    array_iterator_base & decrement(const difference_type displacement)
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        if (m_parent_array == nullptr)
        {
            ARRAY_VIEW_ERROR("Decrementing an invalid array iterator!");
        }
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        m_current_index -= displacement;
        return *this;
    }

    // Reference to the object that holds the data
    // this iterator points to (the array_view that
    // created to the iterator).
    parent_type * m_parent_array;

    // Current dereference index in the m_parent_array.
    difference_type m_current_index;
};

// ========================================================
// template class array_view:
// ========================================================

template<typename T>
class array_view final
{
public:

    //
    // Nested types:
    //

    using value_type             = T;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;

    using pointer                = typename std::add_pointer<value_type>::type;
    using reference              = typename std::add_lvalue_reference<value_type>::type;
    using const_pointer          = typename std::add_pointer<const value_type>::type;
    using const_reference        = typename std::add_lvalue_reference<const value_type>::type;

    using iterator               = array_iterator_base<value_type, array_view, array_view_detail::mutable_iterator_tag>;
    using const_iterator         = array_iterator_base<const value_type, const array_view, array_view_detail::const_iterator_tag>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    //
    // Constructors / assignment:
    //

    array_view() noexcept
        : m_pointer{ nullptr }
        , m_size_in_items{ 0 }
    { }

    template<typename ArrayType, std::size_t ArraySize>
    explicit array_view(ArrayType (&arr)[ArraySize]) noexcept
        : m_pointer{ arr }
        , m_size_in_items{ ArraySize }
    { }

    template<typename ContainerType>
    explicit array_view(ContainerType & container) noexcept
        : m_pointer{ container.data() }
        , m_size_in_items{ container.size() }
    { }

    template<typename ConvertibleType>
    array_view(ConvertibleType * array_ptr, const size_type size_in_items) noexcept
        : m_pointer{ array_ptr }
        , m_size_in_items{ size_in_items }
    { }

    template<typename ConvertibleType>
    array_view(array_view<ConvertibleType> other) noexcept
        : m_pointer{ other.data() }
        , m_size_in_items{ other.size() }
    { }

    template<typename ConvertibleType>
    array_view & operator = (array_view<ConvertibleType> other) noexcept
    {
        m_pointer = other.data();
        m_size_in_items = other.size();
        return *this;
    }

    //
    // Helper methods:
    //

    void reset() noexcept
    {
        m_pointer = nullptr;
        m_size_in_items = 0;
    }

    array_view slice(const size_type offset_in_items) const
    {
        if (data() == nullptr || empty())
        {
            return {}; // Empty slice.
        }

        #if ARRAY_VIEW_DEBUG_CHECKS
        if (offset_in_items >= size())
        {
            ARRAY_VIEW_ERROR("array_view slice offset greater or equal size!");
        }
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        return slice(offset_in_items, size() - offset_in_items);
    }

    array_view slice(const size_type offset_in_items, const size_type item_count) const
    {
        if (data() == nullptr || empty() || item_count == 0)
        {
            return {}; // Empty slice.
        }

        #if ARRAY_VIEW_DEBUG_CHECKS
        if (item_count == 0)
        {
            ARRAY_VIEW_ERROR("array_view slice with zero size!");
        }
        if (offset_in_items >= size())
        {
            ARRAY_VIEW_ERROR("array_view slice offset greater or equal size!");
        }
        if (offset_in_items + item_count > size())
        {
            ARRAY_VIEW_ERROR("array_view slice size is greater than total size!");
        }
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        return { m_pointer + offset_in_items, item_count };
    }

    //
    // Data access:
    //

    const_reference at(const size_type index) const
    {
        // at() always validates the array_view and index.
        // operator[] uses debug checks that can be disabled if
        // you care more about performance than runtime checking.
        check_not_null();
        if (index >= size())
        {
            ARRAY_VIEW_ERROR("array_view::at(): index is out-of-bounds!");
        }
        return *(data() + index);
    }
    reference at(const size_type index)
    {
        // Always checked.
        check_not_null();
        if (index >= size())
        {
            ARRAY_VIEW_ERROR("array_view::at(): index is out-of-bounds!");
        }
        return *(data() + index);
    }

    const_reference operator[](const size_type index) const
    {
        // Unlike with at() these checks can be disabled for better performance.
        #if ARRAY_VIEW_DEBUG_CHECKS
        check_not_null();
        if (index >= size())
        {
            ARRAY_VIEW_ERROR("array_view::operator[]: index is out-of-bounds!");
        }
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        return *(data() + index);
    }
    reference operator[](const size_type index)
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        check_not_null();
        if (index >= size())
        {
            ARRAY_VIEW_ERROR("array_view::operator[]: index is out-of-bounds!");
        }
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        return *(data() + index);
    }

    //
    // Begin/end range iterators, front()/back():
    //

    // forward begin:
    iterator begin() noexcept
    {
        return make_iterator(0);
    }
    const_iterator begin() const noexcept
    {
        return make_const_iterator(0);
    }
    const_iterator cbegin() const noexcept
    {
        return make_const_iterator(0);
    }

    // forward end:
    iterator end() noexcept
    {
        return make_iterator(size());
    }
    const_iterator end() const noexcept
    {
        return make_const_iterator(size());
    }
    const_iterator cend() const noexcept
    {
        return make_const_iterator(size());
    }

    // reverse begin:
    reverse_iterator rbegin() noexcept
    {
        return reverse_iterator{ end() };
    }
    const_reverse_iterator rbegin() const noexcept
    {
        return const_reverse_iterator{ end() };
    }
    const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator{ cend() };
    }

    // reverse end:
    reverse_iterator rend() noexcept
    {
        return reverse_iterator{ begin() };
    }
    const_reverse_iterator rend() const noexcept
    {
        return const_reverse_iterator{ begin() };
    }
    const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator{ cbegin() };
    }

    reference front()
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        check_not_null();
        #endif // ARRAY_VIEW_DEBUG_CHECKS
        return *data();
    }
    const_reference front() const
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        check_not_null();
        #endif // ARRAY_VIEW_DEBUG_CHECKS
        return *data();
    }

    reference back()
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        check_not_null();
        #endif // ARRAY_VIEW_DEBUG_CHECKS
        return *(data() + size() - 1);
    }
    const_reference back() const
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        check_not_null();
        #endif // ARRAY_VIEW_DEBUG_CHECKS
        return *(data() + size() - 1);
    }

    //
    // Miscellaneous queries:
    //

    bool empty() const noexcept
    {
        return size() == 0;
    }
    size_type size() const noexcept
    {
        return m_size_in_items;
    }
    size_type size_bytes() const noexcept
    {
        return m_size_in_items * sizeof(value_type);
    }
    const_pointer data() const noexcept
    {
        return m_pointer;
    }
    pointer data() noexcept
    {
        return m_pointer;
    }

    //
    // Compare against nullptr (test for a null array_view):
    //

    bool operator == (std::nullptr_t) const noexcept
    {
        return data() == nullptr;
    }
    bool operator != (std::nullptr_t) const noexcept
    {
        return !(*this == nullptr);
    }

    //
    // Compare for same array pointer and size:
    //

    bool operator == (const array_view & other) const noexcept
    {
        // Pointers to same memory (or both null).
        if (data() == other.data())
        {
            return true;
        }

        // Different sizes, whole sequence can't be identical.
        if (size() != other.size())
        {
            return false;
        }

        // Compare each element:
        return std::equal(begin(), end(), other.begin());
    }
    bool operator != (const array_view & other) const noexcept
    {
        return !(*this == other);
    }

    //
    // Compare pointer value for ordering (useful for containers/sorting):
    //

    bool operator < (const array_view & other) const noexcept
    {
        return data() < other.data();
    }
    bool operator > (const array_view & other) const noexcept
    {
        return data() > other.data();
    }
    bool operator <= (const array_view & other) const noexcept
    {
        return !(*this > other);
    }
    bool operator >= (const array_view & other) const noexcept
    {
        return !(*this < other);
    }

    //
    // Non-throwing swap() overload for array_views:
    //

    friend void swap(array_view & lhs, array_view & rhs) noexcept
    {
        using std::swap;
        swap(lhs.m_pointer, rhs.m_pointer);
        swap(lhs.m_size_in_items, rhs.m_size_in_items);
    }

private:

    void check_not_null() const
    {
        if (data() == nullptr || empty())
        {
            ARRAY_VIEW_ERROR("array_view pointer is null or size is zero!");
        }
    }

    iterator make_iterator(const difference_type start_offset) noexcept
    {
        return (data() != nullptr) ? iterator{ this, start_offset } : iterator{};
    }

    const_iterator make_const_iterator(const difference_type start_offset) const noexcept
    {
        return (data() != nullptr) ? const_iterator{ this, start_offset } : const_iterator{};
    }

    // Pointer is just a reference to external memory. Not owned by array_view.
    pointer   m_pointer;
    size_type m_size_in_items;
};

//
// make_array_view() helpers:
//
template<typename ArrayType, std::size_t ArraySize>
array_view<ArrayType> make_array_view(ArrayType (&arr)[ArraySize]) noexcept
{
    return { arr, ArraySize };
}
template<typename ArrayType>
array_view<ArrayType> make_array_view(ArrayType * array_ptr, const std::size_t size_in_items) noexcept
{
    return { array_ptr, size_in_items };
}
template<typename ContainerType>
array_view<typename ContainerType::value_type> make_array_view(ContainerType & container) noexcept
{
    return array_view<typename ContainerType::value_type>{ container };
}

// ========================================================
// template class strided_array_view:
// ========================================================

//
// Experimental strided_array_view.
// Allows accessing members of structured types as
// if it was an array_view of the member type itself.
//
// See the example code below for a reference.
//
template
<
    typename T,
    std::size_t OffsetBytes,
    std::size_t StrideBytes
>
class strided_array_view final
{
public:

    //
    // Nested types:
    //

    using value_type      = T;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    using pointer         = typename std::add_pointer<value_type>::type;
    using reference       = typename std::add_lvalue_reference<value_type>::type;
    using const_pointer   = typename std::add_pointer<const value_type>::type;
    using const_reference = typename std::add_lvalue_reference<const value_type>::type;

    using byte_type       = typename std::conditional<std::is_const<value_type>::value, const std::uint8_t, std::uint8_t>::type;
    using byte_ptr_type   = typename std::add_pointer<byte_type>::type;

    //
    // Constructors / assignment:
    //

    strided_array_view() noexcept
        : m_pointer{ nullptr }
        , m_size_in_bytes{ 0 }
    { }

    template<typename StructuredType>
    strided_array_view(StructuredType * array_ptr, const size_type size_in_items) noexcept
        : m_pointer{ reinterpret_cast<byte_ptr_type>(array_ptr) }
        , m_size_in_bytes{ size_in_items * sizeof(StructuredType) }
    { }

    strided_array_view(const strided_array_view & other) = default;
    strided_array_view & operator = (const strided_array_view & other) = default;

    //
    // Miscellaneous queries:
    //

    byte_ptr_type data() const noexcept
    {
        return m_pointer;
    }
    byte_ptr_type data() noexcept
    {
        return m_pointer;
    }
    bool empty() const noexcept
    {
        return size_bytes() == 0;
    }
    size_type size_bytes() const noexcept
    {
        return m_size_in_bytes;
    }
    size_type size() const noexcept
    {
        return size_bytes() / stride_bytes();
    }

    static constexpr size_type offset_bytes() noexcept { return OffsetBytes; }
    static constexpr size_type stride_bytes() noexcept { return StrideBytes; }

    //
    // Compare against nullptr (test for a null strided_array_view):
    //

    bool operator == (std::nullptr_t) const noexcept
    {
        return data() == nullptr;
    }
    bool operator != (std::nullptr_t) const noexcept
    {
        return !(*this == nullptr);
    }

    //
    // Data access:
    //

    const_reference at(const size_type index) const
    {
        // at() always validates the bounds.
        if (data() == nullptr)
        {
            ARRAY_VIEW_ERROR("strided_array_view: null pointer!");
        }
        if (index >= size())
        {
            ARRAY_VIEW_ERROR("strided_array_view::at(): index is out-of-bounds!");
        }

        return *reinterpret_cast<const_pointer>(get_item_raw_ptr(index));
    }
    reference at(const size_type index)
    {
        // Always checked.
        if (data() == nullptr)
        {
            ARRAY_VIEW_ERROR("strided_array_view: null pointer!");
        }
        if (index >= size())
        {
            ARRAY_VIEW_ERROR("strided_array_view::at(): index is out-of-bounds!");
        }

        return *reinterpret_cast<pointer>(get_item_raw_ptr(index));
    }

    const_reference operator[](const size_type index) const
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        if (data() == nullptr)
        {
            ARRAY_VIEW_ERROR("strided_array_view: null pointer!");
        }
        if (index >= size())
        {
            ARRAY_VIEW_ERROR("strided_array_view::operator[]: index is out-of-bounds!");
        }
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        return *reinterpret_cast<const_pointer>(get_item_raw_ptr(index));
    }
    reference operator[](const size_type index)
    {
        #if ARRAY_VIEW_DEBUG_CHECKS
        if (data() == nullptr)
        {
            ARRAY_VIEW_ERROR("strided_array_view: null pointer!");
        }
        if (index >= size())
        {
            ARRAY_VIEW_ERROR("strided_array_view::operator[]: index is out-of-bounds!");
        }
        #endif // ARRAY_VIEW_DEBUG_CHECKS

        return *reinterpret_cast<pointer>(get_item_raw_ptr(index));
    }

    // These are never checked. Use at your own risk.
    byte_ptr_type get_item_raw_ptr(const size_type index) noexcept
    {
        return m_pointer + (index * stride_bytes()) + offset_bytes();
    }
    byte_ptr_type get_item_raw_ptr(const size_type index) const noexcept
    {
        return m_pointer + (index * stride_bytes()) + offset_bytes();
    }

    reference front()
    {
        return operator[](0);
    }
    const_reference front() const
    {
        return operator[](0);
    }

    reference back()
    {
        return operator[](size() - 1);
    }
    const_reference back() const
    {
        return operator[](size() - 1);
    }

    //
    // Non-throwing swap() overload for strided_array_view:
    //

    friend void swap(strided_array_view & lhs, strided_array_view & rhs) noexcept
    {
        using std::swap;
        swap(lhs.m_pointer, rhs.m_pointer);
        swap(lhs.m_size_in_bytes, rhs.m_size_in_bytes);
    }

private:

    byte_ptr_type m_pointer;
    size_type m_size_in_bytes;
};

// ========================================================
// strided_array_view usage example:
// ========================================================

#if 0
void strided_array_view_example()
{
    struct Vec3 { float x, y, z; };
    struct Vec2 { float u, v; };

    struct Vertex
    {
        Vec3 position;
        Vec3 normal;
        Vec2 texcoords;
    };

    const Vertex verts[] // 6 verts
    {
        { { 0.0f, 0.0f, 0.0f },  { 0.0f, 0.0f, 0.0f },  { 0.0f, 0.0f } },
        { { 1.0f, 1.0f, 1.0f },  { 0.1f, 0.1f, 0.1f },  { 1.1f, 1.1f } },
        { { 2.0f, 2.0f, 2.0f },  { 0.2f, 0.2f, 0.2f },  { 2.2f, 2.2f } },
        { { 3.0f, 3.0f, 3.0f },  { 0.3f, 0.3f, 0.3f },  { 3.3f, 3.3f } },
        { { 4.0f, 4.0f, 4.0f },  { 0.4f, 0.4f, 0.4f },  { 4.4f, 4.4f } },
        { { 5.0f, 5.0f, 5.0f },  { 0.5f, 0.5f, 0.5f },  { 5.5f, 5.5f } }
    };

    // Each index of the array refers to the 'position' field of the struct
    auto sav1 = strided_array_view<const Vec3, 0, sizeof(Vertex)>(verts, array_size(verts));

    // Each index of the array refers to the 'normal' field of the struct
    auto sav2 = strided_array_view<const Vec3, sizeof(Vec3), sizeof(Vertex)>(verts, array_size(verts));

    // Each index of the array refers to the 'texcoords' field of the struct
    auto sav3 = strided_array_view<const Vec2, sizeof(Vec3) * 2, sizeof(Vertex)>(verts, array_size(verts));

    assert(!sav1.empty());
    assert(!sav2.empty());
    assert(!sav3.empty());

    assert(sav1.data() == reinterpret_cast<const std::uint8_t *>(verts));
    assert(sav2.data() == reinterpret_cast<const std::uint8_t *>(verts));
    assert(sav3.data() == reinterpret_cast<const std::uint8_t *>(verts));

    assert(sav1.size() == 6);
    assert(sav2.size() == 6);
    assert(sav3.size() == 6);

    assert(sav1.size_bytes() == sizeof(verts));
    assert(sav2.size_bytes() == sizeof(verts));
    assert(sav3.size_bytes() == sizeof(verts));

    for (std::size_t i = 0; i < sav1.size(); ++i)
    {
        assert(std::memcmp(&sav1[i], &verts[i].position, sizeof(Vec3)) == 0);
    }
    for (std::size_t i = 0; i < sav2.size(); ++i)
    {
        assert(std::memcmp(&sav2[i], &verts[i].normal, sizeof(Vec3)) == 0);
    }
    for (std::size_t i = 0; i < sav3.size(); ++i)
    {
        assert(std::memcmp(&sav3[i], &verts[i].texcoords, sizeof(Vec2)) == 0);
    }

    assert(std::memcmp(&sav1.front(), &verts[0].position,  sizeof(Vec3)) == 0);
    assert(std::memcmp(&sav1.back(),  &verts[5].position,  sizeof(Vec3)) == 0);
    assert(std::memcmp(&sav2.front(), &verts[0].normal,    sizeof(Vec3)) == 0);
    assert(std::memcmp(&sav2.back(),  &verts[5].normal,    sizeof(Vec3)) == 0);
    assert(std::memcmp(&sav3.front(), &verts[0].texcoords, sizeof(Vec2)) == 0);
    assert(std::memcmp(&sav3.back(),  &verts[5].texcoords, sizeof(Vec2)) == 0);
}
#endif // 0

#endif // ARRAY_VIEW_HPP
