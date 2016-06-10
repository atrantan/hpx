//  Copyright (c) 2016 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx_init.hpp>
#include <hpx/hpx.hpp>
#include <hpx/util/high_resolution_clock.hpp>
#include <hpx/include/iostreams.hpp>

#include <boost/cstdint.hpp>
#include <boost/format.hpp>
#include <boost/range/functions.hpp>

#include <spmd_block/spmd_block.hpp>
#include <barrier_algorithms/barrier_algorithms.hpp>

#include <utility/median.hpp>

#include <stdexcept>
#include <algorithm>
#include <string>

///////////////////////////////////////////////////////////////////////////////
int delay;  // 0.1 ms
///////////////////////////////////////////////////////////////////////////////
inline void worker_timed(boost::uint64_t delay_ns)
{
    if (delay_ns == 0)
        return;

    boost::uint64_t start = hpx::util::high_resolution_clock::now();

    while (true)
    {
        // Check if we've reached the specified delay.
        if ((hpx::util::high_resolution_clock::now() - start) >= delay_ns)
            break;
    }
}
///////////////////////////////////////////////////////////////////////////////
int num_overlapping_loops = 0;
///////////////////////////////////////////////////////////////////////////////
template <typename Vector>
struct wait_op
{
    typedef typename Vector::value_type value_type;

    void operator()(value_type) const
    {
        worker_timed(delay);
    }
};
///////////////////////////////////////////////////////////////////////////////
template<typename Policy>
boost::uint64_t foreach_vector( hpx::parallel::spmd_block & block
                              , Policy const & p
                              , std::vector<int> const & v
                              , int test_count
                              )
{
    using vector_type = std::vector<int>;

    std::vector<boost::uint64_t> time;

    boost::uint64_t start = hpx::util::high_resolution_clock::now();
    for (int i = 0; i != test_count; ++i)
    {
        std::for_each( v.begin(), v.end(), wait_op<vector_type>() );
        block.barrier_sync(p, std::to_string(i));
    }
    return (hpx::util::high_resolution_clock::now() - start) / test_count;
}
///////////////////////////////////////////////////////////////////////////////
void image( hpx::parallel::spmd_block block
          , std::size_t vector_size
          , int delay_
          , std::string barrier_policy
          , int test_count
          )
{
    delay = delay_;

    std::vector<int> v(vector_size);

    if(barrier_policy == "central")
    {
        hpx::cout << "Time : "
            << foreach_vector(block, hpx::detail::central, v, test_count)
            << " ns\n";
    }
    else if(barrier_policy == "dissemination")
    {
        hpx::cout << "Time : "
            << foreach_vector(block, hpx::detail::dissemination, v, test_count)
            << " ns\n";
    }
    else
    {
        hpx::cout << "Time : "
            << foreach_vector(block, hpx::detail::pairwise, v, test_count)
            << " ns\n";
    }
}
HPX_DEFINE_PLAIN_ACTION(image);

///////////////////////////////////////////////////////////////////////////////
int hpx_main(boost::program_options::variables_map& vm)
{
    std::size_t vector_size = vm["vector_size"].as<std::size_t>();
    delay = vm["work_delay"].as<int>();
    std::string barrier_policy = vm["barrier_policy"].as<std::string>();
    int test_count = vm["test_count"].as<int>();

    image_action act;

    // verify that input is within domain of program
    if (test_count == 0 || test_count < 0) {
        hpx::cout << "test_count cannot be zero or negative...\n" << hpx::flush;
    }
    else if (delay < 0) {
        hpx::cout << "delay cannot be a negative number...\n" << hpx::flush;
    }
    else {
        auto localities = hpx::find_all_localities();
        hpx::parallel::define_spmd_block( localities, act
                                        , vector_size, delay, barrier_policy, test_count
                                        ).get();
    }

    return hpx::finalize();
}

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    boost::program_options::options_description cmdline(
        "usage: " HPX_APPLICATION_STRING " [options]");

    cmdline.add_options()
        ("vector_size"
        , boost::program_options::value<std::size_t>()->default_value(10)
        , "size of vector (default: 10)")

        ("work_delay"
        , boost::program_options::value<int>()->default_value(1000000)
        , "loop delay per element in nanoseconds (default: 1000000)")

        ("barrier_policy"
        , boost::program_options::value<std::string>()->default_value("central")
        , "barrier_policy (default: central)")

        ("test_count"
        , boost::program_options::value<int>()->default_value(100) // for overall time of 10 ms
        , "number of tests to be averaged (default: 100)")
        ;

    return hpx::init(cmdline, argc, argv);
}

