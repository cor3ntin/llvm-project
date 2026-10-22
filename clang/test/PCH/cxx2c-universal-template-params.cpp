// RUN: %clang_cc1 -std=c++2c -include %s -verify %s
//
// RUN: %clang_cc1 -std=c++2c -emit-pch -o %t %s
// RUN: %clang_cc1 -std=c++2c -include-pch %t -verify %s
//
// RUN: %clang_cc1 -std=c++2c -emit-pch -fpch-instantiate-templates -o %t %s
// RUN: %clang_cc1 -std=c++2c -include-pch %t -verify %s

// expected-no-diagnostics

#ifndef HEADER
#define HEADER

template <universal template U> struct S {};
template <universal template... Us> struct P {
  static constexpr int size = sizeof...(Us);
};
template <universal template> struct Unnamed {};
template <typename T> struct Box {};
template <int &> struct IntRef { static constexpr bool value = true; };

int x;
struct Agg { int m; } agg;
template <typename T> constexpr int vt = 0;
template <typename T> concept Con = true;

template <universal template> constexpr int kind = 0;
template <typename T> constexpr int kind<T> = 1;
template <template <universal template...> class TT> constexpr int kind<TT> = 2;
template <template <universal template...> auto VT> constexpr int kind<VT> = 3;
template <template <universal template...> concept CT> constexpr int kind<CT> = 4;
template <decltype(auto) V> constexpr int kind<V> = 5;
template <int &R> constexpr int kind<R> = 6;

template <universal template U> struct K { static constexpr int v = 0; };
template <auto V> struct K<V> { static constexpr int v = 1; };
template <typename T> struct K<T> { static constexpr int v = 2; };

template <template <universal template> class Z, universal template U>
using Apply = Z<U>;
template <universal template U> struct Through : IntRef<U> {};

template <universal template U> constexpr int f() { return 1; }
template <class T> constexpr int f() { return 2; }
template <auto V> constexpr int f() { return 3; }

// Declared here, defined in the main file.
template <universal template U> constexpr int g(S<U>);

template <universal template U> struct Member {
  template <universal template V> constexpr int h(S<U>, S<V>);
  template <universal template... Vs> struct Inner {
    using type = P<U, Vs...>;
  };
};
template <universal template U>
template <universal template V>
constexpr int Member<U>::h(S<U>, S<V>) { return 7; }

template <class T, unsigned long N = 4> struct Vec {};
template <typename, typename> struct Rebind;
template <template <class, universal template...> class C, class T,
          universal template... Args, class U>
struct Rebind<C<T, Args...>, U> { using type = C<U, Args...>; };

template <class T, universal template... Us> struct D { D(T, S<Us>...) {} };

// Specializations formed in the header.
using SInt = S<int>;
using SX = S<x>;
using SParenX = S<(x)>;
using SBox = S<Box>;
using SMember = S<agg.m>;
inline constexpr int KindOfX = kind<x>;

// Partially applied concepts.
template <class T, class U> concept SameAs = __is_same(T, U);
template <template <class> concept C> struct Holder {
  template <class T> static constexpr bool check = C<T>;
};
using HoldsSameAsInt = Holder<concept SameAs<int>>;
template <template <class> concept C> constexpr bool CheckLong = C<long>;

#else

static_assert(kind<int> == 1 && kind<Box> == 2 && kind<vt> == 3 &&
              kind<Con> == 4 && kind<1> == 5 && kind<x> == 6);
static_assert(K<1>::v == 1 && K<int>::v == 2 && K<Box>::v == 0);
static_assert(P<>::size == 0 && P<int, 1, Box, x>::size == 4);
Unnamed<1> u;

static_assert(__is_same(Apply<IntRef, x>, IntRef<x>));
static_assert(Through<agg.m>::value);

static_assert(f<int>() == 2 && f<0>() == 3 && f<x>() == 1);

template <universal template V> constexpr int g(S<V>) { return 5; }
static_assert(g(S<int>{}) == 5);

static_assert(Member<int>{}.h(S<int>{}, S<1>{}) == 7);
static_assert(__is_same(Member<int>::Inner<1, x>::type, P<int, 1, x>));

static_assert(__is_same(Rebind<Vec<int>, long>::type, Vec<long, 4>));
static_assert(__is_same(Rebind<Vec<int, 8>, long>::type, Vec<long, 8>));

D d(1, S<int>{}, S<2>{});
static_assert(__is_same(decltype(d), D<int, int, 2>));

static_assert(__is_same(SInt, S<int>));
static_assert(__is_same(SX, S<x>));
static_assert(!__is_same(SX, SParenX));
static_assert(__is_same(SParenX, S<(x)>));
static_assert(__is_same(SBox, S<Box>));
static_assert(__is_same(SMember, S<agg.m>));
static_assert(KindOfX == 6);

static_assert(HoldsSameAsInt::check<int>);
static_assert(!HoldsSameAsInt::check<long>);
static_assert(!CheckLong<concept SameAs<int>>);
static_assert(CheckLong<concept SameAs<long>>);

#endif
