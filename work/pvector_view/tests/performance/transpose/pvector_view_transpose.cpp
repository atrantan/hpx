//  Copyright (c) 2016 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx_init.hpp>
#include <hpx/hpx.hpp>
#include <hpx/util/high_resolution_clock.hpp>
#include <hpx/include/parallel_algorithm.hpp>
#include <hpx/include/iostreams.hpp>
#include <hpx/include/partitioned_vector.hpp>
#include <hpx/include/parallel_for_each.hpp>

#include <boost/cstdint.hpp>
#include <boost/format.hpp>
#include <boost/range/functions.hpp>

#include <pvector_view/pvector_view.hpp>
#include <pvector_view/local_pvector_view.hpp>

#include <spmd_block/spmd_block.hpp>

#include <utility/median.hpp>

#include <stdexcept>

///////////////////////////////////////////////////////////////////////////////
// Define the vector types to be used.
HPX_REGISTER_PARTITIONED_VECTOR(double);
///////////////////////////////////////////////////////////////////////////////
template <typename Vector>
boost::uint64_t transpose_matrix(Vector & out, Vector & in, std::size_t height, std::size_t width, std::size_t leading_dimension, int test_count)
{
    typedef typename Vector::value_type value_type;

    std::vector<boost::uint64_t> time;

    boost::uint64_t start = hpx::util::high_resolution_clock::now();

    for (int iter = 0; iter != test_count; ++iter)
    {
        boost::uint64_t start = hpx::util::high_resolution_clock::now();
        {
            for (std::size_t j = 0; j<width;  j++)
            for (std::size_t i = 0; i<height; i++)
            {
                out[j + i*leading_dimension] = in[i + j*leading_dimension];
            }
        }
        time.push_back( hpx::util::high_resolution_clock::now() - start );
    }

    return hpx::detail::median(time.begin(),time.end());
}

///////////////////////////////////////////////////////////////////////////////
template <typename Coarray>
boost::uint64_t transpose_coarray(  hpx::parallel::spmd_block & block
                                 ,  Coarray & out
                                 ,  Coarray & in
                                 ,  std::size_t height
                                 ,  std::size_t width
                                 ,  std::size_t local_height
                                 ,  std::size_t local_width
                                 ,  std::size_t local_leading_dimension
                                 ,  int test_count
                                 )
{
    std::vector<boost::uint64_t> time;

    for (int iter = 0; iter != test_count; ++iter)
    {
        boost::uint64_t start = hpx::util::high_resolution_clock::now();
        {
            // Outer Transpose operation
            for (std::size_t j = 0; j<width; j++)
            for (std::size_t i = 0; i<height; i++)
            {
                // Put operation
                out(j,i) = in(i,j);
            }

            block.barrier("transpose" + iter)
            .then(
                [&](hpx::future<void> f1)
                {

                    f1.get();

                    // Inner Transpose operation
                    for ( auto & v : hpx::local_view(out) )
                    {
                        for (std::size_t jj = 0; jj<local_width-1;  jj++)
                        for (std::size_t ii = jj+1; ii<local_height; ii++)
                        {
                            std::swap( v[jj + ii*local_leading_dimension]
                                     , v[ii + jj*local_leading_dimension]
                                     );
                        }
                    }

                    return block.barrier("local_transpose" + iter);
                 }
            ).wait();

        }
        time.push_back( hpx::util::high_resolution_clock::now() - start );
    }

    return hpx::detail::median(time.begin(),time.end());
}
///////////////////////////////////////////////////////////////////////////////
void image_coarray( hpx::parallel::spmd_block block
                  , std::size_t height
                  , std::size_t width
                  , std::size_t local_height
                  , std::size_t local_width
                  , std::size_t local_leading_dimension
                  , int test_count
                  , boost::uint64_t seq_ref
                  )
{
    const std::size_t partition_size = local_height*local_width;

    hpx::coarray<double,2> out( block, "out", {height,width}, hpx::partition<double>(partition_size) );
    hpx::coarray<double,2> in ( block, "in",  {height,width}, hpx::partition<double>(partition_size) );

    // hpx::cout << "hpx::coarray<double>: "
    //     << double(seq_ref)/transpose_coarray(block, out, in, height, width, local_height, local_width, local_leading_dimension, test_count)
    //     << "\n";

    std::size_t size = 2*height*width*local_height*local_width*sizeof(double);

    hpx::cout << "performances : "
    << double(size)/transpose_coarray(block, out, in, height, width, local_height, local_width, local_leading_dimension, test_count)
    << " GBs\n";
}
HPX_DEFINE_PLAIN_ACTION(image_coarray);

///////////////////////////////////////////////////////////////////////////////
int hpx_main(boost::program_options::variables_map& vm)
{
    std::size_t matrix_order = vm["matrix_order"].as<std::size_t>();
    std::size_t partition_order = vm["partition_order"].as<std::size_t>();
    int test_count = vm["test_count"].as<int>();

    matrix_order = (matrix_order/partition_order)*partition_order;             // Ensure that matrix_order is a multiple of partition_order

    image_coarray_action act;

    // verify that input is within domain of program
    if (test_count == 0 || test_count < 0) {
        hpx::cout << "test_count cannot be zero or negative...\n" << hpx::flush;
    }
    else if (partition_order > matrix_order) {
        hpx::cout << "partition_order cannot be greater than matrix_order...\n" << hpx::flush;
    }
    else {
        // 1) retrieve reference time
        std::vector<double> out(matrix_order*matrix_order);
        std::vector<double> in (matrix_order*matrix_order);

        boost::uint64_t seq_ref = transpose_matrix( out, in, matrix_order, matrix_order, matrix_order, test_count); //-V106

        // 2) Coarray and for_each
        std::size_t height = matrix_order / partition_order;
        std::size_t width  = matrix_order / partition_order;

        std::size_t local_height = partition_order;
        std::size_t local_width  = partition_order;
        std::size_t local_leading_dimension = local_height;


        auto localities = hpx::find_all_localities();
        hpx::parallel::define_spmd_block( localities, act
                                        , height, width, local_height, local_width, local_leading_dimension, test_count, seq_ref
                                        ).get();
    }

    return hpx::finalize();
}

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    //initialize program
    std::vector<std::string> cfg;
    cfg.push_back("hpx.os_threads=" +
        boost::lexical_cast<std::string>(hpx::threads::hardware_concurrency()));
    boost::program_options::options_description cmdline(
        "usage: " HPX_APPLICATION_STRING " [options]");

    cmdline.add_options()
        ("matrix_order"
        , boost::program_options::value<std::size_t>()->default_value(20000)
        , "order of matrix (default: 40)")

        ("partition_order"
        , boost::program_options::value<std::size_t>()->default_value(1000)
        , "order of partition (default: 10)")

        ("test_count"
        , boost::program_options::value<int>()->default_value(5) // for overall time of 10 ms
        , "number of tests to be averaged (default: 5)")
        ;

    return hpx::init(cmdline, argc, argv, cfg);
}
