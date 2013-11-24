//  Copyright (c) 2007-2013 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/util/output_id_splitting.hpp>

namespace hpx { namespace util { namespace detail
{
    void output_id_splitting::add_incref_request(naming::id_type const& id,
        hpx::future<bool> const& f)
    {
        naming::gid_type gid = id.get_gid();
        naming::detail::strip_credit_from_gid(gid);

        pending_increfs_.insert(std::make_pair(gid, f));
    }

    // This function finds out whether at least one of the ids which has been
    // split requires its credit to be replenished.
    bool output_id_splitting::process() const
    {
        // go ahead sending the parcel if there are no exhausted ids
        if (exhausted_ids_.empty())
            return true;

        BOOST_FOREACH(naming::id_type const& id, exhausted_ids_)
        {
            // if the exhausted id has an incref pending
            naming::gid_type gid = id.get_gid();
            naming::detail::strip_credit_from_gid(gid);

            if (pending_increfs_.find(gid) != pending_increfs_.end())
            {
            }
        }

        return false;
    }
}}}
