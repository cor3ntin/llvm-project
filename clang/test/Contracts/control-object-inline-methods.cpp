// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -ast-dump %s | FileCheck %s
// expected-no-diagnostics

// A contract on an inline member function declaration is late-parsed: its tokens
// are cached and replayed once the enclosing class is complete. That caching has
// to carry the control-object argument along, and since the argument is a
// constant expression it may contain '<' and '>' of its own - as a nested
// template argument list, inside a parenthesized comparison, or inside a braced
// initializer - none of which may be mistaken for the closing '>'.

#include "Inputs/assertion_control.h"
using namespace std::contracts;

template <class T> struct ctl {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  static constexpr bool assumable = false;
  void operator()(const assertion_context &ctx) const {
    (void)ctx.check();
  }
};

struct tagged {
  const char *tag;
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  static constexpr bool assumable = false;
  void operator()(const assertion_context &ctx) const {
    (void)ctx.check();
  }
};

struct Foo {
  // Simple named control object on an inline member function.
  int a(int x) pre<review_v>(x > 0) { return x; }
  // CHECK: ContractStmt {{.*}} pre {{.*}}control

  // A nested template argument list inside the control object expression.
  int b(int x) pre<ctl<int>{}>(x > 0) { return x; }
  // CHECK: ContractStmt {{.*}} pre {{.*}}control

  // A '>' inside a parenthesized comparison must not close the argument.
  int c(int x, int y) pre<(sizeof(int) > 2) ? review_v : review_v>(x > y) {
    return x;
  }
  // CHECK: ContractStmt {{.*}} pre {{.*}}control

  // Braces, and a string literal containing '>' and '<'.
  int d(int x) pre<tagged{"a > b, c < d"}>(x > 0) { return x; }
  // CHECK: ContractStmt {{.*}} pre {{.*}}control

  // The whole point of late parsing: the control object may be declared later
  // in the class than the contract that names it.
  int e(int x) pre<later_v>(x > 0) { return x; }
  // CHECK: ContractStmt {{.*}} pre {{.*}}control

  // Both a control object and a member referenced by the predicate come from
  // later in the class.
  int f(int x) pre<later_v>(x > limit) { return x; }
  // CHECK: ContractStmt {{.*}} pre {{.*}}control

  static constexpr review later_v{};
  static constexpr int limit = 0;

  // Declared inline, defined out of line.
  int g(int x) pre<review_v>(x > 0);
};

int Foo::g(int x) pre<review_v>(x > 0) { return x; }
// CHECK: ContractStmt {{.*}} pre {{.*}}control

// The unadorned and empty forms still late-parse correctly alongside them.
struct Bar {
  int a(int x) pre(x > later) { return x; }
  int b(int x) pre<>(x > later) { return x; }
  static constexpr int later = 0;
};
