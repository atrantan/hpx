////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(VIEW_ELEMENT_HPP)
#define VIEW_ELEMENT_HPP

#include <iostream>
#include <utility>
#include <boost/cstdint.hpp>

#include <hpx/hpx.hpp>
#include <hpx/components/containers/partitioned_vector/partitioned_vector_component.hpp>

#include <pvector_view/view_element_boundary.hpp>

namespace hpx { namespace detail {

// Struct defining the view of pvector_view element.
    template<typename T, typename Stencil>
    struct view_element
    : public hpx::partitioned_vector_partition<T>
    {
        using segment_iterator = typename hpx::vector_iterator<T>::segment_iterator;

    public:
        explicit view_element(segment_iterator it, Stencil const & stencil)
        : hpx::partitioned_vector_partition<T>( it->get_id() )
        , is_data_here_( hpx::get_locality_id() == hpx::naming::get_locality_id_from_id(it->get_id()) )
        , it_(it)
        , stencil_(stencil)
        {}

        using data_type = typename hpx::server::partitioned_vector<T>::data_type;

        view_element(view_element const &) = default;

    // Not copy-assygnable
        view_element& operator=(view_element const &) = delete;

    // But movable
        view_element(view_element && other) = default;

    // operator overloading (Useful for manual view definition)
        segment_iterator && operator&()
        {
            return std::move(it_);
        }

        bool is_data_here() const
        {
            return is_data_here_;
        }

        data_type const_data() const
        {
            if ( is_data_here() )
            {
                return this->get_ptr()->get_data();
            }
            else
                return this->get_copied_data_sync();
        }

    // Unsafe reference get operation
    // Warning : be careful in making this operation be called after checking with is_data_here()
        data_type & data()
        {
            return this->get_ptr()->get_data();
        }

        data_type const & data() const
        {
            return this->get_ptr()->get_data();
        }

    // Unsafe assignment (Put operation)
    // Warning : be careful in making this operation be invoked by only one locality at a time
        void operator=(data_type && other)
        {
            if ( is_data_here() )
            {
                data_type & ref = data();
                HPX_ASSERT_MSG( ref.size() == other.size()
                , "**CoArray Error** : Try to assign a vector with invalid size to an existing co-indexed element"
                );
                ref = other;
            }
            else
            {
                this->set_data_sync( std::move(other) );
            }
        }

    // Safe assignment (Put operation)
        void operator=(view_element && other)
        {
            if ( other.is_data_here() )
            {
                if( is_data_here() )
                {
                    data_type & ref = data();
                    HPX_ASSERT_MSG( ref.size() == other.size()
                                 , "**CoArray Error** : Try to assign a co-indexed element with invalid size to an existing co-indexed element"
                                 );
                    ref = other.data();
                }

                else
                    this->set_data_sync( other.const_data() );
            }
        }

    // Stencil interfaces
        template<typename ... I>
        auto get_boundary( I ... index )
        {
            return  hpx::detail::view_element_boundary<T,Stencil>
                    ( std::move(it_)
                    , stencil_
                    , int(index)...
                    );
        }


    private:
        const bool is_data_here_;
        segment_iterator it_;
        Stencil const & stencil_;
    };

}}

#endif
