////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2016 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(SPMD_BLOCK_HPP)
#define SPMD_BLOCK_HPP

#include <hpx/runtime/get_locality_id.hpp>
#include <hpx/runtime/naming/name.hpp>
#include <hpx/runtime/serialization/serialize.hpp>
#include <hpx/lcos/future.hpp>
#include <hpx/lcos/broadcast.hpp>

#include <barrier_algorithms/barrier_algorithms.hpp>
#include <make_action/make_action.hpp>

namespace hpx { namespace parallel{

    struct spmd_block
    {
        spmd_block(){}

        spmd_block( std::vector<hpx::naming::id_type> const & localities )
        : localities_(localities)
        {}

        std::vector<hpx::naming::id_type> find_all_localities() const
        {
            return localities_;
        }

        std::size_t get_num_images() const
        {
            return localities_.size();
        }

        std::size_t this_image() const
        {
            using const_iterator = typename std::vector<hpx::naming::id_type>::const_iterator;

            std::size_t rank = 0;
            const_iterator l_it = localities_.cbegin();
            const_iterator l_end = localities_.cend();
            hpx::naming::id_type here = hpx::find_here();

            while( *l_it != here && l_it != l_end ) { l_it++; rank++;}
            return rank;
        }

        template<typename Policy>
        void barrier_sync(Policy const & p, std::string && bname ) const
        {
            hpx::custom_barrier_sync(p, localities_, std::move(bname) );
        }

        template<typename Policy>
        hpx::future<void> barrier(Policy const & p, std::string && bname ) const
        {
            return hpx::custom_barrier( p, localities_, std::move(bname) );
        }


        void barrier_sync( std::string && bname ) const
        {
            hpx::custom_barrier_sync( localities_, std::move(bname) );
        }

        hpx::future<void> barrier( std::string && bname ) const
        {
            return hpx::custom_barrier( localities_, std::move(bname) );
        }

    private:
        std::vector< hpx::naming::id_type > localities_;

    private:
        friend class hpx::serialization::access;

        template <typename Archive>
        void serialize(Archive& ar, unsigned)
        {
            ar & localities_;
        }
    };

    template <typename F, typename ... Args>
    hpx::future<void> define_spmd_block(std::vector<hpx::naming::id_type> const & localities, F && f, Args && ... args)
    {
        constexpr auto a = hpx::make_action(std::move(f));

        spmd_block block(localities);

        return hpx::lcos::broadcast<decltype(a)>(localities, block, std::forward<Args>(args)...);
    }


}}


#endif
