////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine TRAN TAN
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(MAKE_ACTION_HPP)
#define MAKE_ACTION_HPP

#include <hpx/hpx.hpp>
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

        struct caller{
          template<typename F, typename ReturnType, typename ... Args>
          static inline ReturnType call(Args && ... args)
          {
             int * dummy = nullptr;
             return reinterpret_cast<const F&>(*dummy)( std::forward<Args>(args)... );
          }
        };

        template<class F, typename ReturnType, typename Sequence>
        struct make_action_using_sequence
        {};

        template<class F, typename ReturnType, template <typename...> class T, typename ... Args>
        struct make_action_using_sequence< F, ReturnType, T<Args...> >
        {
          using type = typename hpx::actions::make_direct_action<decltype(&caller::call<F,ReturnType,Args...>)
                                                          ,&caller::call<F,ReturnType,Args...>
                                                          >::type;
        };

        template <typename T>
        struct extract_parameters
        {};

        // Specialization for lambdas
        template <typename ClassType, typename ReturnType, typename... Args>
        struct extract_parameters<ReturnType(ClassType::*)(Args...) const>
        {
            using type = hpx::detail::Sequence<Args...>;
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

        struct action_maker
        {
            template<class F>
            constexpr typename action_from_lambda<F>::type operator += (F* f)
            {
                static_assert( !std::is_assignable<F,F>() && std::is_empty<F>()
                             ,"Lambda is required"
                            );

                return typename action_from_lambda<F>::type();
            }
        };
    }

    template< typename F>
    constexpr auto make_action(F && f) -> decltype( hpx::detail::action_maker() += true ? nullptr : hpx::detail::addr_add() + f )
    {
      return hpx::detail::action_maker() += true ? nullptr : hpx::detail::addr_add() + f;
    }

}

#endif
