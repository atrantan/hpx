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

int main()
{

    auto bsp1 = [](hpx::parallel::spmd_block block)
                {
                    auto bsp4 = [](hpx::parallel::spmd_block block)
                                {
                                  std::cout<<"Enter in BSP 4"<<std::endl;
                                };

                    auto bsp2 = [](hpx::parallel::spmd_block block)
                                {
                                    auto bsp3 = [](hpx::parallel::spmd_block block)
                                                {
                                                  std::cout<<"Enter in BSP 3"<<std::endl;
                                                };

                                    std::cout<<"Enter in BSP 2"<<std::endl;

                                    block.run(bsp3);             // potential concurrency in queuing
                                };

                    std::cout<<"Enter in BSP 1"<<std::endl;

                    block.run(bsp2);            // master dispatch actions
                    block.run(bsp4);            // potential concurrency in queuing

                    block.wait("").get();
                };

    auto localities = hpx::find_all_localities();

    hpx::parallel::define_spmd_block(localities, bsp1 ).get();

    std::cout<<"all is done"<<std::endl;

    return 0;
}
