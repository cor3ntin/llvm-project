// RUN: %clang_cc1 -std=c++26 -fcontracts -fcxx-exceptions -fexceptions \
// RUN:   -triple x86_64-linux-gnu -emit-llvm -o - %s \
// RUN:   | FileCheck %s --implicit-check-not=contract.pred.value \
// RUN:                  --implicit-check-not=landingpad

// D4324: contract predicates are evaluated directly. A predicate that throws
// propagates to the caller; the compiler no longer wraps predicates in a
// try/catch that converts exceptions into contract violations (which showed up
// as a contract.pred.value alloca and a landingpad around the predicate).

bool may_throw();

// The predicate is a plain call (not an invoke into a contract landingpad), so
// an exception it raises unwinds normally out of the function.
// CHECK-LABEL: define {{.*}} @_Z1fi(
// CHECK: %[[C:.*]] = call noundef zeroext i1 @_Z9may_throwv()
// CHECK: br i1 %[[C]], label %contract.end, label %contract.violation
void f(int x) { contract_assert(may_throw()); }
