//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___ALGORITHM_SWAP_RANGES_H
#define _LIBCPP___ALGORITHM_SWAP_RANGES_H

#include <__algorithm/iterator_operations.h>
#include <__config>
#include <__cstddef/byte.h>
#include <__cstddef/size_t.h>
#include <__iterator/iterator_traits.h>
#include <__type_traits/is_same.h>
#include <__type_traits/is_trivially_relocatable.h>
#include <__utility/move.h>
#include <__utility/pair.h>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_PUSH_MACROS
#include <__undef_macros>

_LIBCPP_BEGIN_NAMESPACE_STD

// 2+2 iterators: the shorter size will be used.
template <class _AlgPolicy, class _ForwardIterator1, class _Sentinel1, class _ForwardIterator2, class _Sentinel2>
_LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR_SINCE_CXX20 pair<_ForwardIterator1, _ForwardIterator2>
__swap_ranges(_ForwardIterator1 __first1, _Sentinel1 __last1, _ForwardIterator2 __first2, _Sentinel2 __last2) {
  while (__first1 != __last1 && __first2 != __last2) {
    _IterOps<_AlgPolicy>::iter_swap(__first1, __first2);
    ++__first1;
    ++__first2;
  }

  return pair<_ForwardIterator1, _ForwardIterator2>(std::move(__first1), std::move(__first2));
}

// 2+1 iterators: size2 >= size1.
template <class _AlgPolicy, class _ForwardIterator1, class _Sentinel1, class _ForwardIterator2>
_LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR_SINCE_CXX20 pair<_ForwardIterator1, _ForwardIterator2>
__swap_ranges(_ForwardIterator1 __first1, _Sentinel1 __last1, _ForwardIterator2 __first2) {
#if _LIBCPP_STD_VER >= 26
  if ! consteval {
    using _V1Type = iterator_traits<_ForwardIterator1>::value_type;
    using _V2Type = iterator_traits<_ForwardIterator2>::value_type;
    if constexpr(__libcpp_is_contiguous_iterator<_ForwardIterator1>::value &&
                 __libcpp_is_contiguous_iterator<_ForwardIterator2>::value &&
                 is_same_v<_V1Type, _V2Type> &&
                 is_trivially_relocatable_v<_V1Type> && is_replaceable_v<_V1Type>) {
      size_t __distance = distance(__first1, __last1);
      if (__distance == 1)
        iter_swap(__first1, __first2);
      else if (__distance > 0) {
        size_t __numBytes = __distance * sizeof(_V1Type);
        byte *__aptr = reinterpret_cast<byte *> (addressof(*__first1));
        byte *__bptr = reinterpret_cast<byte *> (addressof(*__first2));

        size_t __size  = __numBytes < 512 ? __numBytes : 512;
        size_t __chunk = __numBytes / __size;
        size_t __rem   = __numBytes % __size;

        char __buffer[512];
        if (__chunk > 0) {
          for (std::size_t __n = 0; __n < __chunk; __n++, __aptr += __size, __bptr += __size) {
            __builtin_memcpy(__buffer, __aptr,   __size);
            __builtin_memcpy(__aptr,   __bptr,   __size);
            __builtin_memcpy(__bptr,   __buffer, __size);
            }
          }

          if (__rem > 0) {
            __builtin_memcpy(__buffer, __aptr,   __rem);
            __builtin_memcpy(__aptr,   __bptr,   __rem);
            __builtin_memcpy(__bptr,   __buffer, __rem);
            }
          }
          return pair<_ForwardIterator1, _ForwardIterator2>(__first1 + __distance, __first2 + __distance);
        }
    }
#endif

// else not replaceable, not contiguous, or constexpr
  while (__first1 != __last1) {
    _IterOps<_AlgPolicy>::iter_swap(__first1, __first2);
    ++__first1;
    ++__first2;
  }

  return pair<_ForwardIterator1, _ForwardIterator2>(std::move(__first1), std::move(__first2));
}

template <class _ForwardIterator1, class _ForwardIterator2>
inline _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR_SINCE_CXX20 _ForwardIterator2
swap_ranges(_ForwardIterator1 __first1, _ForwardIterator1 __last1, _ForwardIterator2 __first2) {
  return std::__swap_ranges<_ClassicAlgPolicy>(std::move(__first1), std::move(__last1), std::move(__first2)).second;
}

_LIBCPP_END_NAMESPACE_STD

_LIBCPP_POP_MACROS

#endif // _LIBCPP___ALGORITHM_SWAP_RANGES_H
