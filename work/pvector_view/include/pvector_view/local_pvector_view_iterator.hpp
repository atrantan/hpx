////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(LOCAL_PVECTOR_VIEW_ITERATOR_HPP)
#define LOCAL_PVECTOR_VIEW_ITERATOR_HPP

#include <boost/iterator/iterator_adaptor.hpp>

#include <pvector_view/view_element.hpp>

namespace hpx {

    template <typename DataType, typename BaseIter>
    class local_pvector_view_iterator
      : public boost::iterator_adaptor<
              local_pvector_view_iterator<DataType, BaseIter>
            , BaseIter
            , DataType
            , std::forward_iterator_tag
        >
    {
    private:
        using base_type = boost::iterator_adaptor<
                              local_pvector_view_iterator<DataType, BaseIter>
                            , BaseIter
                            , DataType
                            , std::forward_iterator_tag
                          >;

    public:
        local_pvector_view_iterator()
        {}

        explicit local_pvector_view_iterator(BaseIter && it, BaseIter && end)
        : base_type( std::forward<BaseIter>(it) ), end_( std::forward<BaseIter>(end) )
        {
            satisfy_predicate();
        }

        bool is_at_end() const
        {
            return this->base_reference() == end_;
        }

    private:
        friend class boost::iterator_core_access;

        DataType & dereference() const
        {
            HPX_ASSERT(!is_at_end());
            return this->base_reference()->data();
        }

        void increment()
        {
            ++(this->base_reference());
            satisfy_predicate();
        }

        void satisfy_predicate()
        {
            while ( this->base_reference() != end_ && !this->base_reference()->is_data_here() )
                ++( this->base_reference() );
        }

    private:
        BaseIter end_;
    };
}

#endif
