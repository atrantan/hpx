////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(GET_UNROLLED_LOCALITIES_HPP)
#define GET_UNROLLED_LOCALITIES_HPP

#include <hpx/runtime/naming/id_type.hpp>
#include <algorithm>


namespace hpx {

    std::vector<hpx::naming::id_type>
    get_unrolled_localities( std::vector< hpx::naming::id_type > const & in
                           , std::size_t N
                           , std::size_t unroll
                           )
    {
        std::vector<hpx::naming::id_type> out ( N );

        auto o_end = out.end();

        auto i_begin = in.cbegin();
        auto i_end   = in.cend();

        auto i = i_begin;

        for( auto o = out.begin() ; o<o_end  ; o+=unroll )
        {
           std::fill(o, std::min(o + unroll, o_end ), *i);
           i = (++i != i_end) ? i : i_begin;
       }

       return out;
   }

}
#endif
