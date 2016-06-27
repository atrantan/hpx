////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine TRAN TAN
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#include <hpx/hpx.hpp>
#include <hpx/hpx_main.hpp>
#include <spmd_block/spmd_block.hpp>
#include <barrier_algorithms/barrier_algorithms.hpp>

int main()
{
    auto a =
    [](hpx::parallel::spmd_block block)
    {
        std::cout<<"Welcome in locality "<<hpx::get_locality_id()<<std::endl;

        auto localities = block.find_all_localities();

        hpx::custom_barrier_sync(localities, "barrier");
        std::cout<<"Default barrier reached by locality "<<hpx::get_locality_id()<<std::endl;

        hpx::custom_barrier_sync(hpx::detail::central, localities, "central");
        std::cout<<"Central barrier reached by locality "<<hpx::get_locality_id()<<std::endl;

        hpx::custom_barrier_sync(hpx::detail::dissemination, localities, "dissemination");
        std::cout<<"Dissemination barrier reached by locality "<<hpx::get_locality_id()<<std::endl;

        hpx::custom_barrier_sync(hpx::detail::pairwise, localities, "pairwise");
        std::cout<<"Pairwise barrier reached by locality "<<hpx::get_locality_id()<<std::endl;
    };

    auto localities = hpx::find_all_localities();
    hpx::parallel::define_spmd_block( localities, a ).get();

    return 0;
}
