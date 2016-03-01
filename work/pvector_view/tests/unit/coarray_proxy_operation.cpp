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

#include <pvector_view/get_unrolled_localities.hpp>
#include <pvector_view/pvector_view.hpp>
#include <pvector_view/local_pvector_view.hpp>
#include <make_action/make_action.hpp>

#include <utility>

HPX_REGISTER_PARTITIONED_VECTOR(double);

constexpr auto image_coarray = hpx::make_action(
[](hpx::parallel::spmd_block block)
{
    using const_iterator = typename std::vector<double>::const_iterator;

    auto localities = block.find_all_localities();

    const std::size_t height = 16;
    const std::size_t width  = 16;

    std::size_t local_height = 16;
    std::size_t local_width  = 16;
    std::size_t local_leading_dimension  = local_height;

    hpx::coarray<double,2> in  = { block, "in",  {height,width}, hpx::partition<double>(local_height * local_width) };
    hpx::coarray<double,2> out = { block, "out", {height,width}, hpx::partition<double>(local_height * local_width) };

    // Ensure that only one locality is putting data into the different partitions
    if(hpx::find_here() == localities[0])
    {
        std::size_t idx = 0;

        // traverse all the co-indexed elements
        for (auto && v : in)
        {
            std::vector<double> data(local_height * local_width);
            std::size_t local_idx = 0;

            for( double & d : data)
            {
                d = idx + local_idx++;
            }

            // Put operation
            v = std::move(data);
            idx ++;
        }
    }

    block.barrier_sync("init");

    // Outer Transpose operation
    for (std::size_t j = 0; j<width; j++)
    for (std::size_t i = 0; i<height; i++)
    {
        // Put operation
        out(j,i) = in(i,j);
    }

    block.barrier("transpose")
    .then([&](hpx::future<void> f1){ f1.get();

        // Inner Transpose operation
        for ( auto & v : hpx::local_view(out) )
        {
            for (std::size_t j = 0; j<local_width;  j++)
            for (std::size_t i = 0; i<local_height; i++)
            {
                // Put operation
                std::swap( v[j + i*local_leading_dimension]
                         , v[i + j*local_leading_dimension]
                         );
            }
        }

        block.barrier_sync("local_transpose");
    }).wait();

    // Test the result of the computation
    if(hpx::find_here() == localities[0])
    {
        int idx = 0;
        std::vector<double> result(local_height * local_width);

        for (std::size_t j = 0; j<width; j++)
        for (std::size_t i = 0; i<height; i++)
        {
            std::size_t local_idx = 0;

            for( double & r : result)
            {
                r = idx + local_idx++;
            }

            // transpose the guess result
            for (std::size_t jj = 0; jj<local_width;  jj++)
            for (std::size_t ii = 0; ii<local_height; ii++)
            {
                // Put operation
                std::swap( result[jj + ii*local_leading_dimension]
                         , result[ii + jj*local_leading_dimension]
                         );
            }

            std::vector<double> value = out.get(j,i);  // It's a Get operation

            const_iterator it1 = result.begin(), it2 = value.begin();
            const_iterator end1 = result.end(), end2 = value.end();

            for (; it1 != end1 && it2 != end2; ++it1, ++it2)
            {
                 HPX_TEST_EQ(*it1, *it2);
            }

            idx++;
        }

    }
});


int main()
{
    auto localities = hpx::find_all_localities();
    hpx::parallel::define_spmd_block( localities, image_coarray ).get();

    return 0;
}

