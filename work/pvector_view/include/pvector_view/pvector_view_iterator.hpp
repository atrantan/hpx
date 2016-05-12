////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(PVECTOR_VIEW_ITERATOR_HPP)
#define PVECTOR_VIEW_ITERATOR_HPP

#include <array>
#include <iterator>

#include <boost/iterator/iterator_facade.hpp>

#include <hpx/components/containers/partitioned_vector/partitioned_vector_segmented_iterator.hpp>

#include <pvector_view/view_element.hpp>

#include <utility/make_index_sequence.hpp>

namespace hpx {

    template<typename T, std::size_t N, typename Stencil>
    class pvector_view_iterator
    : public boost::iterator_facade
    < pvector_view_iterator<T,N,Stencil>      // CRTP, just use the pvector_view_iterator name
    , hpx::detail::view_element<T,Stencil>
    , std::random_access_iterator_tag // type of traversal allowed
    , hpx::detail::view_element<T,Stencil>   // Replace reference return by a view_element return
    >
    {
    private:
        using pvector_iterator = hpx::vector_iterator<T>;
        using segment_iterator = typename pvector_iterator::segment_iterator;
        using indices = typename hpx::detail::make_index_sequence<N>::type;

    template<std::size_t... I>
    inline std::size_t  increment_solver( std::size_t dist
      , hpx::detail::integer_sequence<std::size_t, I...>
      ) const
    {
        std::size_t max = N-1;
        std::size_t offset = 0;
        std::size_t carry = dist;
        std::size_t tmp;

  // More expensive than a usual incrementation but did not find another solution :/
        (void)std::initializer_list<int>
        { ( static_cast<void>( carry   -= tmp = (carry/sw_basis_[max-I]) * sw_basis_[max-I]
                             , offset  += (tmp/sw_basis_[max-I]) * hw_basis_[max-I]
                             )
          , 0)...
        };

        return offset;
    }

    public:
        using element_type = hpx::detail::view_element<T,Stencil>;

        explicit pvector_view_iterator(
              segment_iterator const & begin
            , std::array<std::size_t, N+1> const & sw_basis
            , std::array<std::size_t, N+1> const & hw_basis
            , std::size_t count
            , Stencil & stencil
            )
        : t_(begin), begin_(begin), count_(count)
        , sw_basis_(sw_basis), hw_basis_(hw_basis)
        , stencil_(stencil)
        {}

    private:
        friend class boost::iterator_core_access;

        void increment()
        {
            auto offset = increment_solver(++count_, indices() );
            t_ = begin_ + offset;
        }

        void decrement()
        {
            auto offset = increment_solver(--count_, indices() );
            t_ = begin_ + offset;
        }

        void advance(std::size_t  n)
        {
            auto offset = increment_solver(count_+=n, indices() );
            t_ = begin_ + offset;
        }

        bool equal(pvector_view_iterator const& other) const
        {
            return this->count_ == other.count_;
        }

    // Will not return a datatype but a view_element type
        element_type dereference() const
        {
            return hpx::detail::view_element<T,Stencil>( t_, stencil_ );
        }

        std::ptrdiff_t distance_to(pvector_view_iterator const& other) const
        {
            return other.count_ - count_;
        }

        segment_iterator t_, begin_;
        std::size_t count_;
        std::array< std::size_t, N+1 > const & sw_basis_;
        std::array< std::size_t, N+1 > const & hw_basis_;
        Stencil & stencil_;
    };
}

#endif
