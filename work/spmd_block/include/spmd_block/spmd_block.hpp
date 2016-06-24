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
#include <hpx/async.hpp>
#include <hpx/lcos/future.hpp>
#include <hpx/lcos/when_all.hpp>
#include <hpx/lcos/local/spinlock.hpp>
#include <hpx/lcos/broadcast.hpp>
#include <hpx/include/components.hpp>

#include <boost/thread/locks.hpp>

#include <barrier_algorithms/barrier_algorithms.hpp>
#include <make_action/make_action.hpp>


namespace hpx { namespace server
{
    struct spmd_block
    : hpx::components::component_base< spmd_block >
    {
    private:
         using when_all_result = hpx::future< std::vector< hpx::future<void> > >;
         using mutex_type = hpx::lcos::local::spinlock;

    public:

        explicit spmd_block(){}

        explicit spmd_block(std::vector<hpx::id_type> localities)
        : localities_( localities )
        {}

        ~spmd_block()
        {}

        // return future representing the execution of all tasks
        hpx::future<void> when()
        {
            std::vector< hpx::future<void> > spmd_tasks;
            {
                boost::lock_guard<mutex_type> l(mtx_);
                std::swap(spmd_tasks_, spmd_tasks);
            }

            if( ! spmd_tasks.empty() )
                return hpx::when_all( spmd_tasks ).then( []( when_all_result f){ return f.get(); } );

            return hpx::make_ready_future();
        }

        // Action submitted to a spmd_block must have at least one parameter
        // and the first parameter must be a spmd_block

        template <typename F, typename Client, typename ... Args>
        void run(F && f, Client const & client, Args && ... args)
        {
            constexpr auto a = hpx::make_action(std::move(f));

            {
                boost::lock_guard<mutex_type> l(mtx_);
                spmd_tasks_.push_back( hpx::lcos::broadcast<decltype(a)>(localities_, client, std::forward<Args>(args)...) );
            }
        }

    private:
        std::vector< hpx::future<void> > spmd_tasks_;
        std::vector< hpx::id_type > localities_;
        mutable mutex_type mtx_;
    };
}}

namespace hpx { namespace detail{

    struct spmd_block
    : public components::client_base< spmd_block, server::spmd_block >
    {
    private:
        typedef hpx::server::spmd_block server_type;

        typedef hpx::components::client_base<
                spmd_block, server::spmd_block
            > base_type;
    public:
        spmd_block(){}

        spmd_block(id_type const& gid)
          : base_type(gid)
        {}

        spmd_block(hpx::shared_future<id_type> const& gid)
          : base_type(gid)
        {}

        // Return the pinned pointer to the underlying component
        boost::shared_ptr<server::spmd_block > get_ptr() const
        {
            error_code ec(lightweight);
            return hpx::get_ptr< server::spmd_block >( this->get_id() ).get(ec);
        }
    };
}}

namespace hpx { namespace parallel{

    struct spmd_block : public hpx::detail::spmd_block
    {
        using base_type = hpx::detail::spmd_block;

        spmd_block(){}

        spmd_block( std::vector<hpx::id_type> const & localities )
        : localities_(localities), base_type( hpx::new_<base_type>( localities[0], localities ) )
        {}

        std::vector<hpx::id_type> find_all_localities() const
        {
            return localities_;
        }

        std::size_t get_num_images() const
        {
            return localities_.size();
        }

        std::size_t this_image() const
        {
            using const_iterator = typename std::vector<hpx::id_type>::const_iterator;

            std::size_t rank = 0;
            const_iterator l_it = localities_.cbegin();
            const_iterator l_end = localities_.cend();
            hpx::id_type here = hpx::find_here();

            while( *l_it != here && l_it != l_end ) { l_it++; rank++;}
            return rank;
        }

        // Lambda submitted to a spmd_block must have at least one parameter
        // and the first parameter must be a spmd_block
        template <typename F, typename ... Args>
        void run(F && f, Args && ... args)
        {
            if( hpx::find_here() == localities_[0] )
            {
                get_ptr()->run( std::move(f), *this, std::forward<Args>(args)... );
            }
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

        hpx::future<void> wait(std::string && bname) const
        {
            spmd_block const & block (*this);

            return when().then(
                [bname,block](hpx::future<void> f)
                {
                    f.get();
                    block.barrier( std::string(bname) );
                }
            );
        }

        hpx::future<void> when() const
        {
            if( hpx::find_here() == localities_[0] )
            {
                return get_ptr()->when();
            }

            return hpx::make_ready_future();
        }

    private:
        std::vector< hpx::id_type > localities_;

    private:
        friend class hpx::serialization::access;

        template <typename Archive>
        void serialize(Archive& ar, unsigned)
        {
            ar & localities_;
        }
    };

    template <typename F, typename ... Args>
    hpx::future<void> define_spmd_block(std::vector<hpx::id_type> const & localities, F && f, Args && ... args)
    {
        spmd_block block(localities);

        block.run( std::move(f), std::forward<Args>(args)... );

        return block.when();
    }


}}


#endif
