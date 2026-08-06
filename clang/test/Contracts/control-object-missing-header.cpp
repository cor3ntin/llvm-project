// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s

// D4324: the compiler describes a contract to its control object by calling
// std::__create_assertion_static_info, which <contracts> declares. Naming a
// control object without that header in scope is a mistake worth spelling out,
// rather than failing somewhere inside a synthesized call.

// Deliberately no #include: a hand-rolled control object that never saw the
// library. It satisfies everything the compiler reads off the type itself.
struct my_info {};
struct my_control {
  static consteval bool is_ignored(my_info) { return false; }
  static consteval bool constify(my_info) { return false; }
  static constexpr bool assumable = false;
  int operator()(const char *, int, int) const { return 0; }
};
inline constexpr my_control ctl{};

// expected-error@+1 {{cannot find 'std::__create_assertion_static_info'; include <contracts> to name an assertion-control object}}
int f(int x) pre<ctl>(x > 0) { return x; }

// One diagnostic per translation unit is enough; the rest would all say the
// same thing about the same missing include.
int g(int x) post<ctl>(r: r > 0) { return x; }

void h(int x) { contract_assert<ctl>(x > 0); }

// A contract with no control object never needs the library.
int no_control(int x) pre(x > 0) { return x; }
