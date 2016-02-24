////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2016 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////

#include <hpx/hpx.hpp>
#include <hpx/hpx_main.hpp>
#include <hpx/util/lightweight_test.hpp>

#include <spmd_block/spmd_block.hpp>

void bsp3(hpx::parallel::spmd_block block)
{

}
HPX_DEFINE_PLAIN_ACTION(bsp3);


void bsp2(hpx::parallel::spmd_block block)
{
    bsp3_action act3;
    block.run(act3);             // potential concurrency in queuing
}
HPX_DEFINE_PLAIN_ACTION(bsp2);


void bsp1(hpx::parallel::spmd_block block)
{
    bsp2_action act2;
    block.run(act2);                       // master dispatch actions

    bsp3_action act3;
    block.run(act3);            // potential concurrency in queuing

    block.barrier_sync("");

}
HPX_DEFINE_PLAIN_ACTION(bsp1);


int main()
{
    bsp1_action act1;

    auto localities = hpx::find_all_localities();

    hpx::future<void> f = hpx::parallel::define_spmd_block( localities, act1 );
    f.get();

    std::cout<<"all is done"<<std::endl;

    return 0;
}
