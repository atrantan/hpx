////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine TRAN TAN
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(REMOTE_CALL_HPP)
#define REMOTE_CALL_HPP

#include <hpx/hpx.hpp>
#include <hpx/include/plain_actions.hpp>
#include <tuple>
#include <type_traits>


namespace hpx{
// Helpers to actionize a constexpr lambda

    namespace detail{

        struct addr_add
        {
            template<class T>
            friend typename std::remove_reference<T>::type *operator+(addr_add, T &&t)
            {
                return &t;
            }
        };

        struct caller{
          template<typename F, typename ... Args>
          static inline decltype(auto) call(Args && ... args)
          {
             int * dummy = nullptr;
             return reinterpret_cast<const F&>(*dummy)( std::forward<Args>(args)... );
          }
        };

        template<class F, typename Sequence>
        struct make_action_using_sequence
        {};

        template<class F, template <typename...> class T, typename ... Args>
        struct make_action_using_sequence< F, T<Args...> >
        {
          using type = typename hpx::actions::make_direct_action<decltype(&caller::call<F,Args...>)
                                                          ,&caller::call<F,Args...>
                                                          >::type;
        };

        template <typename T>
        struct extract_parameters
        {};

        // Specialization for lambdas
        template <typename ClassType, typename ReturnType, typename... Args>
        struct extract_parameters<ReturnType(ClassType::*)(Args...) const>
        {
            using type = std::tuple<Args...>;
        };

        template <typename F>
        struct action_from_lambda
        {
          using sequence = typename extract_parameters<decltype(&F::operator())>::type;
          using type = typename make_action_using_sequence<F,sequence>::type;
        };

        struct action_maker
        {
            template<class F>
            constexpr auto operator += (F* f)
            {
                static_assert( !std::is_assignable<F,F>() && std::is_empty<F>()
                             ,"Lambda is required"
                            );

                return typename action_from_lambda<F>::type();
            }
        };
    }

    template< typename F>
    constexpr auto make_action(F && f)
    {
      return hpx::detail::action_maker() += true ? nullptr : hpx::detail::addr_add() + f;
    }

}

#endif
