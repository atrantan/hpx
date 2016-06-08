////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2016 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////

#include <hpx/hpx.hpp>
#include <hpx/hpx_main.hpp>
#include <hpx/include/util.hpp>
#include <hpx/util/lightweight_test.hpp>
#include <remote_promise/remote_promise.hpp>

void producer()
{
    hpx::remote_promise<void> my_promise; // Create an empty client
    my_promise.connect_to("promise");
    my_promise.set_value();
}
HPX_DEFINE_PLAIN_ACTION(producer);


void consumer(hpx::id_type dest)
{
    hpx::remote_promise<void> my_promise(dest); // Create a couple server/client
    my_promise.register_as("promise");
    my_promise.get_future().get();
}
HPX_DEFINE_PLAIN_ACTION(consumer);


int main()
{
    producer_action p;
    consumer_action c;

    auto localities = hpx::find_all_localities();

    HPX_ASSERT_MSG( localities.size() >= 2
                  , "This unit test needs to be run with at least 2 localities");


    hpx::id_type orig = localities[0];
    hpx::id_type dest = localities[1];

    hpx::future<void> f1 = hpx::async(p, orig);
    hpx::future<void> f2 = hpx::async(c, dest, dest );

    return 0;
}
