////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2016 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////

#include <hpx/hpx.hpp>
#include <hpx/hpx_main.hpp>
#include <hpx/include/partitioned_vector.hpp>
#include <hpx/util/lightweight_test.hpp>

#include <pvector_view/pvector_view.hpp>
#include <spmd_block/spmd_block.hpp>
#include <make_action/make_action.hpp>

HPX_REGISTER_PARTITIONED_VECTOR(double);

constexpr auto image_coarray = hpx::make_action(
[](hpx::parallel::spmd_block block)
{
    using hpx::_;

    const std::size_t height = 32;
    const std::size_t width  = 4;

    hpx::coarray<double,2> a = { block
                               , "a"
                               , {height,width}
                               , hpx::partition<double>(16, 0.0)
                               , hpx::stencil_view<double,2>({4,4})
                               };

    hpx::coarray<double,2> b = { block
                               , "b"
                               , {height,width}
                               , hpx::partition<double>(16, 0.0)
                               };


    a(2,1).get_boundary(0,-1) = a(2,0).get_boundary(0,1);
    b(2,1).get_boundary(0,-1) = b(2,0).get_boundary(0,1);

});


int main()
{
    auto localities = hpx::find_all_localities();

    hpx::parallel::define_spmd_block( localities, image_coarray ).get();

    return 0;
}

