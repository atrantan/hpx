////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(LOCAL_PVECTOR_VIEW_HPP)
#define LOCAL_PVECTOR_VIEW_HPP

#include <pvector_view/pvector_view.hpp>
#include <pvector_view/local_pvector_view_iterator.hpp>

namespace hpx {

// A pvector_view is a view of a partitioned_vector.
    template<typename T, std::size_t N, typename Derived = void>
    struct local_pvector_view : public pvector_view<T,N,Derived>
    {
    private:
        using base_type = pvector_view<T,N,Derived>;
        using base_iterator = typename base_type::iterator;

    public:
        using value_type = T;

        using iterator
        = typename hpx::local_pvector_view_iterator< std::vector<T>
                                                   , base_iterator
                                                   >;

        explicit local_pvector_view( base_type const & global_pview )
        : base_type(global_pview)
        {}

    // Iterator interfaces
        iterator begin()
        {
            base_type & base(*this);
            return iterator(base.begin(), base.end());
        }

        iterator end()
        {
            base_type & base(*this);
            return iterator( base.end(), base.end() );
        }
    };

    template<typename T, std::size_t N, typename Derived>
    local_pvector_view<T,N,Derived> local_view( pvector_view<T,N,Derived> const & base )
    {
        return local_pvector_view<T,N,Derived>( base );
    }
}

#endif

