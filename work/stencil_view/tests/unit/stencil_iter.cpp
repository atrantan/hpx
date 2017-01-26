////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2016 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////

#include <hpx/hpx.hpp>
#include <hpx/hpx_main.hpp>
#include <hpx/util/lightweight_test.hpp>

#include <stencil_view/stencil_view.hpp>

void stencil_test()
{
    using const_iterator = typename std::vector<double>::const_iterator;

    std::vector<double> my_vector
        = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,
        26,27,28,29,30,31,32};

    auto begin = my_vector.begin();
    auto end   = my_vector.end();

    {
        hpx::stencil_view<double,3,std::vector<double>> my_view({4,4,2});

        auto up    = my_view.get_boundary(begin,end, 0, 0, 1);
        auto down  = my_view.get_boundary(begin,end, 0, 0,-1);
        auto left  = my_view.get_boundary(begin,end, 0,-1, 0);
        auto right = my_view.get_boundary(begin,end, 0, 1, 0);

        std::cout<<"Up:"<<std::endl;
        for (auto & value : up)
        {
            std::cout<<value<<"  ";
        }
        std::cout<<std::endl;


        std::cout<<"Down:"<<std::endl;
        for (auto & value : down)
        {
            std::cout<<value<<"  ";
        }
        std::cout<<std::endl;

        std::cout<<"Left:"<<std::endl;
        for (auto & value : left)
        {
            std::cout<<value<<"  ";
        }
        std::cout<<std::endl;

        std::cout<<"Right:"<<std::endl;
        for (auto & value : right)
        {
            std::cout<<value<<"  ";
        }
        std::cout<<std::endl;
    }

    {
        hpx::stencil_view<double,2,std::vector<double>> my_view({8,4});

        auto up    = my_view.get_boundary(begin,end,-1, 0);
        auto down  = my_view.get_boundary(begin,end, 1, 0);
        auto left  = my_view.get_boundary(begin,end, 0,-1);
        auto right = my_view.get_boundary(begin,end, 0, 1);

        std::cout<<"Up:"<<std::endl;
        for (auto & value : up)
        {
            std::cout<<value<<"  ";
        }
        std::cout<<std::endl;


        std::cout<<"Down:"<<std::endl;
        for (auto & value : down)
        {
            std::cout<<value<<"  ";
        }
        std::cout<<std::endl;

        std::cout<<"Left:"<<std::endl;
        for (auto & value : left)
        {
            std::cout<<value<<"  ";
        }
        std::cout<<std::endl;

        std::cout<<"Right:"<<std::endl;
        for (auto & value : right)
        {
            std::cout<<value<<"  ";
        }
        std::cout<<std::endl;
    }
}


int main()
{
    stencil_test();

    return 0;
}

