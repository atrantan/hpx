////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(VIEW_ELEMENT_BOUNDARY_HPP)
#define VIEW_ELEMENT_BOUNDARY_HPP

#include <iostream>
#include <utility>
#include <boost/cstdint.hpp>

#include <hpx/runtime/get_locality_id.hpp>
#include <hpx/runtime/naming/name.hpp>
#include <hpx/components/containers/partitioned_vector/partitioned_vector_component.hpp>

#include <stencil_view/stencil_view.hpp>

namespace hpx { namespace detail {

// Struct defining the view of pvector_view element boundary.
    template<typename T, typename Stencil>
    struct view_element_boundary
    : public hpx::partitioned_vector_partition<T>
    {
        using data_type = typename hpx::server::partitioned_vector<T>::data_type;
        using segment_iterator = typename hpx::vector_iterator<T>::segment_iterator;
        using boundary_type = typename Stencil::boundary_type;

    public:
        template<typename ... I>
        explicit view_element_boundary(  segment_iterator && it
                                        , Stencil const & stencil
                                        , I ... i
                                        )
        : hpx::partitioned_vector_partition<T>( it->get_id() )
        , is_data_here_( hpx::get_locality_id() == hpx::naming::get_locality_id_from_id(it->get_id()) )
        {
           std::vector<T> dummy_vector( stencil.get_minimum_vector_size() );     // To avoid assertion failure

           auto begin = is_data_here_ ? this->get_ptr()->get_data().begin() : dummy_vector.begin();
           auto end   = is_data_here_ ? this->get_ptr()->get_data().end()   : dummy_vector.end();

           boundary_ = stencil.get_boundary(begin,end,int(i)...);
        }

    // Not copyable
        view_element_boundary(view_element_boundary const &) = delete;
        view_element_boundary& operator=(view_element_boundary const &) = delete;

    // But movable
        view_element_boundary(view_element_boundary && other) = default;


        bool is_data_here() const
        {
            return is_data_here_;
        }

    // Safe assignment (Put operation)
        void operator=(view_element_boundary && other)
        {
            if ( other.is_data_here() )
            {
                auto in  = other.boundary_.begin();
                auto end = other.boundary_.end();
                auto out = boundary_.begin();

                if( is_data_here() )
                {
                    for(/**/; in != end; out++, in++)
                    {
                        *out  = *in;
                    }
                }

                else
                {
                    std::size_t size = std::distance(in,end);

                    std::vector<T> data( size );
                    std::vector<std::size_t> indices( size );

                    auto d_it  = data.begin();
                    auto i_it  = indices.begin();

                    for(/**/; in != end; out++, in++, d_it++, i_it++)
                    {
                        *d_it  = *in;
                        *i_it  = out.raw_pos();
                    }

                    this->set_values_sync(indices,data);
                }
            }
        }

    private:
        const bool is_data_here_;
        boundary_type boundary_;
    };

}}

#endif
