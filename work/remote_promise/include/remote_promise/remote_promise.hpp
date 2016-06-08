////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2016 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(REMOTE_PROMISE_HPP)
#define REMOTE_PROMISE_HPP

#include <hpx/hpx_fwd.hpp>
#include <hpx/lcos/future.hpp>
#include <boost/exception_ptr.hpp>

#include <hpx/runtime/serialization/serialize.hpp>
#include <hpx/include/components.hpp>


namespace hpx { namespace server
{
    template <typename T>
    struct remote_promise
    : hpx::components::component_base< remote_promise<T> >
    {
    private:
        hpx::promise<T> p_;

    public:

        explicit remote_promise(){}

        ~remote_promise()
        {}

        inline hpx::future<T> get_future()
        {
            return p_.get_future();
        }

        T get_value()
        {
            return p_.get_future().get();
        }

        void set_value(T && val)
        {
            p_.set_value( std::move(val) );
        }

        void set_exception(boost::exception_ptr p)
        {
            p_.set_exception(p);
        }

        /// Macros to define HPX component actions for all exported functions.
        HPX_DEFINE_COMPONENT_DIRECT_ACTION(remote_promise, get_value);
        HPX_DEFINE_COMPONENT_DIRECT_ACTION(remote_promise, set_value);
        HPX_DEFINE_COMPONENT_DIRECT_ACTION(remote_promise, set_exception);
    };

    template <>
    struct remote_promise<void>
    : hpx::components::component_base< remote_promise<void> >
    {
    private:
        hpx::promise<void> p_;

    public:

        explicit remote_promise(){}

        ~remote_promise()
        {}

        inline hpx::future<void> get_future()
        {
            return p_.get_future();
        }

        void get_value()
        {
            p_.get_future().get();
        }

        void set_value()
        {
            p_.set_value();
        }

        void set_exception(boost::exception_ptr p)
        {
            p_.set_exception(p);
        }

        /// Macros to define HPX component actions for all exported functions.
        HPX_DEFINE_COMPONENT_DIRECT_ACTION(remote_promise, get_value);
        HPX_DEFINE_COMPONENT_DIRECT_ACTION(remote_promise, set_value);
        HPX_DEFINE_COMPONENT_DIRECT_ACTION(remote_promise, set_exception);
    };
}}

#define HPX_REGISTER_REMOTE_PROMISE(...)                                      \
    HPX_REGISTER_PROMISE_(__VA_ARGS__)                                        \
/**/
#define HPX_REGISTER_PROMISE_(...)                                            \
    HPX_UTIL_EXPAND_(BOOST_PP_CAT(                                            \
        HPX_REGISTER_PROMISE_, HPX_UTIL_PP_NARG(__VA_ARGS__)                  \
    )(__VA_ARGS__))                                                           \
/**/

#define HPX_REGISTER_PROMISE_1(type)                                          \
    HPX_REGISTER_PROMISE_2(type, type)                                        \
/**/
#define HPX_REGISTER_PROMISE_2(type, name)                                    \
    HPX_REGISTER_ACTION(                                                      \
        ::hpx::server::remote_promise<type>::get_value_action,                \
        BOOST_PP_CAT(__promise_get_value_action_, name));                     \
    HPX_REGISTER_ACTION(                                                      \
        ::hpx::server::remote_promise<type>::set_value_action,                \
        BOOST_PP_CAT(__promise_set_value_action_, name));                     \
    HPX_REGISTER_ACTION(                                                      \
        ::hpx::server::remote_promise<type>::set_exception_action,            \
        BOOST_PP_CAT(__promise_set_exception_action_, name));                 \
    typedef ::hpx::components::simple_component<                              \
        ::hpx::server::remote_promise<type>                                   \
    > BOOST_PP_CAT(__promise_, name);                                         \
    HPX_REGISTER_COMPONENT(BOOST_PP_CAT(__promise_, name))                    \

namespace hpx { namespace detail{

    template<typename T>
    struct remote_promise
    : public components::client_base< remote_promise<T>, server::remote_promise<T> >
    {
    private:
        typedef hpx::server::remote_promise<T> server_type;

        typedef hpx::components::client_base<
                remote_promise, server::remote_promise<T>
            > base_type;
    public:
        remote_promise(){}

        remote_promise( hpx::id_type const & where )
        : base_type( hpx::new_<server_type>(where) )
        {}

        // Return the pinned pointer to the underlying component
        boost::shared_ptr< server::remote_promise<T> > get_ptr() const
        {
            error_code ec(lightweight);
            return hpx::get_ptr< server::remote_promise<T> >( this->get_id() ).get(ec);
        }
    };
}}

namespace hpx {

    template<typename T>
    struct remote_promise
    {
        using client_type = hpx::detail::remote_promise<T>;
        using server_type = hpx::server::remote_promise<T>;

        explicit remote_promise()
        : is_promise_here_( false )
        {
            ptr_ = boost::make_shared<client_type>();
        }

        explicit remote_promise(hpx::id_type const & where )
        : is_promise_here_( hpx::find_here() == where )
        {
            ptr_ = boost::make_shared<client_type>(where);
        }

        hpx::future<T> get_future()
        {
            if( is_promise_here_ )
            {
                return ptr_->get_ptr()->get_future();
            }
            else
            {
                HPX_ASSERT( ptr_->get_id() );
                return hpx::async<typename server_type::get_value_action>
                (  ptr_->get_id() );
            }
        }

        void set_value(T && val)
        {
            typename server_type::set_value_action act;

            if( is_promise_here_ )
                ptr_->get_ptr()->set_value( std::move(val) );
            else
            {
                HPX_ASSERT( ptr_->get_id() );
                hpx::apply(act, ptr_->get_id(), std::move(val));
            }
        }

        void set_exception(boost::exception_ptr p)
        {
            typename server_type::set_exception_action act;

            if( is_promise_here_ )
                return ptr_->get_ptr()->set_exception(p);
            else
            {
                HPX_ASSERT(ptr_->get_id());
                hpx::apply(act, ptr_->get_id(), p);
            }
        }

        inline void register_as(std::string && name)
        {
            ptr_->register_as( std::move(name) );
        }

        inline void connect_to(std::string && name)
        {
            ptr_->connect_to( std::move(name) );
        }

    private:
        bool is_promise_here_;
        boost::shared_ptr< hpx::detail::remote_promise<T> > ptr_;
    };

    template<>
    struct remote_promise<void>
    {
        using client_type = hpx::detail::remote_promise<void>;
        using server_type = hpx::server::remote_promise<void>;

        explicit remote_promise()
        : is_promise_here_( false )
        {
            ptr_ = boost::make_shared<client_type>();
        }

        explicit remote_promise(hpx::id_type const & where )
        : is_promise_here_( hpx::find_here() == where )
        {
            ptr_ = boost::make_shared<client_type>(where);
        }

        hpx::future<void> get_future()
        {
            if( is_promise_here_ )
            {
                return ptr_->get_ptr()->get_future();
            }
            else
            {
                HPX_ASSERT( ptr_->get_id() );
                return hpx::async<typename server_type::get_value_action>
                (  ptr_->get_id() );
            }
        }

        void set_value()
        {
            typename server_type::set_value_action act;

            if( is_promise_here_ )
                ptr_->get_ptr()->set_value();
            else
            {
                HPX_ASSERT( ptr_->get_id() );
                hpx::apply(act, ptr_->get_id());
            }
        }

        void set_exception(boost::exception_ptr p)
        {
            typename server_type::set_exception_action act;

            if( is_promise_here_ )
                return ptr_->get_ptr()->set_exception(p);
            else
            {
                HPX_ASSERT(ptr_->get_id());
                hpx::apply(act, ptr_->get_id(), p );
            }
        }

        inline void register_as(std::string && name)
        {
            ptr_->register_as( std::move(name) );
        }

        inline void connect_to(std::string && name)
        {
            ptr_->connect_to( std::move(name) );
        }

    private:
        bool is_promise_here_;
        boost::shared_ptr< hpx::detail::remote_promise<void> > ptr_;
    };
}

#endif
