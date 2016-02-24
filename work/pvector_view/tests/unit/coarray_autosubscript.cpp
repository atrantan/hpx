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
#include <make_action/make_action.hpp>

HPX_REGISTER_PARTITIONED_VECTOR(double);


constexpr auto image_coarray = hpx::make_action(
[](hpx::parallel::spmd_block block)
{
    using hpx::_;
    using hpx::Here;
    using const_iterator = typename std::vector<double>::const_iterator;

    auto localities = block.find_all_localities_sync();
    std::size_t numlocs = localities.size();

    const std::size_t height = 32;
    const std::size_t width  = 4;
    const std::size_t num_partitions  = height*width*numlocs;

    hpx::coarray<double,3> a = { block, "a", {height,width,_}, hpx::partition<double>(4, 0.0) };

    int idx = 0;

    for (std::size_t j = 0; j<width; j++)
    for (std::size_t i = 0; i<height; i++)
    {
        a.data(i,j,Here) = std::vector<double>(4,idx);         // It's a local write operation
        idx++;
    }

    block.barrier_sync("");

    if(hpx::find_here() == localities[0])
    {
        for (std::size_t k = 0; k<numlocs; k++)
        {
            int idx = 0;

            for (std::size_t j = 0; j<width; j++)
                for (std::size_t i = 0; i<height; i++)
                {
                    std::vector<double> result(4,idx);
                    std::vector<double> value = a.get(i,j,k);  // It's a Get operation

                    const_iterator it1 = result.begin(), it2 = value.begin();
                    const_iterator end1 = result.end(), end2 = value.end();

                    for (; it1 != end1 && it2 != end2; ++it1, ++it2)
                    {
                         HPX_TEST_EQ(*it1, *it2);
                    }

                    idx++;
                }
        }
    }
});


int main()
{
    auto localities = hpx::find_all_localities();

    hpx::parallel::define_spmd_block( localities, image_coarray ).get();

    return 0;
}

