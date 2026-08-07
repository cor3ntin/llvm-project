// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++26 -fcontracts -triple x86_64-linux-gnu -emit-llvm -o - %s \
// RUN:   | FileCheck %s
// expected-no-diagnostics

// D4324: a control object named as a temporary must work inside a template just
// as it does outside one, whether or not it mentions a template parameter. The
// contract is re-synthesized on instantiation, so the temporary has to survive
// being transformed as an expression rather than being left as an unresolved
// node.
//
// Ported from GCC's
// g++.dg/contracts/cpp26/d4324-control-object-temporary-in-template.C, where
// exactly this shape ICE'd.

#include "Inputs/assertion_control.h"
using namespace std::contracts;

struct probe {
  const char *message = nullptr;
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  void operator()(const assertion_context &ctx) const {
    (void)ctx.check();
  }
};

// A temporary control object that does not mention T at all.
template <class T> struct V {
  T arr[4];

  T &at(int n) {
    contract_assert<probe("n < 4")>(n < 4);
    return arr[n];
  }
};

// CHECK-LABEL: define {{.*}} @_ZN1VIiE2atEi(
// CHECK: call void @{{.*}}5probe
int use(V<int> &v) { return v.at(2); }

// A dependent control object supplied as a non-type template argument.
template <auto C> int dep(int x) pre<C>(x > 0) { return x; }

// CHECK-LABEL: define {{.*}} @_Z3dep{{.*}}review{{.*}}(
template int dep<review_v>(int);

// A control object that is a temporary whose state genuinely depends on a
// template parameter of the enclosing template, so the control expression is
// value-dependent until instantiation.
struct sized {
  unsigned width;
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  void operator()(const assertion_context &ctx) const {
    (void)ctx.check();
  }
};

template <class T> int with_state(int x) pre<sized{sizeof(T)}>(x > 0) {
  return x;
}
template int with_state<int>(int);
template int with_state<double>(int);

// A dependent control object on a member of a class template, exercising the
// re-synthesis path through TransformContractStmt twice over.
template <class T> struct W {
  int f(int x) pre<probe("member")>(x > 0) { return x; }
};
template struct W<int>;
