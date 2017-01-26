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

#include <hpx/runtime/get_locality_id.hpp>
#include <hpx/runtime/launch_policy.hpp>
#include <hpx/runtime/naming/name.hpp>
#include <hpx/components/containers/partitioned_vector/partitioned_vector_component.hpp>

#include <pvector_view/view_element_boundary.hpp>

namespace hpx { namespace detail {

// Struct defining the view of pvector_view element.
    template<typename T, typename Data, typename Stencil>
    struct view_element
    : public hpx::partitioned_vector_partition<T,Data>
    {
        using segment_iterator
            = typename hpx::vector_iterator<T,Data>::segment_iterator;

    public:
        explicit view_element(segment_iterator it, Stencil const & stencil)
        : hpx::partitioned_vector_partition<T,Data>( it->get_id() ),
          is_data_here_(hpx::get_locality_id()
            == hpx::naming::get_locality_id_from_id(it->get_id()) ),
          it_(it),
          stencil_(stencil)
        {}

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

        Data const_data() const
        {
            if ( is_data_here() )
            {
                return this->get_ptr()->get_data();
            }
            else
                return this->get_copied_data(hpx::launch::sync) ;
        }

    // Unsafe reference get operation
    // Warning : Make sure that is_data_here() has been called before
        Data & data()
        {
            return this->get_ptr()->get_data();
        }

        Data const & data() const
        {
            return this->get_ptr()->get_data();
        }

    // Unsafe assignment (Put operation)
    // Warning : Make sure that operator=() is called in only one locality
        void operator=(Data && other)
        {
            if ( is_data_here() )
            {
                Data & ref = data();

                HPX_ASSERT_MSG( ref.size() == other.size(), \
                    "**CoArray Error** : Vector r-value has invalid size");

                ref = other;
            }
            else
            {
                this->set_data(hpx::launch::sync, std::move(other) );
            }
        }

    // Safe assignment (Put operation)
        template<typename RightStencil>
        void operator=(view_element<T,Data,RightStencil> && other)
        {
            if ( other.is_data_here() )
            {
                if( is_data_here() )
                {
                    Data & ref = data();

                    HPX_ASSERT_MSG( ref.size() == other.size(),
                        "**CoArray Error** : Co-indexed element r-value has " \
                        "invalid size");

                    ref = other.data();
                }

                else
                    this->set_data(hpx::launch::sync, other.const_data() );
            }
        }

        T operator[](std::size_t i) const
        {
            if( is_data_here() )
            {
                return data()[i];
            }

            else
                return this->get_value(hpx::launch::sync,i);
        }

    // Stencil interfaces
        template<typename ... I>
        hpx::detail::view_element_boundary<T,Data,Stencil>
        get_boundary( I ... index )
        {
            return hpx::detail::view_element_boundary<T,Data,Stencil>(
                std::move(it_), stencil_, int(index)... );
        }


    private:
        const bool is_data_here_;
        segment_iterator it_;
        Stencil const & stencil_;
    };

}}

#endif
