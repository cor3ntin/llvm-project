// RUN: %clang_cc1 -std=c++2c -fsyntax-only -verify -fexperimental-auto-nsdmi %s

// expected-no-diagnostics

// The body of an expansion statement is a template region, so a local class
// declared in it has its members instantiated. For a member whose type is
// deduced the initializer is attached when the field is created, rather than
// when it is first needed, and must not then be attached a second time.

constexpr int in_region() {
  int total = 0;
  template for (constexpr auto _ : {0}) {
    struct L {
      auto x = 2;
      auto y = 0.5;
    };
    L l;
    total += l.x;
  }
  return total;
}
static_assert(in_region() == 2);

// Nested regions reinstantiate the class again.
constexpr int nested() {
  int total = 0;
  template for (constexpr auto _ : {0}) {
    template for (constexpr auto __ : {0}) {
      struct L {
        auto x = 3;
      };
      L l;
      total += l.x;
    }
  }
  return total;
}
static_assert(nested() == 3);
