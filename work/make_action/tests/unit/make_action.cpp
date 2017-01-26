////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine TRAN TAN
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////

#include <hpx/hpx.hpp>
#include <hpx/hpx_main.hpp>
#include <hpx/lcos/barrier.hpp>
#include <hpx/lcos/broadcast.hpp>

#include <make_action/make_action.hpp>

#include <type_traits>

int user_function(int x)
{
    std::size_t here = hpx::get_locality_id();
    std::size_t numlocs = hpx::get_num_localities(hpx::launch::sync) ;

    std::cout << "Welcome to locality " << here
              << " among " << numlocs <<" localities"<<std::endl;

    // create the barrier, register it with AGAS
    hpx::lcos::barrier b("barrier_name",numlocs);
    b.wait();

    return x + 1;
}


int main()
{
    constexpr auto a_lambda =
        hpx::make_action([](int x)
                         {
                            return user_function(x);
                         });

    auto localities = hpx::find_all_localities();

    hpx::lcos::broadcast( a_lambda, localities, 40).get();

    return 0;
}
