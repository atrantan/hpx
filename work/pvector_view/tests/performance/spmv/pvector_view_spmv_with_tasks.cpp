#include <hpx/hpx.hpp>
#include <hpx/util/high_resolution_clock.hpp>
#include <hpx/include/iostreams.hpp>
#include <hpx/include/parallel_algorithm.hpp>
#include <hpx/include/parallel_executor_parameters.hpp>

#include <read_mm.h>

#include <pvector_view/pvector_view.hpp>
#include <spmd_block/spmd_block.hpp>
#include <utility/median.hpp>

#include <boost/cstdint.hpp>

#include <iostream>
#include <vector>
#include <iterator>

#include <mkl.h>

using hpx::_;

///////////////////////////////////////////////////////////////////////////////
// Define the vector types to be used.
HPX_REGISTER_PARTITIONED_VECTOR(double);

struct spmatrix
{
    spmatrix(std::string filename)
    {
        auto localities = hpx::find_all_localities();
        std::size_t N = localities.size();

        read_mm(filename, m_, n_, nnz_, rows_, indices_, values_);

        sizes_ = std::vector<int>(N);
        begins_= std::vector<int>(N+1);

         // Initialize ofsets and sizes for distribution issues
        sizes_[0]  = chunksize_ = m_/N + (m_%N>0);
        begins_[0] = 0;

        for(std::size_t i=1; i<N; i++)
        {
            sizes_[i]  = m_/N + (m_%N > i);
            begins_[i] = sizes_[i-1] + begins_[i-1];
        }
        begins_[N] = m_;

        for (int & value : rows_)
        value++;

        for (int & value : indices_)
        value++;
    }

    int m_, n_, nnz_, chunksize_;
    std::vector<int> rows_;
    std::vector<int> indices_;
    std::vector<double> values_;
    std::vector<int> begins_;
    std::vector<int> sizes_;
};


boost::uint64_t spmv_coarray( hpx::parallel::spmd_block & block
                            , spmatrix const & a
                            , std::vector<double> & x
                            , hpx::coarray<double,1> & y
                            , int test_count
                            , int grain_factor
                            , int unroll_factor)
{
    std::size_t N = block.get_num_images();

    int rank = block.this_image();
    int begin = a.begins_[rank];
    int chunksize = a.sizes_[rank];

// Manage inner parallelism
    std::size_t inner_N = hpx::get_os_thread_count()*grain_factor;
    std::vector<int> inner_sizes(inner_N);
    std::vector<int> inner_begins(inner_N + 1);

    inner_sizes[0]  = chunksize/inner_N + (chunksize%inner_N>0);
    inner_begins[0] = 0;

    for(std::size_t i=1; i<inner_N; i++)
    {
        inner_sizes[i]  = chunksize/inner_N + (chunksize%inner_N > i);
        inner_begins[i] = inner_sizes[i-1] + inner_begins[i-1];
    }
    inner_begins[inner_N] = chunksize;

    hpx::parallel::static_chunk_size scs(1);
    auto r=boost::irange(0ul,inner_N);
    int iter_end = test_count / unroll_factor;

    boost::uint64_t start = hpx::util::high_resolution_clock::now();
    for (int iter = 0; iter != iter_end; ++iter)
    {
        hpx::parallel::for_each(
        hpx::parallel::par.with(scs),
        r.begin(), r.end(),
        [&](std::size_t i)
        {
            double * out = y.data(_).data() + inner_begins[i];
            const int * row  = a.rows_.data() + begin + inner_begins[i];
            const int * idx  = a.indices_.data() + *row - 1;
            const double * val = a.values_.data() + *row - 1;

            for (int k = 0; k != unroll_factor; ++k)
            {
                char transa('N');
                mkl_dcsrgemv( &transa
                            , &inner_sizes[i]
                            , val
                            , row
                            , idx
                            , x.data()
                            , out
                            );
            }
        });

        block.barrier(hpx::launch::sync, "spmv" + iter);
    }
    return ( hpx::util::high_resolution_clock::now() - start ) / test_count;

}

///////////////////////////////////////////////////////////////////////////////
int hpx_main(boost::program_options::variables_map& vm)
{
    std::string filename = vm["filename"].as<std::string>();
    int test_count       = vm["test_count"].as<int>();
    std::size_t grain_factor = vm["grain_factor"].as<std::size_t>();
    int unroll_factor    = vm["unroll_factor"].as<int>();

    // verify that input is within domain of program
    if (test_count == 0 || test_count < 0) {
        hpx::cout << "test_count cannot be zero or negative...\n" << hpx::flush;
    }

    auto image_coarray =
    []( hpx::parallel::spmd_block block, std::string filename, int test_count, std::size_t grain_factor, int unroll_factor)
    {
        spmatrix a(filename);

        hpx::coarray<double,1> y( block, "y", {_}, hpx::partition<double>( a.chunksize_ ) );
        std::vector<double> x(a.n_);

        std::size_t size = 2 * a.nnz_;
        std::size_t datasize = (a.nnz_ + 2*a.m_)*sizeof(double);

        boost::uint64_t toc = spmv_coarray(block,a,x,y,test_count,grain_factor,unroll_factor);
        printf("performances : %f GFlops\n", double(size)/toc);
        printf("performances : %f GBs\n", double(datasize)/toc);
    };

    auto localities = hpx::find_all_localities();
    hpx::parallel::define_spmd_block( localities, image_coarray, filename, test_count, grain_factor, unroll_factor).get();

    return hpx::finalize();
}

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    boost::program_options::options_description cmdline(
        "usage: " HPX_APPLICATION_STRING " [options]");

    cmdline.add_options()
    ("filename"
    , boost::program_options::value<std::string>()->default_value("")
    , "filename of the matrix market (default: "")")

    ("test_count"
    , boost::program_options::value<int>()->default_value(100) // for overall time of 10 ms
    , "number of tests to be averaged (default: 100)")

    ("grain_factor"
    , boost::program_options::value<std::size_t>()->default_value(1)
    , "number of tasks per thread (default: 10)")

    ("unroll_factor"
    , boost::program_options::value<int>()->default_value(10) // for overall time of 10 ms
    , "number of iterations to be merged  (default: 10)")
    ;

    return hpx::init(cmdline, argc, argv);
}
