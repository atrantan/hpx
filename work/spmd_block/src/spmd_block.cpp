
//  Copyright (c) 2016 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <spmd_block/spmd_block.hpp>
#include <hpx/include/components.hpp>

HPX_REGISTER_COMPONENT_MODULE();
HPX_REGISTER_COMPONENT(hpx::components::component< hpx::server::spmd_block >, spmd_block);
