//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___UTILITY_SWAP_H
#define _LIBCPP___UTILITY_SWAP_H

#include <__config>
#include <__type_traits/enable_if.h>
#include <__type_traits/is_assignable.h>
#include <__type_traits/is_constructible.h>
#include <__type_traits/is_trivially_assignable.h>
#include <__type_traits/is_trivially_constructible.h>
#include <__type_traits/is_nothrow_assignable.h>
#include <__type_traits/is_nothrow_constructible.h>
#include <__type_traits/is_swappable.h>
#include <__type_traits/is_trivially_relocatable.h>
#include <__utility/declval.h>
#include <__utility/move.h>
#include <cstddef>
#include <typeinfo>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_PUSH_MACROS
#include <__undef_macros>

_LIBCPP_BEGIN_NAMESPACE_STD

#ifndef _LIBCPP_CXX03_LANG
template <class _Tp>
using __swap_result_t = __enable_if_t<is_move_constructible<_Tp>::value && is_move_assignable<_Tp>::value>;
#else
template <class>
using __swap_result_t = void;
#endif

#if _LIBCPP_STD_VER >= 26
template <class _Tp>
  requires std::is_trivially_relocatable_v<_Tp> && std::is_replaceable_v<_Tp>
void swap_value_representations(_Tp& __a, _Tp& __b) {
  // For now we assume the value representation correspond to the memory
  // between the start of the first subobject and the end of the last subobject
  // which holds for both itanium and MSVC.
  constexpr size_t __value_size = __datasizeof(_Tp) - __value_representation_begin(_Tp);
  if constexpr (__value_size == 0)
    return;

  char* __aptr = reinterpret_cast<char*>(&__a) + __value_representation_begin(_Tp);
  char* __bptr = reinterpret_cast<char*>(&__b) + __value_representation_begin(_Tp);

  constexpr size_t __size  = __value_size < 512 ? __datasizeof(_Tp) : 512;
  constexpr size_t __chunk = __value_size / __size;
  constexpr size_t __rem   = __value_size % __size;

  char __buffer[__size];

  if constexpr (__chunk) {
    for (std::size_t __n = 0; __n < __chunk; __n++, __aptr += __size, __bptr += __size) {
      __builtin_memcpy(__buffer, __aptr, __size);
      __builtin_memcpy(__aptr, __bptr, __size);
      __builtin_memcpy(__bptr, __buffer, __size);
    }
  }
  if constexpr (__rem) {
    __builtin_memcpy(__buffer, __aptr, __rem);
    __builtin_memcpy(__aptr, __bptr, __rem);
    __builtin_memcpy(__bptr, __buffer, __rem);
  }
}
#endif

template <class _Tp>
inline _LIBCPP_HIDE_FROM_ABI __swap_result_t<_Tp> _LIBCPP_CONSTEXPR_SINCE_CXX20 swap(_Tp& __x, _Tp& __y)
    _NOEXCEPT_(is_nothrow_move_constructible<_Tp>::value&& is_nothrow_move_assignable<_Tp>::value) {
#if _LIBCPP_STD_VER >= 26
  if !consteval {
    if constexpr (!(std::is_trivially_move_assignable_v<_Tp> && std::is_trivially_move_constructible_v<_Tp>) &&
                  std::is_trivially_relocatable_v<_Tp> && std::is_replaceable_v<_Tp>) {
      swap_value_representations(__x, __y);
      return;
    }
  }
#endif
  _Tp __t(std::move(__x));
  __x = std::move(__y);
  __y = std::move(__t);
}

template <class _Tp, size_t _Np, __enable_if_t<__is_swappable_v<_Tp>, int> >
inline _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR_SINCE_CXX20 void swap(_Tp (&__a)[_Np], _Tp (&__b)[_Np])
    _NOEXCEPT_(__is_nothrow_swappable_v<_Tp>) {
  for (size_t __i = 0; __i != _Np; ++__i) {
    swap(__a[__i], __b[__i]);
  }
}

_LIBCPP_END_NAMESPACE_STD

_LIBCPP_POP_MACROS

#endif // _LIBCPP___UTILITY_SWAP_H
