// RUN: %clang_cc1 -std=c++23 -fsyntax-only -verify %s

// Without -fexperimental-auto-nsdmi, a data member may not be declared with a
// placeholder type.

struct S {
  auto i = 42;          // expected-error {{'auto' not allowed in non-static struct member}}
  decltype(auto) d = 1; // expected-error {{'decltype(auto)' not allowed in non-static struct member}}
};

class C {
  auto i = 42; // expected-error {{'auto' not allowed in non-static class member}}
};

union U {
  auto i = 42; // expected-error {{'auto' not allowed in non-static union member}}
};

// Static data members are unaffected; they have always allowed this.
struct WithStatic {
  static constexpr auto k = 7;
};
static_assert(__is_same(decltype(WithStatic::k), const int));
