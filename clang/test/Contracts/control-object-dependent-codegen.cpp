// RUN: %clang_cc1 -std=c++26 -fcontracts -emit-llvm -o - %s | FileCheck %s

// D4324: when the control object is dependent, the decisions the compiler makes
// about a contract have to be made per instantiation. control-object-dependent.cpp
// covers the ones that show up as diagnostics; this covers the ones that only
// show up in the emitted code - whether the check is there at all, and whether an
// ignored contract is still handed to the optimizer.

#include "Inputs/assertion_control.h"

using namespace std::contracts;

template <bool Ignore, bool Assume> struct ctl {
  static consteval bool is_ignored(assertion_static_info) { return Ignore; }
  static consteval bool constify(assertion_static_info) { return false; }
  static constexpr bool assumable = Assume;
  violation_response operator()(const char *, std::source_location,
                                evaluation_semantic) const {
    return violation_response::terminate;
  }
};

// The control object is a nested class of a class template, so which
// specialization of ctl governs the contract is only known once T is.
template <class T> struct Outer {
  struct Nested {
    static constexpr ctl<sizeof(T) == 1, false> c{};
    static int f(int x) pre<c>(x > 0) { return x; }
  };
};

// sizeof(int) != 1, so is_ignored() is false and the predicate is checked.
// CHECK-LABEL: define {{.*}} @_ZN5OuterIiE6Nested1fEi
// CHECK: br i1
int checked(int x) { return Outer<int>::Nested::f(x); }

// sizeof(char) == 1, so is_ignored() is true: no predicate, no handler call.
// CHECK-LABEL: define {{.*}} @_ZN5OuterIcE6Nested1fEi
// CHECK-NOT: br i1
// CHECK: ret

int skipped(int x) { return Outer<char>::Nested::f(x); }

// An ignored but assumable contract still reaches the optimizer as an
// assumption, and that pairing is likewise decided per instantiation.
template <class T> struct Assumed {
  static constexpr ctl<true, sizeof(T) == 1> c{};
  static int f(int x) pre<c>(x > 0) { return x; }
};

// Ignored and not assumable: nothing at all is emitted for the contract.
// CHECK-LABEL: define {{.*}} @_ZN7AssumedIiE1fEi
// CHECK-NOT: llvm.assume
// CHECK: ret
int not_assumed(int x) { return Assumed<int>::f(x); }

// Ignored and assumable: the predicate survives as an assumption.
// CHECK-LABEL: define {{.*}} @_ZN7AssumedIcE1fEi
// CHECK: call void @llvm.assume
int assumed(int x) { return Assumed<char>::f(x); }
