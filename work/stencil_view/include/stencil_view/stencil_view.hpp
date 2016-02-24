////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(STENCIL_VIEW_HPP)
#define STENCIL_VIEW_HPP

#include <vector>
#include <initializer_list>
#include <iostream>

#include <hpx/components/containers/partitioned_vector/partitioned_vector_segmented_iterator.hpp>
#include <hpx/runtime/serialization/serialize.hpp>

#include <stencil_view/stencil_boundary_iterator.hpp>

namespace hpx {

// Struct defining a view_as_stencil.

// A view_as_stencil is a view of a vector.
    template<typename T, std::size_t N>
    struct stencil_boundary
    {
        using value_type = T;

    private:
        using vector_iterator = typename std::vector<T>::iterator;
        using list_type = std::initializer_list<std::size_t>;

    public:
        using iterator = typename hpx::stencil_boundary_iterator<value_type,N>;

        explicit stencil_boundary()
        : begin_(nullptr)
        {}

        explicit stencil_boundary( vector_iterator && begin
                                  , list_type && sw_sizes
                                  , list_type const & hw_sizes
                                  )
        : begin_( begin )
        {
// Generate two mixed radix basis
            sw_basis_[0] = 1;
            hw_basis_[0] = 1;

            std::size_t  tmp1 = 1;
            auto out1  = sw_basis_.begin() + 1;

            for( std::size_t i : sw_sizes  )
            {
                *out1 = tmp1 *= i;
                ++out1;
            }

            std::size_t  tmp2 = 1;
            auto & sizes = hw_sizes.size() ? hw_sizes : sw_sizes;
            auto out2   = hw_basis_.begin() + 1;

            for( std::size_t i : sizes )
            {
                *out2 = tmp2 *= i;
                ++out2;
            }
        }

    public:
    // Iterator interfaces
        iterator begin()
        {
            return iterator( vector_iterator(begin_)
                           , sw_basis_,hw_basis_
                           , 0);
        }

        iterator end()
        {
            return iterator( vector_iterator(begin_)
                           , sw_basis_, hw_basis_
                           , sw_basis_.back()
                           );
        }

        private:
            std::array< std::size_t, N+1 > sw_basis_, hw_basis_;
            vector_iterator begin_;
    };

// Struct defining a stencil_view.
// A stencil_view is a view of a vector.
    template <typename T, std::size_t N = 1>
    struct stencil_view
    {
    private:
        using vector_iterator = typename std::vector<T>::iterator;
        using list_type = std::initializer_list<std::size_t>;

    public:

        using boundary_type = stencil_boundary<T,N>;

        stencil_view(list_type && dimemsions_size = {})
        : hw_sizes_(dimemsions_size),  minimum_vector_size_(N > 0 ? 1 : 0)
        {
            for( auto const & i : hw_sizes_)
            {
                minimum_vector_size_ *=  i;
            }
        }

        int get_minimum_vector_size() const
        {
            return minimum_vector_size_;
        }

        template <typename ...I>
        boundary_type get_boundary(vector_iterator begin, vector_iterator end, I ... i) const
        {
            HPX_ASSERT_MSG( hw_sizes_.size(), "**Stencil error** : Cannot retrieve any boundary from stencil defined without sizes" );

// Check that the used space is valid for combined sizes
            HPX_ASSERT_MSG( minimum_vector_size_ <= std::distance(begin,end)
                          , "**Stencil Error** : Space defined for the stencil boundary is too small"
                          );

            static_assert( sizeof...(I) == N, "**Stencil error** : Subscript for get_boundary doesn't match the stencil dimension" );

            (void)std::initializer_list<int>
            { ( static_cast<void>(HPX_ASSERT_MSG( (i<=1 && i >=-1), "**Stencil error** : Subscript for get_boundary should only contain 0, 1 or -1" ))
            ,0) ...
            };

            std::size_t offset = 0;
            std::size_t basis = 1;

            auto size = hw_sizes_.begin();

            (void)std::initializer_list<int>
            { ( static_cast<void>(  offset += (i>0 ? *size - 1 : 0) * basis,
                                    basis  *= *(size++)
                                 )
              ,0) ...
            };

            auto iter = hw_sizes_.begin();
            list_type sw_sizes = { ( i!=0 ? (iter++,1) : *(iter++) ) ... };

            return boundary_type( begin + offset
                                 , std::move(sw_sizes)
                                 , hw_sizes_
                                 );
        }

    private:
        list_type hw_sizes_;
        std::size_t minimum_vector_size_;
    };
}

#endif

