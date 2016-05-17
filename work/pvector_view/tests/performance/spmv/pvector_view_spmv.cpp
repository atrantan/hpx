#include <hpx/hpx.hpp>
#include <hpx/util/high_resolution_clock.hpp>
#include <hpx/include/iostreams.hpp>

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
                            , int unroll_factor)
{
    std::vector<boost::uint64_t> time;
    std::size_t N = block.get_num_images();

    int rank = block.this_image();
    int begin = a.begins_[rank];
    int chunksize = a.sizes_[rank];

    double * out = y.data(_).data();

    const int * row  = a.rows_.data() + begin;
    const int * idx  = a.indices_.data() + *row - 1;
    const double * val = a.values_.data() + *row - 1;

    boost::uint64_t start = hpx::util::high_resolution_clock::now();
    for (int iter = 0; iter != test_count; ++iter)
    {
        // for (int i = 0; i < chunksize; i++, row++, out++)
        // {
        //     double tmp = 0.;
        //     int end = *(row + 1);
        //     for ( int o = *row; o < end;  o++, val++, idx++)
        //     {
        //         tmp += *val * x[*idx - 1];
        //     }
        //     *out = tmp;
        // }

        char transa('N');
        mkl_dcsrgemv( &transa
                    , &chunksize
                    , val
                    , row
                    , idx
                    , x.data()
                    , out
                    );

        if( !( (iter + 1) % unroll_factor) )
        {
            block.barrier_sync("spmv" + iter);
        }
    }
    return ( hpx::util::high_resolution_clock::now() - start ) / test_count;
}

///////////////////////////////////////////////////////////////////////////////
void image_coarray( hpx::parallel::spmd_block block, std::string filename, int test_count, int unroll_factor)
{
    spmatrix a(filename);

    hpx::coarray<double,1> y( block, "y", {_}, hpx::partition<double>( a.chunksize_ ) );
    std::vector<double> x(a.n_);

    std::size_t size = 2 * a.nnz_;
    std::size_t datasize = (a.nnz_ + 2*a.m_)*sizeof(double);

    boost::uint64_t toc = spmv_coarray(block,a,x,y,test_count,unroll_factor);
    printf("performances : %f GFlops\n", double(size)/toc);
    printf("performances : %f GBs\n", double(datasize)/toc);
}
HPX_DEFINE_PLAIN_ACTION(image_coarray);
///////////////////////////////////////////////////////////////////////////////

int hpx_main(boost::program_options::variables_map& vm)
{
    std::string filename = vm["filename"].as<std::string>();
    int test_count       = vm["test_count"].as<int>();
    int unroll_factor    = vm["unroll_factor"].as<int>();

    // verify that input is within domain of program
    if (test_count == 0 || test_count < 0) {
        hpx::cout << "test_count cannot be zero or negative...\n" << hpx::flush;
    }

    image_coarray_action act;

    auto localities = hpx::find_all_localities();
    hpx::parallel::define_spmd_block( localities, act, filename, test_count, unroll_factor).get();

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

    ("unroll_factor"
    , boost::program_options::value<int>()->default_value(10) // for overall time of 10 ms
    , "number of iterations to be merged  (default: 10)")
    ;

    return hpx::init(cmdline, argc, argv, cfg);
}
