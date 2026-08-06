// RUN: %clang_cc1 -std=c++26 -fcontracts -fcontract-evaluation-semantic=ignore \
// RUN:   -triple x86_64-linux-gnu -emit-llvm -o - %s \
// RUN:   | FileCheck %s --implicit-check-not='call void @llvm.assume'

// D4324 Step 1 of the algorithm: when a contract is ignored, an assumable
// control object still hands the predicate to the optimizer via llvm.assume;
// a non-assumable control object (and the default path) emit nothing.

#include "Inputs/assertion_control.h"
using namespace std::contracts;

namespace {
// Always ignored, and assumable: the predicate becomes an optimizer assumption.
struct assume_ignored {
  static consteval bool is_ignored(assertion_static_info) { return true; }
  static consteval bool constify(assertion_static_info) { return false; }
  static constexpr bool assumable = true;
  violation_response operator()(const char *, std::source_location,
                                evaluation_semantic) const {
    return violation_response::terminate;
  }
};
inline constexpr assume_ignored assume_ignored_v{};
} // namespace

// CHECK-LABEL: define {{.*}} @_Z8f_assumei(
// CHECK: call void @llvm.assume(
int f_assume(int x) pre<assume_ignored_v>(x > 0) { return x; }

// default_control is ignored under 'ignore' but is not assumable: no assume and
// no runtime check (the implicit-check-not above catches any stray llvm.assume).
// CHECK-LABEL: define {{.*}} @_Z9f_defaulti(
int f_default(int x) pre<default_v>(x > 0) { return x; }

// A plain contract in ignore mode is likewise fully elided.
// CHECK-LABEL: define {{.*}} @_Z7f_plaini(
int f_plain(int x) pre(x > 0) { return x; }
