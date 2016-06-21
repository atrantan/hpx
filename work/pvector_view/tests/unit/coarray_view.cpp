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
#include <pvector_view/local_pvector_view.hpp>
#include <make_action/make_action.hpp>

HPX_REGISTER_PARTITIONED_VECTOR(double);

int main()
{
    auto image_coarray =
    [](hpx::parallel::spmd_block block)
    {
        using const_iterator = typename std::vector<double>::const_iterator;

        constexpr std::size_t N        = 40;
        constexpr std::size_t tile     = 10;
        constexpr std::size_t elt_size = 8;

        hpx::coarray<double,2> a = { block, "a", {N,N}, hpx::partition<double>(elt_size) };
        std::size_t idx = 0;

        for (std::size_t j = 0; j<N; j+=tile)
        for (std::size_t i = 0; i<N; i+=tile)
        {
            hpx::coarray_view<double,2> view ( block, &a(i,j), &a(i+tile-1,j+tile-1), {tile,tile}, {N,N} );

            for ( auto & v : hpx::local_view(view) )
            {
                v = std::vector<double>( elt_size, double(idx) );
            }

            idx++;
        }

        block.barrier_sync("");

        if(block.this_image() == 0)
        {
            int idx = 0;

            for (std::size_t j = 0; j<N; j+=tile)
            for (std::size_t i = 0; i<N; i+=tile)
            {
                std::vector<double> result(1,idx);

                for (std::size_t jj = j, jj_end = j+tile; jj< jj_end; jj++)
                for (std::size_t ii = i, ii_end = i+tile; ii< ii_end; ii++)
                {
                    std::vector<double> value = a.get(ii,jj);  // It's a Get operation

                    const_iterator it1 = result.begin(), it2 = value.begin();
                    const_iterator end1 = result.end(), end2 = value.end();

                    for (; it1 != end1 && it2 != end2; ++it1, ++it2)
                    {
                         HPX_TEST_EQ(*it1, *it2);
                    }
                }
                idx++;
            }
        }
    };

    auto localities = hpx::find_all_localities();

    hpx::parallel::define_spmd_block( localities, image_coarray ).get();

    return 0;
}
