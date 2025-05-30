//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___EXCEPTION_TERMINATE_H
#define _LIBCPP___EXCEPTION_TERMINATE_H

#include <__config>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_UNVERSIONED_NAMESPACE_STD
_LIBCPP_BEGIN_EXPLICIT_ABI_ANNOTATIONS
[[__noreturn__]] _LIBCPP_EXPORTED_FROM_ABI void terminate() _NOEXCEPT;
_LIBCPP_END_EXPLICIT_ABI_ANNOTATIONS
_LIBCPP_END_UNVERSIONED_NAMESPACE_STD

#endif // _LIBCPP___EXCEPTION_TERMINATE_H
