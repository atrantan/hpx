
//  Copyright (c) 2016 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <remote_promise/remote_promise.hpp>
#include <hpx/include/components.hpp>

HPX_REGISTER_COMPONENT_MODULE();

// Register remote_promise<void> by default
HPX_REGISTER_REMOTE_PROMISE(void);

// Register remote_promise<int>
HPX_REGISTER_REMOTE_PROMISE(int);
