// RUN: %clang_cc1 -std=c++26 -fcontracts -triple x86_64-linux-gnu -emit-llvm -o - %s \
// RUN:   | FileCheck %s --implicit-check-not=__handle_contract_violation

// D4324 CodeGen: a contract that names an explicit control object T emits the
// three-step algorithm driven by T, and never the built-in
// __handle_contract_violation dispatch (enforced by --implicit-check-not above):
//
//   1. if T::is_ignored(cfg): stop;
//   2. evaluate the predicate;
//   3. if false, r = T{}(comment, loc, cfg); if r == terminate, trap.
//
// The default build semantic is 'enforce', which maps to evaluation_config
// value 2, so is_ignored is false for all three worked control objects and the
// predicate is always checked.

#include "Inputs/assertion_control.h"
using namespace std::contracts;

// review proceeds after reporting: operator() is called and its result compared
// against violation_response::terminate (1); the trap is on the terminate edge.
// CHECK-LABEL: define {{.*}} @_Z8f_reviewi(
// CHECK: %[[PRED:.*]] = icmp sgt i32 {{.*}}, 0
// CHECK: br i1 %[[PRED]], label %contract.end, label %contract.violation
// CHECK: contract.violation:
// CHECK: %[[R:.*]] = call noundef i32 @{{.*}}6review{{.*}}, i32 noundef 2)
// CHECK: %[[T:.*]] = icmp eq i32 %[[R]], 1
// CHECK: br i1 %[[T]], label %contract.trap, label %contract.end
// CHECK: contract.trap:
// CHECK: call void @llvm.trap()
// CHECK: unreachable
int f_review(int x) pre<review>(x > 0) { return x; }

// mandatory terminates: same shape, calling the mandatory control operator.
// CHECK-LABEL: define {{.*}} @_Z11f_mandatoryi(
// CHECK: br i1 {{.*}}, label %contract.end, label %contract.violation
// CHECK: %[[R2:.*]] = call noundef i32 @{{.*}}9mandatory{{.*}}, i32 noundef 2)
// CHECK: icmp eq i32 %[[R2]], 1
// CHECK: call void @llvm.trap()
int f_mandatory(int x) pre<mandatory>(x > 0) { return x; }

// contract_assert with an explicit control object flows through the same path.
// CHECK-LABEL: define {{.*}} @_Z8f_asserti(
// CHECK: call noundef i32 @{{.*}}15default_control{{.*}}, i32 noundef 2)
// CHECK: call void @llvm.trap()
int f_assert(int x) {
  contract_assert<default_control>(x != 0);
  return x;
}
