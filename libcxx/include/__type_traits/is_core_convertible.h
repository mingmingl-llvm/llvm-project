//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___TYPE_TRAITS_IS_CORE_CONVERTIBLE_H
#define _LIBCPP___TYPE_TRAITS_IS_CORE_CONVERTIBLE_H

#include <__config>
#include <__type_traits/integral_constant.h>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

// [conv.general]/3 says "E is convertible to T" whenever "T t=E;" is well-formed.
// We can't test for that, but we can test implicit convertibility by passing it
// to a function. Notice that __is_core_convertible<void,void> is false,
// and __is_core_convertible<immovable-type,immovable-type> is true in C++17 and later.

template <class _Tp, class _Up, class = void>
inline const bool __is_core_convertible_v = false;

template <class _Tp, class _Up>
inline const bool
    __is_core_convertible_v<_Tp, _Up, decltype(static_cast<void (*)(_Up)>(0)(static_cast<_Tp (*)()>(0)()))> = true;

template <class _Tp, class _Up>
using __is_core_convertible _LIBCPP_NODEBUG = integral_constant<bool, __is_core_convertible_v<_Tp, _Up> >;

#if _LIBCPP_STD_VER >= 20

template <class _Tp, class _Up>
concept __core_convertible_to = __is_core_convertible_v<_Tp, _Up>;

#endif // _LIBCPP_STD_VER >= 20

template <class _Tp, class _Up, bool = __is_core_convertible_v<_Tp, _Up> >
inline const bool __is_nothrow_core_convertible_v = false;

#ifndef _LIBCPP_CXX03_LANG
template <class _Tp, class _Up>
inline const bool __is_nothrow_core_convertible_v<_Tp, _Up, true> =
    noexcept(static_cast<void (*)(_Up) noexcept>(0)(static_cast<_Tp (*)() noexcept>(0)()));
#endif

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___TYPE_TRAITS_IS_CORE_CONVERTIBLE_H
