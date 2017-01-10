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
    template<typename T, std::size_t N, typename Data, typename Derived = void>
    struct local_pvector_view : public hpx::partitioned_vector<T,N,Data,Derived>
    {
    private:
        using base_type = hpx::partitioned_vector<T,N,Data,Derived>;
        using base_iterator = typename base_type::iterator;

    public:
        using value_type = T;

        using iterator
        = typename hpx::local_pvector_view_iterator< Data
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

    template<typename T, std::size_t N, typename Data, typename Derived>
    local_hpx::partitioned_vector<T,N,Data,Derived> local_view( hpx::partitioned_vector<T,N,Data,Derived> const & base )
    {
        return local_hpx::partitioned_vector<T,N,Data,Derived>( base );
    }
}

#endif

