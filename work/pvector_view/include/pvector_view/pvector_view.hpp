////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(PVECTOR_VIEW_HPP)
#define PVECTOR_VIEW_HPP

#include <vector>
#include <initializer_list>

#include <hpx/components/containers/partitioned_vector/partitioned_vector.hpp>
#include <hpx/components/containers/partitioned_vector/partitioned_vector_segmented_iterator.hpp>
#include <hpx/components/containers/container_distribution_policy.hpp>
#include <hpx/runtime/serialization/serialize.hpp>

#include <pvector_view/get_unrolled_localities.hpp>
#include <pvector_view/pvector_view_iterator.hpp>
#include <pvector_view/view_element.hpp>

#include <utility/last_element.hpp>
#include <utility/are_integral.hpp>
#include <utility/make_index_sequence.hpp>

#include <stencil_view/stencil_view.hpp>
#include <spmd_block/spmd_block.hpp>

namespace hpx {

    template<typename T>
    using partition = std::vector<T>;

    namespace detail{

        struct auto_subscript {
            // Defined to pass the ternary operator in offset solver
            // and for automatic size
            constexpr operator std::size_t()
            {
                return std::size_t(-1);
            }
        };

        template<typename C>
        struct cast_if_autosubscript
        {
            using type = C;
        };

        template<>
        struct cast_if_autosubscript<detail::auto_subscript>
        {
            using type = int;
        };

    }

    // Used for "automatic" coarray/view subscript and size
    constexpr hpx::detail::auto_subscript _;

// Struct defining a pvector_view.

// A pvector_view is a view of a partitioned_vector.
    template<typename T, std::size_t N, typename Derived = void>
    struct pvector_view
    {
        using value_type = T;

    private:
        using pvector_iterator = hpx::vector_iterator<T>;
        using segment_iterator = typename pvector_iterator::segment_iterator;
        using segment_type = typename segment_iterator::value_type;
        using traits = typename hpx::traits::segmented_iterator_traits<pvector_iterator>;
        using list_type = std::initializer_list<std::size_t>;
        using stencil_type = hpx::stencil_view<T,N>;

        template<std::size_t... I>
        void fill_basis( list_type const & sizes
                       , std::array<std::size_t,N+1> & basis
                       , std::size_t numlocs
                       , hpx::detail::integer_sequence<std::size_t, I...>
                       ) const
        {
            basis[0] = 1;

            std::size_t  tmp = 1;
            auto in  = sizes.begin();

            (void)std::initializer_list<int>
            { ( static_cast<void>(
                  basis[I+1] = tmp *= ( *in != std::size_t(-1) ? *in : numlocs )
                , in++
                )
                , 0
              )...
            };
        }

    public:
        using iterator
        = typename hpx::pvector_view_iterator<value_type,N,stencil_type>;

        explicit pvector_view( segment_iterator && v_begin)
        : begin_(v_begin)
        {}

        explicit pvector_view( hpx::parallel::spmd_block & block
                             , pvector_iterator && v_begin
                             , pvector_iterator && v_last
                             , list_type && sw_sizes
                             , list_type && hw_sizes = {}
                             , stencil_type && stencil = {}
                             , bool is_automatic_size = false
                             )
        : pvector_view( block
                      , traits::segment(std::forward<pvector_iterator>(v_begin))
                      , traits::segment(std::forward<pvector_iterator>(v_last))
                      , std::forward<list_type>(sw_sizes)
                      , std::forward<list_type>(hw_sizes)
                      , std::forward<stencil_type>(stencil)
                      , is_automatic_size
                      )
        {}


        explicit pvector_view( hpx::parallel::spmd_block & block
                             , segment_iterator && begin
                             , segment_iterator && last
                             , list_type sw_sizes
                             , list_type hw_sizes = {}
                             , stencil_type && stencil = {}
                             , bool is_automatic_size = false
                             )
        : begin_( begin )
        , stencil_(stencil)
        , rank_( block.this_image() )
        , is_automatic_subscript_allowed(is_automatic_size)
        {
            using indices = typename hpx::detail::make_index_sequence<N>::type;

// Physical sizes is equal to logical sizes if physical sizes are not defined
            list_type & hw_sizes_ = hw_sizes.size() ? hw_sizes : sw_sizes;

// Check that sizes of the view are valid regarding its dimension
            HPX_ASSERT_MSG(sw_sizes.size() == N
                          ,"**CoArray Error** : Defined sizes must match the coarray dimension"
                          );

            std::size_t numlocs = block.get_num_images();

// Generate two mixed radix basis
            fill_basis(hw_sizes_, hw_basis_, numlocs, indices() );
            fill_basis(sw_sizes, sw_basis_, numlocs, indices() );

// Check that combined sizes doesn't overflow the used space
            HPX_ASSERT_MSG( hw_basis_[N] <= std::distance(begin,last)
                          , "**CoArray Error** : Space dedicated to the described view is too small"
                          );
        }

    private:

        template<typename... I>
        inline std::size_t offset_solver(I ... index) const
        {
// Check that the subscript is valid regarding the view dimension
            static_assert( sizeof...(I) == N
                , "**CoArray Error** : Subscript must match the coarray dimension"
                );
// Check that all the elements are of integral type
            static_assert( detail::are_integral< typename detail::cast_if_autosubscript<I>::type...>::value, "One or more elements in subscript is not integral" );

            std::size_t  offset = 0;
            std::size_t  i = 0;

            (void)std::initializer_list<int>
            { ( static_cast<void>( offset += ( std::is_same<I,hpx::detail::auto_subscript>::value
                                               ? rank_
                                               : std::size_t(index)
                                             ) * hw_basis_[i++]
                                 )
              , 0
              )...
            };

            // Check that the solved index doesn't overflow the used space
            HPX_ASSERT_MSG( offset< hw_basis_.back(),"**CoArray Error** : Invalid co-array subscript");

            return offset;
        }

    public:
    // Array interfaces
        template<typename... I>
        std::vector<T> & data(I... index)
        {
            using last_element = typename hpx::detail::last_element< I... >::type;
            using condition = typename std::is_same<last_element,detail::auto_subscript>;

            static_assert(condition::value,"**CoArray Error** : Data reference from subscript cannot be retrieved if the last index is not 'hpx::_'");

            HPX_ASSERT_MSG( is_automatic_subscript_allowed
            ,"**CoArray Error** : Data reference from subscript cannot be retrieved if the last dimension size of the current coarray is not defined as automatic"
            );

            std::size_t offset = offset_solver(index... );
            return hpx::detail::view_element<T,stencil_type>(begin_ + offset, stencil_).data();
        }

        template<typename... I>
        std::vector<T> const & data(I... index) const
        {
            using last_element = typename hpx::detail::last_element< I... >::type;
            using condition = typename std::is_same<last_element,detail::auto_subscript>;

            static_assert(condition::value,"**CoArray Error** : Data reference from subscript cannot be retrieved if the last index is not 'hpx::_'");

            HPX_ASSERT_MSG( is_automatic_subscript_allowed
            ,"**CoArray Error** : Data reference from subscript cannot be retrieved if the last dimension size of the current coarray is not defined as automatic"
            );

            std::size_t offset = offset_solver(index... );
            return hpx::detail::view_element<T,stencil_type>(begin_ + offset, stencil_).data();
        }

    // explicit get operation
        template<typename... I>
        std::vector<T> get(I... index) const
        {
            using last_element = typename hpx::detail::last_element< I... >::type;
            using condition = typename std::is_same<last_element,detail::auto_subscript>;

            static_assert(!condition::value,"**CoArray Error** : 'hpx::_' cannot be used for get operation from subscript");

            std::size_t offset = offset_solver(index... );
            return hpx::detail::view_element<T,stencil_type>(begin_ + offset, stencil_).const_data();
        }


    // implicit put operation (Warning: should be used with "operator =" only
    //                         ex1: this->operator()(1,2,3) = some_vector;
    //                         ex2: this->operator()(1,2,3) = this->operator()(3,4,5);
    //                         )
        template<typename... I>
        hpx::detail::view_element<T,stencil_type> operator()(I... index) const
        {
            using last_element = typename hpx::detail::last_element< I... >::type;
            using condition = typename std::is_same<last_element,detail::auto_subscript>;

            static_assert(!condition::value,"**CoArray Error** : 'hpx::_' cannot be used in proxy operation from subscript");

            std::size_t offset = offset_solver( index... );
            return hpx::detail::view_element<T,stencil_type>(begin_ + offset, stencil_);
        }

    // Iterator interfaces
        iterator begin()
        {
            return iterator(begin_
                           ,sw_basis_,hw_basis_
                           ,0
                           ,stencil_
                           );
        }

        iterator end()
        {
            return iterator( begin_
                           , sw_basis_, hw_basis_
                           , sw_basis_.back()
                           , stencil_
                           );
        }


    private:
        std::array< std::size_t, N+1 > sw_basis_, hw_basis_;
        segment_iterator begin_;
        stencil_type stencil_;
        std::size_t rank_;
        bool is_automatic_subscript_allowed;
    };


// Struct defining a coarray.
// A coarray is a pvector_view tied to a partitioned_vector.
    template <typename T, std::size_t N = 1>
    struct coarray : public hpx::pvector_view< T,N,coarray<T,N>>
    {
    private:
        using list_type = std::initializer_list<std::size_t>;
        using base_type = hpx::pvector_view<T,N,coarray>;
        using pvector_iterator = hpx::vector_iterator<T>;
        using traits = typename hpx::traits::segmented_iterator_traits<pvector_iterator>;
        using stencil_type = hpx::stencil_view<T,N>;
        using indices = typename hpx::detail::make_index_sequence<N>::type;

    private:
        template<std::size_t... I>
        std::array<std::size_t,N>  make_default_sizes( std::size_t first_size
                                                     , hpx::detail::integer_sequence<std::size_t, I...>
                                                     ) const
        {
            std::size_t k = 0;
            return { ( I
                     , (k++>0) ? 1ul : first_size
                     )...
                   };
        }

    public:
        coarray()
        : vector_()
        , base_type( traits::segment(vector_.begin()) )
        {}

        coarray( hpx::parallel::spmd_block & block
               , std::string name
               , list_type && codimensions
               , hpx::partition<T> && init_value
               , hpx::stencil_view<T,N> && stencil = {}
               )
        : vector_()
        , base_type( traits::segment(vector_.begin()) )
        {
// Used to access base members
            base_type & view (*this);
            std::size_t elt_size = init_value.size();

            bool is_automatic_size = ( *(codimensions.end() -1) == std::size_t(-1) );

            if ( block.this_image() == 0 )
            {
                std::size_t numlocs = block.get_num_images();
                std::size_t size = N > 0 ? 1 : 0;

                for( auto const & i : codimensions)
                {
                    size *= (i != std::size_t(-1) ? i : numlocs);
                }

                std::size_t n = 1;

                if(N > 0)
                    n = is_automatic_size ? size/numlocs : 1;

                std::vector< hpx::id_type > localities = block.find_all_localities();

                vector_ = hpx::partitioned_vector<T>( elt_size*size
                    , size ? init_value[0] : T(0)
                    , hpx::container_layout( size
                                           , hpx::get_unrolled_localities(localities,size,n)
                                           )
                    );
                vector_.register_as_sync(name + "_hpx_coarray");
            }
            else
                vector_.connect_to_sync(name + "_hpx_coarray");

            if( ! stencil.has_sizes() )
                stencil = hpx::stencil_view<T,N>( make_default_sizes(init_value.size(), indices()) );

            view = base_type( block
                            , vector_.begin()
                            , vector_.end()
                            , std::forward<list_type>(codimensions)
                            , std::forward<list_type>(codimensions)
                            , std::move(stencil)
                            , is_automatic_size
                            );
        }

    private:
        hpx::partitioned_vector<T> vector_;
    };

    template <typename T, std::size_t N = 1>
    struct coarray_view : public hpx::pvector_view< T,N,coarray_view<T,N>>
    {
    private:
        using base_type = hpx::pvector_view<T,N,coarray_view>;
        using pvector_iterator = hpx::vector_iterator<T>;
        using segment_iterator = typename pvector_iterator::segment_iterator;
        using list_type = std::initializer_list<std::size_t>;
        using stencil_type = hpx::stencil_view<T,N>;

    public:
      coarray_view( hpx::parallel::spmd_block & block
                  , segment_iterator && begin
                  , list_type && sw_sizes
                  , list_type && hw_sizes
                  , stencil_type && stencil = {}
                  )
      : base_type( begin + 0 )
      {
          // Used to access base members
          base_type & view (*this);

          bool is_automatic_size = ( *(hw_sizes.end() -1) == std::size_t(-1) );

          std::size_t numlocs = block.get_num_images();
          std::size_t size = N > 0 ? 1 : 0;

          // Compute dummy offset to pass the overflow checking in pvector_view
          for( auto const & i : hw_sizes)
          {
              size *= (i != std::size_t(-1) ? i : numlocs);
          }


          view = base_type(  block
                           , begin + 0
                           , begin + size
                           , std::move(sw_sizes)
                           , std::move(hw_sizes)
                           , std::move(stencil)
                           , is_automatic_size
                           );
      }

    };

}

#endif

