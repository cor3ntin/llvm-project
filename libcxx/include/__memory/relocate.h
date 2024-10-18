#ifndef _LIBCPP___MEMORY_RELOCATE_H
#define _LIBCPP___MEMORY_RELOCATE_H

#include <__config>
#include <__functional/operations.h>
#include <__type_traits/is_const.h>
#include <__type_traits/is_trivially_relocatable.h>
#include <__type_traits/is_polymorphic.h>
#include <cstddef>
#include <typeinfo>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

#if _LIBCPP_STD_VER >= 20

template <class _Tp>
_LIBCPP_EXPORTED_FROM_ABI _Tp* trivially_relocate(_Tp* __begin, _Tp* __end, _Tp* __new_location) noexcept {
    static_assert(is_trivially_relocatable_v<_Tp> && !is_const_v<_Tp>);
	if constexpr (is_polymorphic_v<_Tp>) {
		_LIBCPP_ASSERT(__end - __begin != sizeof(_Tp) || typeid(*__begin) == typeid(_Tp),
		              "__begin does not point to a complete object");
	}
	(void) __builtin_trivially_relocate(__new_location, __begin, sizeof(_Tp) * (__end - __begin));
	return __new_location + (__end - __begin);
}

#endif

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___MEMORY_RELOCATE_H
