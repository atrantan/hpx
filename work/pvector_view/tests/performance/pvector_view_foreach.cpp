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
#include <spmd_block/spmd_block.hpp>
#include <utility/median.hpp>

#include <stdexcept>

///////////////////////////////////////////////////////////////////////////////
// Define the vector types to be used.
HPX_REGISTER_PARTITIONED_VECTOR(int);

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
/*        worker_timed(delay);*/
        worker_timed(delay);
    }
};

///////////////////////////////////////////////////////////////////////////////
template <typename Policy, typename Vector>
boost::uint64_t foreach_vector(Policy const& policy, Vector const& v, int test_count)
{
    typedef typename Vector::value_type value_type;

    std::vector<boost::uint64_t> time;

    boost::uint64_t start = hpx::util::high_resolution_clock::now();

    for (int i = 0; i != test_count; ++i)
    {
        boost::uint64_t start = hpx::util::high_resolution_clock::now();
        {
            hpx::parallel::for_each(
                policy, boost::begin(v), boost::end(v), wait_op<Vector>()
            );
        }
        time.push_back( hpx::util::high_resolution_clock::now() - start );
    }

    return hpx::detail::median(time.begin(),time.end());
}

///////////////////////////////////////////////////////////////////////////////
template <typename Coarray>
boost::uint64_t foreach_coarray(  hpx::parallel::spmd_block & block
                                , Coarray const & v
                                , int test_count
                                )
{
    using hpx::Here;

    std::vector<boost::uint64_t> time;

    for (int i = 0; i != test_count; ++i)
    {
        boost::uint64_t start = hpx::util::high_resolution_clock::now();
        {
            auto const & vlocal = v.data(Here);
            std::for_each( vlocal.begin(), vlocal.end(), wait_op<Coarray>() );
            block.barrier_sync( std::to_string(i) );
        }
        time.push_back( hpx::util::high_resolution_clock::now() - start );
    }

    return hpx::detail::median(time.begin(),time.end());
}
///////////////////////////////////////////////////////////////////////////////
void image_coarray( hpx::parallel::spmd_block block
                  , std::size_t vector_size
                  , int delay_
                  , int test_count
                  , boost::uint64_t seq_ref
                  )
{
    using hpx::_;
    // Constants used by Co-arrays
    const std::size_t numlocs = block.find_all_localities().size();

    delay = delay_;

    // Initialize Coarray and call foreach
    {
        const std::size_t partition_size = vector_size / numlocs;

        hpx::coarray<int,1> v( block, "v", {_}, hpx::partition<int>(partition_size) );

        hpx::cout << "hpx::coarray<int>(par): "
            << double(seq_ref)/foreach_coarray(block,v,test_count)
            << "\n";
    }
}
HPX_DEFINE_PLAIN_ACTION(image_coarray);

///////////////////////////////////////////////////////////////////////////////
int hpx_main(boost::program_options::variables_map& vm)
{
    std::size_t vector_size = vm["vector_size"].as<std::size_t>();
//     bool csvoutput = vm.count("csv_output") != 0;
    delay = vm["work_delay"].as<int>();
    int test_count = vm["test_count"].as<int>();

    image_coarray_action act;

    // verify that input is within domain of program
    if (test_count == 0 || test_count < 0) {
        hpx::cout << "test_count cannot be zero or negative...\n" << hpx::flush;
    }
    else if (delay < 0) {
        hpx::cout << "delay cannot be a negative number...\n" << hpx::flush;
    }
    else {
        // 1) retrieve reference time
        std::vector<int> ref(vector_size);
        boost::uint64_t seq_ref = foreach_vector(hpx::parallel::seq, ref, test_count); //-V106

        // 2) Coarray and for_each
        auto localities = hpx::find_all_localities();
        hpx::parallel::define_spmd_block( localities, act
                                        , vector_size, delay, test_count, seq_ref
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
        ("vector_size"
        , boost::program_options::value<std::size_t>()->default_value(1000)
        , "size of vector (default: 1000)")

        ("work_delay"
        , boost::program_options::value<int>()->default_value(1000000)
        , "loop delay per element in nanoseconds (default: 1000000)")

        ("test_count"
        , boost::program_options::value<int>()->default_value(10) // for overall time of 10 ms
        , "number of tests to be averaged (default: 10)")
        ;

    return hpx::init(cmdline, argc, argv, cfg);
}

