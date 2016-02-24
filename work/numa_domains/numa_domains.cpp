//  Copyright (c) 2007-2012 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

///////////////////////////////////////////////////////////////////////////////
// The purpose of this example is to execute a HPX-thread printing
// "Hello World!" once. That's all.

//[simplest_hello_world_getting_started
// Including 'hpx/hpx_main.hpp' instead of the usual 'hpx/hpx_init.hpp' enables
// to use the plain C-main below as the direct main HPX entry point.
#include <hpx/hpx.hpp>

#include <hpx/include/iostreams.hpp>
#include <hpx/include/threads.hpp>

#include <hpx/parallel/util/numa_allocator.hpp>

#include <vector>
#include <utility>
    
int hpx_main(boost::program_options::variables_map& vm)
{
    // Define aliases for executor type
    using executor_t 
    = hpx::threads::executors::local_priority_queue_attached_executor;


    // extract hardware topology
    hpx::threads::topology const& topo ( hpx::threads::create_topology() );
    std::size_t numa_nodes ( topo.get_number_of_numa_nodes() );
    std::size_t cores ( topo.get_number_of_cores() );
    std::size_t pus ( hpx::threads::hardware_concurrency() );


    // Say hello to the world!
    hpx::cout << "Your machine has : \n"<< hpx::flush;
    hpx::cout << cores <<" HW cores\n" << hpx::flush;
    hpx::cout << pus <<" SW cores\n" << hpx::flush;
    hpx::cout << pus/numa_nodes <<" SW cores per NUMA domain\n" << hpx::flush;

    std::vector<executor_t> execs;
    execs.reserve(numa_nodes);

     // creating our executors ....

    std::size_t i = 0, ii=0;

    while (i < pus)
    {
        std::size_t numa_cores ( topo.get_number_of_numa_node_cores(ii) );
        std::size_t numa_pus ( topo.get_number_of_numa_node_pus(ii) );

        // create executor for this NUMA domain
        execs.emplace_back( i, numa_cores ); // i : 1st thread_id
                                             // numa_cores : number of threads

        printf("Placing %u threads for domain %u\n", numa_cores, ii);

        i+=numa_pus;
        ii++;
    }

    std::vector< hpx::future<void> > workers;

    for (std::size_t i = 0; i != numa_nodes; ++i)
    {
        workers.push_back(
            hpx::async(execs[i],
            [](){
                   printf("Hello from thread %u\n"
                         , hpx::get_worker_thread_num()
                         );
                }
            )
        ); 
    }

    return hpx::finalize(); // Handles HPX shutdown
}

int main(int argc, char* argv[])
{
    using boost::program_options::options_description;

    options_description cmdline("usage: " HPX_APPLICATION_STRING " [options]");

    std::vector<std::string> cfg = {"hpx.numa_sensitive=2" // no-cross NUMA stealing
                                   ,"hpx.os_threads=all"   // use all cores
                                   };  

    return hpx::init(cmdline, argc, argv, cfg);
}


