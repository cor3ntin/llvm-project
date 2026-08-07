// RUN: %clang_cc1 -std=c++26 -fcontracts -fcontract-evaluation-semantic=ignore \
// RUN:   -emit-llvm -o - %s | FileCheck %s --implicit-check-not=__handle_contract_violation

// D4324 step 1: a contract whose control object reports it as ignored produces
// no code whatsoever. Not a predicate, not a context, not a call - the object is
// never even reached, so nothing it might do can be observed.

#include "Inputs/assertion_control.h"
using namespace std::contracts;

namespace {
// Always ignored, whatever the build semantic says.
struct always_ignored {
  static consteval bool is_ignored(assertion_static_info) { return true; }
  void operator()(const assertion_context &ctx) const {
    // Would trap if it ever ran, which it must not.
    if (!ctx.check())
      __builtin_trap();
  }
};
inline constexpr always_ignored always_ignored_v{};
} // namespace

// CHECK-LABEL: define {{.*}} @_Z9f_ignoredi(
// CHECK-NOT: __create_assertion_context
// CHECK-NOT: llvm.trap
// CHECK-NOT: br i1
// CHECK: ret i32
int f_ignored(int x) pre<always_ignored_v>(x > 0) { return x; }

// default_control's is_ignored() is true under the 'ignore' semantic, so the
// same holds for it.
// CHECK-LABEL: define {{.*}} @_Z9f_defaulti(
// CHECK-NOT: __create_assertion_context
// CHECK: ret i32
int f_default(int x) pre<default_v>(x > 0) { return x; }

// An object that is not ignored still gets its dispatch, which is what makes the
// checks above mean something.
// CHECK-LABEL: define {{.*}} @_Z9f_checkedi(
// CHECK: call void @{{.*}}__create_assertion_context
int f_checked(int x) pre<mandatory_v>(x > 0) { return x; }
