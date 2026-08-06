// RUN: %clang_cc1 -std=c++26 -fcontracts -fcontract-evaluation-semantic=enforce -emit-llvm -o - %s | FileCheck %s --check-prefixes=CHECK,ENFORCE
// RUN: %clang_cc1 -std=c++26 -fcontracts -fcontract-evaluation-semantic=ignore -emit-llvm -o - %s | FileCheck %s --check-prefixes=CHECK,IGNORE

// D4324: the compiler describes each contract to its control object by
// synthesizing a call to std::__create_assertion_static_info and passing the
// result to is_ignored(). This checks the description is accurate: a control
// object that ignores on one semantic, or on one check side, gets the answer
// the build and the contract kind imply.

#include "Inputs/assertion_control.h"

using namespace std::contracts;

// Ignored exactly when the build semantic is 'ignore', like default_control.
struct follows_semantic {
  static consteval bool is_ignored(assertion_static_info info) {
    return info.semantic() == evaluation_semantic::ignore;
  }
  static consteval bool constify(assertion_static_info) { return false; }
  static constexpr bool assumable = false;
  violation_response operator()(const char *, std::source_location,
                                evaluation_semantic) const {
    return violation_response::terminate;
  }
};
inline constexpr follows_semantic follows_v{};

// Checks only what sits on a call boundary, so a contract_assert is ignored
// while a precondition is not, whatever the build semantic is.
struct boundary_only {
  static consteval bool is_ignored(assertion_static_info info) {
    return info.side() == assertion_check_side::not_applicable;
  }
  static consteval bool constify(assertion_static_info) { return false; }
  static constexpr bool assumable = false;
  violation_response operator()(const char *, std::source_location,
                                evaluation_semantic) const {
    return violation_response::terminate;
  }
};
inline constexpr boundary_only boundary_v{};

// An ignored contract emits no predicate and no handler call at all, so the
// presence of the branch is what says whether is_ignored() came back false.

// CHECK-LABEL: define {{.*}} @_Z8semantic
// ENFORCE: br i1
// IGNORE-NOT: br i1
int semantic(int x) pre<follows_v>(x > 0) { return x; }

// The side is a property of the contract, not of the build, so this one is
// checked under both semantics.
// CHECK-LABEL: define {{.*}} @_Z12boundary_prei
// CHECK: br i1
int boundary_pre(int x) pre<boundary_v>(x > 0) { return x; }

// ... and this one never is, because a contract_assert has no side.
// CHECK-LABEL: define {{.*}} @_Z15boundary_asserti
// CHECK-NOT: br i1
// CHECK: ret
int boundary_assert(int x) {
  contract_assert<boundary_v>(x > 0);
  return x;
}
