// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -ast-dump %s | FileCheck %s
// expected-no-diagnostics

// Checks that the D4324 control-object argument on pre/post/contract_assert is
// parsed as a constant expression naming a control object, recorded on the
// ContractStmt as a child expression, and that the unadorned and empty-'<>'
// forms leave it unset.

#include "Inputs/assertion_control.h"
using namespace std::contracts;

int f(int x) pre<review_v>(x > 0) post<mandatory_v>(r: r > 0) {
  // CHECK: ContractStmt {{.*}} pre {{.*}}control
  // CHECK-NEXT: DeclRefExpr {{.*}} 'review_v'
  // CHECK: ContractStmt {{.*}} post {{.*}}control
  contract_assert<default_v>(x != 0);
  // CHECK: ContractStmt {{.*}} contract_assert {{.*}}control
  // CHECK-NEXT: DeclRefExpr {{.*}} 'default_v'
  contract_assert(x != 0);
  // CHECK: ContractStmt {{.*}} contract_assert enforce{{$}}
  return x;
}

// A qualified control-object name is accepted too.
int h(int x) pre<std::contracts::review_v>(x > 0) { return x; }
// CHECK: ContractStmt {{.*}} pre {{.*}}control

// The control object may be an anonymous temporary rather than a named object.
int j(int x) pre<review()>(x > 0) { return x; }
// CHECK: ContractStmt {{.*}} pre {{.*}}control
int k(int x) pre<review{}>(x > 0) { return x; }
// CHECK: ContractStmt {{.*}} pre {{.*}}control

// An object of a class-template specialization type works as well.
template <class T> struct ctl {
  static constexpr bool is_ignored(evaluation_config) { return false; }
  static constexpr bool constify = false;
  static constexpr bool assumable = false;
  violation_response operator()(const char *, std::source_location,
                                evaluation_config) const {
    return violation_response::proceed;
  }
};
inline constexpr ctl<int> ctl_int_v{};
int b(int x) pre<ctl_int_v>(x > 0) { return x; }
// CHECK: ContractStmt {{.*}} pre {{.*}}control

// The unadorned form records no control object.
int g(int x) pre(x > 0) { return x; }
// CHECK: ContractStmt {{.*}} pre enforce{{$}}

// An empty '<>' means the same thing as the unadorned form.
int e(int x) pre<>(x > 0) { return x; }
// CHECK: ContractStmt {{.*}} pre enforce{{$}}
int r(int x) post<>(res: res > 0) { return x; }
// CHECK: ContractStmt {{.*}} post enforce{{$}}
void empty_assert(int x) {
  contract_assert<>(x > 0);
  // CHECK: ContractStmt {{.*}} contract_assert enforce{{$}}
}

// A '<' that starts a predicate is still a comparison.
int lt(int x, int y) pre(x < y) { return x; }
// CHECK: ContractStmt {{.*}} pre enforce{{$}}
