////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine TRAN TAN
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(BARRIER_ALGORITHMS_HPP)
#define BARRIER_ALGORITHMS_HPP

#include <hpx/lcos/barrier.hpp>
#include <remote_promise/remote_promise.hpp>
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
            using const_iterator = typename std::vector<hpx::id_type>::const_iterator;

            std::size_t rank = 0;
            const_iterator l_it = localities.cbegin();
            const_iterator l_end = localities.cend();
            const hpx::id_type here = hpx::find_here();

            while( *l_it != here && l_it != l_end ) { l_it++; rank++;}
            return rank;
        }


        template<typename Policy = detail::central_policy>
        struct custom_barrier_sync_impl;

        template<>
        struct custom_barrier_sync_impl<detail::central_policy>
        {
           static void call( std::vector<hpx::id_type> const & localities
                           , std::string && barrier_name
                           )
           {
                std::size_t numlocs = localities.size();
                hpx::id_type here = hpx::find_here();

                if( here == localities[0] )
                {
                    // create the barrier, register it with AGAS
                    hpx::lcos::barrier b = hpx::new_<hpx::lcos::barrier>(here,numlocs);
                    b.register_as(barrier_name + "_default");
                    b.wait();
                }
                else
                {
                    hpx::lcos::barrier b;
                    b.connect_to(barrier_name + "_default");
                    b.wait();
                }
            }
        };

        template<>
        struct custom_barrier_sync_impl<detail::dissemination_policy>
        {
           static void call( std::vector<hpx::id_type> const & localities
                           , std::string && barrier_name
                           )
           {
                const std::size_t numlocs = localities.size();
                const hpx::id_type here = hpx::find_here();
                const std::size_t rank = detail::get_rank(localities);

                const bool is_power_of_two = (!(numlocs & (numlocs - 1)) && numlocs);

                for(std::size_t i = 1, s = 0, iend = localities.size(); i<iend; i*=2, s++)
                {
                    std::size_t dest = is_power_of_two ?  rank ^ i : (rank+i) % iend;

                    // Prepare own flag for external notification
                    hpx::remote_promise<void> brecv(here);
                    brecv.register_as( barrier_name + "_" + std::to_string(rank) + "_" + std::to_string(s) );

                    // Notify the partner
                    hpx::remote_promise<void> bsend;
                    bsend.connect_to( barrier_name + "_" + std::to_string(dest) + "_" + std::to_string(s) );
                    bsend.set_value();

                    // Wait for the other partner
                    brecv.get_future().get();
                }
           }
        };

        template<>
        struct custom_barrier_sync_impl<detail::pairwise_policy>
        {
           static void call( std::vector<hpx::id_type> const & localities
                           , std::string && barrier_name
                           )
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
                    brecv.register_as( barrier_name + "_" + std::to_string(rank) + "_epilogue" );

                    // Notify the partner
                    hpx::remote_promise<void> bsend;
                    bsend.connect_to( barrier_name + "_" + std::to_string(rank-cutoff) + "_prologue" );
                    bsend.set_value();

                    // Wait for the other partner
                    brecv.get_future().get();
                }
                else
                {
                    const bool has_carry_partner = rank < (numlocs - cutoff);

                    if(has_carry_partner)
                    {
                        hpx::remote_promise<void> carry(here);
                        carry.register_as( barrier_name + "_" + std::to_string(rank) + "_prologue" );
                        carry.get_future().get();
                    }

                    for(std::size_t i = 1, s = 0, iend = cutoff; i<iend; i*=2, s++)
                    {
                        std::size_t dest = rank ^ i;

                        // Prepare own flag for external notification
                        hpx::remote_promise<void> brecv(here);
                        brecv.register_as( barrier_name + "_" + std::to_string(rank) + "_" + std::to_string(s) );

                        // Notify the partner
                        hpx::remote_promise<void> bsend;
                        bsend.connect_to( barrier_name + "_" + std::to_string(dest) + "_" + std::to_string(s) );
                        bsend.set_value();

                        // Wait for the other partner
                        brecv.get_future().get();
                    }

                    if(has_carry_partner)
                    {
                        hpx::remote_promise<void> bsend;
                        bsend.connect_to( barrier_name + "_" + std::to_string(rank+cutoff) + "_epilogue" );
                        bsend.set_value();
                    }
                }
           }
        };
    }

    template<typename Policy>
    void custom_barrier_sync( Policy const &
                            , std::vector<hpx::id_type> const & localities
                            , std::string && barrier_name
                            )
    {
        detail::custom_barrier_sync_impl<Policy>::call(localities, barrier_name + "_hpx_barrier");
    }

    void custom_barrier_sync( std::vector<hpx::id_type> const & localities
                            , std::string && barrier_name
                            )
    {
        detail::custom_barrier_sync_impl<>::call(localities, barrier_name + "_hpx_barrier");
    }


    template<typename Policy>
    hpx::future<void> custom_barrier( Policy const & p
                                    , std::vector<hpx::id_type> const & localities
                                    , std::string && barrier_name
                                    )
    {
        return hpx::async( detail::custom_barrier_sync_impl<Policy>::call
                         , localities
                         , barrier_name + "_hpx_barrier"
                         );
    }

    hpx::future<void> custom_barrier( std::vector<hpx::id_type> const & localities
                                    , std::string && barrier_name
                                    )
    {
        using default_policy = detail::central_policy;
        return hpx::async( detail::custom_barrier_sync_impl<default_policy>::call
                         , localities
                         , barrier_name + "_hpx_barrier"
                         );
    }
}

#endif

