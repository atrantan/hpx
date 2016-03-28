#include <hpx/hpx.hpp>
#include <hpx/util/high_resolution_clock.hpp>
#include <hpx/include/iostreams.hpp>
#include <hpx/include/parallel_algorithm.hpp>
#include <hpx/include/parallel_executor_parameters.hpp>

#include <read_mm.h>

#include <utility/median.hpp>

#include <boost/cstdint.hpp>
#include <boost/range.hpp>

#include <iostream>
#include <cstdio>
#include <vector>
#include <iterator>

#include <mkl.h>

struct spmatrix
{
    spmatrix(std::string filename, std::size_t grain_factor)
    {
        std::size_t N = hpx::get_os_thread_count()*grain_factor;

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



///////////////////////////////////////////////////////////////////////////////
void image_vector( spmatrix & a, std::vector<double> const & x, std::string filename, int test_count, int grain_factor, std::size_t rank)
{
    std::vector<double> y( a.chunksize_  );

    for (int iter = 0; iter != test_count; ++iter)
    {
        int begin = a.begins_[rank];
        int chunksize = a.sizes_[rank];

        double * out = y.data();

        const int * row  = a.rows_.data() + begin;
        const int * idx  = a.indices_.data() + *row - 1;
        const double * val = a.values_.data() + *row - 1;

        char transa('N');
        mkl_dcsrgemv( &transa
                    , &chunksize
                    , val
                    , row
                    , idx
                    , x.data()
                    , out
                    );
    }
}
///////////////////////////////////////////////////////////////////////////////

int hpx_main(boost::program_options::variables_map& vm)
{
    std::string filename = vm["filename"].as<std::string>();
    int test_count       = vm["test_count"].as<int>();
    std::size_t grain_factor = vm["grain_factor"].as<std::size_t>();

    // verify that input is within domain of program
    if (test_count == 0 || test_count < 0) {
        hpx::cout << "test_count cannot be zero or negative...\n" << hpx::flush;
    }

    std::size_t N = hpx::get_os_thread_count()*grain_factor;

    hpx::parallel::static_chunk_size scs(1);
    auto r=boost::irange(0ul,N);

    spmatrix a(filename, grain_factor);
    std::size_t size = 2 * a.nnz_;
    std::size_t datasize = (a.nnz_ + 2*a.m_)*sizeof(double);

    std::vector<double> x( a.n_ );

    boost::uint64_t start = hpx::util::high_resolution_clock::now();
          hpx::parallel::for_each(
            hpx::parallel::par.with(scs),
            r.begin(), r.end(),
            [&](std::size_t rank)
            {
                image_vector(a, x, filename, test_count, grain_factor, rank);
            });
     boost::uint64_t toc = ( hpx::util::high_resolution_clock::now() - start ) / test_count;
     printf("performances : %f GFlops\n", double(size)/toc);
     printf("performances : %f GBs\n", double(datasize)/toc);

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
    ("filename"
    , boost::program_options::value<std::string>()->default_value("")
    , "filename of the matrix market (default: "")")

    ("test_count"
    , boost::program_options::value<int>()->default_value(100) // for overall time of 10 ms
    , "number of tests to be averaged (default: 100)")

    ("grain_factor"
    , boost::program_options::value<std::size_t>()->default_value(1)
    , "number of tasks per thread (default: 10)")
    ;

    return hpx::init(cmdline, argc, argv, cfg);
}
