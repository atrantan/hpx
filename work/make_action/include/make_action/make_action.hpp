////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine TRAN TAN
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(MAKE_ACTION_HPP)
#define MAKE_ACTION_HPP

#include <hpx/include/plain_actions.hpp>
#include <type_traits>


namespace hpx{
// Helpers to actionize a constexpr lambda

    namespace detail{

        template <typename ... T>
        struct Sequence
        {};

        struct addr_add
        {
            template<class T>
            friend typename std::remove_reference<T>::type *operator+(addr_add, T &&t)
            {
                return &t;
            }
        };

        template<typename F, typename ReturnType, typename ... Args>
        struct caller{
          static inline ReturnType call(Args... args)
          {
             int * dummy = nullptr;
             return reinterpret_cast<const F&>(*dummy)( args... );
          }
        };

        template<class F, typename ReturnType, typename Sequence>
        struct make_action_using_sequence
        {};

        template<class F, typename ReturnType, template <typename...> class T, typename ... Args>
        struct make_action_using_sequence< F, ReturnType, T<Args...> >
        {
          using type = typename hpx::actions::make_direct_action<decltype(&caller<F,ReturnType,Args...>::call)
                                                          ,&caller<F,ReturnType,Args...>::call
                                                          >::type;
        };

        template <typename T>
        struct extract_parameters
        {};

        // Specialization for lambdas
        template <typename ClassType, typename ReturnType, typename... Args>
        struct extract_parameters<ReturnType(ClassType::*)(Args...) const>
        {
            template<typename T>
            using remove_reference_t = typename std::remove_reference<T>::type;
            using type = hpx::detail::Sequence< remove_reference_t<Args>... >;
        };

        template <typename T>
        struct extract_return_type
        {};

        template <typename ClassType, typename ReturnType, typename... Args>
        struct extract_return_type<ReturnType(ClassType::*)(Args...) const>
        {
            using type = ReturnType;
        };

        template <typename F>
        struct action_from_lambda
        {
          using sequence = typename extract_parameters<decltype(&F::operator())>::type;
          using return_type = typename extract_return_type<decltype(&F::operator())>::type;
          using type = typename make_action_using_sequence<F,return_type,sequence>::type;
        };

        template<typename F>
        struct action_maker
        {
            constexpr typename action_from_lambda<F>::type operator += (F* f)
            {
                static_assert( !std::is_assignable<F,F>::value && std::is_empty<F>::value
                             ,"Lambda without capture list is required"
                            );

                return typename action_from_lambda<F>::type();
            }
        };


        template<typename F, F f>
        struct action_maker< std::integral_constant<F,f> >
        {
            template< typename Dummy>
            constexpr typename hpx::actions::make_direct_action<F,f>::type operator += (Dummy)
            {
                static_assert( !std::is_assignable<F,F>::value && !std::is_empty<F>::value
                             ,"Function is required"
                            );

                return typename hpx::actions::make_direct_action<F,f>::type();;
            }
        };



    }

  template< typename F>
  constexpr auto make_action(F && f) -> decltype( hpx::detail::action_maker<F>() += true ? nullptr : hpx::detail::addr_add() + f )
  {
    return hpx::detail::action_maker<F>() += true ? nullptr : hpx::detail::addr_add() + f;
  }

}

#endif
