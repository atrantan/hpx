////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine TRAN TAN
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#include <hpx/hpx.hpp>
#include <hpx/hpx_main.hpp>
#include <make_action/make_action.hpp>
#include <spmd_block/spmd_block.hpp>
#include <barrier_algorithms/barrier_algorithms.hpp>


int main()
{
    constexpr auto a = hpx::make_action(
    [](hpx::parallel::spmd_block block){

            auto localities = block.find_all_localities();

            std::cout<<"Hello from locality "<<hpx::get_locality_id()<<std::endl;

            hpx::custom_barrier_sync(localities, "barrier");

            std::cout<<"Re-Hello from locality "<<hpx::get_locality_id()<<std::endl;

            hpx::custom_barrier_sync(hpx::detail::pairwise, localities, "bar");
        }
    );

    auto localities = hpx::find_all_localities();
    hpx::parallel::define_spmd_block( localities, a ).get();

    return 0;
}
