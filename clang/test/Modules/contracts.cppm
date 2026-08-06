// Contracts do not survive AST serialization yet, independently of the
// control-object work: ContractSpecifierDecl::CreateDeserialized builds the decl
// with a null DeclContext, and both the IsUninstantiated initializer in
// ContractSpecifierDecl's constructor and the reader's CXXRecordDecl dyn_cast
// then dereference it. Before this test existed the file was a byte-identical
// copy of lambdas.cppm, so nothing ever deserialized a contract.
// XFAIL: *

// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: split-file %s %t
//
// RUN: %clang_cc1 -std=c++26 -fcontracts %t/A.cppm -emit-module-interface \
// RUN:    -o %t/A.pcm
// RUN: %clang_cc1 -std=c++26 -fcontracts -fprebuilt-module-path=%t %t/Use.cpp \
// RUN:    -fsyntax-only -verify
// RUN: %clang_cc1 -std=c++26 -fcontracts -fprebuilt-module-path=%t %t/Use.cpp \
// RUN:    -triple x86_64-linux-gnu -emit-llvm -o - | FileCheck %t/Use.cpp
//
// Test again with reduced BMI.
// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: split-file %s %t
//
// RUN: %clang_cc1 -std=c++26 -fcontracts %t/A.cppm \
// RUN:    -emit-reduced-module-interface -o %t/A.pcm
// RUN: %clang_cc1 -std=c++26 -fcontracts -fprebuilt-module-path=%t %t/Use.cpp \
// RUN:    -fsyntax-only -verify

// Round-trips D4324 contracts through a module interface. The control object is
// stored on ContractStmt as a child expression and the synthesized violation
// call as a derived member, so this exercises both halves of the serialization.

//--- control.h
#ifndef CONTROL_H
#define CONTROL_H

namespace std {
class source_location {
  struct __impl {
    const char *_M_file_name;
    const char *_M_function_name;
    unsigned _M_line;
    unsigned _M_column;
  };
  const __impl *__ptr_ = nullptr;
  using __bsl_ty = decltype(__builtin_source_location());

public:
  static consteval source_location
  current(__bsl_ty __ptr = __builtin_source_location()) noexcept {
    source_location __sl;
    __sl.__ptr_ = static_cast<const __impl *>(__ptr);
    return __sl;
  }
  constexpr source_location() noexcept = default;
};
} // namespace std

namespace std::contracts {
enum class evaluation_semantic : unsigned {
  ignore = 0,
  enforce = 1,
  observe = 2,
  quick_enforce = 3,
};
enum class violation_response { proceed, terminate };

struct review {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  static constexpr bool assumable = false;
  violation_response operator()(const char *, std::source_location,
                                evaluation_semantic) const {
    return violation_response::proceed;
  }
};
inline constexpr review review_v{};

// A stateful control object, to check the control expression itself survives
// rather than being reconstructed from the type alone.
struct labeled {
  const char *label;
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  static constexpr bool assumable = false;
  violation_response operator()(const char *, std::source_location,
                                evaluation_semantic) const {
    return violation_response::proceed;
  }
};
inline constexpr labeled tagged_v{"from the module"};
} // namespace std::contracts

#endif // CONTROL_H

//--- A.cppm
module;
#include "control.h"
export module A;

using namespace std::contracts;

// An inline function: its body, control object, and synthesized violation call
// are all written to the BMI and read back.
export inline int named(int x) pre<review_v>(x > 0) { return x; }

// A stateful control object named in the interface.
export inline int tagged(int x) pre<tagged_v>(x > 0) { return x; }

// A temporary control object.
export inline int temporary(int x) pre<labeled("temp")>(x > 0) { return x; }

// A template: the contract is re-synthesized when the importer instantiates it.
export template <class T> T dependent(T x) pre<review_v>(x > 0) { return x; }

// A dependent control object supplied as a non-type template argument,
// instantiated here so the importer needs no name from control.h.
template <auto C> int with_control(int x) pre<C>(x > 0) { return x; }
export inline int uses_control(int x) { return with_control<review_v>(x); }

// The unadorned form still round-trips with no control object.
export inline int plain(int x) pre(x > 0) { return x; }

//--- Use.cpp
// expected-no-diagnostics
import A;

// CHECK-LABEL: define {{.*}} @_Z3usei(
int use(int x) {
  return named(x) + tagged(x) + temporary(x) + dependent<int>(x) +
         uses_control(x) + plain(x);
}

// The deserialized contracts still lower to the three-step algorithm driven by
// their control objects.
// CHECK: call noundef i32 @{{.*}}6review
// CHECK: call noundef i32 @{{.*}}7labeled
