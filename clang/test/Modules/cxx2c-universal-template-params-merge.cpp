// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: %clang_cc1 -std=c++2c -fmodules -fimplicit-module-maps \
// RUN:   -fmodules-cache-path=%t/cache -I%t %t/Merge.cpp -verify -fsyntax-only
// RUN: %clang_cc1 -std=c++2c -fmodules -fimplicit-module-maps \
// RUN:   -fmodules-cache-path=%t/cache -I%t %t/Mismatch.cpp -verify -fsyntax-only

//--- module.modulemap
module A { header "A.h" export * }
module B { header "B.h" export * }
module First { header "First.h" export * }
module Second { header "Second.h" export * }

//--- Common.h
#ifndef COMMON_H
#define COMMON_H
template <universal template U> struct S {};
template <universal template... Us> struct P {};
template <typename T> struct Box {};
inline int x;

template <universal template U> struct X {
  template <universal template V> constexpr int f(S<U>, S<V>) { return 1; }
  template <universal template... Vs> using type = P<U, Vs...>;
  S<x> member;
};

template <universal template U> constexpr int g(S<U>) { return 2; }
template <class T> constexpr int g(S<T>) { return 3; }
template <universal template U> constexpr int h() { return 4; }
template <class T> constexpr int h() { return 5; }

template <universal template> constexpr int kind = 0;
template <typename T> constexpr int kind<T> = 1;
template <decltype(auto) V> constexpr int kind<V> = 2;
template <int &R> constexpr int kind<R> = 3;
#endif

//--- A.h
#include "Common.h"

//--- B.h
#include "Common.h"

//--- Merge.cpp
// expected-no-diagnostics
#include "A.h"
#include "B.h"

static_assert(X<int>{}.f(S<int>{}, S<1>{}) == 1);
static_assert(__is_same(X<int>::type<1, Box>, P<int, 1, Box>));
static_assert(__is_same(decltype(X<int>::member), S<x>));
static_assert(g(S<1>{}) == 2 && g(S<int>{}) == 3);
static_assert(h<1>() == 4 && h<int>() == 5);
static_assert(kind<Box> == 0 && kind<int> == 1 && kind<1> == 2 &&
              kind<x> == 3);

//--- First.h
struct Kind {
  template <universal template U> void f();
  template <typename U> void f();
};
struct Pack {
  template <universal template U> void f();
  template <universal template... U> void f();
};

//--- Second.h
struct Kind {
  template <typename U> void f();
  template <universal template U> void f();
};
struct Pack {
  template <universal template... U> void f();
  template <universal template U> void f();
};

//--- Mismatch.cpp
#include "First.h"
#include "Second.h"

Kind k;
// expected-error@First.h:* {{'Kind' has different definitions in different modules; first difference is definition in module 'First' found function template 'f' with 1st template parameter being a universal template parameter}}
// expected-note@Second.h:* {{but in 'Second' found function template 'f' with 1st template paramter being a type template parameter}}
Pack p;
// expected-error@First.h:* {{'Pack' has different definitions in different modules; first difference is definition in module 'First' found function template 'f' with 1st template parameter not being a template parameter pack}}
// expected-note@Second.h:* {{but in 'Second' found function template 'f' with 1st template parameter being a template parameter pack}}
