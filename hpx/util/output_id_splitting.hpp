//  Copyright (c) 2007-2013 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(HPX_UTIL_OUTPUT_ID_SPLITTING_NOV_22_2013_0422PM)
#define HPX_UTIL_OUTPUT_ID_SPLITTING_NOV_22_2013_0422PM

#include <hpx/lcos/future.hpp>
#include <hpx/runtime/naming/name.hpp>

#include <vector>
#include <map>

namespace hpx { namespace util { namespace detail
{
    class output_id_splitting
    {
    public:
        bool process() const;

        void add_incref_request(naming::id_type const& id,
            hpx::future<bool> const& f);

        void add_exhausted_id(naming::id_type const& id)
        {
            exhausted_ids_.push_back(id);
        }

        void clear()
        {
            pending_increfs_.clear();
            exhausted_ids_.clear();
        }

    private:
        std::map<hpx::naming::gid_type, hpx::future<bool> > pending_increfs_;
        std::vector<naming::id_type> exhausted_ids_;
    };
}}}

#endif
