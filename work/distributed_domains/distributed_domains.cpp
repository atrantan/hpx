///////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2007-2014 Hartmut Kaiser
//  Copyright (c) 2011 Bryce Adelstein-Lelbach
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////

#include <hpx/hpx_main.hpp>
#include <hpx/include/lcos.hpp>
#include <hpx/include/actions.hpp>
#include <hpx/include/components.hpp>
#include <hpx/include/iostreams.hpp>

#include <boost/ref.hpp>
#include <boost/format.hpp>
#include <boost/thread/locks.hpp>

#include <vector>
#include <list>
#include <set>
#include <array>



std::size_t hello_world_worker(std::size_t desired)
{
    // Returns the OS-thread number of the worker that is running this
    // HPX-thread.
    std::size_t current = hpx::get_worker_thread_num();
    if (current == desired)
    {
        // The HPX-thread has been run on the desired OS-thread.
/*        char const* msg = "hello world from OS-thread %1% on locality %2%";

        hpx::cout << (boost::format(msg) % desired % hpx::get_locality_id())
                  << std::endl << hpx::flush;
*/
        return desired;
    }
    return std::size_t(-1);
}

HPX_PLAIN_ACTION(hello_world_worker, hello_world_worker_action);
//]

///////////////////////////////////////////////////////////////////////////////
//[hello_world_foreman
void hello_world_foreman()
{
    // Get the number of worker OS-threads in use by this locality.
    std::size_t const os_threads = hpx::get_os_thread_count();

    // The HPX-thread has been run on the desired OS-thread.
    char const* msg = "There are %1% OS-thread on locality %2%";

    hpx::cout << (boost::format(msg) % os_threads % hpx::get_locality_id())
              << std::endl << hpx::flush;

    // Find the global name of the current locality.
    hpx::naming::id_type const here = hpx::find_here();

    std::set<std::size_t> attendance;
    for (std::size_t os_thread = 0; os_thread < os_threads; ++os_thread)
        attendance.insert(os_thread);


    while (!attendance.empty())
    {

        std::vector<hpx::lcos::future<std::size_t> > futures;
        futures.reserve(attendance.size());

        for (std::size_t worker : attendance)
        {

            typedef hello_world_worker_action action_type;
            futures.push_back(hpx::async<action_type>(here, worker));
        }

        hpx::lcos::local::spinlock mtx;
        hpx::lcos::wait_each(
            hpx::util::unwrapped([&](std::size_t t) {
                if (std::size_t(-1) != t)
                {
                    boost::lock_guard<hpx::lcos::local::spinlock> lk(mtx);
                    attendance.erase(t);
                }
            }),
            futures);
    }
}


HPX_PLAIN_ACTION(hello_world_foreman, hello_world_foreman_action);

int main()
{

    std::array<char,100> name; 
    int taille;

    // Get a list of all available localities.
    std::vector<hpx::naming::id_type> localities =
        hpx::find_all_localities();

    // Reserve storage space for futures, one for each locality.
    std::vector<hpx::lcos::future<void> > futures;
    futures.reserve(localities.size());

    for (hpx::naming::id_type const& node : localities)
    {
        typedef hello_world_foreman_action action_type;
        futures.push_back(hpx::async<action_type>(node));
    }

    hpx::wait_all(futures);
    return 0;
}
//]
