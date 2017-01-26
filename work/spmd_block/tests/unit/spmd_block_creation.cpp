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
                    std::cout<<"Enter in BSP1"<<std::endl;
                };

    auto localities = hpx::find_all_localities();

    hpx::parallel::define_spmd_block("block1", localities, bsp1 ).get();

    hpx::parallel::define_spmd_block(  "block2", localities,
                                       [](hpx::parallel::spmd_block block)
                                        {
                                            std::cout<< "Enter in BSP2"
                                                     << std::endl;
                                        }
                                    ).get();

    std::cout<<"all is done"<<std::endl;

    return 0;
}
