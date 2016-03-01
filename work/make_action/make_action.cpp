////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine TRAN TAN
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////

#include <hpx/hpx.hpp>
#include <hpx/hpx_main.hpp>
#include <make_action/make_action.hpp>
#include <hpx/include/lcos.hpp>

#include <type_traits>

int user_function(int x)
{
    std::size_t here = hpx::get_locality_id();
    std::size_t numlocs = hpx::get_num_localities_sync();

    std::cout <<"Welcome to locality "<< here <<" among "<< numlocs <<" localities"<<std::endl;

    if( here == 0 )
    {
        // create the barrier, register it with AGAS
        hpx::lcos::barrier b = hpx::new_<hpx::lcos::barrier>(hpx::find_here(),numlocs);
        b.register_as("barrier_name");
        b.wait();
    }
    else
    {
        hpx::lcos::barrier b;
        b.connect_to("barrier_name");
        b.wait();
    }

    return x + 1;
}


auto a_lambda = hpx::make_action([](int x)
{
    return user_function(x);
});

auto a_function = hpx::make_action( std::integral_constant<int(*)(int),&user_function>() );


int main()
{
    auto localities = hpx::find_all_localities();

    {
        std::vector< hpx::future<int> > join;

        int i=0;

        for (auto & l : localities)
        {
            if ( l != hpx::find_here() )
            join.push_back( hpx::async(a_lambda, l, i++) );
        }

        join.push_back( hpx::async( a_lambda, hpx::find_here(), i++) );

        hpx::wait_all(join);
    }

    {
        std::vector< hpx::future<int> > join;

        int i=0;

        for (auto & l : localities)
        {
            if ( l != hpx::find_here() )
            join.push_back( hpx::async(a_function, l, i++) );
        }

        join.push_back( hpx::async( a_function, hpx::find_here(), i++) );

        hpx::wait_all(join);
    }

    return 0;
}
