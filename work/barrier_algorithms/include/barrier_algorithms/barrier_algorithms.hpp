////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine TRAN TAN
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(BARRIER_ALGORITHMS_HPP)
#define BARRIER_ALGORITHMS_HPP

#include <hpx/lcos/barrier.hpp>
#include <hpx/runtime/launch_policy.hpp>

#include <remote_promise/remote_promise.hpp>

#include <memory>
#include <string>

namespace hpx{

// Implementation of barrier
    namespace detail{

        struct central_policy{};
        struct pairwise_policy{};
        struct dissemination_policy{};

        constexpr central_policy central;
        constexpr pairwise_policy pairwise;
        constexpr dissemination_policy dissemination;

        std::size_t get_rank( std::vector<hpx::id_type> const & localities )
        {
            using const_iterator
                = typename std::vector<hpx::id_type>::const_iterator;

            std::size_t rank = 0;
            const_iterator l_it = localities.cbegin();
            const_iterator l_end = localities.cend();
            const hpx::id_type here = hpx::find_here();

            while( *l_it != here && l_it != l_end ) { l_it++; rank++;}
            return rank;
        }


        template<typename Policy = detail::central_policy>
        struct custom_barrier_impl;

        template<>
        struct custom_barrier_impl<detail::central_policy>
        {
            static hpx::future<void> call(
                std::vector<hpx::id_type> const & localities,
                std::string && barrier_name)
            {
                using barrier_type = hpx::lcos::barrier;
                using event_type = hpx::future<void>;

                std::size_t numlocs = localities.size();
                std::size_t root
                    = hpx::naming::get_locality_id_from_id(localities[0]);

                // create the barrier, register it with AGAS
                return hpx::lcos::barrier(
                    barrier_name + "_default", numlocs, root)
                        .wait(hpx::launch::async);
            }
        };

        template<>
        struct custom_barrier_impl<detail::dissemination_policy>
        {
            static hpx::future<void> call(
                std::vector<hpx::id_type> const & localities,
                std::string && barrier_name)
            {
                const std::size_t numlocs = localities.size();
                const hpx::id_type here = hpx::find_here();
                const std::size_t rank = detail::get_rank(localities);

                const bool is_power_of_two
                    = (!(numlocs & (numlocs - 1)) && numlocs);

                hpx::future<void> f = hpx::make_ready_future();

                for(std::size_t i = 1, s = 0, iend = localities.size();
                    i<iend;
                    i*=2, s++)
                {
                    std::size_t dest
                        = is_power_of_two ?  rank ^ i : (rank+i) % iend;

                    f = f.then(
                            [barrier_name, here, rank, dest, s]
                                ( hpx::future<void> f_ )
                                {
                                    f_.get();

                                // Prepare own flag for external notification
                                    hpx::remote_promise<void> brecv(here);

                                    brecv.register_as( barrier_name
                                        + "_" + std::to_string(rank)
                                        + "_" + std::to_string(s) );

                                // Notify the partner
                                    hpx::remote_promise<void> bsend;

                                    bsend.connect_to( barrier_name
                                        + "_" + std::to_string(dest)
                                        + "_" + std::to_string(s) );

                                    bsend.set_value();

                                // Asynchronously wait for the other partner
                                    return brecv.get_future()
                                        .then([brecv](hpx::future<void> event)
                                                {
                                                    event.get();
                                                });
                                });
                }

                return f;
           }
        };

        template<>
        struct custom_barrier_impl<detail::pairwise_policy>
        {
            static hpx::future<void> call(
                std::vector<hpx::id_type> const & localities,
                std::string && barrier_name)
           {
                const std::size_t numlocs = localities.size();
                const hpx::id_type here = hpx::find_here();
                const std::size_t rank = detail::get_rank(localities);

                // Evaluate the cutoff
                std::size_t i = 1;
                while( (i*=2) <= numlocs );
                const std::size_t cutoff = i >> 1;

                if(rank >= cutoff)
                {
                    // Prepare own flag for external notification
                    hpx::remote_promise<void> brecv(here);
                    brecv.register_as(barrier_name
                        + "_" + std::to_string(rank) + "_epilogue");

                    // Notify the partner
                    hpx::remote_promise<void> bsend;
                    bsend.connect_to( barrier_name
                        + "_" + std::to_string(rank-cutoff) + "_prologue");

                    bsend.set_value();

                    // Wait for the other partner
                    return brecv.get_future()
                        .then(
                            [brecv]( hpx::future<void> event )
                            {
                                event.get();
                            });
                }
                else
                {
                    const bool has_carry_partner = rank < (numlocs - cutoff);
                    hpx::future<void> f = hpx::make_ready_future();

                    if(has_carry_partner)
                    {
                        hpx::remote_promise<void> carry(here);

                        carry.register_as(
                            barrier_name
                            + "_" + std::to_string(rank) + "_prologue" );

                        f = carry.get_future()
                                .then(
                                    [carry]( hpx::future<void> event )
                                    {
                                        event.get();
                                    });
                    }

                    for(std::size_t i = 1, s = 0, iend = cutoff;
                        i<iend;
                        i*=2, s++)
                    {
                        std::size_t dest = rank ^ i;

                        f = f.then(
                            [barrier_name, here, rank, dest, s]
                            (hpx::future<void> f_)
                            {
                                f_.get();

                        // Prepare own flag for external notification
                                hpx::remote_promise<void> brecv(here);

                                brecv.register_as(barrier_name
                                    + "_" + std::to_string(rank)
                                    + "_" + std::to_string(s));

                        // Notify the partner
                                hpx::remote_promise<void> bsend;

                                bsend.connect_to(barrier_name
                                    + "_" + std::to_string(dest)
                                    + "_" + std::to_string(s));

                                bsend.set_value();

                        // Wait for the other partner
                                return brecv.get_future()
                                    .then(
                                        [brecv](hpx::future<void> event)
                                        {
                                            event.get();
                                        });
                            });
                    }

                    if(has_carry_partner)
                    {
                        std::size_t dest = rank+cutoff;

                        f = f.then(
                                [barrier_name, dest]( hpx::future<void> f_ )
                                    {
                                        f_.get();

                                        hpx::remote_promise<void> bsend;

                                        bsend.connect_to(barrier_name
                                            + "_" + std::to_string(dest)
                                            + "_epilogue");

                                        bsend.set_value();
                                    });
                    }

                    return f;
                }


           }
        };
    }

    template<typename Policy>
    hpx::future<void> custom_barrier(
        Policy const &,
        std::vector<hpx::id_type> const & localities,
        std::string && barrier_name)
        {
            return detail::custom_barrier_impl<Policy>::call(
                         localities, barrier_name + "_hpx_barrier");
        }


    template<typename Policy>
    void custom_barrier(hpx::launch::sync_policy const &,
                        Policy const & p,
                        std::vector<hpx::id_type> const & localities,
                        std::string && barrier_name
                       )
    {
        custom_barrier(p, localities, barrier_name + "_hpx_barrier").get();
    }
}

#endif

