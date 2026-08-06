// RUN: %clang_cc1 -std=c++26 -fcontracts -triple x86_64-linux-gnu -emit-llvm -o - %s \
// RUN:   | FileCheck %s --implicit-check-not=__handle_contract_violation

// D4324 CodeGen: a contract naming an explicit control object does not evaluate
// its predicate in place. The predicate is emitted as a standalone
// `bool(void **)`, and the enclosing function only builds an assertion_context
// describing the evaluation and hands it to the object:
//
//   1. if T::is_ignored(info): stop (see control-object-assume.cpp);
//   2. otherwise call obj(ctx), and let the object decide whether to evaluate
//      the predicate via ctx.check(), how many times, and what to do about it.
//
// So there is no branch on the predicate here and no trap: both belong to the
// control object now. The built-in __handle_contract_violation dispatch is never
// used either, which --implicit-check-not above enforces.

#include "Inputs/assertion_control.h"
using namespace std::contracts;

// The address of every entity the predicate reads travels in a void * array. A
// one-parameter predicate needs one slot, holding the parameter's address.
// CHECK-LABEL: define {{.*}} @_Z8f_reviewi(
// CHECK: %[[ARGS:.*]] = alloca [1 x ptr]
// CHECK: store ptr %x.addr, ptr %[[ARGS]]
// CHECK: %[[DECAY:.*]] = getelementptr inbounds [1 x ptr], ptr %[[ARGS]], i64 0, i64 0
// CHECK: call void @{{.*}}__create_assertion_context{{.*}}(ptr {{.*}} %[[CTX:.*]], ptr noundef @.str, {{.*}}ptr noundef @[[CHECK0:[_A-Za-z0-9]*__contract_check[_A-Za-z0-9]*]], ptr noundef %[[DECAY]])
// CHECK: call void @{{.*}}6review{{.*}}assertion_context{{.*}}(ptr {{.*}}@_ZNSt9contracts8review_vE, ptr {{.*}} %[[CTX]])
// CHECK-NOT: br i1
// CHECK: ret i32
int f_review(int x) pre<review_v>(x > 0) { return x; }

// The predicate itself lives in the checker, reached through the array rather
// than through the parameter directly.
// CHECK: define {{.*}} i1 @[[CHECK0]](ptr noundef %__args)
// CHECK: %[[SLOT:.*]] = getelementptr inbounds ptr, ptr %{{.*}}, i32 0
// CHECK: %[[ADDR:.*]] = load ptr, ptr %[[SLOT]]
// CHECK: %[[VAL:.*]] = load i32, ptr %[[ADDR]]
// CHECK: icmp sgt i32 %[[VAL]], 0

// mandatory takes the same shape; only the object called differs.
// CHECK-LABEL: define {{.*}} @_Z11f_mandatoryi(
// CHECK: call void @{{.*}}__create_assertion_context
// CHECK: call void @{{.*}}9mandatory{{.*}}assertion_context
// CHECK-NOT: br i1
// CHECK: ret i32
int f_mandatory(int x) pre<mandatory_v>(x > 0) { return x; }

// contract_assert with an explicit control object flows through the same path.
// CHECK-LABEL: define {{.*}} @_Z8f_asserti(
// CHECK: call void @{{.*}}__create_assertion_context
// CHECK: call void @{{.*}}15default_control{{.*}}assertion_context
int f_assert(int x) {
  contract_assert<default_v>(x != 0);
  return x;
}

// A temporary control object is materialized for the call rather than loaded
// from a named object, and is otherwise handled identically.
// CHECK-LABEL: define {{.*}} @_Z6f_tempi(
// CHECK: call void @{{.*}}__create_assertion_context
// CHECK: call void @{{.*}}6review{{.*}}assertion_context
int f_temp(int x) pre<review{}>(x > 0) { return x; }
