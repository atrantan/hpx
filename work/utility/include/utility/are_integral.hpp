////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2015 Antoine Tran Tan
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////
#if !defined(ARE_INTEGRAL_HPP)
#define ARE_INTEGRAL_HPP

#include <type_traits>

namespace hpx {  namespace detail {

template<typename ... T>
struct are_integral
{
    template<bool ...>
    struct bools;

    using type = std::is_same< bools<true, std::is_integral<T>::value ...>
                             , bools<std::is_integral<T>::value ...,true >
                             >;

    static constexpr bool value = type::value;
};

}}

#endif
