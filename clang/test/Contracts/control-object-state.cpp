// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++26 -fcontracts -triple x86_64-linux-gnu -emit-llvm -o - %s \
// RUN:   | FileCheck %s
// expected-no-diagnostics

// D4324: pre<...> names a constant-expression control object, so a control type
// may carry real per-instance state. Two constexpr objects of the same control
// type each carry a different diagnostic string, and naming a different one on
// two assertions must pass that particular object to operator() rather than
// collapsing both onto one shared (or default-constructed) instance.
//
// Ported from GCC's g++.dg/contracts/cpp26/d4324-control-object-state.C, which
// checks the same property by running the program; here the object identity is
// checked in the IR instead, since the Contracts cc1 suite has no runtime.

#include "Inputs/assertion_control.h"
using namespace std::contracts;

struct labeled {
  const char *label;
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  static constexpr bool assumable = false;
  void operator()(const assertion_context &ctx) const {
    (void)ctx.check();
  }
};

inline constexpr labeled first{"first diagnostic"};
inline constexpr labeled second{"second diagnostic"};

// Each function passes the address of its own control object, so the two calls
// really do observe two distinct instances.
// CHECK-LABEL: define {{.*}} @_Z1fi(
// CHECK: call void @{{.*}}7labeled{{.*}}(ptr {{[^,]*}}@first,
int f(int x) pre<first>(x > 0) { return x; }

// CHECK-LABEL: define {{.*}} @_Z1gi(
// CHECK: call void @{{.*}}7labeled{{.*}}(ptr {{[^,]*}}@second,
int g(int x) pre<second>(x > 0) { return x; }

// A prvalue temporary, built via aggregate paren-init, named directly in
// pre<...> instead of through a separate named constexpr object.
// CHECK-LABEL: define {{.*}} @_Z1ji(
// CHECK: call void @{{.*}}7labeled{{.*}}(ptr
int j(int x) pre<labeled("temp diagnostic")>(x > 0) { return x; }

// The braced form of the same thing.
// CHECK-LABEL: define {{.*}} @_Z1ki(
// CHECK: call void @{{.*}}7labeled{{.*}}(ptr
int k(int x) pre<labeled{"braced diagnostic"}>(x > 0) { return x; }

// A stateful control object also works on contract_assert and post.
void h(int x) { contract_assert<second>(x > 0); }
int p(const int x) post<first>(x > 0) { return x; }
