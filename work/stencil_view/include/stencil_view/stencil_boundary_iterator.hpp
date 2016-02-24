////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(STENCIL_DIRECTION_ITERATOR_HPP)
#define STENCIL_DIRECTION_ITERATOR_HPP

#include <array>
#include <iterator>

#include <boost/iterator/iterator_facade.hpp>
#include <utility/make_index_sequence.hpp>

namespace hpx {

    template<typename T, std::size_t N>
    class stencil_boundary_iterator
    : public boost::iterator_facade
    < stencil_boundary_iterator<T,N>      // CRTP, just use the stencil_boundary_iterator name
    , T
    , std::random_access_iterator_tag      // type of traversal allowed
    >
    {
    private:
        using vector_iterator = typename std::vector<T>::iterator;
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
        explicit stencil_boundary_iterator(
              vector_iterator && begin
            , std::array<std::size_t, N+1> const & sw_basis
            , std::array<std::size_t, N+1> const & hw_basis
            , std::size_t count
            )
        : t_(begin), begin_(begin), count_(count)
        , sw_basis_(sw_basis), hw_basis_(hw_basis)
        {}

    std::size_t raw_pos() const
    {
        return increment_solver(count_, indices() );
    }

    private:
        friend class boost::iterator_core_access;

        void increment()
        {
            std::size_t offset = increment_solver(++count_, indices() );
            t_ = begin_ + offset;
        }

        void decrement()
        {
            std::size_t offset = increment_solver(--count_, indices() );
            t_ = begin_ + offset;
        }

        void advance(std::size_t  n)
        {
            std::size_t offset = increment_solver(count_+=n, indices() );
            t_ = begin_ + offset;
        }

        bool equal(stencil_boundary_iterator const& other) const
        {
            return this->count_ == other.count_;
        }

        T & dereference() const
        {
            return *t_;
        }

        std::size_t distance_to(stencil_boundary_iterator const& other) const
        {
            return other.count_ > count_
            ? other.count_ - count_
            : count_ - other.count_;
        }

        vector_iterator t_, begin_;
        std::size_t count_;
        std::array< std::size_t, N+1 > const & sw_basis_;
        std::array< std::size_t, N+1 > const & hw_basis_;
    };

    //Specialization for 2D stencils

    template<typename T>
    class stencil_boundary_iterator<T,2>
    : public boost::iterator_facade
    < stencil_boundary_iterator<T,2>      // CRTP, just use the stencil_boundary_iterator name
    , T
    , std::random_access_iterator_tag      // type of traversal allowed
    >
    {
    private:
        using vector_iterator = typename std::vector<T>::iterator;

    public:
        explicit stencil_boundary_iterator(
              vector_iterator && begin
            , std::array<std::size_t, 3> const & sw_basis
            , std::array<std::size_t, 3> const & hw_basis
            , std::size_t count
            )
        : incx_( sw_basis[1] == 1 ? hw_basis[1] : 1 )
        , t_( begin + count*incx_ )
        , begin_(begin)
        {}

        std::size_t raw_pos() const
        {
            return std::distance(begin_,t_);
        }

    private:
        friend class boost::iterator_core_access;

        void increment()
        {
            t_ += incx_;
        }

        void decrement()
        {
            t_ -= incx_;
        }

        void advance(std::size_t n)
        {
            t_ += n*incx_;
        }

        bool equal(stencil_boundary_iterator const& other) const
        {
            return this->t_ == other.t_;
        }

        T & dereference() const
        {
            return *t_;
        }

        std::size_t distance_to(stencil_boundary_iterator const& other) const
        {
            return std::distance(t_,other.t_);
        }

        std::size_t incx_;
        vector_iterator t_, begin_;

    };

}

#endif
