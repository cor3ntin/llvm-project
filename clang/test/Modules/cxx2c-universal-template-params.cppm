// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: split-file %s %t
//
// RUN: %clang_cc1 -std=c++2c %t/A.cppm -emit-module-interface -o %t/A.pcm
// RUN: %clang_cc1 -std=c++2c %t/Use.cpp -fprebuilt-module-path=%t -verify -fsyntax-only
//
// RUN: %clang_cc1 -std=c++2c %t/A.cppm -emit-reduced-module-interface -o %t/A.pcm
// RUN: %clang_cc1 -std=c++2c %t/Use.cpp -fprebuilt-module-path=%t -verify -fsyntax-only

//--- A.cppm
export module A;

export template <universal template U> struct S {};
export template <universal template... Us> struct P {
  static constexpr int size = sizeof...(Us);
};
export template <typename T> struct Box {};
export template <int &> struct IntRef {};
export int x;

export template <universal template> constexpr int kind = 0;
export template <typename T> constexpr int kind<T> = 1;
export template <template <universal template...> class TT>
constexpr int kind<TT> = 2;
export template <decltype(auto) V> constexpr int kind<V> = 3;
export template <int &R> constexpr int kind<R> = 4;

export template <template <universal template> class Z, universal template U>
using Apply = Z<U>;

export template <universal template U> constexpr int f() { return 1; }
export template <class T> constexpr int f() { return 2; }

export template <universal template U> struct Member {
  template <universal template V> constexpr int g(S<U>, S<V>) { return 3; }
};

export template <class T, unsigned long N = 4> struct Vec {};
export template <typename, typename> struct Rebind;
export template <template <class, universal template...> class C, class T,
                 universal template... Args, class U>
struct Rebind<C<T, Args...>, U> { using type = C<U, Args...>; };

export using SX = S<x>;
export using SParenX = S<(x)>;

//--- Use.cpp
// expected-no-diagnostics
import A;

static_assert(kind<int> == 1 && kind<Box> == 2 && kind<1> == 3 &&
              kind<x> == 4);
static_assert(P<int, 1, Box, x>::size == 4);
static_assert(__is_same(Apply<IntRef, x>, IntRef<x>));
static_assert(f<int>() == 2 && f<x>() == 1);
static_assert(Member<int>{}.g(S<int>{}, S<1>{}) == 3);
static_assert(__is_same(Rebind<Vec<int>, long>::type, Vec<long, 4>));
static_assert(__is_same(SX, S<x>));
static_assert(!__is_same(SX, SParenX));
