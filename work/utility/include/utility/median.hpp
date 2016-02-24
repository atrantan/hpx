////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(MEDIAN_HPP)
#define MEDIAN_HPP

namespace hpx {  namespace detail {

    template<class RandAccessIter>
    inline
    typename RandAccessIter::value_type median(RandAccessIter begin, RandAccessIter end)
    {
        using T = typename RandAccessIter::value_type;

        std::size_t size = end - begin;
        std::size_t middleIdx = size/2;
        RandAccessIter target = begin + middleIdx;
        std::nth_element(begin, target, end);

        if(size % 2) return *target;
        else
        {
            T a = *target;
            RandAccessIter targetNeighbor= target-1;
            std::nth_element(begin, targetNeighbor, end);
            return (a+*targetNeighbor)/T(2);
        }
    }

}}

#endif
