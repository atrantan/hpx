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
#include <hpx/util/output_id_splitting.hpp>
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

///////////////////////////////////////////////////////////////////////////////
//
//          Here is how our distributed garbage collection works
//
// Each id_type instance - while always referring to some (possibly remote)
// entity - can either be 'managed' or 'unmanaged'. If an id_type instance is
// 'unmanaged' it does not perform any garbage collection. Otherwise (if it's 
// 'managed'), all of its copies are globally tracked which allows to
// automatically delete the entity a particular id_type instance is referring
// to after the last reference to it goes out of scope.
//
// An id_type instance is essentially a shared_ptr<> maintaining two reference
// counts: a local reference count and a global one. The local reference count
// is incremented whenever the id_type instance is copied locally, and decremented
// whenever one of the local copies goes out of scope. At the point when the last
// local copy goes out of scope, it returns its current share of the global
// reference count back to AGAS. The share of the global reference count owned
// by all copies of an id_type instance on a single locality is called its
// credit. Credits are issued in chunks which allows to create a global copy
// of an id_type instance (like passing it to another locality) without needing
// to talk to AGAS to request a global reference count increment. The referenced
// entity is free'd when the global reference count falls to zero.
//
// Any newly created object assumes an initial credit. This credit is not
// accounted for by AGAS as long as no global increment or decrement requests
// are received. It is important to understand that there is no way to distingish
// whether an object has already been deleted (and therefore no entry exists in
// the table storing the global reference count for this object) or whether the
// object is still alive but no increment/decrement requests have been received
// by AGAS yet. While this is a pure optimization to avoid storing global
// reference counts for all objects, it has implications for the implemented
// garbage collection algorithms at large.
//
// As long as an id_type instance is not sent to another locality (a locality
// different from the initial locality creating the referenced entity), all
// lifetime management for this entity can be handled purely local without
// even talking to AGAS.
//
// Sending an id_type instance to another locality (which includes using an
// id_type as the destination for an action) splits the current credit into
// two parts. One part stays with the id_type on the sending locality, the
// part is sent along to the destination locality where it turns into the
// global credit associated with the remote copy of the id_type. As stated
// above, this allows to avoid talking to AGAS for incrementing the global
// reference count as long as there is sufficient global credit left in order
// to be split.
//
// The current share of the global credit associated with an id_type instance
// is encoded in the bits 80..95 of the underlying gid_type. The most
// significant bit of this bit range (bit 95) encodes whether the given id_type
// has been split at any time. This information is needed to be able to decide
// whether a garbage collection can be assumed to be a purely local operation.
//
// If the global credit is too small to be split, a separate AGAS request is
// issued which increments the global reference count by a certain amount
// which enables more split operations. Please note that the amount of
// credits sent along to the other locality is never larger than the known
// current credit before splitting. IOW, either splitting succeeds (as
// sufficient current credits are available) or the outgoing parcel has to wait
// for AGAS acknowledging the increment operation on the associated global
// reference count.
//
// The goal is to avoid the situation as much as possible where an outgoing
// parcel is delayed because of the necessary AGAS request incrementing the
// global reference count. For this reason the implementation starts an
// asynchronous AGAS increment operation as soon as the current credit drops
// below a given threshold. Once this request to AGAS is acknowledged, the
// credit is replenished. The hope is that the asynchronous operation finishes
// before the id_type runs out of sufficient credits for a split operation.
//
// The second most significant bit of the credit bit range (bit 94) encodes
// whether a asynchronous increment operation is in flight. This avoids more
// than one of those requests concurrently. It is set when the operation is
// started and is reset as soon as the AGAS request is acknowledged.
//
// The various constants used for the described scheme are:
//
//    HPX_GLOBALCREDIT_INITIAL
//    HPX_GLOBALCREDIT_THRESHOLD
//    HPX_GLOBALCREDIT_SEND_ALONG
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
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

            // We add the new credits after the AGAS has acknowledged to have
            // executed the incref.
            if (credit) {
                gid_type::mutex_type::scoped_lock l(&gid);
                detail::add_credit_to_gid(gid, credit, true);
            }

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
        template <typename Archive>
        naming::gid_type id_type_impl::preprocess_gid(Archive& ar) const
        {
            util::detail::output_id_splitting& manage_splitting = ar.get_data();

            gid_type::mutex_type::scoped_lock l(this);

            // If the initial credit is zero the gid is 'unmanaged' and no
            // additional action needs to be performed.
            boost::int16_t credit = detail::get_credit_from_gid(*this);
            if (0 != credit)
            {
                id_type this_id(const_cast<id_type_impl*>(this));
                naming::gid_type newid;

                while (true) {
                    // Request new credits for this id if the remaining credits fall
                    // below the defined threshold
                    if (credit == 2)
                    {
                        // Credits are exhausted, we store the id for the parcelports to
                        // replenish the credits synchronously and delay sending the
                        // parcel until done.
                        //
                        // Note that this should happen only very seldom as we start
                        // replenishing credits very early (see below), which should leave
                        // sufficient time before the credit is actually exhausted.
                        manage_splitting.add_exhausted_id(this_id);
                    }
                    else if (!gid_is_being_replenished(*this) &&
                        detail::get_credit_from_gid(*this) < HPX_GLOBALCREDIT_THRESHOLD)
                    {
                        // set the flag that tells about the ongoing replenish operation
                        set_async_incref_mask_for_gid(const_cast<id_type_impl&>(*this));

                        {
                            util::scoped_unlock<gid_type::mutex_type::scoped_lock> ul(l);

                            // Note: the future returned by retrieve_new_credits()
                            //       keeps this instance alive.
                            manage_splitting.add_incref_request(
                                retrieve_new_credits(const_cast<id_type_impl&>(*this),
                                    HPX_GLOBALCREDIT_THRESHOLD, this_id));
                        }

                        // if in the meantime the credit has dropped below the critical
                        // margin we have to retry
                        credit = detail::get_credit_from_gid(*this);
                        if (credit == 2)
                            continue;
                    }

                    // now we split the credit
                    newid = detail::split_credits_for_gid(
                        const_cast<id_type_impl&>(*this), divides_credit(2));
                    break;
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
                // set the flag that tells about the ongoing replenish operation
                set_async_incref_mask_for_gid(*this);

                // We unlock the lock as all operations on the local credit
                // have been performed and we don't want the lock to be
                // pending during the (possibly remote) AGAS operation.
                l.unlock();

                // note: the future returned by retrieve_new_credits()
                //       keeps this instance alive as it is passed along
                //       as the keep_alive parameter
                retrieve_new_credits(*this, HPX_GLOBALCREDIT_INITIAL,
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
                naming::gid_type split_id(preprocess_gid(ar));
                ar << split_id << type_;
            }
            else {
                gid_serialization_data data;
                data.gid_ = preprocess_gid(ar);
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

