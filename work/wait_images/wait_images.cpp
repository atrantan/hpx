////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine TRAN TAN
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#include <hpx/runtime/actions/plain_action.hpp>
#include <hpx/hpx_main.hpp>
#include <hpx/include/lcos.hpp>
#include <hpx/include/actions.hpp>

#include <make_action/make_action.hpp>
#include <wait_images/wait_images.hpp>

void image()
{
    std::cout<<"Hello from locality "<<hpx::get_locality_id()<<std::endl;
    hpx::wait_images("barrier");
}
HPX_DEFINE_PLAIN_ACTION(image);


int main()
{
    constexpr auto a = hpx::make_action(
    [](){std::cout<<"Hello from locality "<<hpx::get_locality_id()<<std::endl;
         hpx::wait_images("barrier");
        }
    );

    auto localities = hpx::find_all_localities();

// action with constexpr lambda

    std::vector< hpx::future<void> > join;

    for( auto const & l : localities )
    join.push_back( hpx::async(a, l) );

    hpx::wait_all(join);


// action with global function

/*    image_action a2;

    std::vector< hpx::future<void> > join;

    for( auto const & l : localities )
    join.push_back( hpx::async(a2, l) );

    hpx::wait_all(join);*/

    return 0;
}
