// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -ast-dump %s | FileCheck %s
// expected-no-diagnostics

// Checks that the D4324 control-object type argument on pre/post/contract_assert
// is parsed and recorded on the ContractStmt, and that the unadorned forms
// leave it unset.

#include "Inputs/assertion_control.h"
using namespace std::contracts;

int f(int x) pre<review>(x > 0) post<mandatory>(r: r > 0) {
  // CHECK: ContractStmt {{.*}} pre {{.*}}control review
  // CHECK: ContractStmt {{.*}} post {{.*}}control mandatory
  contract_assert<default_control>(x != 0);
  // CHECK: ContractStmt {{.*}} contract_assert {{.*}}control default_control
  contract_assert(x != 0);
  // CHECK: ContractStmt {{.*}} contract_assert enforce{{$}}
  return x;
}

// A qualified control-object name is accepted too.
int h(int x) pre<std::contracts::review>(x > 0) { return x; }
// CHECK: ContractStmt {{.*}} pre {{.*}}control {{.*}}review

// The unadorned form records no control object.
int g(int x) pre(x > 0) { return x; }
// CHECK: ContractStmt {{.*}} pre enforce{{$}}
