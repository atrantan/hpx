////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2016 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(SPMD_BLOCK_HPP)
#define SPMD_BLOCK_HPP

#include <hpx/hpx_fwd.hpp>
#include <hpx/lcos/future.hpp>
#include <hpx/lcos/when_all.hpp>
#include <hpx/include/lcos.hpp>
#include <hpx/async.hpp>
#include <hpx/lcos/local/spinlock.hpp>

#include <hpx/runtime/serialization/serialize.hpp>
#include <hpx/include/components.hpp>

#include <boost/thread/locks.hpp>


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
        {
            wait();
        }

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

        template <typename Action, typename Client, typename ... Args>
        void run(Action && a, Client const & client, Args && ... args)
        {
            using localities_type = std::vector< hpx::id_type >;
            using const_iterator = typename localities_type::const_iterator;

            std::vector< hpx::future<void> > join;
            const_iterator l_it  = localities_.cbegin() + 1;
            const_iterator l_end = localities_.cend();

            for( ; l_it != l_end ; l_it++ )
            {
                join.push_back( hpx::async( std::forward<Action>(a), *l_it, Client(client), std::forward<Args>(args)... ) );
            }
            join.push_back( hpx::async( std::forward<Action>(a), localities_[0], Client(client), std::forward<Args>(args)... ) );

            {
                boost::lock_guard<mutex_type> l(mtx_);
                spmd_tasks_.push_back( hpx::when_all( join ).then( []( when_all_result f){ return f.get(); } ) );
            }
        }

        std::vector<hpx::id_type> find_all_localities() const
        {
            return localities_;
        }

        void wait()
        {
            when().get();
        }

        HPX_DEFINE_COMPONENT_DIRECT_ACTION(spmd_block, find_all_localities);
        HPX_DEFINE_COMPONENT_DIRECT_ACTION(spmd_block, wait);

    private:
        std::vector< hpx::future<void> > spmd_tasks_;
        std::vector< hpx::id_type > localities_;
        mutable mutex_type mtx_;
    };
}}

HPX_REGISTER_ACTION_DECLARATION(
      hpx::server::spmd_block::find_all_localities_action
    , spmd_block_find_all_localities_action
    );
HPX_REGISTER_ACTION_DECLARATION(
      hpx::server::spmd_block::wait_action
    , spmd_block_wait_action
    );

namespace hpx { namespace parallel{

    class spmd_block
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

        std::vector<hpx::id_type> find_all_localities_sync() const
        {
            return find_all_localities().get();
        }

        hpx::future< std::vector<hpx::id_type> > find_all_localities() const
        {
            HPX_ASSERT(this->get_id());
            return hpx::async<typename server_type::find_all_localities_action>(
                this->get_id()
                );
        }

        // Action submitted to a spmd_block must have one and only one parameter
        // of type spmd_block
        template <typename Action, typename ... Args>
        void run(Action && a, Args && ... args)
        {
            if( hpx::find_here() == find_all_localities_sync()[0] )
            {
                get_ptr()->run( std::forward<Action>(a), *this, std::forward<Args>(args)... );
            }
        }

        // return future representing the execution of all tasks
        hpx::future<void> when()
        {
            if( hpx::find_here() == find_all_localities_sync()[0] )
            {
                return get_ptr()->when();
            }

            return hpx::make_ready_future();
        }

        void barrier_sync( std::string && bname )
        {
            barrier( std::move(bname) ).get();
        }


        hpx::future<void> barrier( std::string && bname )
        {
            std::vector<hpx::id_type> localities = find_all_localities_sync();
            std::size_t numlocs = localities.size();
            hpx::id_type here = hpx::find_here();

            std::string barrier_name = bname + "_hpx_spmd_barrier";

            if( here == localities[0] )
            {
                // create the barrier, register it with AGAS
                hpx::lcos::barrier b = hpx::new_<hpx::lcos::barrier>(here,numlocs);

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

        void wait_sync() const
        {
            return wait().get();
        }

        hpx::future<void> wait() const
        {
            HPX_ASSERT(this->get_id());
            return hpx::async< typename server_type::wait_action >( this->get_id() );
        }
    };

    template <typename Action, typename ... Args>
    hpx::future<void> define_spmd_block(std::vector<hpx::id_type> const & localities, Action && a, Args && ... args)
    {
        spmd_block block = hpx::new_<spmd_block>( localities[0], localities );

        block.run( std::forward<Action>(a), std::forward<Args>(args)... );

        return block.when();
    }


}}


#endif
