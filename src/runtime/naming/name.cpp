//  Copyright (c) 2007-2013 Hartmut Kaiser
//  Copyright (c) 2011      Bryce Lelbach
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx_fwd.hpp>
#include <hpx/runtime/naming/name.hpp>
#include <hpx/exception.hpp>
#include <hpx/state.hpp>
#include <hpx/util/portable_binary_iarchive.hpp>
#include <hpx/util/portable_binary_oarchive.hpp>
#include <hpx/util/base_object.hpp>
#include <hpx/runtime/applier/applier.hpp>
#include <hpx/runtime/components/stubs/runtime_support.hpp>
#include <hpx/runtime/actions/continuation.hpp>
#include <hpx/runtime/agas/addressing_service.hpp>
#include <hpx/runtime/agas/interface.hpp>
#include <hpx/lcos/future.hpp>
#include <hpx/lcos/local/packaged_continuation.hpp>

#include <boost/serialization/version.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/is_bitwise_serializable.hpp>
#include <boost/serialization/array.hpp>

#include <boost/mpl/bool.hpp>

namespace hpx { namespace naming { namespace detail
{
    struct gid_serialization_data;
}}}

namespace boost { namespace serialization
{
    template <>
    struct is_bitwise_serializable<
            hpx::naming::detail::gid_serialization_data>
       : boost::mpl::true_
    {};
}}

namespace hpx { namespace naming
{
    namespace detail
    {
        void decrement_refcnt(detail::id_type_impl* p)
        {
            // Talk to AGAS only if this gid was split at some time in the past,
            // i.e. if a reference actually left the original locality.
            // Alternatively we need to go this way if the id has never been
            // resolved, which means we don't know anything about the component
            // type.
            naming::address addr;

            if (gid_was_split(*p) ||
                !naming::get_agas_client().resolve_cached(*p, addr))
            {
                // guard for wait_abort and other shutdown issues
                try {
                    // decrement global reference count for the given gid,
                    boost::int32_t credits = detail::get_credit_from_gid(*p);
                    BOOST_ASSERT(0 != credits);

                    if (get_runtime_ptr())
                    {
                        // Fire-and-forget semantics.
                        error_code ec(lightweight);
                        agas::decref(*p, credits, ec);
                    }
                }
                catch (hpx::exception const& e) {
                    if (e.get_error() != thread_interrupted) {
                        LTM_(error)
                            << "Unhandled exception while executing decrement_refcnt:"
                            << e.what();
                    }
                }
            }
            else {
                // If the gid was not split at any point in time we can assume
                // that the referenced object is fully local.
                components::component_type t = addr.type_;

                BOOST_ASSERT(t != components::component_invalid);

                // Third parameter is the count of how many components to destroy.
                // FIXME: The address should still be in the cache, but it could
                // be evicted. It would be nice to have a way to pass the address
                // directly to free_component_sync.
                try {
                    using components::stubs::runtime_support;
                    runtime_support::free_component_sync(t, *p, 1);
                }
                catch (hpx::exception const& e) {
                    // This request might come in too late and the thread manager
                    // was already stopped. We ignore the request if that's the
                    // case.
                    if (e.get_error() != invalid_status) {
                        throw;      // rethrow if not invalid_status
                    }
                    else if (!threads::threadmanager_is(hpx::stopping)) {
                        throw;      // rethrow if not stopping
                    }
                }
            }
            delete p;   // delete local gid representation in any case
        }

        // custom deleter for managed gid_types, will be called when the last
        // copy of the corresponding naming::id_type goes out of scope
        void gid_managed_deleter (id_type_impl* p)
        {
            // a credit of zero means the component is not (globally) reference
            // counted
            boost::int32_t credits = detail::get_credit_from_gid(*p);
            if (0 != credits)
            {
                // We take over the ownership of the gid_type object here
                // as the shared_ptr is assuming it has been properly deleted
                // already. The actual deletion happens in the decrement_refcnt
                // once it is executed.
                //error_code ec;
                //applier::register_work(boost::bind(decrement_refcnt, p),
                //    "decrement global gid reference count",
                //    threads::thread_state(threads::pending),
                //    threads::thread_priority_normal, std::size_t(-1), ec);
                //if (ec)
                //{
                    // if we are not able to spawn a new thread, we need to execute
                    // the deleter directly
                    decrement_refcnt(p);
                //}
            }
            else {
                delete p;   // delete local gid representation if needed
            }
        }

        // custom deleter for unmanaged gid_types, will be called when the last
        // copy of the corresponding naming::id_type goes out of scope
        void gid_unmanaged_deleter (id_type_impl* p)
        {
            delete p;   // delete local gid representation only
        }

        ///////////////////////////////////////////////////////////////////////
        id_type_impl::deleter_type id_type_impl::get_deleter(id_type_management t)
        {
            switch (t) {
            case unmanaged:
                return &detail::gid_unmanaged_deleter;
            case managed:
                return &detail::gid_managed_deleter;
            default:
                BOOST_ASSERT(false);          // invalid management type
                return &detail::gid_unmanaged_deleter;
            }
            return 0;
        }

        ///////////////////////////////////////////////////////////////////////
        static bool synchronize_with_async_incref(hpx::future<bool>& f,
            boost::uint16_t credit, naming::id_type const& /*keep_alive*/,
            naming::gid_type& gid)
        {
            // this rethrows if the AGAS operation went wrong.
            bool result = f.get();

            gid_type::mutex_type::scoped_lock l(&gid);

            // We add the new credits after the AGAS has acknowledged to have
            // executed the incref.
            if (credit)
                detail::add_credit_to_gid(gid, credit);

            return result;
        }

        hpx::future<bool> retrieve_new_credits(naming::gid_type& id,
            boost::uint16_t credit, id_type const& keep_alive)
        {
            // Perform the incref asynchronously, but add the new credits only
            // after AGAS has acknowledged the request.
            using util::placeholders::_1;
            return agas::incref_async(id, id, credit).then(
                util::bind(synchronize_with_async_incref, _1, credit,
                    keep_alive, boost::ref(id)));
        }

        ///////////////////////////////////////////////////////////////////////
        // prepare the given id, note: this function modifies the passed id
        naming::gid_type id_type_impl::preprocess_gid() const
        {
            gid_type::mutex_type::scoped_lock l(this);

            // If the initial credit is zero the gid is 'unmanaged' and no
            // additional action needs to be performed.
            boost::int16_t oldcredits = detail::get_credit_from_gid(*this);
            if (0 != oldcredits)
            {
                id_type this_id(const_cast<id_type_impl*>(this));
                naming::gid_type newid;

                while (true) {
                    // Request new credits for this id if the remaining credits fall
                    // below the defined threshold
                    if (detail::get_credit_from_gid(*this) <= HPX_GLOBALCREDIT_SEND_ALONG)
                    {
                        // Credits are exhausted, we throw an exception to unravel
                        // serialization. The parcelports will catch this exception,
                        // replenish the credits synchronously and retry sending the
                        // parcel.
                        //
                        // Note that this should happen only very seldom as we start
                        // replenishing credits very early (see below), which should leave
                        // sufficient time before the credit is actually exhausted.
                        throw exhausted_credit(this_id);
                    }
                    else if (detail::get_credit_from_gid(*this) < HPX_GLOBALCREDIT_THRESHOLD)
                    {
                        util::scoped_unlock<gid_type::mutex_type::scoped_lock> ul(l);

                        // Note: the future returned by retrieve_new_credits()
                        //       keeps this instance alive.
                        retrieve_new_credits(const_cast<id_type_impl&>(*this),
                            HPX_GLOBALCREDIT_THRESHOLD, this_id);
                    }

                    // if in the meantime the credit has dropped below the critical margin
                    // we have to retry
                    if (detail::get_credit_from_gid(*this) > HPX_GLOBALCREDIT_SEND_ALONG)
                    {
                        // now we split the credit
                        newid = detail::split_credits_for_gid(
                            const_cast<id_type_impl&>(*this),
                            subtracts_credit(HPX_GLOBALCREDIT_SEND_ALONG));
                        break;
                    }
                }

                // none of the ids should be left without credits
                BOOST_ASSERT(detail::get_credit_from_gid(*this) != 0);
                BOOST_ASSERT(detail::get_credit_from_gid(newid) != 0);

                return newid;
            }

            BOOST_ASSERT(unmanaged == type_);
            return *this;
        }

        // prepare the given id, note: this function modifies the passed id
        void id_type_impl::postprocess_gid()
        {
            gid_type::mutex_type::scoped_lock l(this);

            // If the initial credit after deserialization is 1 we need to
            // add more global credits.
            boost::int16_t credits = detail::get_credit_from_gid(*this);
            if (1 == credits)
            {
                // We unlock the lock as all operations on the local credit
                // have been performed and we don't want the lock to be
                // pending during the (possibly remote) AGAS operation.
                l.unlock();

                // note: the future returned by retrieve_new_credits()
                //       keeps this instance alive as it is passed along
                //       as the keep_alive parameter
                retrieve_new_credits(*this, HPX_INITIAL_GLOBALCREDIT,
                    id_type(this));
            }
        }

        struct gid_serialization_data
        {
            gid_type gid_;
            boost::uint16_t type_;
        };

        // serialization
        template <typename Archive>
        void id_type_impl::save(Archive& ar) const
        {
            if(ar.flags() & util::disable_array_optimization) {
                naming::gid_type split_id(preprocess_gid());
                ar << split_id << type_;
            }
            else {
                gid_serialization_data data;
                data.gid_ = preprocess_gid();
                data.type_ = type_;

                ar.save(data);
            }
        }

        template <typename Archive>
        void id_type_impl::load(Archive& ar)
        {
            if(ar.flags() & util::disable_array_optimization) {
                // serialize base class and management type
                ar >> static_cast<gid_type&>(*this);
                ar >> type_;
            }
            else {
                gid_serialization_data data;
                ar.load(data);

                static_cast<gid_type&>(*this) = data.gid_;
                type_ = static_cast<id_type_management>(data.type_);
            }

            if (detail::unmanaged != type_ && detail::managed != type_) {
                HPX_THROW_EXCEPTION(version_too_new, "id_type::load",
                    "trying to load id_type with unknown deleter");
            }

            // make sure the credits get properly updated on receival
            postprocess_gid();
        }

        // explicit instantiation for the correct archive types
        template HPX_EXPORT void id_type_impl::save(
            util::portable_binary_oarchive&) const;

        template HPX_EXPORT void id_type_impl::load(
            util::portable_binary_iarchive&);

        /// support functions for boost::intrusive_ptr
        void intrusive_ptr_add_ref(id_type_impl* p)
        {
            ++p->count_;
        }

        void intrusive_ptr_release(id_type_impl* p)
        {
            if (0 == --p->count_)
                id_type_impl::get_deleter(p->get_management_type())(p);
        }
    }   // detail

    ///////////////////////////////////////////////////////////////////////////
    template <class Archive>
    void id_type::save(Archive& ar, const unsigned int version) const
    {
        bool isvalid = gid_ != 0;
        ar.save(isvalid);
        if (isvalid)
            gid_->save(ar);
    }

    template <class Archive>
    void id_type::load(Archive& ar, const unsigned int version)
    {
        if (version > HPX_IDTYPE_VERSION) {
            HPX_THROW_EXCEPTION(version_too_new, "id_type::load",
                "trying to load id_type of unknown version");
        }

        bool isvalid;
        ar.load(isvalid);
        if (isvalid) {
            boost::intrusive_ptr<detail::id_type_impl> gid(
                new detail::id_type_impl);
            gid->load(ar);
            std::swap(gid_, gid);
        }
    }

    // explicit instantiation for the correct archive types
    template HPX_EXPORT void id_type::save(
        util::portable_binary_oarchive&, const unsigned int version) const;

    template HPX_EXPORT void id_type::load(
        util::portable_binary_iarchive&, const unsigned int version);

    ///////////////////////////////////////////////////////////////////////////
    char const* const management_type_names[] =
    {
        "unknown_deleter",    // -1
        "unmanaged",          // 0
        "managed"             // 1
    };

    char const* get_management_type_name(id_type::management_type m)
    {
        if (m < id_type::unknown_deleter || m > id_type::managed)
            return "invalid";
        return management_type_names[m + 1];
    }

    ///////////////////////////////////////////////////////////////////////////
    inline naming::id_type get_colocation_id_sync(naming::id_type const& id, error_code& ec)
    {
        // FIXME: Resolve the locality instead of deducing it from the target 
        //        GID, otherwise this will break once we start moving objects.
        boost::uint32_t locality_id = get_locality_id_from_gid(id.get_gid());
        return get_id_from_locality_id(locality_id);
    }

    inline lcos::future<naming::id_type> get_colocation_id(naming::id_type const& id)
    {
        return lcos::make_ready_future(naming::get_colocation_id_sync(id, throws));
    }
}}

namespace hpx
{
    naming::id_type get_colocation_id_sync(naming::id_type const& id, error_code& ec)
    {
        return naming::get_colocation_id(id).get(ec);
    }

    lcos::future<naming::id_type> get_colocation_id(naming::id_type const& id)
    {
        return naming::get_colocation_id(id);
    }
}

