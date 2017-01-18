////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2016 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(SPMD_BLOCK_HPP)
#define SPMD_BLOCK_HPP

#include <hpx/runtime/get_locality_id.hpp>
#include <hpx/runtime/launch_policy.hpp>
#include <hpx/runtime/naming/name.hpp>
#include <hpx/runtime/serialization/serialize.hpp>
#include <hpx/lcos/future.hpp>
#include <hpx/lcos/barrier.hpp>
#include <hpx/lcos/broadcast.hpp>

#include <make_action/make_action.hpp>

#include <memory>

namespace hpx { namespace parallel{

    struct spmd_block
    {
        spmd_block(){}

        spmd_block( std::string name, std::vector<hpx::naming::id_type> const & localities )
        : name_(name), localities_(localities)
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


        void sync_all() const
        {

           if (!barrier_)
           {
               barrier_ = std::make_shared<hpx::lcos::barrier>(
                 name_ + "_barrier"
               , localities_.size()
               );
           }

           barrier_->wait();
        }

        hpx::future<void> sync_all(hpx::launch::async_policy const &) const
        {
           if (!barrier_)
           {
               barrier_ = std::make_shared<hpx::lcos::barrier>(
                 name_+ "_barrier"
               , localities_.size()
               );
           }

           return barrier_->wait(hpx::launch::async);
        }

    private:
        std::string name_;
        std::vector< hpx::naming::id_type > localities_;
        mutable std::shared_ptr<hpx::lcos::barrier> barrier_;

    private:
        friend class hpx::serialization::access;

        template <typename Archive>
        void serialize(Archive& ar, unsigned int const)
        {
            ar & name_ & localities_;
        }
    };

    template <typename F, typename ... Args>
    hpx::future<void> define_spmd_block(std::string && name, std::vector<hpx::naming::id_type> const & localities, F && f, Args && ... args)
    {
        constexpr auto a = hpx::make_action(std::move(f));

        std::vector< hpx::future<void> > join;

        spmd_block block( name, localities);

        for (auto & l : localities)
        {
            if ( l != hpx::find_here() )
            join.push_back( hpx::async(a, l, block, std::forward<Args>(args)...) );
        }

        join.push_back( hpx::async( a, hpx::find_here(), block, std::forward<Args>(args)...) );

        return hpx::when_all(join).then( [](auto join_){ join_.get(); } );
    }


}}


#endif
