////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine TRAN TAN
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#include <hpx/include/lcos.hpp>
#include <hpx/include/actions.hpp>

#if !defined(WAIT_IMAGES_HPP)
#define WAIT_IMAGES_HPP

namespace hpx{

// Implementation of barrier
    namespace detail{

        template<std::size_t N>
        struct wait_images_sync_impl
        {
           static void call(std::string && barrier_name, std::initializer_list<std::size_t> && list)
           {
                bool is_in_list = false;
                std::size_t here = hpx::get_locality_id();

                for(auto && i : list)
                    is_in_list |= (here == i);

                if( here == *list.begin() )
                {
                    // create the barrier, register it with AGAS
                    hpx::lcos::barrier b = hpx::new_<hpx::lcos::barrier>(hpx::find_here(),N);
                    b.register_as(barrier_name);
                    b.wait();
                }
                else if( is_in_list )
                {
                    hpx::lcos::barrier b;
                    b.connect_to(barrier_name);
                    b.wait();
                }
           }
        };

        template<>
        struct wait_images_sync_impl<0ul>
        {
            static void call(std::string && barrier_name, std::initializer_list<std::size_t> &&)
            {
                std::size_t here = hpx::get_locality_id();
                std::size_t numlocs = hpx::get_num_localities_sync();

                if( here == 0 )
                {
                    // create the barrier, register it with AGAS
                    hpx::lcos::barrier b = hpx::new_<hpx::lcos::barrier>(hpx::find_here(),numlocs);
                    b.register_as(barrier_name);
                    b.wait();
                }
                else
                {
                    hpx::lcos::barrier b;
                    b.connect_to(barrier_name);
                    b.wait();
                }
            }
        };

        template<std::size_t N>
        struct wait_images_impl
        {
           static hpx::future<void> call(std::string && barrier_name, std::initializer_list<std::size_t> && list)
           {
                bool is_in_list = false;
                std::size_t here = hpx::get_locality_id();

                for(auto && i : list)
                    is_in_list |= (here == i);

                if( here == *list.begin() )
                {
                    // create the barrier, register it with AGAS
                    hpx::lcos::barrier b = hpx::new_<hpx::lcos::barrier>(hpx::find_here(),N);

                    b.register_as(barrier_name);

                    return b.wait_async().then(
                            [b](hpx::future<void> f){ return f.get(); }
                            );
                }
                else if( is_in_list )
                {
                    hpx::lcos::barrier b;
                    b.connect_to(barrier_name);
                    return b.wait_async();
                }
           }
        };

        template<>
        struct wait_images_impl<0ul>
        {
            static hpx::future<void> call(std::string && barrier_name, std::initializer_list<std::size_t> &&)
            {
                std::size_t here = hpx::get_locality_id();
                std::size_t numlocs = hpx::get_num_localities_sync();

                if( here == 0 )
                {
                    // create the barrier, register it with AGAS
                    hpx::lcos::barrier b = hpx::new_<hpx::lcos::barrier>(hpx::find_here(),numlocs);

                    b.register_as(barrier_name);

                    return b.wait_async().then(
                            [b](hpx::future<void> f){ return f.get(); }
                            );
                }
                else
                {
                    hpx::lcos::barrier b;
                    b.connect_to(barrier_name);
                    return b.wait_async();
                }
            }
        };
    }

    template<typename ... I>
    void wait_images_sync(std::string barrier_name, I && ... i)
    {
        detail::wait_images_sync_impl<sizeof...(I)>::call(barrier_name + "_hpx_wait_images", {std::size_t(i)...} );
    }

    template<typename ... I>
    hpx::future<void> wait_images(std::string barrier_name, I && ... i)
    {
        return detail::wait_images_impl<sizeof...(I)>::call(barrier_name + "_hpx_wait_images", {std::size_t(i)...} );
    }
}

#endif

