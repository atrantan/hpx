#include <hpx/hpx_init.hpp>
#include <hpx/hpx.hpp>

#include <read_mm.h>
#include <pvector_view/pvector_view.hpp>
#include <spmd_block/spmd_block.hpp>

#include <iostream>
#include <vector>

using hpx::Here;
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
    }

    int m_, n_, nnz_, chunksize_;
    std::vector<int> rows_;
    std::vector<int> indices_;
    std::vector<double> values_;
    std::vector<int> begins_;
    std::vector<int> sizes_;
};



void spmv( hpx::parallel::spmd_block & block, spmatrix & a, hpx::coarray<double,1> & x, hpx::coarray<double,1> & y )
{
    int rank = block.rank();
    int begin = a.begins_[rank];
    int chunksize = a.sizes_[rank];

    std::vector<double>::iterator out = y.data(Here).begin();

    std::vector<int>::iterator row    = a.rows_.begin()    + begin;
    std::vector<int>::iterator idx    = a.indices_.begin() + *row;
    std::vector<double>::iterator val = a.values_.begin()  + *row;

    for (int i = 0; i < chunksize; i++, row++, out++)
    {
        int end = *(row + 1);

        for ( int o = *row  ; o < end;  o++, val++, idx++)
        {
            int other_rank = 0;
            do
            {
                ++other_rank;
            }
            while( *idx >= a.begins_[other_rank] );
            other_rank --;

            int local_idx = *idx - a.begins_[other_rank];

            *out += *val * x(other_rank)[ local_idx ];
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
void image_coarray( hpx::parallel::spmd_block block, std::string filename)
{
    spmatrix a(filename);

    hpx::coarray<double,1> y( block, "y", {_}, hpx::partition<double>( a.chunksize_ ) );
    hpx::coarray<double,1> x( block, "x", {_}, hpx::partition<double>( a.chunksize_ ) );

    spmv(block,a,x,y);

    block.barrier_sync("");
}
HPX_DEFINE_PLAIN_ACTION(image_coarray);
///////////////////////////////////////////////////////////////////////////////

int hpx_main(boost::program_options::variables_map& vm)
{
    std::string filename = vm["filename"].as<std::string>();

    image_coarray_action act;

    auto localities = hpx::find_all_localities();
    hpx::parallel::define_spmd_block( localities, act, filename ).get();

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
    ;

    return hpx::init(cmdline, argc, argv, cfg);
}
