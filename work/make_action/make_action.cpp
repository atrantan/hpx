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

constexpr auto a = hpx::make_action(
  [](int x)->int
  {
/*   hpx::wait_images_sync("");*/
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
);


int main()
{
    auto localities = hpx::find_all_localities();

    std::vector< hpx::future<int> > join;

    int i=0;

    for (auto & l : localities)
    {
        if ( l != hpx::find_here() )
        join.push_back( hpx::async(a, l, i++) );
    }

    join.push_back( hpx::async(a, hpx::find_here(), i++) );

    hpx::wait_all(join);

    return 0;
}
